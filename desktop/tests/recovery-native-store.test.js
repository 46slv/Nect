import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const exec = promisify(execFile);
const verifier = fileURLToPath(
  new URL('../scripts/verify-recovery-native-store.mjs', import.meta.url),
);
const A = 'run-fixture/d-1',
  B = 'run-fixture/d-2';
const write = (file, value) => fs.writeFile(file, JSON.stringify(value));

async function fixture(t) {
  const root = await fs.realpath(await fs.mkdtemp(path.join(os.tmpdir(), 'nect-store-test-')));
  t.after(() => fs.rm(root, { recursive: true, force: true }));
  const store = path.join(root, 'bin/nect-svg-acceptance-recovery');
  await fs.mkdir(path.join(store, 'run-fixture'), { recursive: true });
  await fs.mkdir(path.join(root, 'evidence'));
  await fs.mkdir(path.join(root, 'native-cases'));
  const launch = {
    PID: 1234,
    Port: 12345,
    TestBuild: true,
    StartUTC: '2026-10-22T00:00:00Z',
    Executable: path.join(root, 'bin/fixture.exe'),
    Profile: path.join(root, 'profile'),
    RecoveryRoot: store,
    SHA256: 'a'.repeat(64),
  };
  const exit = {
    PID: launch.PID,
    Port: launch.Port,
    PIDAbsent: true,
    BrowserPIDsAbsent: true,
    DirectDescendantsAbsent: true,
    PortAbsent: true,
  };
  const receipt = {
    Kind: 'test',
    PID: launch.PID,
    StartUTC: launch.StartUTC,
    EXE: launch.Executable,
    SHA256: launch.SHA256,
    Profile: launch.Profile,
    RecoveryRoot: launch.RecoveryRoot,
    CloseRequests: 1,
    PageClosed: true,
    FinishCloseRequested: true,
  };
  const scenario = {
    Passed: true,
    Phase: 'passed',
    WriterRestored: true,
    ANativeToken: A,
    BNativeToken: B,
    LatestAProject: { name: 'A', value: 90 },
    ABaselineProject: { name: 'A', value: 50 },
    BBefore: { project: { name: 'B', value: 50 } },
    Writes: [
      { id: A, revision: 0, outcome: 'native-success', savedAt: 1 },
      { id: B, revision: 0, outcome: 'native-success', savedAt: 2 },
      { id: A, revision: 2, outcome: 'native-success', savedAt: 3 },
    ],
  };
  const records = [
    [
      A + '.json',
      {
        id: A,
        version: 1,
        name: 'NECT_RECOVERY_ACCEPTANCE_A',
        savedAt: 3,
        contents: JSON.stringify(scenario.LatestAProject),
      },
    ],
    [
      A + '.previous.json',
      {
        id: A,
        version: 1,
        name: 'NECT_RECOVERY_ACCEPTANCE_A',
        savedAt: 1,
        contents: JSON.stringify(scenario.ABaselineProject),
      },
    ],
    [
      B + '.json',
      {
        id: B,
        version: 1,
        name: 'NECT_RECOVERY_ACCEPTANCE_B',
        savedAt: 2,
        contents: JSON.stringify(scenario.BBefore.project),
      },
    ],
  ];
  const persist = async () => {
    await write(path.join(root, 'evidence/launch.json'), launch);
    await write(path.join(root, 'evidence/exit.json'), exit);
    await write(path.join(root, 'native-cases/receipt.json'), receipt);
    await write(path.join(root, 'native-cases/scenario.json'), scenario);
    for (const [relative, record] of records) await write(path.join(store, relative), record);
  };
  await persist();
  return {
    root,
    store,
    launch,
    exit,
    receipt,
    scenario,
    records,
    persist,
    output: path.join(root, 'evidence/native-store-readback.json'),
    run: () => exec(process.execPath, [verifier, root]),
  };
}

test('store verifier reads only the three correlated records and never overwrites a receipt', async (t) => {
  const f = await fixture(t);
  // Malformed unrelated records must not be read or enumerated by the verifier.
  await fs.writeFile(path.join(f.store, 'unrelated.json'), 'not JSON');
  const { stdout } = await f.run();
  const result = JSON.parse(stdout);
  assert.deepEqual(
    result.Records.map((r) => r.Role),
    ['A-current', 'A-previous', 'B-current'],
  );
  assert.equal(result.NativeSuccessTimestampsMatch, true);
  const original = await fs.readFile(f.output, 'utf8');
  assert.equal(original, JSON.stringify(result, null, 2));
  await assert.rejects(f.run, /EEXIST/);
  assert.equal(await fs.readFile(f.output, 'utf8'), original);
  for (const [relative, record] of f.records)
    assert.deepEqual(JSON.parse(await fs.readFile(path.join(f.store, relative), 'utf8')), record);
});

test('store verifier accepts producer close-evaluation diagnostics only with independent closure/exit evidence', async (t) => {
  const f = await fixture(t);
  delete f.receipt.FinishCloseRequested;
  f.receipt.CloseEvaluation = 'Execution context destroyed during normal close';
  await f.persist();
  await f.run();
  assert.equal(JSON.parse(await fs.readFile(f.output, 'utf8')).ALatestExact, true);
  await fs.rm(f.output);
  f.exit.PIDAbsent = false;
  await f.persist();
  await assert.rejects(f.run);
  await assert.rejects(fs.stat(f.output), { code: 'ENOENT' });
});

for (const [label, fields, value] of [
  ['unproven process exit', 'exit.PIDAbsent', false],
  ['unproven browser exit', 'exit.BrowserPIDsAbsent', undefined],
  ['unproven descendant exit', 'exit.DirectDescendantsAbsent', false],
  ['unproven port exit', 'exit.PortAbsent', false],
  ['mismatched exit PID', 'exit.PID', 1235],
  ['missing launch and exit PID', ['launch.PID', 'exit.PID', 'receipt.PID'], undefined],
  ['mismatched exit port', 'exit.Port', 12346],
  ['invalid port', ['launch.Port', 'exit.Port'], 70000],
  ['normal build', 'launch.TestBuild', false],
  ['mismatched native receipt', 'receipt.StartUTC', '2026-10-21T00:00:00Z'],
  ['failed native receipt', 'receipt.Failure', { Message: 'fixture' }],
  ['missing finish-close evidence', 'receipt.FinishCloseRequested', undefined],
  ['writer not restored', 'scenario.WriterRestored', false],
  ['nonterminal scenario', 'scenario.Phase', 'failure-visible'],
  ['traversal token', 'scenario.ANativeToken', '../outside'],
  ['cross-run tokens', 'scenario.BNativeToken', 'run-other/d-2'],
  ['same A and B token', 'scenario.BNativeToken', A],
  ['unexpected record identity', 'records.0.1.id', B],
  ['unsupported record version', 'records.0.1.version', 2],
  ['unexpected previous A name', 'records.1.1.name', 'ordinary'],
  ['nonserialized contents', 'records.0.1.contents', { name: 'A', value: 90 }],
  ['unexpected A contents', 'records.0.1.contents', '{}'],
  ['unexpected previous contents', 'records.1.1.contents', '{}'],
  ['unexpected B contents', 'records.2.1.contents', '{}'],
  ['unmatched native write timestamp', 'records.0.1.savedAt', 4],
  ['missing native write evidence', 'scenario.Writes', []],
])
  test('store verifier rejects ' + label + ' without a success receipt', async (t) => {
    const f = await fixture(t);
    for (const field of [fields].flat()) {
      const keys = field.split('.'),
        last = keys.pop();
      const target = keys.reduce((object, key) => object[key], f);
      target[last] = value;
    }
    await f.persist();
    await assert.rejects(f.run);
    await assert.rejects(fs.stat(f.output), { code: 'ENOENT' });
  });

test('store verifier rejects an executable/store outside the stage before reading records', async (t) => {
  const f = await fixture(t);
  f.launch.Executable = path.join(path.dirname(f.root), 'outside.exe');
  f.launch.RecoveryRoot = path.join(path.dirname(f.root), 'nect-svg-acceptance-recovery');
  await f.persist();
  await assert.rejects(f.run, /outside the owned stage/);
  await assert.rejects(fs.stat(f.output), { code: 'ENOENT' });
});

test('store verifier rejects symlink redirection of an otherwise matching record', async (t) => {
  const f = await fixture(t);
  const outside = await fs.mkdtemp(path.join(os.tmpdir(), 'nect-store-outside-'));
  t.after(() => fs.rm(outside, { recursive: true, force: true }));
  const target = path.join(outside, 'record.json'),
    record = path.join(f.store, A + '.json');
  await fs.writeFile(target, 'do not read this unrelated file');
  await fs.rm(record);
  try {
    await fs.symlink(target, record, 'file');
  } catch (error) {
    if (error.code === 'EPERM') return t.skip('Host does not permit symlink fixture creation');
    throw error;
  }
  await assert.rejects(f.run, /Stage path must not contain symlinks/);
  await assert.rejects(fs.stat(f.output), { code: 'ENOENT' });
});

test('store verifier rejects malformed evidence JSON without reading the store', async (t) => {
  const f = await fixture(t);
  await fs.writeFile(path.join(f.root, 'evidence/exit.json'), '{invalid');
  await assert.rejects(f.run, /SyntaxError/);
  await assert.rejects(fs.stat(f.output), { code: 'ENOENT' });
});
