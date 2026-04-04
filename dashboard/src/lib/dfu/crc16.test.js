import { describe, expect, it } from 'vitest'
import { crc16 } from './crc16.js'

describe('crc16', () => {
  it('returns 0xFFFF for empty input (init value)', () => {
    expect(crc16(new Uint8Array(0))).toBe(0xffff)
  })

  it('matches fixed vectors for Nordic HCI CRC16-CCITT', () => {
    expect(crc16(new Uint8Array([0x00]))).toBe(0xe1f0)
    expect(crc16(new Uint8Array([0x01]))).toBe(0xe1d1)
  })
})
