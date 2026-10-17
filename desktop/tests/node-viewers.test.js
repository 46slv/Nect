import test from 'node:test';
import assert from 'node:assert/strict';
import { document, node, serialize, exportSVG } from '../src/core.js';
import { createViewerState, bindViewer, resolveViewer } from '../src/node-viewers.js';

test('Independent public bindings preserve document and Output; live evaluation follows edits', () => {
  const doc = document(),
    layer = doc.layers[0],
    a = node('Rectangle'),
    b = node('Math');
  layer.nodes.push(a, b);
  const state = createViewerState(),
    before = serialize(doc),
    svg = exportSVG(doc);
  bindViewer(state, 0, layer, a);
  bindViewer(state, 1, layer, b);
  assert.equal(resolveViewer(doc, state.slots[0]).type, 'Path');
  assert.equal(resolveViewer(doc, state.slots[1]).type, 'Scalar');
  assert.equal(serialize(doc), before);
  assert.equal(exportSVG(doc), svg);
  const old = resolveViewer(doc, state.slots[0]).value;
  a.params.size.value[0] += 10;
  assert.notDeepEqual(resolveViewer(doc, state.slots[0]).value, old);
  b.params.a.value = 47;
  assert.equal(resolveViewer(doc, state.slots[1]).detail, String(47 + b.params.b.value));
  a.name = 'Renamed';
  assert.equal(resolveViewer(doc, state.slots[0]).label, 'Renamed');
  layer.nodes = layer.nodes.filter((n) => n !== a);
  assert.equal(resolveViewer(doc, state.slots[0]).status, 'Missing');
  layer.nodes.push(a);
  assert.equal(resolveViewer(doc, state.slots[0]).status, 'Live');
});
test('Public ports, empty, error, missing, typed values and replacement state are distinct', () => {
  const doc = document(),
    layer = doc.layers[0],
    state = createViewerState();
  const separate = node('Separate Vector');
  layer.nodes.push(separate);
  assert.throws(() => bindViewer(state, 0, layer, separate), /public output/);
  bindViewer(state, 0, layer, separate, 'x');
  assert.equal(resolveViewer(doc, state.slots[0]).type, 'Scalar');
  bindViewer(state, 0, layer, separate, 'y');
  assert.equal(resolveViewer(doc, state.slots[0]).status, 'Live');
  const vector = node('Null');
  layer.nodes.push(vector);
  bindViewer(state, 0, layer, vector);
  assert.equal(resolveViewer(doc, state.slots[0]).type, 'Vector2');
  const color = node('Color');
  layer.nodes.push(color);
  bindViewer(state, 0, layer, color);
  assert.equal(resolveViewer(doc, state.slots[0]).type, 'Color');
  const curve = node('Curve');
  curve.points = [];
  layer.nodes.push(curve);
  bindViewer(state, 0, layer, curve);
  assert.equal(resolveViewer(doc, state.slots[0]).status, 'Empty');
  const math = node('Math');
  math.params.a.expression = 'missing(';
  math.params.b.value = 0;
  layer.nodes.push(math);
  bindViewer(state, 0, layer, math);
  assert.equal(resolveViewer(doc, state.slots[0]).status, 'Error');
  assert.equal(resolveViewer(doc, createViewerState().slots[0]).status, 'Unassigned');
});

test('Painted paths retain authored gradient and images retain stage placement', () => {
  const doc = document(),
    layer = doc.layers[0],
    state = createViewerState();
  const rectangle = node('Rectangle'),
    paint = node('Linear Gradient Fill');
  paint.params.path.source = rectangle.id;
  layer.nodes.push(rectangle, paint);
  bindViewer(state, 0, layer, paint);
  const painted = resolveViewer(doc, state.slots[0]);
  assert.equal(painted.type, 'PaintedPath');
  assert.equal(painted.value.fill.kind, 'linear-gradient');
  assert.ok(painted.bounds.every(Number.isFinite));
  const image = node('Image');
  image.asset = 'pixel';
  image.params.position.value = [12, 23];
  image.params.size.value = [45, 67];
  layer.nodes.push(image);
  doc.imageAssets = {
    pixel: {
      name: 'red.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
    },
  };
  bindViewer(state, 1, layer, image);
  const result = resolveViewer(doc, state.slots[1]);
  assert.equal(result.status, 'Live');
  assert.deepEqual(result.bounds, [12, 23, 57, 90]);
  assert.deepEqual(result.value.position, [12, 23]);
  delete doc.imageAssets.pixel;
  assert.equal(resolveViewer(doc, state.slots[1]).status, 'Error');
});
