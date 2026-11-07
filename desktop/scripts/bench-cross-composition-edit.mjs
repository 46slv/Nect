import assert from 'node:assert/strict';
import { pathToFileURL } from 'node:url';
import { blankDocument, node, evaluate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

// Synthetic stable-ID fixture. No font, GUI, filesystem, or product defaults.
export function referenceEditFixture(count) {
  assert.ok(Number.isInteger(count) && count > 0 && count <= 10000);
  const target = blankDocument(),
    source = blankDocument(),
    sink = node('Value'),
    curve = node('Curve');
  target.layers[0].id = 'art-target';
  source.layers[0].id = 'art-source';
  sink.id = 'reference-sink';
  curve.id = 'source-curve';
  const seed = curve.points[0];
  curve.points = Array.from({ length: count }, (_, index) => {
    const point = structuredClone(seed);
    point.id = `point-${index}`;
    point.position.value = [index, 11];
    return point;
  });
  target.layers[0].nodes.push(sink);
  source.layers[0].nodes.push(curve);
  const project = migrateLegacyDocumentToCompositionProject(target);
  project.compositions[0].id = 'comp-target';
  project.compositions.push({
    id: 'comp-source',
    name: 'Source',
    width: source.width,
    height: source.height,
    layers: source.layers,
    editor: source.editor,
  });
  return project;
}
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
const read = (owner) => {
  const doc = owner.snapshot();
  return evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(
    'reference-sink',
    'value',
  );
};

export function measureReferenceEdits(count, sampleCount = 5) {
  assert.ok(Number.isInteger(sampleCount) && sampleCount >= 2 && sampleCount <= 20);
  const owner = createCompositionSessionOwner(referenceEditFixture(count));
  send(owner, 'set-parameter-expression', {
    layerId: 'art-target',
    nodeId: 'reference-sink',
    key: 'value',
    expression: `ref(componentComposition, "comp-source", "art-source", "source-curve", "point-${count - 1}.position", "x")`,
  });
  assert.equal(read(owner), count - 1);
  owner.switchComposition('comp-source');
  const samples = [];
  for (let index = 0; index < sampleCount; index++) {
    const start = performance.now();
    send(owner, 'set-parameter', {
      layerId: 'art-source',
      nodeId: 'source-curve',
      key: `point-${count - 1}.position`,
      value: [count + index, 11],
    });
    samples.push(performance.now() - start);
  }
  // Correctness stays outside the timed dispatch, and is mandatory on every run.
  owner.switchComposition('comp-target');
  const value = count + sampleCount - 1;
  assert.equal(read(owner), value);
  const final = serializeCompositionProject(owner.projectSnapshot());
  send(owner, 'undo');
  assert.equal(read(owner), value - 1);
  send(owner, 'redo');
  assert.equal(read(owner), value);
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), final);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(final), {
    activeCompositionId: 'comp-target',
  });
  assert.equal(read(reopened), value);
  const sorted = [...samples].sort((a, b) => a - b),
    middle = Math.floor(sorted.length / 2),
    medianMs = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  return { count, sampleCount, samples, medianMs, minMs: sorted[0], maxMs: sorted.at(-1), value };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  console.log(
    JSON.stringify(
      {
        node: process.version,
        platform: process.platform,
        arch: process.arch,
        measurement:
          'Synchronous common Session dispatch; synthetic Node fixture, not GUI/native latency',
        fixtures: [100, 1000, 10000].map((count) => measureReferenceEdits(count)),
      },
      null,
      2,
    ),
  );
}
