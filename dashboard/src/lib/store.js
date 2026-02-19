/**
 * Reactive state store for Ghost Operator dashboard.
 * Uses Vue 3's reactivity system (reactive/ref).
 */
import { reactive, ref } from 'vue'
import * as ble from './ble.js'
import * as serial from './dfu/serial.js'
import { performDfu } from './dfu/dfu.js'
import { parseResponse, parseSettings, parseStatus, buildQuery, buildSet, buildAction } from './protocol.js'

// --- Reactive state ---

export const connectionState = reactive({
  connected: false,
  deviceName: '',
  connecting: false,
  error: '',
})

export const status = reactive({
  connected: false,
  kb: true,
  ms: true,
  bat: 100,
  profile: 1,
  mode: 0,
  mouseState: 0,
  uptime: 0,
  kbNext: '---',
})

export const settings = reactive({
  keyMin: 2000,
  keyMax: 6500,
  mouseJig: 15000,
  mouseIdle: 30000,
  mouseAmp: 1,
  lazyPct: 0,
  busyPct: 0,
  dispBright: 80,
  saverBright: 20,
  saverTimeout: 5,
  animStyle: 2,
  name: 'GhostOperator',
  slots: [2, 28, 28, 28, 28, 28, 28, 28],
})

export const availableKeys = ref([])
export const dirty = ref(false)
export const saving = ref(false)
export const statusMessage = ref('')
export const dfuActive = ref(false)

// Snapshot of last-saved settings for dirty detection
let savedSnapshot = ''

// Status polling interval
let pollInterval = null

// Pending response handler
let pendingResolve = null

// --- BLE line handler ---

function handleLine(line) {
  const parsed = parseResponse(line)

  if (parsed.type === 'settings') {
    const s = parseSettings(parsed.data)
    Object.assign(settings, s)
    savedSnapshot = JSON.stringify(s)
    dirty.value = false
  } else if (parsed.type === 'status') {
    Object.assign(status, parseStatus(parsed.data))
  } else if (parsed.type === 'keys') {
    availableKeys.value = parsed.data
  } else if (parsed.type === 'ok' || parsed.type === 'error') {
    if (pendingResolve) {
      pendingResolve(parsed)
      pendingResolve = null
    }
  }
}

// --- Actions ---

/**
 * Connect to a Ghost Operator device via BLE.
 */
export async function connectDevice() {
  connectionState.connecting = true
  connectionState.error = ''

  try {
    ble.onLine(handleLine)
    ble.onDisconnect(() => {
      connectionState.connected = false
      connectionState.deviceName = ''
      stopPolling()
      statusMessage.value = 'Disconnected'
    })

    const name = await ble.connect()
    connectionState.connected = true
    connectionState.deviceName = name
    statusMessage.value = 'Connected'

    // Fetch initial data
    await ble.send(buildQuery('keys'))
    // Small delay to let keys response arrive before settings query
    await sleep(100)
    await ble.send(buildQuery('settings'))
    await sleep(100)
    await ble.send(buildQuery('status'))

    startPolling()
  } catch (err) {
    connectionState.error = err.message
    statusMessage.value = 'Connection failed'
  } finally {
    connectionState.connecting = false
  }
}

/**
 * Disconnect from the device.
 */
export function disconnectDevice() {
  ble.disconnect()
  connectionState.connected = false
  connectionState.deviceName = ''
  stopPolling()
}

/**
 * Send a setting change to the device (in-memory only, not saved to flash).
 */
export async function setSetting(key, value) {
  await ble.send(buildSet(key, value))
  // Update local state immediately for responsive UI
  if (key === 'slots') {
    settings.slots = String(value).split(',').map(Number)
  } else if (key === 'name') {
    settings.name = value
  } else if (key in settings) {
    settings[key] = typeof settings[key] === 'number' ? Number(value) : value
  }
  dirty.value = JSON.stringify(settings) !== savedSnapshot
}

/**
 * Save settings to flash on the device.
 */
export async function saveToFlash() {
  saving.value = true
  statusMessage.value = 'Saving...'
  await sendAndWait(buildAction('save'))
  savedSnapshot = JSON.stringify({ ...settings })
  dirty.value = false
  saving.value = false
  statusMessage.value = 'Saved'
  setTimeout(() => { if (statusMessage.value === 'Saved') statusMessage.value = '' }, 2000)
}

/**
 * Reset device to factory defaults.
 */
export async function resetDefaults() {
  statusMessage.value = 'Resetting...'
  await sendAndWait(buildAction('defaults'))
  // Re-fetch settings to reflect defaults
  await sleep(100)
  await ble.send(buildQuery('settings'))
  statusMessage.value = 'Defaults restored'
  setTimeout(() => { if (statusMessage.value === 'Defaults restored') statusMessage.value = '' }, 2000)
}

/**
 * Reboot the device.
 */
export async function rebootDevice() {
  statusMessage.value = 'Rebooting...'
  await ble.send(buildAction('reboot'))
  // Device will disconnect on reboot
}

/**
 * Refresh settings from device.
 */
export async function refreshSettings() {
  await ble.send(buildQuery('settings'))
}

/**
 * Orchestrate a Serial DFU firmware update.
 * Sends !serialdfu over BLE, waits for reboot, then transfers via Web Serial.
 *
 * @param {Uint8Array} datFile - Init packet from DFU ZIP
 * @param {Uint8Array} binFile - Firmware binary from DFU ZIP
 * @param {function} onProgress - (percent, message) => void
 * @param {function} onWaitingPort - Called when user needs to pick serial port
 * @returns {Promise<void>}
 */
export async function startSerialDfu(datFile, binFile, onProgress, onWaitingPort) {
  dfuActive.value = true
  stopPolling()

  try {
    // 1. Send !serialdfu over BLE to reboot device into serial DFU mode
    onProgress(0, 'Sending DFU command...')
    await ble.send(buildAction('serialdfu'))

    // 2. Wait for BLE disconnect (device reboots)
    onProgress(0, 'Waiting for device to reboot...')
    const disconnectDeadline = Date.now() + 5000
    while (ble.isConnected() && Date.now() < disconnectDeadline) {
      await sleep(200)
    }

    // 3. Wait for USB CDC enumeration
    onProgress(0, 'Waiting for USB serial port...')
    await sleep(3000)

    // 4. Signal UI to show "Select Serial Port" button
    await onWaitingPort()

    // 5. Open serial port (user picks from Chrome dialog)
    await serial.open(115200)

    // 6. Perform DFU transfer
    await performDfu(datFile, binFile, onProgress)

    // 7. Close serial port
    await serial.close()
  } finally {
    // dfuActive stays true — component controls when to clear it
  }
}

// --- Internal helpers ---

function sendAndWait(cmd) {
  return new Promise((resolve) => {
    pendingResolve = resolve
    ble.send(cmd)
    // Timeout fallback
    setTimeout(() => {
      if (pendingResolve === resolve) {
        pendingResolve = null
        resolve({ type: 'error', data: { message: 'timeout' } })
      }
    }, 3000)
  })
}

function startPolling() {
  stopPolling()
  pollInterval = setInterval(async () => {
    if (ble.isConnected()) {
      await ble.send(buildQuery('status'))
    }
  }, 5000)
}

function stopPolling() {
  if (pollInterval) {
    clearInterval(pollInterval)
    pollInterval = null
  }
}

function sleep(ms) {
  return new Promise(r => setTimeout(r, ms))
}
