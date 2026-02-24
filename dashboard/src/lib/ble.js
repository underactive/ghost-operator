/**
 * Web Bluetooth connection layer for Ghost Operator.
 * Connects via Nordic UART Service (NUS) to the BLE UART on the nRF52840.
 *
 * Reconnection strategy:
 * 1. Try getDevices() to reconnect to a previously-authorized device (no picker needed)
 * 2. Fall back to requestDevice() picker for first-time pairing
 */

// Nordic UART Service UUIDs (standard, hardcoded in Adafruit BLEUart)
const NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
const NUS_RXD_UUID     = '6e400002-b5a3-f393-e0a9-e50e24dcca9e' // web writes TO device
const NUS_TXD_UUID     = '6e400003-b5a3-f393-e0a9-e50e24dcca9e' // device notifies TO web

let device = null
let server = null
let rxCharacteristic = null
let txCharacteristic = null
let rxBuffer = ''

let onLineReceived = null
let onDisconnected = null

/**
 * Check if Web Bluetooth is available in this browser.
 */
export function isWebBluetoothAvailable() {
  return !!(navigator.bluetooth && navigator.bluetooth.requestDevice)
}

/**
 * Connect to a Ghost Operator device.
 * Tries reconnecting to a previously-authorized device first (bypasses picker).
 * Falls back to the Chrome device picker for first-time pairing.
 */
export async function connect() {
  // Try reconnecting to a previously-authorized device (no picker dialog)
  if (navigator.bluetooth.getDevices) {
    const known = await navigator.bluetooth.getDevices()
    for (const d of known) {
      try {
        return await connectToDevice(d, 3000)
      } catch {
        // Not reachable — try next or fall through to picker
      }
    }
  }

  // Fall back to device picker dialog (first-time pairing)
  const d = await navigator.bluetooth.requestDevice({
    acceptAllDevices: true,
    optionalServices: [NUS_SERVICE_UUID],
  })
  return await connectToDevice(d)
}

/**
 * Connect to a specific BluetoothDevice and set up NUS.
 * @param {BluetoothDevice} d
 * @param {number} [timeoutMs] - Optional connection timeout in ms
 */
async function connectToDevice(d, timeoutMs) {
  d.addEventListener('gattserverdisconnected', handleDisconnect)

  try {
    let connectPromise = d.gatt.connect()
    if (timeoutMs) {
      const timeout = new Promise((_, reject) =>
        setTimeout(() => reject(new Error('Connection timeout')), timeoutMs)
      )
      connectPromise = Promise.race([connectPromise, timeout])
    }
    server = await connectPromise
    device = d
    return await setupNUS()
  } catch (err) {
    d.removeEventListener('gattserverdisconnected', handleDisconnect)
    try { d.gatt.disconnect() } catch {}
    cleanup()
    throw err
  }
}

/**
 * Discover NUS service and set up RX/TX characteristics.
 */
async function setupNUS() {
  const service = await server.getPrimaryService(NUS_SERVICE_UUID)
  rxCharacteristic = await service.getCharacteristic(NUS_RXD_UUID)
  txCharacteristic = await service.getCharacteristic(NUS_TXD_UUID)
  await txCharacteristic.startNotifications()
  txCharacteristic.addEventListener('characteristicvaluechanged', handleTxNotification)
  return device.name || 'Ghost Operator'
}

/**
 * Disconnect from the device.
 */
export function disconnect() {
  if (device && device.gatt.connected) {
    device.gatt.disconnect()
  }
  cleanup()
}

/**
 * Send a command string to the device (appends newline).
 * Chunks at 20 bytes for default MTU compatibility.
 */
export async function send(msg) {
  if (!rxCharacteristic) {
    throw new Error('Not connected')
  }
  const data = new TextEncoder().encode(msg + '\n')
  for (let offset = 0; offset < data.length; offset += 20) {
    const chunk = data.slice(offset, offset + 20)
    await rxCharacteristic.writeValueWithoutResponse(chunk)
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
 * Check if currently connected.
 */
export function isConnected() {
  return !!(device && device.gatt && device.gatt.connected)
}

// --- Internal ---

function handleTxNotification(event) {
  const value = new TextDecoder().decode(event.target.value)
  rxBuffer += value

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

function handleDisconnect() {
  cleanup()
  if (onDisconnected) {
    onDisconnected()
  }
}

function cleanup() {
  if (txCharacteristic) {
    try { txCharacteristic.removeEventListener('characteristicvaluechanged', handleTxNotification) } catch {}
  }
  rxCharacteristic = null
  txCharacteristic = null
  server = null
  device = null
  rxBuffer = ''
}
