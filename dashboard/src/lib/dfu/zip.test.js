import { strToU8, zipSync } from 'fflate'
import { describe, expect, it } from 'vitest'
import { getDfuPackageInfo, parseDfuZip } from './zip.js'

function makeMinimalDfuZip() {
  const manifest = {
    manifest: {
      application: {
        dat_file: 'pkg.dat',
        bin_file: 'pkg.bin',
      },
    },
  }
  const zipBytes = zipSync({
    'manifest.json': strToU8(JSON.stringify(manifest)),
    'pkg.dat': new Uint8Array([0xaa, 0xbb]),
    'pkg.bin': new Uint8Array([0x01, 0x02, 0x03, 0x04]),
  })
  return zipBytes.buffer.slice(zipBytes.byteOffset, zipBytes.byteOffset + zipBytes.byteLength)
}

describe('parseDfuZip', () => {
  it('extracts manifest, dat, and bin from a valid package', () => {
    const { manifest, datFile, binFile } = parseDfuZip(makeMinimalDfuZip())
    expect(manifest.manifest.application.dat_file).toBe('pkg.dat')
    expect(manifest.manifest.application.bin_file).toBe('pkg.bin')
    expect(datFile).toEqual(new Uint8Array([0xaa, 0xbb]))
    expect(binFile).toEqual(new Uint8Array([0x01, 0x02, 0x03, 0x04]))
  })

  it('throws when manifest.json is missing', () => {
    const raw = zipSync({ 'other.txt': strToU8('x') })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    expect(() => parseDfuZip(buf)).toThrow(/manifest\.json/)
  })

  it('throws when manifest has no application entry', () => {
    const manifest = { manifest: { bootloader: { dat_file: 'a.dat', bin_file: 'a.bin' } } }
    const raw = zipSync({ 'manifest.json': strToU8(JSON.stringify(manifest)) })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    expect(() => parseDfuZip(buf)).toThrow(/no application entry/)
  })

  it('throws when manifest is missing dat_file or bin_file fields', () => {
    const manifest = { manifest: { application: { bin_file: 'pkg.bin' } } }
    const raw = zipSync({
      'manifest.json': strToU8(JSON.stringify(manifest)),
      'pkg.bin': new Uint8Array([0x01]),
    })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    expect(() => parseDfuZip(buf)).toThrow(/dat_file or bin_file/)
  })

  it('throws when dat file referenced in manifest is not present in ZIP', () => {
    const manifest = { manifest: { application: { dat_file: 'missing.dat', bin_file: 'pkg.bin' } } }
    const raw = zipSync({
      'manifest.json': strToU8(JSON.stringify(manifest)),
      'pkg.bin': new Uint8Array([0x01]),
    })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    expect(() => parseDfuZip(buf)).toThrow(/missing\.dat/)
  })

  it('throws when bin file referenced in manifest is not present in ZIP', () => {
    const manifest = { manifest: { application: { dat_file: 'pkg.dat', bin_file: 'missing.bin' } } }
    const raw = zipSync({
      'manifest.json': strToU8(JSON.stringify(manifest)),
      'pkg.dat': new Uint8Array([0xaa]),
    })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    expect(() => parseDfuZip(buf)).toThrow(/missing\.bin/)
  })

  it('resolves dat and bin files stored under a subdirectory path', () => {
    const manifest = {
      manifest: {
        application: {
          dat_file: 'application/nrf52840_xxaa.dat',
          bin_file: 'application/nrf52840_xxaa.bin',
        },
      },
    }
    const raw = zipSync({
      'manifest.json': strToU8(JSON.stringify(manifest)),
      'application/nrf52840_xxaa.dat': new Uint8Array([0xdd]),
      'application/nrf52840_xxaa.bin': new Uint8Array([0xbb, 0xcc]),
    })
    const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)
    const { datFile, binFile } = parseDfuZip(buf)
    expect(datFile).toBeInstanceOf(Uint8Array)
    expect(binFile).toBeInstanceOf(Uint8Array)
    expect(datFile).toEqual(new Uint8Array([0xdd]))
    expect(binFile).toEqual(new Uint8Array([0xbb, 0xcc]))
  })
})

describe('getDfuPackageInfo', () => {
  it('reports size from bin payload', () => {
    const info = getDfuPackageInfo({}, new Uint8Array(1024))
    expect(info.firmwareSize).toBe(1024)
    expect(info.firmwareSizeKB).toBe('1.0')
  })
})
