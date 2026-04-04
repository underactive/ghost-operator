import { describe, expect, it, beforeEach } from 'vitest'
import { buildSet, platform } from './store.js'

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
