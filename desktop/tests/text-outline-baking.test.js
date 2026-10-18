import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';
import { duplicateArtworkLayer } from '../src/artwork-duplication.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  parameterReferenceExpression,
  validate,
  setCharacterOffsets,
  setCharacterTransforms,
  setCharacterStyles,
  duplicateNodes,
} from '../src/core.js';
const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const font = opentype.parse(
  bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
);
function fixture(configure = () => {}, fonts = { default: font }) {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill'),
    out = node('Output');
  text.text = 'BO';
  layer.nodes = [text, fill, out];
  layer.output = out.id;
  connect(layer, text.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  configure({ doc, layer, text, fill, out });
  const session = new CommandSession(doc, { fonts: () => fonts });
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: text.id, ...payload },
    });
  const current = () => session.snapshot().layers[0].nodes.find((n) => n.id === text.id);
  return { doc, layer, text, fill, out, session, send, current };
}
test('real-font outlines and SVG are exact, dependencies gone, one Undo/Redo, reopen without fonts', () => {
  const f = fixture(({ text }) => {
    text.params.size.expression = '32 * 2';
  });
  const before = f.session.snapshot(),
    outlines = evaluate({ ...f.layer, output: f.text.id }, { default: font }).path;
  const svg = exportSVG(before, { default: font });
  f.send('bake-text-outline');
  const baked = f.current(),
    after = f.session.snapshot();
  assert.equal(baked.type, 'Multi Path');
  assert.equal(baked.id, f.text.id);
  assert.deepEqual(baked.position, f.text.position);
  assert.deepEqual(baked.params, {});
  assert.equal(baked.font, '');
  assert.equal(baked.text, '');
  assert.equal(baked.characterPositions, undefined);
  assert.equal(baked.units.length, 2);
  assert.ok(baked.units.every((u) => u.contours.length > 1));
  assert.deepEqual(
    evaluate({ ...after.layers[0], output: baked.id }).path.contours,
    outlines.contours,
  );
  const reopened = deserialize(serialize(after));
  assert.equal(exportSVG(reopened), svg);
  assert.doesNotMatch(svg, /<text|font-family|@font-face/);
  assert.equal(evaluate(reopened.layers[0]).path.characterOccurrences, undefined);
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), before);
  f.send('redo');
  assert.deepEqual(f.session.snapshot(), after);
});
test('valid upstream values freeze while downstream modifiers and wires remain', () => {
  const f = fixture(({ layer, text, fill }) => {
    const value = node('Value');
    value.params.value.value = 83;
    layer.nodes.push(value);
    connect(layer, value.id, text.id, 'size');
    const transform = node('Transform');
    transform.params.offset.value = [17, -13];
    layer.nodes.push(transform);
    connect(layer, text.id, transform.id, 'path');
    connect(layer, transform.id, fill.id, 'path');
  });
  const before = exportSVG(f.doc, { default: font });
  f.send('bake-text-outline');
  assert.equal(exportSVG(f.session.snapshot()), before);
  assert.equal(f.session.snapshot().layers[0].nodes.length, f.layer.nodes.length);
  assert.deepEqual(f.current().params, {});
});
test('unit translation keeps holes and relative handles together, separate unit unchanged', () => {
  const f = fixture();
  f.send('bake-text-outline');
  const before = f.current(),
    unit = before.units[0];
  f.send('translate-path-units', { unitIds: [unit.id], delta: [37, -18] });
  const after = f.current(),
    moved = new Set(
      before.contours.filter((c) => unit.contours.includes(c.id)).flatMap((c) => c.points),
    );
  after.points.forEach((pt, i) => {
    assert.deepEqual(
      pt.position.value,
      before.points[i].position.value.map(
        (v, axis) => v + (moved.has(pt.id) ? [37, -18][axis] : 0),
      ),
    );
    assert.deepEqual(pt.handleA, before.points[i].handleA);
    assert.deepEqual(pt.handleB, before.points[i].handleB);
  });
  f.send('undo');
  assert.deepEqual(f.current(), before);
  f.send('redo');
  assert.deepEqual(f.current(), after);
});
test('empty, unavailable font, locks and invalid driven input fail transactionally', () => {
  for (const [configure, fonts, pattern] of [
    [
      ({ text }) => {
        text.text = '   ';
      },
      { default: font },
      /no outlines/,
    ],
    [() => {}, {}, /Font/],
    [
      ({ layer }) => {
        layer.locked = true;
      },
      { default: font },
      /locked/,
    ],
    [
      ({ text }) => {
        text.params.size.locked = true;
      },
      { default: font },
      /locked/,
    ],
    [
      ({ text }) => {
        text.params.size.expression = '1 / 0';
      },
      { default: font },
      /finite/,
    ],
  ]) {
    const f = fixture(configure, fonts),
      before = f.session.snapshot(),
      state = f.session.state;
    assert.throws(() => f.send('bake-text-outline'), pattern);
    assert.deepEqual(f.session.snapshot(), before);
    assert.deepEqual(f.session.state, state);
  }
});
test('Position and numeric parameter consumers refuse baking without dangling references', () => {
  for (const mode of ['wire', 'reference', 'expression']) {
    const f = fixture(({ layer, text }) => {
      const other = node(mode === 'reference' ? 'Reference' : 'Vector');
      layer.nodes.push(other);
      if (mode === 'wire') connect(layer, text.id, other.id, 'value', false, 'position');
      else if (mode === 'reference') other.reference = { nodeId: text.id, portId: 'position' };
      else {
        const value = node('Value');
        layer.nodes.push(value);
        value.params.value.expression = parameterReferenceExpression(text.id, 'size');
      }
    });
    const before = f.session.snapshot();
    assert.throws(() => f.send('bake-text-outline'), /references are in use/);
    assert.deepEqual(f.session.snapshot(), before);
  }
});
test('Multi Path direct handles, points and unit gesture cancel/commit are atomic', () => {
  const f = fixture();
  f.send('bake-text-outline');
  const before = f.current(),
    pointId = before.points[0].id;
  f.send('create-point-handles', { pointId, delta: [5, 9] });
  assert.deepEqual(f.current().points[0].handleA.value, [-5, -9]);
  assert.deepEqual(f.current().points[0].handleB.value, [5, 9]);
  f.send('undo');
  assert.deepEqual(f.current(), before);
  let gesture = f.send('begin-gesture', {
    target: {
      kind: 'path-units-translate',
      layerId: f.layer.id,
      nodeId: f.text.id,
      unitIds: [before.units[0].id],
    },
  });
  gesture = f.send('update-gesture', {
    gestureId: gesture.gestureId,
    expectedPreviewRevision: gesture.expectedPreviewRevision,
    update: { delta: [20, 10] },
  });
  f.send('cancel-gesture', { gestureId: gesture.gestureId });
  assert.deepEqual(f.current(), before);
  f.send('set-parameter', { key: `${pointId}.position`, value: [12, 14] });
  assert.deepEqual(f.current().points[0].position.value, [12, 14]);
});
test('bake caps and malformed container topology are rejected', () => {
  const f = fixture(({ text }) => {
    text.text = 'B'.repeat(1000);
  });
  const before = f.session.snapshot();
  assert.throws(() => f.send('bake-text-outline'), /budget/);
  assert.deepEqual(f.session.snapshot(), before);
  const g = fixture();
  g.send('bake-text-outline');
  const doc = g.session.snapshot();
  doc.layers[0].nodes[0].contours[0].points.push('missing-point');
  assert.throws(() => validate(doc), /point reference/);
});

test('vertical text, wrapping and rich character adjustments bake exact evaluated shape', () => {
  for (const mode of ['vertical-rl', 'horizontal-tb']) {
    const f = fixture(({ doc, text }) => {
      doc.schemaVersion = 9;
      text.writingMode = mode;
      text.leading = { mode: 'manual', value: 97 };
      text.alignment = 'center';
      text.characterPositions = setCharacterOffsets(text.text, undefined, { c2: [14, -7] });
      text.characterPositions = setCharacterTransforms(text.text, text.characterPositions, {
        c1: { rotation: 17, scale: 1.3 },
      });
      text.characterPositions = setCharacterStyles(text.text, text.characterPositions, {
        c2: { size: 37, width: 1.4 },
      });
      if (mode === 'horizontal-tb') text.textWrap = { width: 80 };
    });
    const svg = exportSVG(f.doc, { default: font });
    f.send('bake-text-outline');
    assert.equal(exportSVG(f.session.snapshot()), svg);
    assert.equal(f.current().units.length, 2);
  }
});
test('type on path detaches attachment without changing glyph unit geometry', () => {
  const f = fixture(({ doc, layer, text }) => {
    doc.schemaVersion = 7;
    const curve = node('Curve');
    layer.nodes.push(curve);
    text.pathAttachment = {
      nodeId: curve.id,
      contourId: curve.id,
      startMode: 'distance',
      start: 0,
      reversed: false,
    };
  });
  const svg = exportSVG(f.doc, { default: font });
  f.send('bake-text-outline');
  assert.equal(f.current().pathAttachment, undefined);
  assert.equal(f.current().units.length, 2);
  assert.equal(exportSVG(f.session.snapshot()), svg);
});
test('existing Remap edits keep their fingerprint and resulting SVG through bake', () => {
  const f = fixture(({ layer, text, fill }) => {
    const remap = node('Path Remap');
    layer.nodes.push(remap);
    connect(layer, text.id, remap.id, 'path');
    connect(layer, remap.id, fill.id, 'path');
    const input = evaluate({ ...layer, output: text.id }, { default: font }).path;
    remap.edits = [
      {
        pointId: input.contours[0].points[0].id,
        position: [7, -4],
        fingerprint: input.fingerprint,
      },
    ];
  });
  const svg = exportSVG(f.doc, { default: font });
  f.send('bake-text-outline');
  assert.equal(exportSVG(f.session.snapshot()), svg);
});
test('Alt handle click and drag work on Curve and Remap with Undo/cancel and lock checks', () => {
  for (const remapped of [false, true]) {
    const f = fixture(({ layer }) => {
      const curve = node('Curve');
      layer.nodes.push(curve);
      if (remapped) {
        const remap = node('Path Remap');
        layer.nodes.push(remap);
        connect(layer, curve.id, remap.id, 'path');
      }
    });
    const layer = f.session.snapshot().layers[0],
      target = layer.nodes.at(-1),
      curve = layer.nodes.find((n) => n.type === 'Curve'),
      pointId = curve.points[0].id;
    const before = f.session.snapshot();
    let gesture = f.send('begin-gesture', {
      target: { kind: 'point-handles', layerId: layer.id, nodeId: target.id, pointId },
    });
    gesture = f.send('update-gesture', {
      gestureId: gesture.gestureId,
      expectedPreviewRevision: gesture.expectedPreviewRevision,
      update: {},
    });
    f.send('commit-gesture', {
      gestureId: gesture.gestureId,
      expectedPreviewRevision: gesture.expectedPreviewRevision,
    });
    let shape = evaluate(
      { ...f.session.snapshot().layers[0], output: target.id },
      { default: font },
    ).path;
    assert.ok(Math.hypot(...shape.contours[0].points[0].handleB) > 0);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), before);
    f.send('create-point-handles', { nodeId: target.id, pointId, delta: [11, -2] });
    shape = evaluate(
      { ...f.session.snapshot().layers[0], output: target.id },
      { default: font },
    ).path;
    assert.deepEqual(shape.contours[0].points[0].handleB, [11, -2]);
    assert.deepEqual(shape.contours[0].points[0].handleA, [-11, 2]);
  }
});
test('duplicate Multi Path has valid remapped topology and independent geometry', () => {
  const f = fixture();
  f.send('bake-text-outline');
  const doc = f.session.snapshot(),
    layer = doc.layers[0];
  const [copy] = duplicateNodes(layer, [f.text.id]);
  validate(doc);
  assert.equal(copy.units.length, 2);
  assert.notEqual(copy.points[0].id, f.current().points[0].id);
});

test('unit and handle locked controls fail atomically; stale Remap handles cannot be revived', () => {
  const f = fixture();
  f.send('bake-text-outline');
  const baked = f.current(),
    pointId = baked.points[0].id;
  f.send('set-parameter-lock', { key: `${pointId}.position`, locked: true });
  const before = f.session.snapshot();
  assert.throws(
    () => f.send('translate-path-units', { unitIds: [baked.units[0].id], delta: [1, 2] }),
    /locked/,
  );
  assert.throws(() => f.send('create-point-handles', { pointId, delta: [1, 2] }), /locked/);
  assert.deepEqual(f.session.snapshot(), before);
  const g = fixture(({ layer, text }) => {
    const remap = node('Path Remap');
    layer.nodes.push(remap);
    connect(layer, text.id, remap.id, 'path');
    const path = evaluate({ ...layer, output: text.id }, { default: font }).path;
    remap.edits = [
      { pointId: path.contours[0].points[0].id, handleA: [3, 4], fingerprint: 'obsolete' },
    ];
  });
  const remap = g.layer.nodes.at(-1),
    original = g.session.snapshot();
  assert.throws(
    () =>
      g.send('create-point-handles', {
        nodeId: remap.id,
        pointId: remap.edits[0].pointId,
        delta: [1, 2],
      }),
    /suspended/,
  );
  assert.deepEqual(g.session.snapshot(), original);
});

test('baked artwork moves and duplicates through simple routes with isolated topology', () => {
  for (const transformed of [false, true]) {
    const f = fixture(({ layer, text, fill }) => {
      text.text = 'BO'.repeat(5);
      if (transformed) {
        const t = node('Transform');
        layer.nodes.push(t);
        connect(layer, text.id, t.id, 'path');
        connect(layer, t.id, fill.id, 'path');
      }
    });
    f.send('bake-text-outline');
    const before = f.session.snapshot();
    assert.ok(resolveArtworkMoveTarget(before, f.layer.id).supported);
    f.send('translate-artworks', { layerIds: [f.layer.id], delta: [20, -10] });
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), before);
    const doc = f.session.snapshot();
    const duplicated = duplicateArtworkLayer(doc, f.layer.id);
    validate(doc);
    assert.equal(doc.layers.length, 2);
    const copy = doc.layers[1].nodes.find((n) => n.type === 'Multi Path');
    assert.ok(copy);
    assert.notEqual(copy.points[0].id, f.current().points[0].id);
    copy.points[0].position.value[0] += 100;
    assert.deepEqual(doc.layers[0], before.layers[0]);
    validate(doc);
  }
});
