/**
 * CRC16-CCITT (init 0xFFFF, polynomial 0x8005, bit-reversed).
 * Matches the CRC used by Nordic SDK 11 HCI transport layer.
 */
export function crc16(data) {
  let crc = 0xFFFF
  for (let i = 0; i < data.length; i++) {
    crc = ((crc >> 8) & 0xFF) | ((crc << 8) & 0xFFFF)
    crc ^= data[i]
    crc ^= (crc & 0xFF) >> 4
    crc ^= (crc << 12) & 0xFFFF
    crc ^= ((crc & 0xFF) << 5) & 0xFFFF
  }
  return crc & 0xFFFF
}
