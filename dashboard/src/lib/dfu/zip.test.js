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
})

describe('getDfuPackageInfo', () => {
  it('reports size from bin payload', () => {
    const info = getDfuPackageInfo({}, new Uint8Array(1024))
    expect(info.firmwareSize).toBe(1024)
    expect(info.firmwareSizeKB).toBe('1.0')
  })
})
