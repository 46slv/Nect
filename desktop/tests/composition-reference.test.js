import { paintedGroup, groupGeometry } from '../src/composition-reference.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  layer,
  node,
  connect,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  validate,
  duplicateNodes,
  CommandSession,
  pathBounds,
} from '../src/core.js';
import { resolveArtworkMoveTarget } from '../src/artwork-move-target.js';

function fixture() {
  const doc = document(),
    source = doc.layers[0],
    target = layer('Instances'),
    shape = node('Rectangle'),
    fill = node('Fill'),
    out = node('Output'),
    ref = node('Composition Reference'),
    dest = node('Output');
  shape.params.center.value = [50, 50];
  shape.params.size.value = [80, 80];
  fill.params.color.value = { hex: '#ff0000', alpha: 0.5 };
  source.nodes = [shape, fill, out];
  source.output = out.id;
  source.visible = false;
  connect(source, shape.id, fill.id, 'path');
  connect(source, fill.id, out.id, 'path');
  ref.compositionReference = { compositionId: source.id };
  target.nodes = [ref, dest];
  target.output = dest.id;
  connect(target, ref.id, dest.id, 'path');
  doc.layers.push(target);
  doc.schemaVersion = 31;
  validate(doc);
  return {
    doc,
    source,
    target,
    shape,
    fill,
    out,
    ref,
    dest,
    run: () => evaluate(target, {}, doc.imageAssets, doc),
  };
}
test('live stable Composition output keeps paint, hidden sources, edits and output rewires without stale caches', () => {
  const f = fixture();
  let r = f.run();
  assert.equal(r.error, null);
  assert.equal(r.path.children[0].fill.alpha, 0.5);
  assert.equal(r.path.kind, 'painted-group');
  assert.ok(f.run().stats.reused > 0);
  f.shape.params.center.value = [150, 50];
  assert.equal(pathBounds(f.run().path)[0][1], 150);
  f.fill.params.color.value.alpha = 0.25;
  assert.equal(f.run().path.children[0].fill.alpha, 0.25);
  const second = node('Rectangle');
  second.params.center.value = [250, 50];
  f.source.nodes.push(second);
  connect(f.source, second.id, f.fill.id, 'path');
  assert.equal(pathBounds(f.run().path)[0][1], 250);
  f.source.name = 'Renamed';
  assert.equal(f.ref.compositionReference.compositionId, f.source.id);
  const svg = exportSVG(f.doc);
  assert.equal((svg.match(/fill-opacity="0.25"/g) || []).length, 1);
  const loaded = deserialize(serialize(f.doc));
  assert.equal(exportSVG(loaded), svg);
  f.doc.layers.splice(0, 1);
  assert.match(f.run().error, /Missing Composition/);
  assert.match(serialize(f.doc), new RegExp(f.source.id));
});
test('paint-aware Transform and Join keep independent appearances and duplicate instances', () => {
  const f = fixture(),
    copy = duplicateNodes(f.target, [f.ref.id])[0],
    tr = node('Transform'),
    join = node('Join Geometry');
  copy.params.position.value = [120, 0];
  f.target.nodes.push(tr, join);
  connect(f.target, copy.id, tr.id, 'path');
  assert.equal(tr.paintMode, true);
  tr.params.offset.value = [10, 20];
  tr.params.scale.value = 2;
  connect(f.target, f.ref.id, join.id, 'a');
  connect(f.target, tr.id, join.id, 'b');
  connect(f.target, join.id, f.dest.id, 'path');
  validate(f.doc);
  const r = f.run();
  assert.equal(r.error, null);
  assert.equal(r.path.children.length, 2);
  assert.equal(r.path.children[1].children[0].children[0].fill.alpha, 0.5);
  assert.equal(copy.compositionReference.compositionId, f.source.id);
  assert.ok(exportSVG(f.doc).includes('matrix(2 0 0 2 10 20)'));
  const convert = node('Convert to Path');
  f.target.nodes.push(convert);
  connect(f.target, join.id, convert.id, 'path');
  const geometry = f.run().run(convert.id);
  assert.equal(geometry.kind, undefined);
  assert.equal(geometry.children, undefined);
  assert.ok(geometry.contours.length);
  const boolean = node('Boolean'),
    ordinary = node('Transform');
  f.target.nodes.push(boolean, ordinary);
  connect(f.target, ordinary.id, boolean.id, 'a');
  assert.throws(() => connect(f.target, f.ref.id, ordinary.id, 'path'), /Painted/);
  assert.equal(ordinary.paintMode, undefined);
  assert.equal(ordinary.params.path.source, undefined);
});
test('schema and cycle gates reject direct and indirect targets atomically; missing targets remain explicit', () => {
  const f = fixture(),
    session = new CommandSession(f.doc),
    send = (id, target) =>
      session.dispatch({
        ...session.state,
        type: 'set-composition-reference',
        payload: { layerId: f.target.id, nodeId: id, target },
      });
  const before = serialize(session.snapshot());
  assert.throws(() => send(f.ref.id, { compositionId: f.target.id }), /cycle/);
  assert.equal(serialize(session.snapshot()), before);
  send(f.ref.id, null);
  assert.match(evaluate(session.snapshot().layers[1], {}, {}, session.snapshot()).error, /Choose/);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), before);
  const back = node('Composition Reference');
  back.compositionReference = { compositionId: f.target.id };
  f.source.nodes.push(back);
  assert.throws(() => validate(f.doc), /cycle/);
  f.source.nodes.pop();
  f.doc.schemaVersion = 30;
  assert.throws(() => validate(f.doc), /schemaVersion 31/);
  f.doc.schemaVersion = 31;
  f.ref.compositionReference = { compositionId: 'deleted-composition' };
  assert.doesNotThrow(() => validate(f.doc));
  assert.match(f.run().error, /Missing Composition/);
});
test('Composition artwork position and terminal Transform support ordinary canvas Move', () => {
  const f = fixture();
  const move = resolveArtworkMoveTarget(f.doc, f.target.id);
  assert.equal(move.supported, true, move.reason);
  assert.equal(move.target.parameter, 'position');
  const tr = node('Transform');
  f.target.nodes.push(tr);
  connect(f.target, f.ref.id, tr.id, 'path');
  connect(f.target, tr.id, f.dest.id, 'path');
  const transformed = resolveArtworkMoveTarget(f.doc, f.target.id);
  assert.equal(transformed.supported, true, transformed.reason);
  assert.equal(transformed.target.nodeId, tr.id);
});

test('normal artwork translation is one Undo and protects source state and locks', () => {
  const f = fixture(),
    session = new CommandSession(f.doc),
    before = serialize(session.snapshot()),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: f.target.id, ...payload } });
  send('translate-artworks', { layerIds: [f.target.id], delta: [15, -8] });
  let doc = session.snapshot();
  assert.deepEqual(
    doc.layers[1].nodes.find((n) => n.id === f.ref.id).params.position.value,
    [15, -8],
  );
  assert.deepEqual(doc.layers[0], f.source);
  assert.equal(session.historyView.past.length, 1);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('set-parameter-lock', { nodeId: f.ref.id, key: 'position', locked: true });
  const locked = serialize(session.snapshot());
  assert.throws(
    () => send('translate-artworks', { layerIds: [f.target.id], delta: [1, 1] }),
    /locked/,
  );
  assert.equal(serialize(session.snapshot()), locked);
});
test('gradient servers, stroke scale and nested image alpha are carried as vector/image children', () => {
  const f = fixture(),
    gradient = node('Linear Gradient Fill'),
    stroke = node('Stroke');
  f.source.nodes.push(gradient, stroke);
  connect(f.source, f.shape.id, gradient.id, 'path');
  connect(f.source, gradient.id, stroke.id, 'path');
  connect(f.source, stroke.id, f.out.id, 'path');
  const copy = duplicateNodes(f.target, [f.ref.id])[0],
    join = node('Join Geometry'),
    tr = node('Transform');
  copy.params.position.value = [100, 0];
  f.target.nodes.push(join, tr);
  connect(f.target, f.ref.id, join.id, 'a');
  connect(f.target, copy.id, join.id, 'b');
  connect(f.target, join.id, tr.id, 'path');
  connect(f.target, tr.id, f.dest.id, 'path');
  tr.params.scale.value = 2;
  const svg = exportSVG(f.doc),
    ids = [...svg.matchAll(/<linearGradient id="([^"]+)"/g)].map((m) => m[1]);
  assert.equal(ids.length, 2);
  assert.equal(new Set(ids).size, 2);
  for (const id of ids) assert.ok(svg.includes(`url(#${id})`));
  assert.ok(svg.includes('stroke-width="2"'));
  assert.ok(svg.includes('matrix(2 0 0 2 0 0)'));
  const image = node('Image'),
    out = node('Image Output');
  image.asset = 'image-test';
  image.params.position.value = [12, 23];
  image.params.size.value = [45, 67];
  f.doc.imageAssets = {
    'image-test': {
      name: 'red.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
    },
  };
  f.source.nodes = [image, out];
  f.source.output = out.id;
  connect(f.source, image.id, out.id, 'path');
  assert.equal(f.run().error, null);
  assert.match(exportSVG(f.doc), /<image[^>]+href="data:image\/png;base64/);
  assert.equal(exportSVG(deserialize(serialize(f.doc))), exportSVG(f.doc));
});

test('source typography export restrictions survive appearance grouping and explicit geometry conversion', () => {
  const f = fixture(),
    source = f.run().path.children[0];
  source.characterOccurrences = [{ source: 'source-text', suspended: true }];
  const first = paintedGroup([source]);
  assert.match(first.exportBlockers[0], /Character positions/);
  assert.equal(first.children[0].characterOccurrences, undefined);
  const nested = paintedGroup([first], { matrix: [1, 0, 0, 1, 20, 30] });
  assert.deepEqual(nested.exportBlockers, first.exportBlockers);
  assert.deepEqual(groupGeometry(nested).exportBlockers, first.exportBlockers);
});
