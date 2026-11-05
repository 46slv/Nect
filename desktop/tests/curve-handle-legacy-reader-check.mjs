// Run in a Git checkout with the baseline commit available. Extract only the
// baseline reader into this worktree's ignored test-results directory.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import {
  blankDocument,
  node,
  CommandSession,
  parameterReferenceExpression as ref,
  serialize,
  deserialize,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

const desktop = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const output = resolve(desktop, 'test-results/curve-handle-baseline-reader');
const archive = resolve(output, 'baseline.tar');
mkdirSync(output, { recursive: true });
execFileSync(
  'git',
  [
    'archive',
    '--format=tar',
    `--output=${archive}`,
    'e91b914672b33174d2e6b1391c2e79a1cafeb0fc',
    'desktop/src',
    'desktop/package.json',
  ],
  { cwd: resolve(desktop, '..') },
);
execFileSync('tar', ['-xf', archive, '-C', output]);
const oldProject = await import(
  pathToFileURL(resolve(output, 'desktop/src/composition-project-model.js'))
);
const oldCore = await import(pathToFileURL(resolve(output, 'desktop/src/core.js')));
const doc = blankDocument(),
  layer = doc.layers[0],
  curve = node('Curve'),
  sink = node('Value');
curve.id = 'curve';
curve.points[0].id = 'anchor';
sink.id = 'sink';
layer.nodes.push(curve, sink);
const session = new CommandSession(doc);
const apply = (expression) =>
  session.dispatch({
    ...session.state,
    type: 'set-parameter-expression',
    payload: {
      layerId: layer.id,
      nodeId: sink.id,
      key: 'value',
      expression,
    },
  });
apply(ref('curve', 'anchor.position', 'x'));
const v60 = session.snapshot();
const wrapper60 = migrateLegacyDocumentToCompositionProject(v60);
assert.equal(wrapper60.compositionSchemaVersion, 2);
assert.equal(wrapper60.schemaVersion, 60);
assert.doesNotThrow(() =>
  oldProject.deserializeCompositionProject(serializeCompositionProject(wrapper60)),
);
const legacyUnknown = structuredClone(wrapper60);
legacyUnknown.compositions[0].layers[0].nodes.find((n) => n.id === 'sink').params.value.expression =
  ref('curve', 'missing.handleA', 'x');
assert.doesNotThrow(() => oldProject.deserializeCompositionProject(JSON.stringify(legacyUnknown)));
assert.throws(
  () =>
    oldCore.deserialize(JSON.stringify({ ...v60, layers: legacyUnknown.compositions[0].layers })),
  /Scalar parameter/,
);
for (const source of [
  'ref("deleted", "anchor.handleA", "length")',
  'ref("deleted", "radius", "length")',
]) {
  const oldDocument = structuredClone(v60);
  oldDocument.layers[0].nodes.find((n) => n.id === 'sink').params.value.expression = source;
  assert.throws(() => oldCore.deserialize(serialize(oldDocument)), /component must be x or y/);
  assert.throws(() => deserialize(serialize(oldDocument)), /length|schemaVersion 61/);
}
apply(ref('curve', 'anchor.handleA', 'length'));
const v61 = session.snapshot(),
  wrapper61 = migrateLegacyDocumentToCompositionProject(v61);
assert.equal(wrapper61.compositionSchemaVersion, 2);
assert.equal(wrapper61.schemaVersion, 61);
assert.throws(
  () => oldProject.deserializeCompositionProject(serializeCompositionProject(wrapper61)),
  /Unsupported Composition project version/,
);
assert.throws(() => oldCore.deserialize(serialize(v61)), /Unsupported project version/);
console.log(
  'PASS baseline reader: schema 60 wrapper accepted; schema 61 wrapper and Document rejected',
);
console.log(
  'PASS baseline legacy ambiguity: wrapper preserves missing handle text; Document rejects it',
);
console.log('PASS baseline/new readers both reject schema 60 missing-owner length syntax');
