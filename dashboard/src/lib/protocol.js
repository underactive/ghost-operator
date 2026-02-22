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
    mouseStyle: parseInt(data.mouseStyle) || 0,
    lazyPct: parseInt(data.lazyPct) || 0,
    busyPct: parseInt(data.busyPct) || 0,
    dispBright: parseInt(data.dispBright) || 80,
    saverBright: parseInt(data.saverBright) || 20,
    saverTimeout: parseInt(data.saverTimeout ?? '5'),
    animStyle: parseInt(data.animStyle ?? '2'),
    name: data.name || 'GhostOperator',
    btWhileUsb: parseInt(data.btWhileUsb) || 0,
    scroll: parseInt(data.scroll) || 0,
    dashboard: parseInt(data.dashboard) || 0,
    decoy: parseInt(data.decoy) || 0,
    schedMode: parseInt(data.schedMode) || 0,
    schedStart: parseInt(data.schedStart ?? '108'),
    schedEnd: parseInt(data.schedEnd ?? '204'),
    slots: (data.slots || '2,28,28,28,28,28,28,28').split(',').map(Number),
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
  }
}

/** Profile index to name mapping (matches firmware PROFILE_NAMES[]) */
export const PROFILE_NAMES = ['LAZY', 'NORMAL', 'BUSY']

/** Mode index to name mapping (matches firmware MODE_NAMES[]) */
export const MODE_NAMES = ['NORMAL', 'MENU', 'SLOTS', 'NAME', 'DECOY']

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

/** Format a 5-minute slot index (0-287) as H:MM */
export function formatTime5(slot) {
  const totalMinutes = slot * 5
  const h = Math.floor(totalMinutes / 60)
  const m = totalMinutes % 60
  return `${h}:${String(m).padStart(2, '0')}`
}
