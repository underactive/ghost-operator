import { describe, expect, it } from 'vitest'
import {
  buildAction,
  buildQuery,
  buildSet,
  formatTime5,
  parseResponse,
  parseSettings,
  parseStatus,
} from './protocol.js'

describe('parseResponse', () => {
  it('parses key=value settings-style lines', () => {
    const r = parseResponse('!settings|keyMin=2000|keyMax=6500')
    expect(r.type).toBe('settings')
    expect(r.data).toEqual({ keyMin: '2000', keyMax: '6500' })
  })

  it('parses keys as bare list', () => {
    const r = parseResponse('!keys|F13|F14|NONE')
    expect(r.type).toBe('keys')
    expect(r.data).toEqual(['F13', 'F14', 'NONE'])
  })

  it('parses ok and error lines', () => {
    expect(parseResponse('+ok')).toEqual({ type: 'ok', data: {} })
    expect(parseResponse('-err:bad')).toEqual({
      type: 'error',
      data: { message: 'bad' },
    })
  })
})

describe('buildQuery / buildSet / buildAction', () => {
  it('builds protocol strings', () => {
    expect(buildQuery('status')).toBe('?status')
    expect(buildSet('keyMin', '2000')).toBe('=keyMin:2000')
    expect(buildAction('save')).toBe('!save')
  })
})

describe('parseSettings', () => {
  it('coerces numeric fields and defaults', () => {
    const s = parseSettings({
      keyMin: '1000',
      keyMax: '2000',
      slots: '3,4,5,6,7,8,9,10',
      jobPerf: '7',
    })
    expect(s.keyMin).toBe(1000)
    expect(s.keyMax).toBe(2000)
    expect(s.slots).toEqual([3, 4, 5, 6, 7, 8, 9, 10])
    expect(s.jobPerf).toBe(7)
  })
})

describe('parseStatus', () => {
  it('maps booleans and numbers', () => {
    const s = parseStatus({
      connected: '1',
      usb: '0',
      bat: '87',
      profile: '2',
    })
    expect(s.connected).toBe(true)
    expect(s.usb).toBe(false)
    expect(s.bat).toBe(87)
    expect(s.profile).toBe(2)
  })
})

describe('formatTime5', () => {
  it('formats slot index to H:MM', () => {
    expect(formatTime5(108)).toBe('9:00')
    expect(formatTime5(0)).toBe('0:00')
  })
})
