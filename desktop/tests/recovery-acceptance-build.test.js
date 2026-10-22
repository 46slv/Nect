import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import config from '../vite.config.mjs';
test('normal and explicit test frontend use different compile-time constants and output profiles', () => {
  const normal = config({ mode: 'production' }),
    testBuild = config({ mode: 'recovery-acceptance-test' });
  assert.equal(normal.define.__NECT_RECOVERY_ACCEPTANCE_BUILD__, 'false');
  assert.equal(normal.build.outDir, 'dist');
  assert.equal(testBuild.define.__NECT_RECOVERY_ACCEPTANCE_BUILD__, 'true');
  assert.equal(testBuild.build.outDir, 'dist-recovery-acceptance-test');
  for (const [c, expected] of [
    [normal, 'normal\n'],
    [testBuild, 'recovery-acceptance-test\n'],
  ]) {
    const assets = [];
    c.plugins[0].generateBundle.call({ emitFile: (a) => assets.push(a) });
    assert.equal(assets[0].source, expected);
  }
});
test('one-shot test scenario exposes copied results only and restores writer after a setup failure', async () => {
  const { runRecoveryAcceptance } = await import('../src/recovery-acceptance-test.js');
  const original = async () => {
    throw Error('Must not write');
  };
  const recovery = { current: { generation: 0 }, write: original, flush: async () => true };
  globalThis.window = { __NECT_RECOVERY_TEST_INSTANCE__: true, __TAURI_INTERNALS__: {} };
  globalThis.document = { querySelector: () => null };
  const api = {
    commandState: { expectedRevision: 0 },
    fileState: { binding: null },
    load: () => {
      throw Error('Bounded setup fixture failure');
    },
  };
  try {
    await runRecoveryAcceptance(recovery, api);
    const descriptor = Object.getOwnPropertyDescriptor(
        window,
        '__NECT_RECOVERY_ACCEPTANCE_RESULT__',
      ),
      result = window.__NECT_RECOVERY_ACCEPTANCE_RESULT__;
    assert.equal(typeof descriptor.get, 'function');
    assert.equal(descriptor.set, undefined);
    assert.equal(descriptor.configurable, false);
    assert.equal(recovery.write, original);
    assert.equal(result.WriterRestored, true);
    assert.equal(result.Phase, 'failed');
    assert.equal(result.Failure.Message, 'Bounded setup fixture failure');
    assert.deepEqual(result.Writes, []);
    result.Phase = 'tampered';
    assert.equal(window.__NECT_RECOVERY_ACCEPTANCE_RESULT__.Phase, 'failed');
    assert.equal(window.runRecoveryAcceptance, undefined);
  } finally {
    delete globalThis.window;
    delete globalThis.document;
  }
});
