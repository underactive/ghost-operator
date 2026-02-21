/**
 * Reactive state store for Ghost Operator dashboard.
 * Uses Vue 3's reactivity system (reactive/ref).
 */
import { reactive, ref } from 'vue'
import * as transport from './serial.js'
import * as dfuSerial from './dfu/serial.js'
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
  usb: false,
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
  mouseStyle: 0,
  lazyPct: 0,
  busyPct: 0,
  dispBright: 80,
  saverBright: 20,
  saverTimeout: 5,
  animStyle: 2,
  name: 'GhostOperator',
  btWhileUsb: 0,
  scroll: 0,
  dashboard: 0,
  decoy: 0,
  slots: [2, 28, 28, 28, 28, 28, 28, 28],
})

export const availableKeys = ref([])
export const decoyNames = ref([])
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

// --- Line handler ---

function handleLine(line) {
  const parsed = parseResponse(line)

  if (parsed.type === 'settings') {
    const s = parseSettings(parsed.data)
    Object.assign(settings, s)
    savedSnapshot = JSON.stringify(s)
    dirty.value = false
    // Update device name from settings (serial doesn't provide it at connect time)
    if (s.name && connectionState.connected) {
      connectionState.deviceName = s.name
    }
  } else if (parsed.type === 'status') {
    Object.assign(status, parseStatus(parsed.data))
  } else if (parsed.type === 'keys') {
    availableKeys.value = parsed.data
  } else if (parsed.type === 'decoys') {
    decoyNames.value = parsed.data
  } else if (parsed.type === 'ok' || parsed.type === 'error') {
    if (pendingResolve) {
      pendingResolve(parsed)
      pendingResolve = null
    }
  }
}

// --- Actions ---

/**
 * Connect to a Ghost Operator device via USB serial.
 */
export async function connectDevice() {
  connectionState.connecting = true
  connectionState.error = ''

  try {
    transport.onLine(handleLine)
    transport.onDisconnect(() => {
      connectionState.connected = false
      connectionState.deviceName = ''
      stopPolling()
      statusMessage.value = 'Disconnected'
    })

    const name = await transport.connect()
    connectionState.connected = true
    connectionState.deviceName = name
    statusMessage.value = 'Connected'

    // Enable real-time status push from device
    await transport.send(buildSet('statusPush', 1))
    await sleep(50)

    // Fetch initial data
    await transport.send(buildQuery('keys'))
    await sleep(50)
    await transport.send(buildQuery('decoys'))
    // Small delay to let responses arrive before settings query
    await sleep(100)
    await transport.send(buildQuery('settings'))
    await sleep(100)
    await transport.send(buildQuery('status'))

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
  transport.disconnect()
  connectionState.connected = false
  connectionState.deviceName = ''
  stopPolling()
}

/**
 * Send a setting change to the device (in-memory only, not saved to flash).
 */
export async function setSetting(key, value) {
  await transport.send(buildSet(key, value))
  // Update local state immediately for responsive UI
  if (key === 'slots') {
    settings.slots = String(value).split(',').map(Number)
  } else if (key === 'name') {
    settings.name = value
  } else if (key === 'decoy') {
    settings.decoy = Number(value)
    // Sync name display when selecting a preset
    if (settings.decoy > 0 && settings.decoy <= decoyNames.value.length) {
      settings.name = decoyNames.value[settings.decoy - 1]
    }
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
  await transport.send(buildQuery('settings'))
  statusMessage.value = 'Defaults restored'
  setTimeout(() => { if (statusMessage.value === 'Defaults restored') statusMessage.value = '' }, 2000)
}

/**
 * Reboot the device.
 */
export async function rebootDevice() {
  statusMessage.value = 'Rebooting...'
  await transport.send(buildAction('reboot'))
  // Device will disconnect on reboot
}

/**
 * Refresh settings from device.
 */
export async function refreshSettings() {
  await transport.send(buildQuery('settings'))
}

/**
 * Orchestrate a Serial DFU firmware update.
 * Sends !serialdfu over USB serial (config transport), waits for reboot,
 * then transfers via Web Serial DFU transport (SLIP/HCI protocol).
 *
 * @param {Uint8Array} datFile - Init packet from DFU ZIP
 * @param {Uint8Array} binFile - Firmware binary from DFU ZIP
 * @param {function} onProgress - (percent, message) => void
 * @param {function} onWaitingPort - Called when user needs to pick serial port
 * @param {function} [onLog] - Terminal log (append-only, separate from progress bar): (message, level) => void
 * @returns {Promise<void>}
 */
export async function startSerialDfu(datFile, binFile, onProgress, onWaitingPort, onLog = () => {}) {
  dfuActive.value = true
  stopPolling()

  try {
    // 1. Send !serialdfu over config serial to reboot device into serial DFU mode
    onProgress(0, 'Sending DFU command...')
    // Terminal log (append-only, separate from progress bar)
    onLog('Sending DFU command over serial...', 'info')
    await transport.send(buildAction('serialdfu'))

    // 2. Brief delay then disconnect config serial (device reboots)
    onProgress(0, 'Waiting for device to reboot...')
    // Terminal log (append-only, separate from progress bar)
    onLog('Disconnecting config port...', 'info')
    await sleep(200)
    await transport.disconnect()
    connectionState.connected = false
    connectionState.deviceName = ''

    // 3. Wait for USB CDC re-enumeration in bootloader mode
    onProgress(0, 'Waiting for USB serial port...')
    // Terminal log (append-only, separate from progress bar)
    onLog('Waiting for USB serial port...', 'info')
    await sleep(3000)

    // 4. Signal UI to show "Select Serial Port" button
    await onWaitingPort()

    // 5. Open serial port for DFU (user picks from Chrome dialog)
    // Terminal log (append-only, separate from progress bar)
    onLog('Opening DFU serial port at 115200 baud', 'info')
    await dfuSerial.open(115200)
    // Terminal log (append-only, separate from progress bar)
    onLog('DFU serial port ready', 'info')

    // 6. Perform DFU transfer
    await performDfu(datFile, binFile, onProgress, onLog)

    // 7. Close DFU serial port
    await dfuSerial.close()
    // Terminal log (append-only, separate from progress bar)
    onLog('DFU serial port closed', 'info')
  } catch (err) {
    // Terminal log (append-only, separate from progress bar)
    onLog(`DFU error: ${err.message}`, 'error')
    throw err
  } finally {
    // dfuActive stays true — component controls when to clear it
  }
}

// --- Internal helpers ---

function sendAndWait(cmd) {
  return new Promise((resolve) => {
    pendingResolve = resolve
    transport.send(cmd)
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
    if (transport.isConnected()) {
      await transport.send(buildQuery('status'))
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
