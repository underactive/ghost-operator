import { describe, expect, it, beforeEach } from 'vitest'
import { buildSet, handleLine, platform, settings, status } from './store.js'

describe('buildSet (JSON protocol path)', () => {
  beforeEach(() => {
    platform.value = 'c6'
  })

  it('keeps empty string as string, not 0', () => {
    const cmd = JSON.parse(buildSet('keyMin', ''))
    expect(cmd.d.keyMin).toBe('')
    expect(typeof cmd.d.keyMin).toBe('string')
  })

  it('keeps whitespace-only string as string, not 0', () => {
    const cmd = JSON.parse(buildSet('keyMin', '   '))
    expect(cmd.d.keyMin).toBe('   ')
    expect(typeof cmd.d.keyMin).toBe('string')
  })

  it('converts valid numeric string to number', () => {
    const cmd = JSON.parse(buildSet('keyMin', '2000'))
    expect(cmd.d.keyMin).toBe(2000)
    expect(typeof cmd.d.keyMin).toBe('number')
  })

  it('keeps non-numeric string as string', () => {
    const cmd = JSON.parse(buildSet('name', 'ghost'))
    expect(cmd.d.name).toBe('ghost')
    expect(typeof cmd.d.name).toBe('string')
  })

  it('splits slots string into number array', () => {
    const cmd = JSON.parse(buildSet('slots', '1,2,3,4,5,6,7,8'))
    expect(cmd.d.slots).toEqual([1, 2, 3, 4, 5, 6, 7, 8])
  })

  it('splits clickSlots string into number array', () => {
    const cmd = JSON.parse(buildSet('clickSlots', '1,7,7,7,7,7,7'))
    expect(cmd.d.clickSlots).toEqual([1, 7, 7, 7, 7, 7, 7])
  })

  it('passes array through for slots without re-splitting', () => {
    const cmd = JSON.parse(buildSet('slots', [1, 2, 3]))
    expect(cmd.d.slots).toEqual([1, 2, 3])
  })
})

describe('buildSet (text protocol path)', () => {
  beforeEach(() => {
    platform.value = null
  })

  it('returns text protocol format', () => {
    expect(buildSet('keyMin', '2000')).toBe('=keyMin:2000')
  })

  it('does not coerce empty string to number in text path', () => {
    expect(buildSet('keyMin', '')).toBe('=keyMin:')
  })
})

describe('handleLine', () => {
  beforeEach(() => {
    platform.value = null
    status.bat = 0
    status.batMv = 0
    status.connected = false
    settings.keyMin = 2000
    settings.keyMax = 6500
    settings.slots = [2, 28, 28, 28, 28, 28, 28, 28]
    settings.clickSlots = [1, 7, 7, 7, 7, 7, 7]
  })

  it('text protocol: parses status response and updates reactive state', () => {
    handleLine('!status|connected=1|kb=1|ms=1|bat=85|batMv=0|profile=1|mode=0|mouseState=0|uptime=100|kbNext=F16|timeSynced=0|daySecs=0|schedSleeping=0|simBlock=0|simMode=0|simPhase=0|simProfile=0|totalKeys=0|totalMousePx=0|totalClicks=0')
    expect(status.bat).toBe(85)
    expect(status.connected).toBe(true)
  })

  it('JSON protocol: parses status response and updates reactive state', () => {
    handleLine(JSON.stringify({ t: 'r', k: 'status', d: { bat: 72, connected: true, batMv: 0 } }))
    expect(status.bat).toBe(72)
    expect(status.connected).toBe(true)
  })

  it('text protocol: parses settings response with array fields', () => {
    handleLine('!settings|keyMin=1500|keyMax=5000|slots=3,4,5,6,7,8,9,10|clickSlots=0,1,2,3,4,5,6|mouseJig=15000|mouseIdle=30000|mouseAmp=1|mouseStyle=0|lazyPct=0|busyPct=0|dispBright=80|saverBright=20|saverTimeout=5|animStyle=2|dispFlip=0|name=TestDevice|btWhileUsb=0|scroll=0|dashboard=1|invertDial=0|decoy=0|schedMode=0|schedStart=108|schedEnd=204|opMode=0|jobSim=0|jobPerf=5|jobStart=96|phantom=0|winSwitch=0|switchKeys=0|headerDisp=0|sound=0|soundType=0|sysSounds=0|volumeTheme=0|encButton=0|sideButton=0|shiftDur=480|lunchDur=30|totalKeys=0|totalMousePx=0|totalClicks=0')
    expect(settings.keyMin).toBe(1500)
    expect(settings.slots).toEqual([3, 4, 5, 6, 7, 8, 9, 10])
    expect(settings.clickSlots).toEqual([0, 1, 2, 3, 4, 5, 6])
  })

  it('JSON protocol: parses settings response with array fields', () => {
    handleLine(JSON.stringify({ t: 'r', k: 'settings', d: { keyMin: 1000, slots: [1, 2, 3, 4, 5, 6, 7, 8], clickSlots: [0, 1, 2, 3, 4, 5, 6] } }))
    expect(settings.keyMin).toBe(1000)
    expect(settings.slots).toEqual([1, 2, 3, 4, 5, 6, 7, 8])
    expect(settings.clickSlots).toEqual([0, 1, 2, 3, 4, 5, 6])
  })

  it('auto-detects platform from JSON status response', () => {
    handleLine(JSON.stringify({ t: 'r', k: 'status', d: { bat: 80, batMv: 0, platform: 'c6' } }))
    expect(platform.value).toBe('c6')
  })

  it('malformed JSON does not throw', () => {
    expect(() => handleLine('{bad json')).not.toThrow()
  })

  it('unknown text response type is silently ignored', () => {
    const batBefore = status.bat
    expect(() => handleLine('!unknowntype|foo=bar')).not.toThrow()
    expect(status.bat).toBe(batBefore)
  })
})
