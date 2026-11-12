import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, connect, evaluate, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

test('combined shape, Boolean, offset and subdivision preserve Remap history and Project reload', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    circle = node('Circle'),
    polygon = node('Polygon'),
    boolean = node('Boolean'),
    offset = node('Closed Path Offset'),
    subdivide = node('Subdivide Path'),
    remap = node('Path Remap'),
    fill = node('Fill');
  boolean.operation = 'union';
  offset.params.amount.value = 8;
  subdivide.params.segments.value = 2;
  layer.nodes.push(circle, polygon, boolean, offset, subdivide, remap, fill);
  connect(layer, circle.id, boolean.id, 'a');
  connect(layer, polygon.id, boolean.id, 'b');
  connect(layer, boolean.id, offset.id, 'path');
  connect(layer, offset.id, subdivide.id, 'path');
  connect(layer, subdivide.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');

  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const output = () => {
    const current = owner.snapshot();
    return evaluate(current.layers[0], {}, current.imageAssets, current);
  };
  const source = output().run(subdivide.id),
    pointId = source.contours[0].points[0].id;
  send('edit-remap', { layerId: layer.id, nodeId: remap.id, action: 'enable', pointId });
  send('set-parameter', {
    layerId: layer.id,
    nodeId: remap.id,
    key: `remap.${pointId}.position`,
    value: [4, 6],
  });
  const before = owner.projectSnapshot(),
    result = output().run(remap.id);
  assert.equal(result.contours[0].points[0].id, pointId);
  assert.equal(output().warnings.length, 0);

  send('set-parameter', { layerId: layer.id, nodeId: offset.id, key: 'amount', value: 12 });
  const changed = owner.projectSnapshot();
  assert.ok(output().warnings.some((warning) => warning.includes('suspended')));
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(output().run(remap.id), result);
  send('redo');
  assert.deepEqual(owner.projectSnapshot(), changed);
  const saved = serializeCompositionProject(changed),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(saved));
  assert.deepEqual(reopened.projectSnapshot(), changed);
  const reopenedDoc = reopened.snapshot();
  assert.ok(
    evaluate(reopenedDoc.layers[0], {}, reopenedDoc.imageAssets, reopenedDoc).warnings.some(
      (warning) => warning.includes('suspended'),
    ),
  );
});
