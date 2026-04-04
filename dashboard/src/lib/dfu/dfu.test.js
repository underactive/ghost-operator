import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import * as serial from './serial.js'
import { performDfu } from './dfu.js'
import { slipDecode } from './slip.js'

vi.mock('./serial.js', () => ({
  write: vi.fn(),
  readFrame: vi.fn(),
}))

// Minimal valid SLIP frame: two SLIP_END bytes → decodes to empty Uint8Array
const ACK_FRAME = new Uint8Array([0xc0, 0xc0])

// Decode a SLIP-encoded HCI packet to its inner bytes:
// [4-byte header][payload bytes][2-byte CRC]
function innerBytes(slipPacket) {
  return slipDecode(slipPacket)
}

// Extract DFU opcode: first uint32 (little-endian) of payload, at offset 4 of inner bytes
function opcode(pkt) {
  const inner = innerBytes(pkt)
  return new DataView(inner.buffer, inner.byteOffset).getUint32(4, true)
}

// Extract HCI sequence number: bits 0–2 of byte 0
function seq(pkt) {
  return innerBytes(pkt)[0] & 0x07
}

// HCI payload length: inner.length minus 4-byte header and 2-byte CRC
function payloadSize(pkt) {
  return innerBytes(pkt).length - 6
}

beforeEach(() => {
  vi.clearAllMocks()
  serial.readFrame.mockResolvedValue(ACK_FRAME)
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

// Start DFU and advance all fake timers (erase wait, post-init delay, page delays)
async function runDfu(dat, bin, onProgress = () => {}) {
  const p = performDfu(dat, bin, onProgress)
  await vi.runAllTimersAsync()
  return p
}

describe('performDfu integration', () => {
  it('sends opcodes in correct 5-step sequence: START(3) → INIT(1) → DATA(4)×N → STOP(5)', async () => {
    await runDfu(new Uint8Array(2), new Uint8Array(512))

    const pkts = serial.write.mock.calls.map(c => c[0])
    expect(pkts).toHaveLength(4)  // START + INIT + 1 DATA + STOP
    expect(pkts.map(opcode)).toEqual([3, 1, 4, 5])
  })

  it('increments seqNo 1→2→…→7→0 with correct modulo-8 wraparound', async () => {
    // 6 data chunks → 9 packets total; seqNo wraps from 7 to 0 between DATA[4] and DATA[5]
    await runDfu(new Uint8Array(2), new Uint8Array(6 * 512))

    const pkts = serial.write.mock.calls.map(c => c[0])
    expect(pkts).toHaveLength(9)  // START + INIT + 6 DATA + STOP
    expect(pkts.map(seq)).toEqual([1, 2, 3, 4, 5, 6, 7, 0, 1])
  })

  it('handles non-512-byte-aligned firmware: last chunk is the remainder bytes', async () => {
    // 700 bytes → two chunks: 512 bytes then 188 bytes
    await runDfu(new Uint8Array(2), new Uint8Array(700))

    const pkts = serial.write.mock.calls.map(c => c[0])
    expect(pkts).toHaveLength(5)  // START + INIT + DATA1 + DATA2 + STOP
    // payloadSize = 4-byte DFU opcode prefix + chunk data bytes
    expect(payloadSize(pkts[2])).toBe(4 + 512)  // full first chunk
    expect(payloadSize(pkts[3])).toBe(4 + 188)  // partial last chunk
  })

  it('fires progress callbacks in monotonically increasing order through all phases', async () => {
    const progress = []
    await runDfu(new Uint8Array(2), new Uint8Array(512), (pct) => progress.push(pct))

    // START=0%, erase=2%, INIT=5%, data-start=8%, DATA(1/1)=90%, STOP=95%, done=100%
    expect(progress).toEqual([0, 2, 5, 8, 90, 95, 100])
    for (let i = 1; i < progress.length; i++) {
      expect(progress[i]).toBeGreaterThanOrEqual(progress[i - 1])
    }
  })

  it('ACK timeout resets seqNo to 0 so the next packet re-uses seq 1', async () => {
    // First readFrame call (START ACK) times out; subsequent calls succeed
    serial.readFrame
      .mockRejectedValueOnce(new Error('Serial read timeout'))
      .mockResolvedValue(ACK_FRAME)

    await runDfu(new Uint8Array(2), new Uint8Array(4))

    const pkts = serial.write.mock.calls.map(c => c[0])
    expect(seq(pkts[0])).toBe(1)  // START: 0→1 (normal pre-increment)
    expect(seq(pkts[1])).toBe(1)  // INIT: timeout reset seqNo to 0, then pre-incremented to 1
  })
})
