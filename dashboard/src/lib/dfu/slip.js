/**
 * SLIP framing and HCI packet construction for Nordic DFU serial transport.
 *
 * Reference: Nordic SDK 11 hci_slip.c / hci_transport.c
 * and adafruit-nrfutil dfu_transport_serial.py
 */
import { crc16 } from './crc16.js'

// SLIP special bytes
const SLIP_END = 0xC0
const SLIP_ESC = 0xDB
const SLIP_ESC_END = 0xDC
const SLIP_ESC_ESC = 0xDD

// HCI packet type for DFU
const HCI_PACKET_TYPE = 14  // DATA_INTEGRITY_CHECK_PRESENT | RELIABLE_PACKET

/**
 * SLIP-encode a data buffer (add framing + escape special bytes).
 * @param {Uint8Array} data - Raw data to encode
 * @returns {Uint8Array} SLIP-framed data
 */
export function slipEncode(data) {
  const out = [SLIP_END]
  for (let i = 0; i < data.length; i++) {
    if (data[i] === SLIP_END) {
      out.push(SLIP_ESC, SLIP_ESC_END)
    } else if (data[i] === SLIP_ESC) {
      out.push(SLIP_ESC, SLIP_ESC_ESC)
    } else {
      out.push(data[i])
    }
  }
  out.push(SLIP_END)
  return new Uint8Array(out)
}

/**
 * SLIP-decode a frame (strip delimiters + unescape).
 * @param {Uint8Array} frame - SLIP-framed data
 * @returns {Uint8Array} Unescaped payload
 */
export function slipDecode(frame) {
  const out = []
  let inEsc = false
  for (let i = 0; i < frame.length; i++) {
    if (frame[i] === SLIP_END) continue
    if (inEsc) {
      if (frame[i] === SLIP_ESC_END) out.push(SLIP_END)
      else if (frame[i] === SLIP_ESC_ESC) out.push(SLIP_ESC)
      else out.push(frame[i])
      inEsc = false
    } else if (frame[i] === SLIP_ESC) {
      inEsc = true
    } else {
      out.push(frame[i])
    }
  }
  return new Uint8Array(out)
}

/**
 * Build a complete HCI packet: 4-byte header + payload + 2-byte CRC16, then SLIP-encode.
 *
 * HCI header layout (4 bytes):
 *   byte0: seq_no | (ack_no << 3) | (data_integrity << 6) | (reliable << 7)
 *   byte1: packet_type | ((payload_len & 0x0F) << 4)
 *   byte2: (payload_len >> 4) & 0xFF
 *   byte3: header_checksum = (~(b0 + b1 + b2) + 1) & 0xFF
 *
 * @param {number} seq - Sequence number (0-7)
 * @param {Uint8Array} payload - DFU command payload
 * @returns {Uint8Array} SLIP-encoded HCI packet
 */
export function buildHciPacket(seq, payload) {
  const len = payload.length
  const ackNo = (seq + 1) % 8

  const b0 = (seq & 0x07) | ((ackNo & 0x07) << 3) | (1 << 6) | (1 << 7)
  const b1 = (HCI_PACKET_TYPE & 0x0F) | ((len & 0x0F) << 4)
  const b2 = (len >> 4) & 0xFF
  const b3 = (~(b0 + b1 + b2) + 1) & 0xFF

  // Assemble: header + payload + CRC16
  const packet = new Uint8Array(4 + len + 2)
  packet[0] = b0
  packet[1] = b1
  packet[2] = b2
  packet[3] = b3
  packet.set(payload, 4)

  const crc = crc16(packet.subarray(0, 4 + len))
  packet[4 + len] = crc & 0xFF
  packet[4 + len + 1] = (crc >> 8) & 0xFF

  return slipEncode(packet)
}

/**
 * Parse an ACK packet from the bootloader.
 * Returns the sequence number being ACK'd, or -1 on error.
 * @param {Uint8Array} rawBytes - Raw bytes received (SLIP-decoded)
 * @returns {number} ACK sequence number, or -1
 */
export function parseAck(rawBytes) {
  if (rawBytes.length < 4) return -1
  // ACK packet: header only (no payload, no CRC for ACK-only packets)
  // ack_no is in bits [3:5] of byte 0
  const ackNo = (rawBytes[0] >> 3) & 0x07
  return ackNo
}
