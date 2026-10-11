import assert from 'node:assert/strict';
import { access } from 'node:fs/promises';
import { crc32, makeZip, verifyZip, sanitizePdbPath, createCandidate } from '../../tools/package_indesign.mjs';

// Run in process with the certified inputs; no shell, Adobe host, or installation.
export async function runPackagingTests({ files, scratchOutput }) {
  const passed = [];
  assert.equal(crc32(Buffer.from('123456789')), 0xcbf43926);
  passed.push('standard CRC-32 vector');
  const samples = new Map([['empty', Buffer.alloc(0)], ['dir/test.txt', Buffer.from('payload')]]);
  const zip = makeZip(samples);
  assert.equal(verifyZip(zip, samples), 2);
  assert.deepEqual(makeZip(samples), zip);
  passed.push('deterministic archive with empty file');
  const corrupt = Buffer.from(zip);
  corrupt[corrupt.indexOf(Buffer.from('payload'))] ^= 1;
  assert.throws(() => verifyZip(corrupt, samples));
  assert.throws(() => verifyZip(zip.subarray(0, zip.length - 1), samples));
  passed.push('corrupt and truncated archive refusal');
  for (const name of ['build/Release-RouteX/InDesignStrokeEngine.pln', 'build/Release-UI/StrokeMasterUI.pln']) {
    const original = files.get(name), clean = sanitizePdbPath(original);
    assert.equal(clean.spans.length, 1);
    assert.deepEqual(sanitizePdbPath(clean.bytes).bytes, clean.bytes);
    let changes = 0;
    for (let i = 0; i < original.length; i++) if (original[i] !== clean.bytes[i]) {
      changes++;
      assert.ok(i >= clean.spans[0].begin && i < clean.spans[0].end);
    }
    assert.ok(changes > 0);
    const signed = Buffer.from(original), pe = signed.readUInt32LE(0x3c);
    signed.writeUInt32LE(1, pe + 24 + 112 + 32);
    assert.throws(() => sanitizePdbPath(signed), /Signed/);
    passed.push('bounded, idempotent debug-path edit and signed-input refusal: ' + name.split('/').pop());
  }
  assert.throws(() => sanitizePdbPath(Buffer.alloc(128)), /Not PE/);
  passed.push('invalid PE refusal');
  const reject = async (mutate, pattern, forbiddenTokens = []) => {
    const invalid = new Map(files); mutate(invalid);
    await assert.rejects(createCandidate({ files: invalid, output: scratchOutput, forbiddenTokens }), pattern);
    await assert.rejects(access(scratchOutput), error => error.code === 'ENOENT');
  };
  await reject(map => map.set('build/Release-UI/StrokeMasterUI.pln', Buffer.alloc(8)), /Uncertified/);
  await reject(map => map.set('../escape', Buffer.from('bad')), /Unsafe/);
  await reject(map => map.set('private.txt', Buffer.from('sensitive-test-token')), /Identifiable/, ['sensitive-test-token']);
  await reject(map => map.delete('LICENSE'), /Required file missing/);
  await reject(map => { for (const name of map.keys()) if (name.startsWith('build/Release-UI/(StrokeMasterUI Resources)/')) map.delete(name); }, /Resources missing/);
  passed.push('uncertified input, traversal, privacy, license, and resources refusals write nothing');
  return passed;
}
