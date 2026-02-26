/**
 * Reactive state store for Ghost Operator dashboard.
 * Uses Vue 3's reactivity system (reactive/ref).
 */
import { reactive, ref } from 'vue'
import * as serialTransport from './serial.js'
import * as bleTransport from './ble.js'
import * as dfuSerial from './dfu/serial.js'
import { performDfu } from './dfu/dfu.js'
import { parseResponse, parseSettings, parseStatus, parseWorkMode, parseSimBlocks, buildQuery, buildSet, buildAction } from './protocol.js'

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
  batMv: 0,
  profile: 1,
  mode: 0,
  mouseState: 0,
  uptime: 0,
  kbNext: '---',
  timeSynced: false,
  daySecs: 0,
  schedSleeping: false,
  // Simulation mode runtime status
  simBlock: 0,
  simMode: 0,
  simPhase: 0,
  simProfile: 0,
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
  invertDial: 0,
  decoy: 0,
  schedMode: 0,
  schedStart: 18,
  schedEnd: 34,
  slots: [2, 28, 28, 28, 28, 28, 28, 28],
  // Simulation mode
  opMode: 0,
  jobSim: 0,
  jobPerf: 5,
  jobStart: 96,
  phantom: 0,
  clickSlots: [1, 7, 7, 7, 7, 7, 7],
  winSwitch: 0,
  switchKeys: 0,
  headerDisp: 0,
  // Sound
  sound: 0,
  soundType: 0,
  // Shift/lunch
  shiftDur: 480,
  lunchDur: 30,
})

export const availableKeys = ref([])
export const decoyNames = ref([])
export const dirty = ref(false)
export const saving = ref(false)
export const statusMessage = ref('')
export const dfuActive = ref(false)
export const transportType = ref(null) // 'usb' | 'ble' | null

// Simulation tuning state
export const simModes = reactive([])      // Array of 11 parsed work mode objects
export const simBlocks = reactive([])     // Array of block arrays per job (3 jobs)
export const simDataDirty = ref(false)
export const simDataLoading = ref(false)

// Active transport module (serial or ble)
let activeTransport = null

// Battery history (up to 720 entries = 1 hour at 5s polling)
export const batteryHistory = reactive([])
const BATTERY_HISTORY_MAX = 720
let lastBatteryDevice = ''

// Snapshot of last-saved settings for dirty detection
let savedSnapshot = ''

// Status polling interval
let pollInterval = null

// Pending response handler
let pendingResolve = null

// --- Battery history persistence ---

function batteryStorageKey(name) {
  return `ghost_battery_${name || 'unknown'}`
}

function saveBatteryHistory(name) {
  try {
    localStorage.setItem(batteryStorageKey(name), JSON.stringify(batteryHistory))
  } catch { /* quota exceeded — silently drop */ }
}

function loadBatteryHistory(name) {
  batteryHistory.splice(0)
  try {
    const raw = localStorage.getItem(batteryStorageKey(name))
    if (raw) {
      const arr = JSON.parse(raw)
      if (Array.isArray(arr)) {
        // Trim stale entries older than 1 hour
        const cutoff = Date.now() - 3600000
        const recent = arr.filter(e => e.t > cutoff)
        batteryHistory.push(...recent.slice(-BATTERY_HISTORY_MAX))
      }
    }
  } catch { /* corrupt data — start fresh */ }
}

function pushBatterySample(pct, mv) {
  if (mv <= 0) return
  batteryHistory.push({ t: Date.now(), pct, mv })
  if (batteryHistory.length > BATTERY_HISTORY_MAX) {
    batteryHistory.splice(0, batteryHistory.length - BATTERY_HISTORY_MAX)
  }
  // Persist every 12 samples (~60s at 5s polling) to reduce write frequency
  if (batteryHistory.length % 12 === 0) {
    saveBatteryHistory(connectionState.deviceName)
  }
}

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
    const s = parseStatus(parsed.data)
    Object.assign(status, s)
    pushBatterySample(s.bat, s.batMv)
  } else if (parsed.type === 'keys') {
    availableKeys.value = parsed.data
  } else if (parsed.type === 'decoys') {
    decoyNames.value = parsed.data
  } else if (parsed.type === 'wmode') {
    const mode = parseWorkMode(parsed.data)
    simModes[mode.idx] = mode
  } else if (parsed.type === 'simblocks') {
    const jobIdx = parseInt(parsed.data.job) || 0
    simBlocks[jobIdx] = parseSimBlocks(parsed.data)
  } else if (parsed.type === 'ok' || parsed.type === 'error') {
    if (pendingResolve) {
      pendingResolve(parsed)
      pendingResolve = null
    }
  }
}

// --- Actions ---

/**
 * Connect using the given transport module.
 * @param {object} transport - serial or ble transport module
 * @param {'usb'|'ble'} type - transport type identifier
 */
async function connectWithTransport(transport, type) {
  connectionState.connecting = true
  connectionState.error = ''

  try {
    activeTransport = transport
    transport.onLine(handleLine)
    transport.onDisconnect(() => {
      saveBatteryHistory(connectionState.deviceName)
      connectionState.connected = false
      connectionState.deviceName = ''
      transportType.value = null
      activeTransport = null
      stopPolling()
      statusMessage.value = 'Disconnected'
    })

    const name = await transport.connect()
    connectionState.connected = true
    connectionState.deviceName = name
    transportType.value = type
    statusMessage.value = 'Connected'

    // Load battery history for this device (or clear if different device)
    if (name !== lastBatteryDevice) {
      lastBatteryDevice = name
    }
    loadBatteryHistory(name)

    // BLE has higher latency — use longer handshake delays
    const shortDelay = type === 'ble' ? 150 : 50
    const longDelay = type === 'ble' ? 200 : 100

    // Enable real-time status push from device
    await transport.send(buildSet('statusPush', 1))
    await sleep(shortDelay)

    // Sync wall clock time to device
    await syncTimeToDevice()
    await sleep(shortDelay)

    // Fetch initial data
    await transport.send(buildQuery('keys'))
    await sleep(shortDelay)
    await transport.send(buildQuery('decoys'))
    // Small delay to let responses arrive before settings query
    await sleep(longDelay)
    await transport.send(buildQuery('settings'))
    await sleep(longDelay)
    await transport.send(buildQuery('status'))

    // Fetch sim tuning data for simulation mode
    await sleep(longDelay)
    await fetchSimData()

    startPolling()
  } catch (err) {
    activeTransport = null
    transportType.value = null
    connectionState.error = err.message
    statusMessage.value = 'Connection failed'
  } finally {
    connectionState.connecting = false
  }
}

/** Connect via USB (Web Serial). */
export function connectUSB() {
  return connectWithTransport(serialTransport, 'usb')
}

/** Connect via Bluetooth (Web Bluetooth). */
export function connectBLE() {
  return connectWithTransport(bleTransport, 'ble')
}

/** @deprecated Use connectUSB() or connectBLE(). Kept for backwards compat. */
export const connectDevice = connectUSB

/**
 * Disconnect from the device.
 */
export async function disconnectDevice() {
  stopPolling()
  if (activeTransport) {
    await activeTransport.disconnect()
  }
  connectionState.connected = false
  connectionState.deviceName = ''
  transportType.value = null
  activeTransport = null
}

/**
 * Send a setting change to the device (in-memory only, not saved to flash).
 */
export async function setSetting(key, value) {
  await activeTransport.send(buildSet(key, value))
  // Update local state immediately for responsive UI
  if (key === 'slots') {
    settings.slots = String(value).split(',').map(Number)
  } else if (key === 'clickSlots') {
    settings.clickSlots = String(value).split(',').map(Number)
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
  simDataDirty.value = false
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
  await activeTransport.send(buildQuery('settings'))
  statusMessage.value = 'Defaults restored'
  setTimeout(() => { if (statusMessage.value === 'Defaults restored') statusMessage.value = '' }, 2000)
}

/**
 * Reboot the device.
 */
export async function rebootDevice() {
  statusMessage.value = 'Rebooting...'
  await activeTransport.send(buildAction('reboot'))
  // Device will disconnect on reboot
}

/**
 * Refresh settings from device.
 */
export async function refreshSettings() {
  await activeTransport.send(buildQuery('settings'))
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
    await activeTransport.send(buildAction('serialdfu'))

    // 2. Brief delay then disconnect config serial (device reboots)
    onProgress(0, 'Waiting for device to reboot...')
    // Terminal log (append-only, separate from progress bar)
    onLog('Disconnecting config port...', 'info')
    await sleep(200)
    await activeTransport.disconnect()
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

/**
 * Sync the current wall clock time to the device.
 */
async function syncTimeToDevice() {
  const now = new Date()
  const daySeconds = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds()
  await activeTransport.send(buildSet('time', daySeconds))
}

/**
 * Fetch all sim tuning data (work modes + day blocks).
 */
export async function fetchSimData() {
  if (!activeTransport || !activeTransport.isConnected()) return
  simDataLoading.value = true
  const delay = transportType.value === 'ble' ? 150 : 50
  for (let i = 0; i < 11; i++) {
    await activeTransport.send(buildQuery(`wmode:${i}`))
    await sleep(delay)
  }
  for (let j = 0; j < 3; j++) {
    await activeTransport.send(buildQuery(`simblocks:${j}`))
    await sleep(delay)
  }
  simDataLoading.value = false
  simDataDirty.value = false
}

/**
 * Set a single work mode parameter.
 */
export async function setWorkModeParam(modeIdx, field, value) {
  await activeTransport.send(buildSet('wmode', `${modeIdx}:${field}:${value}`))
  // Re-fetch that mode to update local state
  await sleep(50)
  await activeTransport.send(buildQuery(`wmode:${modeIdx}`))
  simDataDirty.value = true
  dirty.value = true
}

/**
 * Set a full profile timing (12 values) for a work mode.
 */
export async function setWorkModeTiming(modeIdx, profileIdx, timing) {
  const csv = [
    timing.burstKeysMin, timing.burstKeysMax,
    timing.interKeyMinMs, timing.interKeyMaxMs,
    timing.burstGapMinMs, timing.burstGapMaxMs,
    timing.keyHoldMinMs, timing.keyHoldMaxMs,
    timing.mouseDurMinMs, timing.mouseDurMaxMs,
    timing.idleDurMinMs, timing.idleDurMaxMs,
  ].join(',')
  await activeTransport.send(buildSet('wmode', `${modeIdx}:t${profileIdx}:${csv}`))
  await sleep(50)
  await activeTransport.send(buildQuery(`wmode:${modeIdx}`))
  simDataDirty.value = true
  dirty.value = true
}

/**
 * Save sim data to flash on device.
 */
export async function saveSimDataToFlash() {
  await sendAndWait(buildAction('savesim'))
  simDataDirty.value = false
  statusMessage.value = 'Sim data saved'
  setTimeout(() => { if (statusMessage.value === 'Sim data saved') statusMessage.value = '' }, 2000)
}

/**
 * Reset all work modes to factory defaults.
 */
export async function resetSimDataToDefaults() {
  await sendAndWait(buildAction('resetsim'))
  await sleep(100)
  await fetchSimData()
  statusMessage.value = 'Sim data reset to defaults'
  setTimeout(() => { if (statusMessage.value === 'Sim data reset to defaults') statusMessage.value = '' }, 2000)
}

// --- Internal helpers ---

function sendAndWait(cmd) {
  return new Promise((resolve) => {
    pendingResolve = resolve
    activeTransport.send(cmd)
    // Timeout fallback
    setTimeout(() => {
      if (pendingResolve === resolve) {
        pendingResolve = null
        resolve({ type: 'error', data: { message: 'timeout' } })
      }
    }, 3000)
  })
}

let pollCount = 0

function startPolling() {
  stopPolling()
  pollCount = 0
  pollInterval = setInterval(async () => {
    if (activeTransport && activeTransport.isConnected()) {
      await activeTransport.send(buildQuery('status'))
      pollCount++
      // Re-sync time every 5 minutes (60 polls * 5s = 300s)
      if (pollCount % 60 === 0) {
        await syncTimeToDevice()
      }
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

// --- Settings Export/Import ---

const EXPORTABLE_KEYS = [
  'keyMin', 'keyMax', 'mouseJig', 'mouseIdle', 'mouseAmp', 'mouseStyle', 'scroll',
  'lazyPct', 'busyPct',
  'dispBright', 'saverBright', 'saverTimeout', 'animStyle',
  'decoy', 'name', 'btWhileUsb', 'invertDial',
  'schedMode', 'schedStart', 'schedEnd',
  'opMode', 'jobSim', 'jobPerf', 'jobStart',
  'phantom', 'clickSlots', 'winSwitch', 'switchKeys', 'headerDisp',
  'sound', 'soundType',
  'shiftDur', 'lunchDur',
  'slots',
]

/**
 * Export current settings to a JSON file download.
 */
export function exportSettings() {
  const exported = {}
  for (const key of EXPORTABLE_KEYS) {
    if (key === 'slots') {
      exported.slots = settings.slots.join(',')
    } else {
      exported[key] = settings[key]
    }
  }

  const payload = {
    ghost_operator: {
      version: 1,
      device_name: settings.name || 'GhostOperator',
      exported_at: new Date().toISOString(),
      settings: exported,
    },
  }

  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  const date = new Date().toISOString().slice(0, 10)
  const name = (settings.name || 'GhostOperator').replace(/[^a-zA-Z0-9_-]/g, '_')
  a.href = url
  a.download = `${name}_settings_${date}.json`
  a.click()
  URL.revokeObjectURL(url)

  statusMessage.value = 'Settings exported'
  setTimeout(() => { if (statusMessage.value === 'Settings exported') statusMessage.value = '' }, 2000)
}

/**
 * Import settings from a JSON file.
 * @param {File} file - JSON file selected by user
 * @returns {Promise<{applied: number, skipped: string[], errors: string[]}>}
 */
export async function importSettings(file) {
  const text = await file.text()
  let data
  try {
    data = JSON.parse(text)
  } catch {
    throw new Error('Invalid JSON file')
  }

  if (!data.ghost_operator || !data.ghost_operator.settings) {
    throw new Error('Not a Ghost Operator settings file')
  }

  const imported = data.ghost_operator.settings
  const result = { applied: 0, skipped: [], errors: [] }

  for (const key of EXPORTABLE_KEYS) {
    if (!(key in imported)) {
      result.skipped.push(key)
      continue
    }

    let value = imported[key]

    // Sanitize name: strip non-printable chars, limit to 14 chars
    if (key === 'name' && typeof value === 'string') {
      value = value.replace(/[^\x20-\x7E]/g, '').substring(0, 14)
    }

    try {
      await setSetting(key, value)
      result.applied++
      await sleep(50)
    } catch (err) {
      result.errors.push(`${key}: ${err.message}`)
    }
  }

  // Report unknown keys in file (forward compat)
  for (const key of Object.keys(imported)) {
    if (!EXPORTABLE_KEYS.includes(key)) {
      result.skipped.push(`${key} (unknown)`)
    }
  }

  // Save to flash
  await saveToFlash()

  return result
}
