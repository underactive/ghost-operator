/**
 * Nordic Legacy DFU protocol implementation for serial transport.
 *
 * Implements the SDK 11 DFU serial protocol used by the Adafruit nRF52 bootloader.
 * Reference: adafruit-nrfutil dfu_transport_serial.py
 *
 * Transfer sequence:
 *   1. Start DFU (opcode 3) — declare firmware mode and sizes
 *   2. Wait for flash erase (~eraseTime based on firmware size)
 *   3. Init packet (opcode 1) — send .dat file + 2-byte padding
 *   4. Data packets (opcode 4) — 512-byte chunks of .bin file
 *   5. Stop data (opcode 5) — bootloader validates, activates, and reboots
 *
 * Opcodes (from adafruit-nrfutil, NOT sequential):
 *   DFU_INIT_PACKET      = 1
 *   DFU_START_PACKET      = 3
 *   DFU_DATA_PACKET       = 4
 *   DFU_STOP_DATA_PACKET  = 5
 */
import { buildHciPacket, slipDecode } from './slip.js'
import * as serial from './serial.js'

// DFU opcodes — these are NOT sequential (matches adafruit-nrfutil exactly)
const DFU_INIT_PACKET = 1
const DFU_START_PACKET = 3
const DFU_DATA_PACKET = 4
const DFU_STOP_DATA_PACKET = 5

// Firmware update mode: application-only
const DFU_UPDATE_MODE_APP = 4

// Transfer chunk size — matches adafruit-nrfutil (512 bytes per data packet)
const DATA_CHUNK_SIZE = 512

// Number of chunks between page-write delays
const CHUNKS_PER_PAGE = 8

// Delay after page write (ms)
const PAGE_WRITE_DELAY = 100

// Flash erase time estimate: ~0.09s per 4KB page
const ERASE_MS_PER_PAGE = 90
const PAGE_SIZE = 4096

/**
 * HCI sequence number. Pre-incremented before each send (first packet = seq 1).
 * Matches Python's HciPacket class variable behavior.
 * Reset to 0 on ACK timeout (matches Python's get_ack_nr timeout handler).
 */
let seqNo = 0

/**
 * Send a packet and drain the ACK response from the bootloader.
 *
 * Matches adafruit-nrfutil's send_packet() behavior exactly:
 * - Send the packet once
 * - Read the ACK response (to drain the serial buffer)
 * - Do NOT validate the ACK — just move on
 * - On timeout (1s), reset sequence number to 0 (matches Python)
 *
 * The Python code never retries or validates ACKs. It sends each packet
 * once and reads the response purely to keep the serial buffer clear.
 */
async function sendPacket(payload) {
  // Pre-increment sequence number (matches Python HciPacket.__init__)
  seqNo = (seqNo + 1) % 8

  const packet = buildHciPacket(seqNo, payload)
  await serial.write(packet)

  // Drain the ACK response (1s timeout, matching Python's ACK_PACKET_TIMEOUT)
  try {
    const frame = await serial.readFrame(1000)
    slipDecode(frame)
  } catch {
    // Timeout — reset sequence number (matches Python's get_ack_nr timeout handler)
    seqNo = 0
  }
}

/**
 * Perform a complete DFU transfer over serial.
 *
 * @param {Uint8Array} datFile - Init packet (.dat file from DFU ZIP)
 * @param {Uint8Array} binFile - Firmware image (.bin file from DFU ZIP)
 * @param {function} onProgress - Callback: (percent, message) => void
 * @param {function} [onLog] - Terminal log (append-only, separate from progress bar): (message, level) => void
 */
export async function performDfu(datFile, binFile, onProgress, onLog = () => {}) {
  // Reset sequence number (first packet will pre-increment to 1)
  seqNo = 0

  const totalChunks = Math.ceil(binFile.length / DATA_CHUNK_SIZE)
  const transferStart = performance.now()
  let lastLoggedPct = -2 // force first log at 0%

  // ── Step 1: Start DFU ──────────────────────────────────────────────
  // Payload: [int32(DFU_START_PACKET)] [int32(mode)] [int32(sd_size)]
  //          [int32(bl_size)] [int32(app_size)]
  onProgress(0, 'Starting DFU...')
  // Terminal log (append-only, separate from progress bar)
  onLog('Starting DFU (application mode)', 'info')
  onLog(`Firmware: ${(binFile.length / 1024).toFixed(1)} KB (${binFile.length} bytes), ${totalChunks} chunks`, 'info')

  const startPayload = new Uint8Array(20)
  const startView = new DataView(startPayload.buffer)
  startView.setUint32(0, DFU_START_PACKET, true)     // opcode 3
  startView.setUint32(4, DFU_UPDATE_MODE_APP, true)   // mode 4 (app only)
  startView.setUint32(8, 0, true)                      // softdevice size
  startView.setUint32(12, 0, true)                     // bootloader size
  startView.setUint32(16, binFile.length, true)        // application size

  await sendPacket(startPayload)

  // ── Step 2: Wait for flash erase ───────────────────────────────────
  const pages = Math.ceil(binFile.length / PAGE_SIZE)
  const eraseMs = Math.max(pages * ERASE_MS_PER_PAGE, 1000)
  onProgress(2, `Erasing flash (${pages} pages)...`)
  // Terminal log (append-only, separate from progress bar)
  onLog(`Erasing flash (${pages} page${pages !== 1 ? 's' : ''})...`, 'info')
  const eraseStart = performance.now()
  await sleep(eraseMs)
  // Terminal log (append-only, separate from progress bar)
  onLog(`Erase wait complete (${((performance.now() - eraseStart) / 1000).toFixed(1)}s)`, 'info')

  // ── Step 3: Send init packet ───────────────────────────────────────
  // Payload: [int32(DFU_INIT_PACKET)] [dat_bytes...] [int16(0x0000)]
  // The 2-byte zero padding at the end matches adafruit-nrfutil exactly.
  onProgress(5, 'Sending init packet...')
  // Terminal log (append-only, separate from progress bar)
  onLog(`Sending init packet (${datFile.length} bytes)...`, 'info')
  const initPayload = new Uint8Array(4 + datFile.length + 2)
  new DataView(initPayload.buffer).setUint32(0, DFU_INIT_PACKET, true)
  initPayload.set(datFile, 4)
  // Last 2 bytes already 0x0000 from Uint8Array zero-initialization

  await sendPacket(initPayload)

  // Brief delay after INIT — give bootloader time to validate init data
  // and prepare for firmware reception before we start sending chunks.
  await sleep(300)

  // ── Step 4: Send firmware data in 512-byte chunks ──────────────────
  // Each chunk: [int32(DFU_DATA_PACKET)] [chunk_bytes]
  onProgress(8, 'Transferring firmware...')
  // Terminal log (append-only, separate from progress bar)
  onLog('Transferring firmware...', 'info')
  const dataStart = performance.now()

  for (let i = 0; i < totalChunks; i++) {
    const offset = i * DATA_CHUNK_SIZE
    const end = Math.min(offset + DATA_CHUNK_SIZE, binFile.length)
    const chunk = binFile.slice(offset, end)

    const dataPayload = new Uint8Array(4 + chunk.length)
    new DataView(dataPayload.buffer).setUint32(0, DFU_DATA_PACKET, true)
    dataPayload.set(chunk, 4)

    try {
      await sendPacket(dataPayload)
    } catch (err) {
      // Terminal log (append-only, separate from progress bar)
      onLog(`Error at chunk ${i + 1}/${totalChunks}: ${err.message}`, 'error')
      throw err
    }

    // Progress: 8% to 90% during data transfer
    const pct = 8 + Math.round((i + 1) / totalChunks * 82)
    onProgress(pct, `Transferring... ${i + 1}/${totalChunks} chunks`)

    // Terminal log at 2% intervals (append-only, separate from progress bar)
    if (pct - lastLoggedPct >= 2) {
      onLog(`Writing at 0x${offset.toString(16).toUpperCase().padStart(6, '0')}... (${pct}%)`, 'info')
      lastLoggedPct = pct
    }

    // Delay every N chunks for flash page write
    if ((i + 1) % CHUNKS_PER_PAGE === 0 && i + 1 < totalChunks) {
      await sleep(PAGE_WRITE_DELAY)
    }
  }

  // Terminal log (append-only, separate from progress bar)
  const dataElapsed = ((performance.now() - dataStart) / 1000).toFixed(1)
  onLog(`Wrote ${binFile.length} bytes (${(binFile.length / 1024).toFixed(1)} KB) in ${dataElapsed}s`, 'success')

  // ── Step 5: Stop data — triggers validate + activate + reboot ──────
  // Payload: [int32(DFU_STOP_DATA_PACKET)]
  // After this, the bootloader validates the firmware, activates it,
  // and reboots into the new application. No separate validate/activate
  // packets are needed (they are no-ops in the Python reference).
  onProgress(95, 'Finalizing...')
  // Terminal log (append-only, separate from progress bar)
  onLog('Finalizing...', 'info')
  const stopPayload = new Uint8Array(4)
  new DataView(stopPayload.buffer).setUint32(0, DFU_STOP_DATA_PACKET, true)

  // The bootloader may reboot immediately after receiving STOP, so
  // the ACK timeout in sendPacket is expected and harmless.
  await sendPacket(stopPayload)

  onProgress(100, 'Firmware update complete!')
  // Terminal log (append-only, separate from progress bar)
  onLog('Firmware update complete!', 'success')
}

function sleep(ms) {
  return new Promise(r => setTimeout(r, ms))
}
