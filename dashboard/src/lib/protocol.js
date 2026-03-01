/**
 * Protocol parser/builder for Ghost Operator BLE UART.
 *
 * Response format: !type|key=val|key=val|...
 * Ack format:      +ok
 * Error format:    -err:message
 */

/**
 * Parse a response line from the device.
 * Returns { type, data } where data is an object of key-value pairs.
 *
 * Examples:
 *   "!settings|keyMin=2000|keyMax=6500" -> { type: 'settings', data: { keyMin: '2000', keyMax: '6500' } }
 *   "!keys|F13|F14|F15"                -> { type: 'keys', data: ['F13', 'F14', 'F15'] }
 *   "+ok"                              -> { type: 'ok', data: {} }
 *   "-err:unknown key"                 -> { type: 'error', data: { message: 'unknown key' } }
 */
export function parseResponse(line) {
  if (line.startsWith('+')) {
    return { type: 'ok', data: {} }
  }

  if (line.startsWith('-err:')) {
    return { type: 'error', data: { message: line.substring(5) } }
  }

  if (line.startsWith('!')) {
    const parts = line.substring(1).split('|')
    const type = parts[0]

    // ?keys and ?decoys responses: values are bare strings (no = sign)
    if (type === 'keys' || type === 'decoys') {
      return { type, data: parts.slice(1) }
    }

    // All other responses: key=value pairs
    const data = {}
    for (let i = 1; i < parts.length; i++) {
      const eqIdx = parts[i].indexOf('=')
      if (eqIdx !== -1) {
        data[parts[i].substring(0, eqIdx)] = parts[i].substring(eqIdx + 1)
      }
    }
    return { type, data }
  }

  return { type: 'unknown', data: { raw: line } }
}

/**
 * Build a query command.
 * queryType: 'status' | 'settings' | 'keys'
 */
export function buildQuery(queryType) {
  return `?${queryType}`
}

/**
 * Build a set command.
 * key: setting name, value: new value
 */
export function buildSet(key, value) {
  return `=${key}:${value}`
}

/**
 * Build an action command.
 * action: 'save' | 'defaults' | 'reboot'
 */
export function buildAction(action) {
  return `!${action}`
}

/**
 * Parse settings response data into typed values.
 * Converts string values to appropriate JS types.
 */
export function parseSettings(data) {
  return {
    keyMin: parseInt(data.keyMin) || 2000,
    keyMax: parseInt(data.keyMax) || 6500,
    mouseJig: parseInt(data.mouseJig) || 15000,
    mouseIdle: parseInt(data.mouseIdle) || 30000,
    mouseAmp: parseInt(data.mouseAmp) || 1,
    mouseStyle: parseInt(data.mouseStyle) ?? 0,
    lazyPct: parseInt(data.lazyPct) ?? 0,
    busyPct: parseInt(data.busyPct) ?? 0,
    dispBright: parseInt(data.dispBright) || 80,
    saverBright: parseInt(data.saverBright) || 20,
    saverTimeout: parseInt(data.saverTimeout ?? '5'),
    animStyle: parseInt(data.animStyle ?? '2'),
    name: data.name || 'GhostOperator',
    btWhileUsb: parseInt(data.btWhileUsb) ?? 0,
    scroll: parseInt(data.scroll) ?? 0,
    dashboard: parseInt(data.dashboard) ?? 0,
    invertDial: parseInt(data.invertDial) ?? 0,
    decoy: parseInt(data.decoy) ?? 0,
    schedMode: parseInt(data.schedMode) ?? 0,
    schedStart: parseInt(data.schedStart ?? '108'),
    schedEnd: parseInt(data.schedEnd ?? '204'),
    slots: (data.slots || '2,28,28,28,28,28,28,28').split(',').map(Number),
    // Simulation mode settings
    opMode: parseInt(data.opMode) ?? 0,
    jobSim: parseInt(data.jobSim) ?? 0,
    jobPerf: parseInt(data.jobPerf ?? '5'),
    jobStart: parseInt(data.jobStart ?? '96'),
    phantom: parseInt(data.phantom) ?? 0,
    clickSlots: (data.clickSlots || '1,7,7,7,7,7,7').split(',').map(Number),
    winSwitch: parseInt(data.winSwitch) ?? 0,
    switchKeys: parseInt(data.switchKeys) ?? 0,
    headerDisp: parseInt(data.headerDisp) ?? 0,
    // Sound settings
    sound: parseInt(data.sound) ?? 0,
    soundType: parseInt(data.soundType) ?? 0,
    sysSounds: parseInt(data.sysSounds) ?? 0,
    // Volume control settings
    volumeTheme: parseInt(data.volumeTheme) ?? 0,
    encButton: parseInt(data.encButton) ?? 0,
    sideButton: parseInt(data.sideButton) ?? 0,
    // Game settings
    ballSpeed: parseInt(data.ballSpeed) ?? 0,
    paddleSize: parseInt(data.paddleSize) ?? 0,
    startLives: parseInt(data.startLives) ?? 0,
    highScore: parseInt(data.highScore) ?? 0,
    snakeSpeed: parseInt(data.snakeSpeed) ?? 0,
    snakeWalls: parseInt(data.snakeWalls) ?? 0,
    snakeHiScore: parseInt(data.snakeHiScore) ?? 0,
    racerSpeed: parseInt(data.racerSpeed) ?? 0,
    racerHiScore: parseInt(data.racerHiScore) ?? 0,
    // Shift/lunch settings
    shiftDur: parseInt(data.shiftDur ?? '480'),
    lunchDur: parseInt(data.lunchDur ?? '30'),
  }
}

/**
 * Parse status response data into typed values.
 */
export function parseStatus(data) {
  return {
    connected: data.connected === '1',
    usb: data.usb === '1',
    kb: data.kb === '1',
    ms: data.ms === '1',
    bat: parseInt(data.bat) || 0,
    batMv: parseInt(data.batMv) || 0,
    profile: parseInt(data.profile ?? '1'),
    mode: parseInt(data.mode) || 0,
    mouseState: parseInt(data.mouseState) || 0,
    uptime: parseInt(data.uptime) || 0,
    kbNext: data.kbNext || '---',
    timeSynced: data.timeSynced === '1',
    daySecs: parseInt(data.daySecs) || 0,
    schedSleeping: data.schedSleeping === '1',
    // Simulation mode runtime status (only present when opMode=1)
    simBlock: parseInt(data.simBlock) || 0,
    simMode: parseInt(data.simMode) || 0,
    simPhase: parseInt(data.simPhase) || 0,
    simProfile: parseInt(data.simProfile) || 0,
  }
}

/** Profile index to name mapping (matches firmware PROFILE_NAMES[]) */
export const PROFILE_NAMES = ['LAZY', 'NORMAL', 'BUSY']

/** Profile index to label mapping for UI display */
export const PROFILE_LABEL_NAMES = ['Lazy', 'Normal', 'Busy']

/** Mode index to name mapping (matches firmware MODE_NAMES[]) */
export const MODE_NAMES = ['NORMAL', 'MENU', 'SLOTS', 'NAME', 'DECOY', 'SCHED', 'MODE', 'CLOCK', 'CRSL', 'CSLOT']

/** Mouse state index to name mapping */
export const MOUSE_STATE_NAMES = ['IDLE', 'MOVING', 'RETURNING']

/** Animation style index to name mapping (matches firmware ANIM_NAMES[]) */
export const ANIM_NAMES = ['ECG', 'EQ', 'Ghost', 'Matrix', 'Radar', 'None']

/** Mouse style index to name mapping (matches firmware MOUSE_STYLE_NAMES[]) */
export const MOUSE_STYLE_NAMES = ['Bezier', 'Brownian']

/** Screensaver timeout index to name mapping (matches firmware SAVER_NAMES[]) */
export const SAVER_NAMES = ['Never', '1 min', '5 min', '10 min', '15 min', '30 min']

/** Schedule mode index to name mapping (matches firmware SCHEDULE_MODE_NAMES[]) */
export const SCHEDULE_MODE_NAMES = ['Off', 'Sleep', 'Full auto']

/** Operation mode index to name mapping (matches firmware OP_MODE_NAMES[]) */
export const OP_MODE_NAMES = ['Simple', 'Simulation', 'Volume', 'Breakout', 'Snake', 'Racer']

/** Job simulation index to name mapping (matches firmware JOB_SIM_NAMES[]) */
export const JOB_SIM_NAMES = ['Staff', 'Developer', 'Designer']

/** Simulation phase index to name mapping (matches firmware PHASE_NAMES[]) */
export const PHASE_NAMES = ['TYPE', 'MOUSE', 'IDLE', 'SWITCH', 'K+M', 'M+K']

/** Click type index to name mapping (matches firmware CLICK_TYPE_NAMES[]) */
export const CLICK_TYPE_NAMES = ['Left', 'Middle', 'Right', 'Button 4', 'Button 5', 'Wheel Up', 'Wheel Down', 'NONE']

/** Switch keys index to name mapping (matches firmware SWITCH_KEYS_NAMES[]) */
export const SWITCH_KEYS_NAMES = ['Alt-Tab', 'Cmd-Tab']

/** Header display index to name mapping (matches firmware HEADER_DISP_NAMES[]) */
export const HEADER_DISP_NAMES = ['Job name', 'Device name']

/** Key sound type index to name mapping (matches firmware KeyboardSound enum) */
export const KEY_SOUND_NAMES = ['MX Blue', 'MX Brown', 'Membrane', 'Buckling Spring', 'Thock']

/** Work mode index to short name mapping (matches firmware WORK_MODES[].shortName) */
export const WORK_MODE_NAMES = [
  'Email', 'Read', 'Code', 'Browse', 'Chat',
  'Meeting', 'Docs', 'Coffee', 'Lunch', 'IRL Mtg', 'Files'
]

/**
 * Parse a ?wmode:N response into a structured object.
 * Response: !wmode|idx=N|name=Email|kb=65|wL=25|wN=15|wB=60|t0=5,15,...|t1=...|t2=...|dMin=60|dMax=300|pMin=15|pMax=60
 */
export function parseWorkMode(data) {
  const parseTiming = (csv) => {
    const v = (csv || '').split(',').map(Number)
    return {
      burstKeysMin: v[0] || 0, burstKeysMax: v[1] || 0,
      interKeyMinMs: v[2] || 0, interKeyMaxMs: v[3] || 0,
      burstGapMinMs: v[4] || 0, burstGapMaxMs: v[5] || 0,
      keyHoldMinMs: v[6] || 0, keyHoldMaxMs: v[7] || 0,
      mouseDurMinMs: v[8] || 0, mouseDurMaxMs: v[9] || 0,
      idleDurMinMs: v[10] || 0, idleDurMaxMs: v[11] || 0,
    }
  }
  return {
    idx: parseInt(data.idx) || 0,
    name: data.name || '???',
    kb: parseInt(data.kb) || 0,
    wL: parseInt(data.wL) || 0,
    wN: parseInt(data.wN) || 0,
    wB: parseInt(data.wB) || 0,
    timing: [parseTiming(data.t0), parseTiming(data.t1), parseTiming(data.t2)],
    dMin: parseInt(data.dMin) || 0,
    dMax: parseInt(data.dMax) || 0,
    pMin: parseInt(data.pMin) || 0,
    pMax: parseInt(data.pMax) || 0,
  }
}

/**
 * Parse a ?simblocks:N response into an array of block objects.
 * Response: !simblocks|job=N|b0=AM Email,0,30,0:40,1:40,4:20|b1=...
 */
export function parseSimBlocks(data) {
  const blocks = []
  for (let i = 0; i < 12; i++) {
    const raw = data[`b${i}`]
    if (!raw) break
    const parts = raw.split(',')
    const name = parts[0]
    const startMin = parseInt(parts[1]) || 0
    const durMin = parseInt(parts[2]) || 0
    const modes = []
    for (let j = 3; j < parts.length; j++) {
      const [modeId, weight] = parts[j].split(':').map(Number)
      modes.push({ modeId, weight })
    }
    blocks.push({ name, startMin, durMin, modes })
  }
  return blocks
}

/** Format a 5-minute slot index (0-287) as H:MM */
export function formatTime5(slot) {
  const totalMinutes = slot * 5
  const h = Math.floor(totalMinutes / 60)
  const m = totalMinutes % 60
  return `${h}:${String(m).padStart(2, '0')}`
}

/** Format a duration in minutes as "Xh" or "Xh Ym" */
export function formatShiftDuration(minutes) {
  const h = Math.floor(minutes / 60)
  const m = minutes % 60
  return m > 0 ? `${h}h ${m}m` : `${h}h`
}

/** Format milliseconds as human-readable (e.g., "1.5s", "30s", "2m") */
export function formatMs(ms) {
  if (ms < 1000) return `${ms}ms`
  if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`
  return `${(ms / 60000).toFixed(1)}m`
}
