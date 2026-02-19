/**
 * Web Serial config transport for Ghost Operator.
 * Drop-in replacement for ble.js — same public API, uses USB serial instead of BLE UART.
 * The device's USB CDC serial port speaks the same ?/=/! text protocol.
 */

let port = null
let reader = null
let readLoopActive = false
let rxBuffer = ''

let onLineReceived = null
let onDisconnected = null

/**
 * Check if Web Serial API is available in this browser.
 */
export function isWebSerialAvailable() {
  return !!(navigator.serial && navigator.serial.requestPort)
}

/**
 * Connect to a Ghost Operator device via USB serial.
 * Opens Chrome's port picker, then starts an async read loop.
 * Returns the device name (fetched later via ?settings).
 */
export async function connect() {
  port = await navigator.serial.requestPort()
  await port.open({ baudRate: 115200, bufferSize: 4096 })

  // Start async read loop
  readLoopActive = true
  readLoop()

  // Device name isn't known at connect time (unlike BLE which has it in the GATT name).
  // The store will update connectionState.deviceName from the ?settings response.
  return 'Ghost Operator'
}

/**
 * Disconnect from the device — cancel reader and close port.
 */
export async function disconnect() {
  readLoopActive = false

  if (reader) {
    try { await reader.cancel() } catch {}
    try { reader.releaseLock() } catch {}
    reader = null
  }

  if (port) {
    try { await port.close() } catch {}
    port = null
  }

  rxBuffer = ''
}

/**
 * Send a command string to the device (appends newline).
 * No chunking needed — USB serial handles arbitrary lengths.
 */
export async function send(msg) {
  if (!port || !port.writable) {
    throw new Error('Not connected')
  }
  const writer = port.writable.getWriter()
  try {
    await writer.write(new TextEncoder().encode(msg + '\n'))
  } finally {
    writer.releaseLock()
  }
}

/**
 * Register a callback for complete lines received from the device.
 */
export function onLine(callback) {
  onLineReceived = callback
}

/**
 * Register a callback for disconnection events.
 */
export function onDisconnect(callback) {
  onDisconnected = callback
}

/**
 * Check if currently connected (port open and streams available).
 */
export function isConnected() {
  return !!(port && port.readable && port.writable)
}

// --- Internal ---

/**
 * Async read loop — reads UTF-8 text from serial, splits on newlines,
 * fires onLineReceived for each complete line.
 * Firmware debug output (boot banner, mode changes, etc.) will appear as
 * lines too — protocol.js parseResponse() returns { type: 'unknown' } for
 * non-protocol lines, and store.js handleLine() ignores them.
 */
async function readLoop() {
  const decoder = new TextDecoder()

  while (readLoopActive && port && port.readable) {
    reader = port.readable.getReader()

    try {
      while (readLoopActive) {
        const { value, done } = await reader.read()
        if (done) break

        rxBuffer += decoder.decode(value, { stream: true })

        // Process complete lines
        let newlineIdx
        while ((newlineIdx = rxBuffer.indexOf('\n')) !== -1) {
          const line = rxBuffer.substring(0, newlineIdx).replace(/\r$/, '')
          rxBuffer = rxBuffer.substring(newlineIdx + 1)
          if (line.length > 0 && onLineReceived) {
            onLineReceived(line)
          }
        }
      }
    } catch (err) {
      // ReadableStream errors on disconnect — expected
      if (readLoopActive) {
        console.warn('[Serial] Read error:', err.message)
      }
    } finally {
      try { reader.releaseLock() } catch {}
      reader = null
    }
  }

  // If we exited the loop unexpectedly (USB disconnect), fire callback
  if (port) {
    port = null
    rxBuffer = ''
    if (onDisconnected) {
      onDisconnected()
    }
  }
}
