import { describe, expect, it } from 'vitest'
import { crc16 } from './crc16.js'
import { buildHciPacket, parseAck, slipDecode, slipEncode } from './slip.js'

describe('slipEncode / slipDecode', () => {
  it('round-trips simple payloads', () => {
    const raw = new Uint8Array([1, 2, 3, 4])
    expect(slipDecode(slipEncode(raw))).toEqual(raw)
  })

  it('escapes SLIP_END and SLIP_ESC bytes', () => {
    const raw = new Uint8Array([0xc0, 0xdb, 0x00])
    expect(slipDecode(slipEncode(raw))).toEqual(raw)
  })
})

describe('buildHciPacket', () => {
  it('embeds CRC16 over header+payload in little-endian order', () => {
    const payload = new Uint8Array([0x06, 0x01, 0x02])
    const framed = buildHciPacket(2, payload)
    const inner = slipDecode(framed)
    expect(inner.length).toBe(4 + payload.length + 2)
    const crc = crc16(inner.subarray(0, 4 + payload.length))
    expect(inner[4 + payload.length] | (inner[4 + payload.length + 1] << 8)).toBe(crc)
  })
})

describe('parseAck', () => {
  it('reads ack sequence from byte 0 bits [3:5]', () => {
    const raw = new Uint8Array([0x18, 0, 0, 0])
    expect(parseAck(raw)).toBe(3)
  })

  it('returns -1 for short frames', () => {
    expect(parseAck(new Uint8Array([1, 2]))).toBe(-1)
  })
})
