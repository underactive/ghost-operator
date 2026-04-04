import { describe, expect, it } from 'vitest'
import { crc16 } from './crc16.js'

describe('crc16', () => {
  it('returns 0xFFFF for empty input (init value)', () => {
    expect(crc16(new Uint8Array(0))).toBe(0xffff)
  })

  it('matches fixed vectors for Nordic HCI CRC16-CCITT', () => {
    expect(crc16(new Uint8Array([0x00]))).toBe(0xe1f0)
    expect(crc16(new Uint8Array([0x01]))).toBe(0xf1d1)
  })

  it('matches CRC16-CCITT standard check value for "123456789"', () => {
    // Universal reference vector: CRC RevEng catalogue, poly=0x1021 init=0xFFFF refin=false refout=false xorout=0x0000
    const bytes = new Uint8Array([0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39])
    expect(crc16(bytes)).toBe(0x29b1)
  })
})
