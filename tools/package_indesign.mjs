import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { dirname } from 'node:path';

export const provenance = Object.freeze({
  schema: 1,
  status: 'local evaluation candidate; external distribution blocked by unassigned prefix',
  sourceCommit: 'c6673588e7ea1e536c8bcd9aec2f72fb8de518fc',
  host: 'InDesign 21.6.0.57, Windows x64',
  certification: 'C32',
  engine: '73fb1805e5a5a477ca549a97633385991cad46045c4c5d527dcb92bd753ddc2b',
  panel: '5f4f5bab16e6c9c2621e7a9c291c1bdfd22396625f66db13759a2325e9371e46',
  prefix: '0x5A200 (unassigned)',
});
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
export function crc32(bytes) {
  let value = 0xffffffff;
  for (const byte of bytes) {
    value ^= byte;
    for (let bit = 0; bit < 8; bit++) value = (value >>> 1) ^ ((value & 1) ? 0xedb88320 : 0);
  }
  return (value ^ 0xffffffff) >>> 0;
}

// Stored ZIP entries have a fixed epoch and sorted names, so inputs fix the bytes.
export function makeZip(files) {
  const locals = [], centrals = [];
  let offset = 0;
  for (const [name, data] of [...files].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)) {
    const filename = Buffer.from(name, 'utf8');
    const local = Buffer.alloc(30), central = Buffer.alloc(46), crc = crc32(data);
    local.writeUInt32LE(0x04034b50); local.writeUInt16LE(20, 4); local.writeUInt16LE(0x800, 6);
    local.writeUInt16LE(33, 12); local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18); local.writeUInt32LE(data.length, 22); local.writeUInt16LE(filename.length, 26);
    central.writeUInt32LE(0x02014b50); central.writeUInt16LE(20, 4); central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x800, 8); central.writeUInt16LE(33, 14); central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20); central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(filename.length, 28); central.writeUInt32LE(offset, 42);
    locals.push(local, filename, data); centrals.push(central, filename);
    offset += local.length + filename.length + data.length;
  }
  const directory = Buffer.concat(centrals), end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50); end.writeUInt16LE(files.size, 8); end.writeUInt16LE(files.size, 10);
  end.writeUInt32LE(directory.length, 12); end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, directory, end]);
}

// Read both directory and local headers; check every length, checksum, and payload.
export function verifyZip(zip, expected) {
  const end = zip.length - 22;
  if (end < 0 || zip.readUInt32LE(end) !== 0x06054b50) throw new Error('Missing ZIP end');
  const count = zip.readUInt16LE(end + 10), start = zip.readUInt32LE(end + 16);
  if (count !== expected.size || start + zip.readUInt32LE(end + 12) !== end) throw new Error('Invalid ZIP directory');
  let at = start;
  const seen = new Set();
  for (let i = 0; i < count; i++) {
    if (zip.readUInt32LE(at) !== 0x02014b50) throw new Error('Invalid central header');
    const len = zip.readUInt16LE(at + 28), size = zip.readUInt32LE(at + 24), crc = zip.readUInt32LE(at + 16);
    const name = zip.toString('utf8', at + 46, at + 46 + len), local = zip.readUInt32LE(at + 42);
    if (seen.has(name) || !expected.has(name)) throw new Error('Unexpected or duplicate entry');
    seen.add(name);
    if (zip.readUInt32LE(local) !== 0x04034b50 || zip.readUInt16LE(local + 8) !== 0) throw new Error('Invalid local header');
    const nlen = zip.readUInt16LE(local + 26), extra = zip.readUInt16LE(local + 28);
    if (zip.toString('utf8', local + 30, local + 30 + nlen) !== name || zip.readUInt32LE(local + 22) !== size || zip.readUInt32LE(local + 14) !== crc) throw new Error('Header mismatch');
    const begin = local + 30 + nlen + extra, bytes = zip.subarray(begin, begin + size);
    if (begin + size > start || crc32(bytes) !== crc || !bytes.equals(expected.get(name))) throw new Error('Corrupt ZIP payload');
    at += 46 + len + zip.readUInt16LE(at + 30) + zip.readUInt16LE(at + 32);
  }
  if (at !== end) throw new Error('Directory size mismatch');
  return count;
}

// Replace only the path in a PE CodeView RSDS record; keep its identity and size.
export function sanitizePdbPath(input) {
  const bytes = Buffer.from(input), pe = bytes.readUInt32LE(0x3c);
  if (bytes.toString('ascii', 0, 2) !== 'MZ' || bytes.readUInt32LE(pe) !== 0x4550) throw new Error('Not PE');
  const optional = pe + 24, magic = bytes.readUInt16LE(optional);
  if (magic !== 0x20b) throw new Error('Expected PE32+');
  const directories = optional + 112;
  if (bytes.readUInt32LE(directories + 32) || bytes.readUInt32LE(directories + 36)) throw new Error('Signed binary requires rebuild');
  const sectionCount = bytes.readUInt16LE(pe + 6), sections = optional + bytes.readUInt16LE(pe + 20);
  const mapRva = (rva, length) => {
    for (let i = 0; i < sectionCount; i++) {
      const section = sections + 40 * i, va = bytes.readUInt32LE(section + 12), rawSize = bytes.readUInt32LE(section + 16), raw = bytes.readUInt32LE(section + 20);
      if (rva >= va && rva + length <= va + rawSize && raw + rva - va + length <= bytes.length) return raw + rva - va;
    }
    throw new Error('Unmapped debug RVA');
  };
  const rva = bytes.readUInt32LE(directories + 48), size = bytes.readUInt32LE(directories + 52);
  if (!rva || !size || size % 28) throw new Error('Invalid debug directory');
  const debug = mapRva(rva, size), spans = [];
  for (let at = debug; at < debug + size; at += 28) {
    if (bytes.readUInt32LE(at + 12) !== 2) continue;
    const length = bytes.readUInt32LE(at + 16), pointer = bytes.readUInt32LE(at + 24), address = bytes.readUInt32LE(at + 20);
    if (length < 25 || pointer + length > bytes.length || mapRva(address, length) !== pointer || bytes.toString('ascii', pointer, pointer + 4) !== 'RSDS') throw new Error('Invalid CodeView record');
    const begin = pointer + 24, end = bytes.indexOf(0, begin);
    if (end < begin || end >= pointer + length) throw new Error('Unterminated PDB path');
    const original = bytes.toString('utf8', begin, end), name = original.split(/[\\/]/).pop();
    if (!/^[A-Za-z0-9_.-]+\.pdb$/.test(name) || Buffer.byteLength(name) > end - begin) throw new Error('Invalid PDB basename');
    bytes.fill(0, begin, end + 1); bytes.write(name, begin, 'utf8');
    spans.push({ begin, end: end + 1, basename: name });
  }
  if (spans.length !== 1) throw new Error('Expected one CodeView record');
  for (let i = 0; i < bytes.length; i++) if (bytes[i] !== input[i] && !spans.some(s => i >= s.begin && i < s.end)) throw new Error('Change outside PDB path');
  return { bytes, spans };
}

export async function createCandidate({ files, output, forbiddenTokens = [] }) {
  const payload = new Map(files);
  const debugEdits = [];
  for (const [name, digest] of [['build/Release-RouteX/InDesignStrokeEngine.pln', provenance.engine], ['build/Release-UI/StrokeMasterUI.pln', provenance.panel]]) {
    if (!payload.has(name) || hash(payload.get(name)) !== digest) throw new Error('Uncertified input binary: ' + name);
    const cleaned = sanitizePdbPath(payload.get(name));
    payload.set(name, cleaned.bytes);
    debugEdits.push({ path: name, certifiedSha256: digest, packagedSha256: hash(cleaned.bytes), changedRanges: cleaned.spans });
  }
  for (const [name, bytes] of payload) {
    if (!/^[A-Za-z0-9_ .()/-]+$/.test(name) || name.startsWith('/') || name.split('/').some(part => !part || part === '..' || part === '.')) throw new Error('Unsafe package path');
    if (!Buffer.isBuffer(bytes)) throw new Error('Payload must be bytes');
    if (/\.(pdb|log|indd|pdf|eps|ps)$/i.test(name)) throw new Error('Excluded artifact type');
    for (const token of forbiddenTokens.filter(Boolean)) {
      if (bytes.includes(Buffer.from(token, 'utf8')) || bytes.includes(Buffer.from(token, 'utf16le'))) throw new Error('Identifiable material in ' + name);
    }
  }
  for (const name of ['LICENSE', 'LICENSE-EXCEPTION', 'INSTALL.md', 'tools/install-indesign-plugin.ps1', 'source/plugin/build-plugin.ps1']) if (!payload.has(name)) throw new Error('Required file missing: ' + name);
  for (const name of ['InDesignStrokeEngine', 'StrokeMasterUI']) {
    const variant = name === 'StrokeMasterUI' ? 'Release-UI' : 'Release-RouteX';
    const prefix = 'build/' + variant + '/(' + name + ' Resources)/';
    if (![...payload.keys()].some(key => key.startsWith(prefix))) throw new Error('Resources missing: ' + name);
  }
  const manifest = { ...provenance, binaryEdits: debugEdits, sourceOverlay: 'PDBALTPATH basename in both project files; no correction code changes', files: [...payload].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0).map(([path, bytes]) => ({ path, bytes: bytes.length, sha256: hash(bytes) })) };
  payload.set('manifest.json', Buffer.from(JSON.stringify(manifest, null, 2) + '\n'));
  const zip = makeZip(payload);
  verifyZip(zip, payload);
  await mkdir(dirname(output), { recursive: true });
  await writeFile(output, zip, { flag: 'wx' });
  verifyZip(await readFile(output), payload);
  const receipt = { ...provenance, binaryEdits: debugEdits, hostAcceptance: 'pending for metadata-sanitized binary copies', archive: output.split(/[\\/]/).pop(), sha256: hash(zip), bytes: zip.length, entries: payload.size, packageVerified: true, hostTestsRerun: false, installTestRerun: false };
  await writeFile(output + '.json', JSON.stringify(receipt, null, 2) + '\n', { flag: 'wx' });
  return { receipt, payload };
}
