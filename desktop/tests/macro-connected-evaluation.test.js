import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  connect,
  definitions,
  deserialize,
  evaluate,
  node,
  serialize,
  validate,
} from '../src/core.js';
import { nodePickerItems } from '../src/node-catalog.js';

function fixture() {
  const definition = {
      format: 'nect.macro',
      formatVersion: 1,
      id: 'double',
      revision: 1,
      name: 'Double',
      input: { type: 'Path', nodeId: 'a', socket: 'path' },
      output: { type: 'Path', nodeId: 'b', port: null },
      nodes: ['a', 'b'].map((id) => ({
        id,
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
      })),
      edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
      parameters: [
        { id: 'scale', label: 'Scale', nodeId: 'a', key: 'scale', type: 'Scalar', defaultValue: 1 },
      ],
    },
    doc = blankDocument(),
    layer = doc.layers[0],
    rectangle = node('Rectangle'),
    macro = node('Macro'),
    transform = node('Transform'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 66;
  doc.macroDefinitions = [definition];
  macro.macro = { definitionId: 'double', definitionRevision: 1, overrides: {} };
  layer.nodes = [rectangle, macro, transform, fill, output];
  layer.output = output.id;
  connect(layer, rectangle.id, macro.id, 'path');
  connect(layer, macro.id, transform.id, 'path');
  connect(layer, transform.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  validate(doc);
  return { doc, layer, rectangle, macro, transform };
}

test('a pinned Macro evaluates its connected Rectangle through two existing Transform steps', () => {
  const f = fixture(),
    before = structuredClone(f.doc),
    frame = evaluate(f.layer, {}, {}, f.doc),
    source = frame.run(f.rectangle.id),
    output = frame.path;
  assert.ifError(frame.error);
  assert.deepEqual(
    output.contours.map((contour) => contour.id),
    source.contours.map((contour) => contour.id),
  );
  assert.deepEqual(
    output.contours.flatMap((contour) => contour.points.map((point) => point.id)),
    source.contours.flatMap((contour) => contour.points.map((point) => point.id)),
  );
  assert.deepEqual(f.doc, before);
  const reopened = deserialize(serialize(f.doc)),
    after = evaluate(reopened.layers[0], {}, {}, reopened);
  assert.ifError(after.error);
  assert.deepEqual(after.path, output);
});

test('per-instance override changes the same-layer output without a stale persistent cache', () => {
  const f = fixture(),
    first = evaluate(f.layer, {}, {}, f.doc).path;
  f.macro.macro.overrides.scale = 2;
  const edited = evaluate(f.layer, {}, {}, f.doc);
  assert.ifError(edited.error);
  assert.notDeepEqual(edited.path.contours, first.contours);
  f.macro.macro.overrides.scale = 1;
  const restored = evaluate(f.layer, {}, {}, f.doc);
  assert.ifError(restored.error);
  assert.deepEqual(restored.path.contours, first.contours);
});

test('invalid pins, unsupported sources and actual Path overflow fail closed', () => {
  const f = fixture();
  f.macro.macro.definitionRevision = 2;
  assert.throws(() => validate(f.doc), /definition|pin/i);
  f.macro.macro.definitionRevision = 1;
  const circle = node('Circle');
  f.layer.nodes.push(circle);
  connect(f.layer, circle.id, f.macro.id, 'path');
  assert.match(evaluate(f.layer, {}, {}, f.doc).error, /Rectangle/);
  connect(f.layer, f.rectangle.id, f.macro.id, 'path');
  f.macro.macro.overrides.scale = Number.MAX_VALUE;
  assert.match(evaluate(f.layer, {}, {}, f.doc).error, /finite/i);
});

test('an unbound Macro is not offered by the ordinary Add-node picker', () => {
  assert.equal(
    nodePickerItems(definitions).some((entry) => entry.typeId === 'Macro'),
    false,
  );
});
