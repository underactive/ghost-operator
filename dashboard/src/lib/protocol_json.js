/**
 * JSON protocol builders/parsers for Ghost Operator C6 devices.
 * Used when the device platform is detected as 'c6'.
 *
 * Protocol format:
 *   Query:   { "t": "q", "k": "<key>", ... }
 *   Set:     { "t": "s", "d": { ... } }
 *   Command: { "t": "c", "k": "<action>" }
 *
 * Response format:
 *   Reply:   { "t": "r", "k": "<type>", "d": { ... } }
 *   OK:      { "t": "ok" }
 *   Error:   { "t": "err", "m": "<message>" }
 *   Push:    { "t": "p", "k": "<type>", "d": { ... } }
 */

/** Build a JSON query */
export function buildJsonQuery(key, params = {}) {
  return JSON.stringify({ t: 'q', k: key, ...params })
}

/** Build a JSON set command (partial settings update) */
export function buildJsonSet(data) {
  return JSON.stringify({ t: 's', d: data })
}

/** Build a JSON action command */
export function buildJsonCommand(key) {
  return JSON.stringify({ t: 'c', k: key })
}

/**
 * Parse a JSON response line.
 * Returns { type, data, json } matching the text protocol's return shape.
 */
export function parseJsonLine(line) {
  try {
    const msg = JSON.parse(line)
    if (msg.t === 'r') {
      return { type: msg.k, data: msg.d, json: true }
    } else if (msg.t === 'ok') {
      return { type: 'ok', data: {}, json: true }
    } else if (msg.t === 'err') {
      return { type: 'error', data: { message: msg.m }, json: true }
    } else if (msg.t === 'p') {
      return { type: msg.k, data: msg.d, json: true, push: true }
    }
    return { type: 'unknown', data: msg, json: true }
  } catch {
    return { type: 'error', data: { message: 'JSON parse error' }, json: true }
  }
}
