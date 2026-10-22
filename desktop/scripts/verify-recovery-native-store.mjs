// Reads only the exact records created in this test stage, after verified exit.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
const inputRoot = process.argv[2];
assert.ok(inputRoot && path.isAbsolute(inputRoot), 'An absolute owned stage is required');
const root = await fs.realpath(inputRoot);
const within = (file) => {
  const relative = path.relative(root, file);
  assert.ok(
    relative &&
      !path.isAbsolute(relative) &&
      relative !== '..' &&
      !relative.startsWith('..' + path.sep),
    'Path is outside the owned stage',
  );
};
// Validate both lexical and physical paths before reading; junctions/symlinks must
// not redirect a purported stage record into another run or an ordinary store.
const bytesAt = async (file) => {
  within(file);
  assert.equal(await fs.realpath(file), file, 'Stage path must not contain symlinks');
  assert.ok((await fs.lstat(file)).isFile(), 'Expected a regular stage file');
  return fs.readFile(file);
};
const read = async (file) =>
  JSON.parse((await bytesAt(file)).toString('utf8').replace(/^\uFEFF/, ''));
const launch = await read(path.join(root, 'evidence/launch.json')),
  exit = await read(path.join(root, 'evidence/exit.json')),
  receipt = await read(path.join(root, 'native-cases/receipt.json')),
  scenario = await read(path.join(root, 'native-cases/scenario.json'));
assert.equal(launch.TestBuild, true);
assert.ok(Number.isSafeInteger(launch.PID) && launch.PID > 0, 'Missing launch PID');
assert.ok(
  Number.isSafeInteger(launch.Port) && launch.Port > 0 && launch.Port <= 65535,
  'Invalid launch port',
);
for (const key of ['Executable', 'Profile', 'RecoveryRoot']) {
  assert.ok(
    typeof launch[key] === 'string' && path.isAbsolute(launch[key]),
    'Invalid launch ' + key,
  );
  within(path.resolve(launch[key]));
}
assert.ok(
  typeof launch.StartUTC === 'string' && Number.isFinite(Date.parse(launch.StartUTC)),
  'Invalid launch start',
);
assert.match(launch.SHA256, /^[a-f0-9]{64}$/i);
assert.equal(exit.PID, launch.PID);
for (const k of ['PIDAbsent', 'BrowserPIDsAbsent', 'DirectDescendantsAbsent', 'PortAbsent'])
  assert.equal(exit[k], true);
assert.equal(receipt.CloseRequests, 1);
assert.equal(receipt.PageClosed, true);
// A successful close can destroy the evaluation context before its return is
// observed. The producer records that in CloseEvaluation, then independently
// waits for PageClosed and identity-specific exit proof.
assert.ok(
  receipt.FinishCloseRequested === true ||
    (typeof receipt.CloseEvaluation === 'string' && receipt.CloseEvaluation.length > 0),
  'Missing close evaluation evidence',
);
assert.equal(receipt.Kind, 'test');
assert.equal(receipt.Failure, undefined, 'Native verifier did not finish successfully');
for (const key of ['PID', 'StartUTC', 'SHA256', 'Profile', 'RecoveryRoot'])
  assert.equal(receipt[key], launch[key], 'Native receipt does not match launch ' + key);
assert.equal(receipt.EXE, launch.Executable);
assert.equal(scenario.Passed, true);
assert.equal(scenario.WriterRestored, true);
assert.equal(scenario.Phase, 'passed');
assert.ok(Array.isArray(scenario.Writes), 'Missing native write evidence');
const store = path.resolve(path.dirname(launch.Executable), 'nect-svg-acceptance-recovery');
assert.equal(path.resolve(launch.RecoveryRoot), store);
within(store);
assert.equal(await fs.realpath(store), store, 'Store must not contain symlinks');
assert.equal(exit.Port, launch.Port);
const A = scenario.ANativeToken,
  B = scenario.BNativeToken;
for (const t of [A, B]) assert.match(t, /^run-[A-Za-z0-9-]+\/d-[0-9]+$/);
assert.equal(A.split('/')[0], B.split('/')[0]);
assert.notEqual(A, B);
const records = [];
const get = async (token, suffix, role) => {
  const file = path.join(store, token + suffix),
    bytes = await bytesAt(file),
    record = JSON.parse(bytes.toString('utf8'));
  assert.equal(record.id, token);
  assert.equal(record.version, 1);
  assert.equal(typeof record.contents, 'string');
  assert.ok(Number.isSafeInteger(record.savedAt) && record.savedAt > 0, 'Invalid savedAt');
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
assert.equal(previousA.name, 'NECT_RECOVERY_ACCEPTANCE_A');
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
