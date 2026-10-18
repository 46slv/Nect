import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  point,
  serialize,
  deserialize,
  exportScene,
  exportSVG,
  CommandSession,
} from '../src/core.js';

function fixture(paint = 'Fill') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    curve = node('Curve'),
    join = node('Join Geometry'),
    fill = node(paint);
  curve.points = [point(0, 0), point(40, 0), point(40, 30), point(0, 30)];
  curve.closed = true;
  join.params.a.source = curve.id;
  fill.params.path.source = join.id;
  output.params.path.source = fill.id;
  layer.nodes.unshift(curve, join, fill);
  return { doc, layer, curve, join, fill, output };
}
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

test('SVG and scene export accept frozen Session Join documents without history changes', () => {
  const { doc } = fixture(),
    session = new CommandSession(doc),
    view = session.historyView,
    original = view.doc,
    before = serialize(original),
    state = session.state,
    expected = exportSVG(structuredClone(original));
  assert.ok(Object.isFrozen(original.layers[0].nodes[0].points[0].position));
  for (let i = 0; i < 2; i++) {
    assert.equal(exportSVG(original), expected);
    const scene = exportScene(original);
    assert.equal(scene.length, 1);
    assert.equal(scene[0].value.contours.length, 1);
    assert.notEqual(scene[0].layer, original.layers[0]);
    scene[0].layer.name = 'Detached export';
  }
  assert.match(expected, /<path /);
  assert.equal(view.doc, original);
  assert.equal(serialize(original), before);
  assert.deepEqual(session.state, state);
  assert.equal(view.past.length, 0);
  assert.equal(view.future.length, 0);
  assert.throws(() => {
    original.schemaVersion = 23;
  }, TypeError);
});

for (const frozen of [false, true]) {
  test(`legacy Join export migrates privately (${frozen ? 'frozen' : 'mutable'})`, () => {
    const { doc, join } = fixture();
    doc.schemaVersion = 2;
    delete doc.editor;
    delete join.joinInputOrder;
    const before = serialize(doc),
      migrated = deserialize(before),
      expected = exportSVG(migrated);
    assert.equal(migrated.schemaVersion, 23);
    if (frozen) freeze(doc);
    assert.equal(exportSVG(doc), expected);
    assert.deepEqual(exportScene(doc), exportScene(migrated));
    assert.equal(serialize(doc), before);
    assert.equal(doc.schemaVersion, 2);
    assert.equal(join.joinInputOrder, undefined);
    assert.equal(Object.isFrozen(join), frozen);
  });
}

test('schema-v1 paint migration exports frozen input without inserting authored nodes', () => {
  const { doc, layer, curve, output } = fixture();
  doc.schemaVersion = 1;
  delete doc.editor;
  layer.nodes = [curve, output];
  output.params.path.type = 'Path';
  output.params.path.source = curve.id;
  const before = serialize(doc),
    expected = exportSVG(deserialize(before));
  freeze(doc);
  assert.equal(exportSVG(doc), expected);
  assert.equal(exportScene(doc)[0].value.contours.length, 1);
  assert.equal(serialize(doc), before);
});

test('export preserves mutable asset metadata and does not freeze caller-owned objects', () => {
  const { doc } = fixture();
  doc.imageAssets = {
    'image-test': {
      name: 'synthetic.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
      metadata: { note: 'caller owned' },
    },
  };
  const before = serialize(doc),
    asset = doc.imageAssets['image-test'];
  exportScene(doc);
  exportSVG(doc);
  assert.equal(serialize(doc), before);
  assert.equal(Object.isFrozen(asset), false);
  assert.equal(Object.isFrozen(asset.metadata), false);
  assert.equal(Object.isFrozen(doc.layers[0]), false);
});

test('export rejection preserves input even after a private migration has started', () => {
  const { doc, join, output } = fixture();
  doc.schemaVersion = 2;
  delete doc.editor;
  delete join.joinInputOrder;
  output.params.path.type = 'Scalar';
  const before = serialize(doc);
  for (const exporter of [exportScene, exportSVG]) {
    assert.throws(() => exporter(doc), /Invalid parameter type/);
    assert.equal(serialize(doc), before);
    assert.equal(join.joinInputOrder, undefined);
  }
  const malformed = fixture().doc;
  malformed.layers[0].nodes.find((n) => n.type === 'Join Geometry').joinInputOrder = ['a', 'a'];
  for (const exporter of [exportScene, exportSVG])
    assert.throws(() => exporter(freeze(malformed)), /Invalid Join input order/);
});

test('SVG gradients retain distinct original layer indices after cloning and filtering', () => {
  const { doc } = fixture('Linear Gradient Fill'),
    second = fixture('Radial Gradient Fill').layer,
    hidden = blankLayer('Hidden');
  hidden.visible = false;
  doc.layers.unshift(hidden);
  doc.layers.push(second);
  const before = serialize(doc),
    svg = exportSVG(freeze(doc));
  assert.match(svg, /id="nect-gradient-1"/);
  assert.match(svg, /id="nect-gradient-2"/);
  assert.match(svg, /url\(#nect-gradient-1\)/);
  assert.match(svg, /url\(#nect-gradient-2\)/);
  assert.doesNotMatch(svg, /nect-gradient--1/);
  assert.equal(serialize(doc), before);
});
