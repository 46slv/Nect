import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import {
  document,
  node,
  validate,
  serialize,
  deserialize,
  CommandSession,
  definitions,
} from '../src/core.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodePickerItems } from '../src/node-catalog.js';

test('normal composition rejects and hides the scratch-only developer fixture', (t) => {
  t.diagnostic(`normal test child pid=${process.pid}`);
  const type = 'Dev Affine Fixture';
  assert.equal(builtinNodeRegistry.get(type), undefined);
  assert.throws(() => node(type), /Unknown node/);
  assert.equal(
    nodePickerItems(definitions).some((item) => item.id === type),
    false,
  );
  const doc = document(),
    fake = node('Math');
  fake.type = type;
  doc.layers[0].nodes.push(fake);
  const json = serialize(doc);
  assert.throws(() => validate(structuredClone(doc)), /Invalid|Unknown/);
  assert.throws(() => deserialize(json), /Invalid|Unknown/);
  assert.throws(() => new CommandSession(structuredClone(doc)), /Invalid|Unknown/);
});

test('fresh isolated manifest child shares actual core, Session, codec and synthetic Inspector', (t) => {
  const registration = new URL('./dev-affine-register.mjs', import.meta.url).href,
    script = fileURLToPath(new URL('./dev-affine-scratch-run.mjs', import.meta.url));
  const result = spawnSync(process.execPath, ['--import', registration, script], {
    encoding: 'utf8',
    timeout: 30000,
    windowsHide: true,
  });
  assert.equal(result.error, undefined, result.error?.message);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(result.signal, null);
  const report = JSON.parse(result.stdout.trim());
  assert.equal(report.registrySame, true);
  assert.equal(report.headless, 10);
  assert.equal(report.uiAfterEdit, 19);
  assert.equal(report.undoRedo, true);
  assert.equal(report.reopen, true);
  assert.equal(report.menuHidden, true);
  assert.equal(report.atomicInvalid, true);
  t.diagnostic(
    `isolated child pid=${report.pid}; first output=${report.firstOutputUtc}; core=${report.coreUrl}; registry=${report.registryUrl}; manifest=${report.manifestUrl}`,
  );
});
