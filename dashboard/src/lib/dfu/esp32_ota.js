/**
 * ESP32 OTA firmware transfer over USB serial.
 *
 * Protocol (after the text handshake "!ota" / "+ok:ota"):
 *   1. Send [4 bytes LE uint32: firmware_size]
 *   2. Read "+ready\n" (device called Update.begin())
 *   3. For each 4096-byte chunk: send [raw bytes], read "+ok\n"
 *   4. Read "+done\n" (device called Update.end(), about to reboot)
 */

const CHUNK_SIZE = 4096

/**
 * Read a newline-terminated text line from a raw SerialPort.
 * Throws on timeout or if the line starts with "-err:".
 */
async function readLine(port, timeoutMs) {
  const decoder = new TextDecoder()
  let buf = ''
  let reader = null
  let timedOut = false

  const timer = setTimeout(() => {
    timedOut = true
    if (reader) {
      try { reader.cancel() } catch {}
    }
  }, timeoutMs)

  try {
    reader = port.readable.getReader()
    while (true) {
      const { value, done } = await reader.read()
      if (done || timedOut) break

      buf += decoder.decode(value, { stream: true })
      const nlIdx = buf.indexOf('\n')
      if (nlIdx !== -1) {
        const line = buf.substring(0, nlIdx).replace(/\r$/, '')
        clearTimeout(timer)
        reader.releaseLock()
        reader = null

        if (line.startsWith('-err:')) {
          throw new Error(`Device error: ${line.substring(5)}`)
        }
        return line
      }
    }

    if (timedOut) throw new Error('Serial read timeout')
    throw new Error('Serial port closed')
  } finally {
    clearTimeout(timer)
    if (reader) {
      try { reader.releaseLock() } catch {}
    }
  }
}

/**
 * Write raw bytes to a SerialPort.
 */
async function writeBytes(port, data) {
  const writer = port.writable.getWriter()
  try {
    await writer.write(data instanceof Uint8Array ? data : new Uint8Array(data))
  } finally {
    writer.releaseLock()
  }
}

/**
 * Perform an ESP32 OTA firmware update over a raw SerialPort.
 *
 * @param {SerialPort} port - Web Serial port (open, text read loop paused)
 * @param {Uint8Array} binFile - Firmware binary (.bin)
 * @param {function} onProgress - (percent, message) => void
 * @param {function} [onLog] - (message, level) => void
 */
export async function performEsp32Ota(port, binFile, onProgress, onLog = () => {}) {
  const totalChunks = Math.ceil(binFile.length / CHUNK_SIZE)

  onProgress(0, 'Starting OTA...')
  onLog(`Firmware: ${(binFile.length / 1024).toFixed(1)} KB (${binFile.length} bytes), ${totalChunks} chunks`, 'info')

  // --- Step 1: Send firmware size as 4-byte LE header ---
  const header = new Uint8Array(4)
  new DataView(header.buffer).setUint32(0, binFile.length, true)
  await writeBytes(port, header)
  onLog('Sent firmware size header', 'info')

  // --- Step 2: Wait for +ready ---
  onProgress(2, 'Waiting for device...')
  onLog('Waiting for device to prepare OTA partition...', 'info')
  const readyLine = await readLine(port, 10000)
  if (!readyLine.startsWith('+ready')) {
    throw new Error(`Unexpected response: ${readyLine}`)
  }
  onLog('Device ready, starting transfer', 'info')
  onProgress(5, 'Transferring firmware...')

  // --- Step 3: Send firmware in chunks ---
  const transferStart = performance.now()
  let lastLoggedPct = -2

  for (let i = 0; i < totalChunks; i++) {
    const offset = i * CHUNK_SIZE
    const end = Math.min(offset + CHUNK_SIZE, binFile.length)
    const chunk = binFile.slice(offset, end)

    await writeBytes(port, chunk)

    const ackLine = await readLine(port, 10000)
    if (!ackLine.startsWith('+ok')) {
      throw new Error(`Chunk ${i + 1} failed: ${ackLine}`)
    }

    // Progress: 5% to 95% during data transfer
    const pct = 5 + Math.round((i + 1) / totalChunks * 90)
    onProgress(pct, `Transferring... ${i + 1}/${totalChunks} chunks`)

    // Log at ~2% intervals
    if (pct - lastLoggedPct >= 2) {
      onLog(`Writing at 0x${offset.toString(16).toUpperCase().padStart(6, '0')}... (${pct}%)`, 'info')
      lastLoggedPct = pct
    }
  }

  const elapsed = ((performance.now() - transferStart) / 1000).toFixed(1)
  onLog(`Wrote ${binFile.length} bytes (${(binFile.length / 1024).toFixed(1)} KB) in ${elapsed}s`, 'success')

  // --- Step 4: Wait for +done ---
  onProgress(96, 'Finalizing...')
  onLog('Waiting for device to validate firmware...', 'info')
  const doneLine = await readLine(port, 30000)
  if (!doneLine.startsWith('+done')) {
    throw new Error(`Finalization failed: ${doneLine}`)
  }

  onProgress(100, 'Firmware update complete!')
  onLog('Firmware update complete! Device is rebooting.', 'success')
}
