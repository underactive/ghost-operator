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

  it('packs header bytes with correct bit layout', () => {
    // seq=2, ackNo=3, data_integrity=1, reliable=1, packet_type=14, len=3
    const payload = new Uint8Array([0x06, 0x01, 0x02])
    const framed = buildHciPacket(2, payload)
    const inner = slipDecode(framed)
    // b0 = seq(2) | ackNo(3)<<3 | 1<<6 | 1<<7 = 0x02 | 0x18 | 0x40 | 0x80 = 0xDA
    expect(inner[0]).toBe(0xDA)
    // b1 = packet_type(14) | (len(3) & 0x0F)<<4 = 0x0E | 0x30 = 0x3E
    expect(inner[1]).toBe(0x3E)
    // b2 = len >> 4 = 0 (len=3 fits in low nibble)
    expect(inner[2]).toBe(0x00)
  })

  it('header checksum is 2s complement: (b0+b1+b2+b3) & 0xFF === 0', () => {
    const payload = new Uint8Array([0x06, 0x01, 0x02])
    const framed = buildHciPacket(2, payload)
    const inner = slipDecode(framed)
    expect((inner[0] + inner[1] + inner[2] + inner[3]) & 0xFF).toBe(0)
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
