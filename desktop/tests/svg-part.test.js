import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  CommandSession,
  blankDocument,
  document,
  node,
  connect,
  serialize,
  deserialize,
  exportSVG,
  evaluate,
} from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';
const Parser = new Window().DOMParser;
const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 160"><path d="M10 10H90V90H10Z M30 30V70H70V30Z" fill="#ff0000" fill-opacity="0.5"/><rect x="60" y="10" width="80" height="80" fill="#0000ff" fill-opacity="0.5"/><path d="M10 120H140" fill="none" stroke="#008000" stroke-width="4" stroke-linecap="round"/><rect x="150" y="110" width="20" height="20" fill="none"/></svg>`;
const tree = (s = svg) => readSVGSource(s, Parser);
function fixture(blank = false) {
  const d = blank ? blankDocument() : document();
  d.layers[0].nodes.find((n) => n.id === d.layers[0].output).params.path.locked = false;
  const s = new CommandSession(d);
  const send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  return {
    s,
    d,
    send,
    add: () => send('add-svg-part', { layerId: d.layers[0].id, source: tree(), name: 'mark.svg' }),
  };
}
for (const blank of [false, true])
  test(`SVG part appends into ${blank ? 'empty' : 'existing'} Composition and is self-contained with one Undo`, () => {
    const f = fixture(blank),
      before = serialize(f.s.snapshot()),
      result = f.add(),
      after = f.s.snapshot();
    assert.equal(after.width, f.d.width);
    assert.equal(after.height, f.d.height);
    assert.equal(after.name, f.d.name);
    assert.equal(after.layers[0].id, f.d.layers[0].id);
    assert.equal(after.layers.length, f.d.layers.length + 1);
    assert.equal(after.layers.at(-1).visible, false);
    assert.equal(after.layers.at(-1).id, result.sourceLayerId);
    const ref = after.layers[0].nodes.find((n) => n.id === result.nodeId);
    assert.equal(ref.type, 'Composition Reference');
    assert.equal(ref.name, 'SVG · mark.svg');
    const view = evaluate(after.layers[0], {}, {}, after);
    assert.equal(view.error, null);
    const exported = exportSVG(after);
    assert.match(exported, /#ff0000/);
    assert.match(exported, /#0000ff/);
    assert.match(exported, /stroke-linecap="round"/);
    assert.match(exported, /fill-opacity="0.5"/);
    assert.equal(exportSVG(deserialize(serialize(after))), exported);
    assert.equal(f.s.historyView.past.length, 1);
    f.send('undo');
    assert.equal(serialize(f.s.snapshot()), before);
    f.send('redo');
    assert.deepEqual(f.s.snapshot(), after);
  });
test('Add SVG retains CSS-pixel user geometry and leaves the destination Composition size unchanged', () => {
  const f = fixture(true),
    before = f.s.snapshot(),
    result = f.send('add-svg-part', {
      layerId: before.layers[0].id,
      source: tree('<svg width="100mm" height="50mm"><rect width="96" height="48"/></svg>'),
      name: 'physical.svg',
    }),
    after = f.s.snapshot(),
    source = after.layers.find((layer) => layer.id === result.sourceLayerId),
    geometry = source.nodes.find((n) => n.type === 'Multi Path'),
    xs = geometry.points.map((point) => point.position.value[0]),
    ys = geometry.points.map((point) => point.position.value[1]);

  assert.equal(after.width, before.width);
  assert.equal(after.height, before.height);
  assert.equal(Math.max(...xs) - Math.min(...xs), 96);
  assert.equal(Math.max(...ys) - Math.min(...ys), 48);
});

test('Add SVG rejects unsupported viewport scaling atomically without replacing existing work', () => {
  const f = fixture(true);
  f.add();
  const before = serialize(f.s.snapshot()),
    state = f.s.state;

  assert.throws(
    () =>
      f.send('add-svg-part', {
        layerId: f.d.layers[0].id,
        source: tree(
          '<svg width="100mm" height="50mm" viewBox="0 0 200 150" preserveAspectRatio="xMidYMid meet"><rect width="200" height="150"/></svg>',
        ),
      }),
    /viewBox scaling is not supported/,
  );
  assert.equal(serialize(f.s.snapshot()), before);
  assert.deepEqual(f.s.state, state);
});

test('SVG instances duplicate, move and transform without changing embedded paths or unrelated layers', () => {
  const f = fixture();
  // Distinct nodes are minted by another document; add it before the Session.
  const d = f.d;
  d.layers.push({ ...document().layers[0], id: 'other-composition' });
  const s = new CommandSession(d),
    send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  const result = send('add-svg-part', {
    layerId: d.layers[0].id,
    source: tree(),
    name: 'mark.svg',
  });
  const embedded = serialize(s.snapshot()).match(/SVG source/);
  assert.ok(embedded);
  const sourceBefore = structuredClone(s.snapshot().layers.at(-1));
  const copied = send('duplicate-nodes', { layerId: result.layerId, nodeIds: [result.nodeId] });
  const ref = s
    .snapshot()
    .layers[0].nodes.filter((n) => n.type === 'Composition Reference')
    .at(-1);
  send('set-parameter-base', {
    layerId: result.layerId,
    nodeId: ref.id,
    key: 'position',
    value: [200, 50],
  });
  const tr = send('create-node', { layerId: result.layerId, nodeType: 'Transform' });
  send('connect', { layerId: result.layerId, sourceId: ref.id, nodeId: tr.nodeId, key: 'path' });
  send('set-parameter-base', {
    layerId: result.layerId,
    nodeId: tr.nodeId,
    key: 'rotation',
    value: 30,
  });
  const after = s.snapshot();
  assert.deepEqual(after.layers.at(-1), sourceBefore);
  assert.deepEqual(after.layers[1], d.layers[1]);
  assert.equal(after.layers[0].nodes.find((n) => n.id === tr.nodeId).paintMode, true);
  const resultPath = evaluate(after.layers[0], {}, {}, after).run(tr.nodeId);
  assert.equal(resultPath.kind, 'painted-group');
  assert.equal(resultPath.children[0].matrix[4], 200);
  assert.ok(copied);
});
test('two separately imported gradient parts retain their independent local paint servers', () => {
  const f = fixture(true),
    markup = `<svg viewBox="0 0 200 160"><defs><linearGradient id="ramp" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="100" y2="80" spreadMethod="pad" color-interpolation="sRGB"><stop offset="0" stop-color="#ff0000" stop-opacity="0"/><stop offset="1" stop-color="#123456" stop-opacity="0.8"/></linearGradient></defs><path d="M0 0H100V100H0Z M20 20V80H80V20Z" fill="url(#ramp)" fill-opacity="0.5" stroke="#abcdef" stroke-opacity="0.3" stroke-width="3"/></svg>`;
  for (let i = 0; i < 2; i++)
    f.send('add-svg-part', {
      layerId: f.d.layers[0].id,
      source: tree(markup),
      name: 'gradient.svg',
      at: [i * 100, 0],
    });
  const d = f.s.snapshot(),
    output = exportSVG(d);
  const ids = [...output.matchAll(/<linearGradient id="([^"]+)"/g)].map((m) => m[1]);
  assert.equal(ids.length, 2);
  assert.equal(new Set(ids).size, 2);
  assert.match(output, /stop-opacity="0.4"/);
  assert.match(output, /stroke-opacity="0.3"/);
  assert.equal(exportSVG(deserialize(serialize(d))), output);
});

test('unsupported SVG, locked targets, stale requests and budget failure preserve document and redo atomically', () => {
  const f = fixture(true);
  f.add();
  f.send('undo');
  const before = serialize(f.s.snapshot()),
    state = f.s.state;
  for (const markup of [
    '<svg><script>alert(1)</script></svg>',
    '<svg><image href="https://example.invalid/image.png"/></svg>',
    '<svg><path d="M0 0H20V20Z" onclick="x()"/></svg>',
    '<svg><path d="M0 0H20V20Z" filter="url(#x)"/></svg>',
    '<svg><path d="M0 0H20V20Z" fill-rule="evenodd"/></svg>',
    '<svg><use href="#x"/></svg>',
    '<svg><text>x</text></svg>',
  ]) {
    assert.throws(() =>
      f.send('add-svg-part', { layerId: f.d.layers[0].id, source: tree(markup) }),
    );
    assert.equal(serialize(f.s.snapshot()), before);
    assert.deepEqual(f.s.state, state);
  }
  assert.throws(
    () =>
      f.s.dispatch({
        ...state,
        expectedRevision: state.expectedRevision - 1,
        type: 'add-svg-part',
        payload: { layerId: f.d.layers[0].id, source: tree() },
      }),
    /STALE_REVISION/,
  );
  const d = f.s.snapshot();
  d.layers[0].locked = true;
  const locked = new CommandSession(d);
  assert.throws(
    () =>
      locked.dispatch({
        ...locked.state,
        type: 'add-svg-part',
        payload: { layerId: d.layers[0].id, source: tree() },
      }),
    /unlocked/,
  );
  const excessive = '<svg>' + '<rect width="1" height="1"/>'.repeat(2049) + '</svg>';
  assert.throws(() =>
    f.send('add-svg-part', { layerId: f.d.layers[0].id, source: tree(excessive) }),
  );
  assert.equal(serialize(f.s.snapshot()), before);
  f.send('redo');
  assert.match(exportSVG(f.s.snapshot()), /#ff0000/);
});
