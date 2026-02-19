/**
 * Web Serial API transport layer for DFU serial communication.
 * Handles port selection, open/close, and SLIP-frame-level reading.
 */

const SLIP_END = 0xC0

let port = null

/**
 * Check if Web Serial API is available in this browser.
 */
export function isWebSerialAvailable() {
  return !!(navigator.serial && navigator.serial.requestPort)
}

/**
 * Prompt user to select a serial port and open it.
 * @param {number} baudRate - Baud rate (default 115200)
 */
export async function open(baudRate = 115200) {
  // No VID filter — the bootloader reuses the same USB CDC port as the app,
  // which may have Seeed's VID (not Adafruit's 0x239A).
  port = await navigator.serial.requestPort()
  await port.open({ baudRate, bufferSize: 4096 })

  // Toggle DTR to signal "terminal ready" — matches adafruit-nrfutil behavior.
  // Many USB CDC devices don't start serial processing until DTR is asserted.
  await port.setSignals({ dataTerminalReady: false })
  await new Promise(r => setTimeout(r, 50))
  await port.setSignals({ dataTerminalReady: true })
  await new Promise(r => setTimeout(r, 100))
}

/**
 * Close the serial port and release resources.
 */
export async function close() {
  if (port) {
    try { await port.close() } catch {}
    port = null
  }
}

/**
 * Write raw bytes to the serial port.
 * @param {Uint8Array} data
 */
export async function write(data) {
  if (!port || !port.writable) throw new Error('Serial port not open')
  const writer = port.writable.getWriter()
  try {
    await writer.write(data)
  } finally {
    writer.releaseLock()
  }
}

/**
 * Read a complete SLIP frame from the serial port.
 *
 * Each call creates a fresh reader and releases it when done. This prevents
 * orphaned reader.read() promises from consuming data meant for later reads
 * (a problem when using Promise.race for timeouts with a shared reader).
 *
 * @param {number} timeoutMs - Maximum wait time (default 5000ms)
 * @returns {Promise<Uint8Array>} Raw bytes of the SLIP frame (including delimiters)
 */
export async function readFrame(timeoutMs = 5000) {
  if (!port || !port.readable) throw new Error('Serial port not open')

  // Fresh reader per call — avoids orphaned-read issues
  const localReader = port.readable.getReader()

  const buffer = []
  let started = false
  let timedOut = false

  const timeoutId = setTimeout(() => {
    timedOut = true
    // releaseLock() causes the pending read() to reject with TypeError,
    // which we catch below. The stream stays open for future readers.
    try { localReader.releaseLock() } catch {}
  }, timeoutMs)

  try {
    while (!timedOut) {
      let result
      try {
        result = await localReader.read()
      } catch {
        // TypeError from releaseLock() during timeout — expected
        break
      }

      if (result.done) break

      const chunk = result.value
      for (let i = 0; i < chunk.length; i++) {
        if (chunk[i] === SLIP_END) {
          if (started && buffer.length > 0) {
            // End of frame — return collected bytes
            buffer.push(SLIP_END)
            clearTimeout(timeoutId)
            try { localReader.releaseLock() } catch {}
            return new Uint8Array(buffer)
          }
          // Start of frame
          started = true
          buffer.length = 0
          buffer.push(SLIP_END)
        } else if (started) {
          buffer.push(chunk[i])
        }
      }
    }
  } finally {
    clearTimeout(timeoutId)
    try { localReader.releaseLock() } catch {}
  }

  throw new Error('Serial read timeout')
}

/**
 * Check if the serial port is currently open.
 */
export function isOpen() {
  return !!(port && port.readable && port.writable)
}
