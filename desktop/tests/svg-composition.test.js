import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  CommandSession,
  blankDocument,
  serialize,
  deserialize,
  evaluate,
  exportSVG,
} from '../src/core.js';
import { importSVG } from '../src/svg-import.js';
import { readSVGSource } from '../src/svg-tree.js';

const Parser = new Window().DOMParser;
function fixture(body) {
  const markup = `<svg viewBox="0 0 200 160">${body}</svg>`;
  const session = new CommandSession(blankDocument());
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const layerId = session.snapshot().layers[0].id;
  const result = send('add-svg-part', {
    layerId,
    name: 'public-test.svg',
    source: readSVGSource(markup, Parser),
  });
  const source = () => session.snapshot().layers.find((l) => l.id === result.sourceLayerId);
  return { markup, session, send, result, source, layerId };
}
const connected = (n) => n.joinInputOrder.map((key) => n.params[key].source).filter(Boolean);
const shape = '<path d="M10 10H90V90H10Z M30 30V70H70V30Z"/>';

test('consecutive equal paints use editable Paths, one expandable Join and one paint branch', () => {
  const f = fixture(`<g fill="#ff0000" fill-opacity="0.5">${shape}${shape}${shape}</g>`);
  const source = f.source();
  assert.equal(f.session.snapshot().layers.length, 2);
  assert.equal(source.visible, false);
  assert.equal(source.nodes.filter((n) => n.type === 'Multi Path').length, 3);
  assert.equal(source.nodes.filter((n) => n.type === 'Fill').length, 1);
  assert.equal(source.nodes.filter((n) => n.type === 'Path Remap').length, 0);
  const joins = source.nodes.filter((n) => n.type === 'Join Geometry');
  assert.equal(joins.length, 1);
  assert.equal(connected(joins[0]).length, 3);
  assert.equal(joins[0].paintMode, undefined);
  const evaluated = evaluate(source);
  assert.equal(evaluated.error, null);
  assert.equal(evaluated.path.contours.length, 6);
  assert.equal(new Set(evaluated.path.contours.map((c) => c.group)).size, 3);
  const exported = exportSVG(f.session.snapshot());
  assert.equal((exported.match(/fill-opacity="0.5"/g) || []).length, 3);
  const path = source.nodes.find((n) => n.type === 'Multi Path');
  const before = serialize(f.session.snapshot());
  f.send('translate-points', {
    layerId: source.id,
    nodeId: path.id,
    pointIds: [path.points[0].id],
    delta: [3, 4],
  });
  assert.deepEqual(
    f.source().nodes.find((n) => n.id === path.id).points[0].position.value,
    [13, 14],
  );
  assert.notEqual(exportSVG(f.session.snapshot()), exported);
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before);
  f.send('redo');
  assert.equal(
    exportSVG(deserialize(serialize(f.session.snapshot()))),
    exportSVG(f.session.snapshot()),
  );
});

test('paint changes split only consecutive branches and retain fill, stroke, alpha and source order', () => {
  const f = fixture(
    `<g fill="#ff0000" fill-opacity="0.5" stroke="#008000" stroke-opacity="0.3" stroke-width="4" stroke-linecap="round" stroke-linejoin="bevel" stroke-dasharray="3 2">${shape}${shape}</g><g fill="#0000ff">${shape}</g><g fill="#ff0000" fill-opacity="0.5" stroke="#008000" stroke-opacity="0.3" stroke-width="4" stroke-linecap="round" stroke-linejoin="bevel" stroke-dasharray="3 2">${shape}</g>`,
  );
  const source = f.source();
  assert.equal(source.nodes.filter((n) => n.type === 'Fill').length, 3);
  assert.equal(source.nodes.filter((n) => n.type === 'Stroke').length, 2);
  const final = source.nodes.find((n) => n.id === source.output);
  const join = source.nodes.find((n) => n.id === final.params.path.source);
  assert.equal(join.type, 'Join Geometry');
  assert.equal(join.paintMode, true);
  assert.equal(connected(join).length, 3);
  const svg = exportSVG(f.session.snapshot());
  const paths = [...svg.matchAll(/<path [^>]+/g)].map((m) => m[0]);
  assert.deepEqual(
    paths.map((p) => /fill="([^"]+)"/.exec(p)[1]),
    ['#ff0000', '#ff0000', '#0000ff', '#ff0000'],
  );
  assert.match(paths[0], /stroke-opacity="0.3"/);
  assert.match(paths[0], /stroke-linecap="round"/);
  assert.match(paths[0], /stroke-linejoin="bevel"/);
  assert.match(paths[0], /stroke-dasharray="3 2"/);
});

test('existing grouped imports compact binary Join trees without changing the document importer', () => {
  const body = `<g data-layer="Repeated">${shape.repeat(16)}</g>`;
  const original = importSVG(`<svg>${body}</svg>`, Parser);
  assert.equal(original.layers[0].nodes.filter((n) => n.type === 'Join Geometry').length, 15);
  const f = fixture(body);
  assert.equal(f.source().nodes.filter((n) => n.type === 'Multi Path').length, 16);
  assert.equal(f.source().nodes.filter((n) => n.type === 'Join Geometry').length, 1);
  assert.equal(evaluate(f.source()).error, null);
});

test('large imported paths retain the established editable Remap route above Multi Path budget', () => {
  const f = fixture(`<path d="M0 0${'L1 1L0 0'.repeat(5000)}"/>`);
  const source = f.source();
  assert.equal(source.nodes.filter((n) => n.type === 'Imported Path').length, 1);
  assert.equal(source.nodes.filter((n) => n.type === 'Path Remap').length, 1);
  assert.equal(source.nodes.filter((n) => n.type === 'Multi Path').length, 0);
  assert.equal(evaluate(source).error, null);
  assert.equal(evaluate(source).path.contours[0].points.length, 10001);
});

test('dense source boundary keeps direct point sockets bounded without flattening geometry', () => {
  for (const count of [1024, 1025]) {
    const path =
      'M0 0' +
      Array.from({ length: count - 1 }, (_, i) => `L${(i + 1) % 2} ${(i + 1) % 2}`).join('');
    const f = fixture(`<path d="${path}"/>`);
    const source = f.source();
    assert.equal(
      source.nodes.filter((n) => n.type === 'Multi Path').length,
      count === 1024 ? 1 : 0,
    );
    assert.equal(
      source.nodes.filter((n) => n.type === 'Path Remap').length,
      count === 1025 ? 1 : 0,
    );
    assert.equal(evaluate(source).path.contours[0].points.length, count);
  }
});

test('painted reference remains movable, scalable, maskable and copy/pastable with its shared source', () => {
  const f = fixture(`<g fill="#123456" fill-opacity="0.4">${shape}${shape}</g>`);
  const beforeSource = f.source();
  f.send('set-parameter-base', {
    layerId: f.layerId,
    nodeId: f.result.nodeId,
    key: 'position',
    value: [20, 30],
  });
  const transform = f.send('create-node', { layerId: f.layerId, nodeType: 'Transform' });
  f.send('connect', {
    layerId: f.layerId,
    sourceId: f.result.nodeId,
    nodeId: transform.nodeId,
    key: 'path',
  });
  f.send('set-parameter-base', {
    layerId: f.layerId,
    nodeId: transform.nodeId,
    key: 'scale',
    value: 2,
  });
  const mask = f.send('create-node', { layerId: f.layerId, nodeType: 'Mask' });
  const rectangle = f.send('create-node', { layerId: f.layerId, nodeType: 'Rectangle' });
  f.send('connect', {
    layerId: f.layerId,
    sourceId: transform.nodeId,
    nodeId: mask.nodeId,
    key: 'path',
  });
  f.send('connect', {
    layerId: f.layerId,
    sourceId: rectangle.nodeId,
    nodeId: mask.nodeId,
    key: 'mask',
  });
  const outputId = f.session.snapshot().layers[0].output;
  f.send('connect', { layerId: f.layerId, sourceId: mask.nodeId, nodeId: outputId, key: 'path' });
  assert.match(exportSVG(f.session.snapshot()), /<mask /);
  f.send('copy-nodes', {
    layerId: f.layerId,
    nodeIds: [f.result.nodeId, transform.nodeId, rectangle.nodeId, mask.nodeId],
  });
  const pasted = f.send('paste-nodes', { layerId: f.layerId });
  const d = f.session.snapshot();
  const copied = d.layers[0].nodes.filter((n) => pasted.nodeIds.includes(n.id));
  assert.equal(
    copied.find((n) => n.type === 'Composition Reference').compositionReference.compositionId,
    f.result.sourceLayerId,
  );
  assert.deepEqual(f.source(), beforeSource);
  assert.equal(evaluate(d.layers[0], {}, {}, d).error, null);
  assert.equal(exportSVG(deserialize(serialize(d))), exportSVG(d));
});
