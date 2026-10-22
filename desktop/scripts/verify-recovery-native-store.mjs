// Reads only the exact records created in this test stage, after verified exit.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
const root = process.argv[2];
assert.ok(root && path.isAbsolute(root));
const read = async (p) => JSON.parse((await fs.readFile(p, 'utf8')).replace(/^\uFEFF/, ''));
const launch = await read(path.join(root, 'evidence/launch.json')),
  exit = await read(path.join(root, 'evidence/exit.json')),
  receipt = await read(path.join(root, 'native-cases/receipt.json')),
  scenario = await read(path.join(root, 'native-cases/scenario.json'));
assert.equal(launch.TestBuild, true);
assert.equal(exit.PID, launch.PID);
for (const k of ['PIDAbsent', 'BrowserPIDsAbsent', 'DirectDescendantsAbsent', 'PortAbsent'])
  assert.equal(exit[k], true);
assert.equal(receipt.CloseRequests, 1);
assert.equal(receipt.PageClosed, true);
assert.equal(scenario.Passed, true);
assert.equal(scenario.WriterRestored, true);
const store = path.resolve(path.dirname(launch.Executable), 'nect-svg-acceptance-recovery');
assert.equal(path.resolve(launch.RecoveryRoot), store);
const relativeStore = path.relative(root, store);
assert.ok(relativeStore && !path.isAbsolute(relativeStore) && !relativeStore.startsWith('..'));
assert.equal(exit.Port, launch.Port);
const A = scenario.ANativeToken,
  B = scenario.BNativeToken;
for (const t of [A, B]) assert.match(t, /^run-[A-Za-z0-9-]+\/d-[0-9]+$/);
assert.equal(A.split('/')[0], B.split('/')[0]);
assert.notEqual(A, B);
const records = [];
const get = async (token, suffix, role) => {
  const file = path.join(store, token + suffix),
    bytes = await fs.readFile(file),
    record = JSON.parse(bytes.toString('utf8'));
  assert.equal(record.id, token);
  assert.equal(record.version, 1);
  records.push({
    Role: role,
    File: file,
    Bytes: bytes.length,
    SHA256: createHash('sha256').update(bytes).digest('hex'),
    Name: record.name,
    SavedAt: record.savedAt,
  });
  return record;
};
const currentA = await get(A, '.json', 'A-current'),
  previousA = await get(A, '.previous.json', 'A-previous'),
  currentB = await get(B, '.json', 'B-current');
assert.deepEqual(JSON.parse(currentA.contents), scenario.LatestAProject);
assert.deepEqual(JSON.parse(previousA.contents), scenario.ABaselineProject);
assert.deepEqual(JSON.parse(currentB.contents), scenario.BBefore.project);
assert.equal(currentA.name, 'NECT_RECOVERY_ACCEPTANCE_A');
assert.equal(currentB.name, 'NECT_RECOVERY_ACCEPTANCE_B');
for (const [record, rev] of [
  [currentA, 2],
  [previousA, 0],
  [currentB, 0],
]) {
  const w = scenario.Writes.findLast(
    (w) => w.id === record.id && w.revision === rev && w.outcome === 'native-success',
  );
  assert.ok(w);
  assert.equal(w.savedAt, record.savedAt);
}
const result = {
  NewOwnedStoreOnly: true,
  SameIsolatedRun: true,
  ALatestExact: true,
  APreviousExact: true,
  BExact: true,
  NativeSuccessTimestampsMatch: true,
  NoOtherRecoveryPathsRead: true,
  Records: records,
};
await fs.writeFile(
  path.join(root, 'evidence/native-store-readback.json'),
  JSON.stringify(result, null, 2),
  { flag: 'wx' },
);
console.log(JSON.stringify(result, null, 2));
