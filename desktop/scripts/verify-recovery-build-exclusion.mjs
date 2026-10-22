import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
const [normal, test, normalExe, testExe, output] = process.argv.slice(2);
assert.ok([normal, test, normalExe, testExe, output].every((p) => p && path.isAbsolute(p)));
const hash = (b) => createHash('sha256').update(b).digest('hex');
const tokens = [
  '__NECT_RECOVERY_ACCEPTANCE_RESULT__',
  '__NECT_RECOVERY_TEST_INSTANCE__',
  'NECT_RECOVERY_TEST_ONLY_A_WRITE_FAILURE',
  'NECT_RECOVERY_ACCEPTANCE_A',
];
async function assets(dir) {
  const names = (await fs.readdir(path.join(dir, 'assets'))).filter((n) => n.endsWith('.js'));
  const files = await Promise.all(
    names.map(async (n) => ({ name: n, bytes: await fs.readFile(path.join(dir, 'assets', n)) })),
  );
  return {
    text: files.map((f) => f.bytes.toString('utf8')).join('\n'),
    files: files.map((f) => ({ name: f.name, sha256: hash(f.bytes), bytes: f.bytes.length })),
  };
}
const a = await assets(normal),
  b = await assets(test);
assert.equal(
  (await fs.readFile(path.join(normal, 'nect-build-profile.txt'), 'utf8')).trim(),
  'normal',
);
assert.equal(
  (await fs.readFile(path.join(test, 'nect-build-profile.txt'), 'utf8')).trim(),
  'recovery-acceptance-test',
);
for (const t of tokens) {
  assert.equal(a.text.includes(t), false, 'Normal frontend contains ' + t);
  assert.equal(b.text.includes(t), true, 'Test frontend lacks ' + t);
}
const n = await fs.readFile(normalExe),
  t = await fs.readFile(testExe),
  marker = Buffer.from('__NECT_RECOVERY_TEST_INSTANCE__');
assert.equal(n.includes(marker), false);
assert.equal(t.includes(marker), true);
assert.notEqual(hash(n), hash(t));
const receipt = {
  NormalInjectionModuleStringsAbsent: true,
  TestModuleStringsPresent: true,
  NormalNativeMarkerAbsent: true,
  TestNativeMarkerPresent: true,
  Tokens: tokens,
  Normal: { EXE: normalExe, Bytes: n.length, SHA256: hash(n), Assets: a.files },
  Test: { EXE: testExe, Bytes: t.length, SHA256: hash(t), Assets: b.files },
};
await fs.writeFile(output, JSON.stringify(receipt, null, 2), { flag: 'wx' });
console.log(JSON.stringify(receipt, null, 2));
