import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, blankDocument, evaluate, pathBounds, serialize } from '../src/core.js';
import { appendArtwork } from '../src/artwork-creation.js';

function fixture() {
  const doc = blankDocument();
  const ids = [100, 280, 650].map((x, i) => {
    const created = appendArtwork(doc, 'Rectangle', { at: [x, [100, 230, 410][i]] });
    doc.layers.at(-1).nodes[0].params.size.value = [40 + i * 20, 30 + i * 10];
    return created.layerId;
  });
  doc.layers.shift();
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const bounds = () =>
    ids.map((id) =>
      pathBounds(evaluate(session.snapshot().layers.find((layer) => layer.id === id)).path),
    );
  return { session, send, ids, bounds };
}
const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-6, `${a} != ${b}`);

test('single artwork aligns to artboard with exact Undo and no-op; unsupported references preserve state', () => {
  const { session: s, send, ids, bounds } = fixture(),
    before = serialize(s.snapshot());
  send('align-artworks', { layerIds: [ids[0]], action: 'right', reference: 'artboard' });
  near(bounds()[0][0][2], s.snapshot().width);
  const after = serialize(s.snapshot()),
    revision = s.state.expectedRevision;
  send('align-artworks', { layerIds: [ids[0]], action: 'right', reference: 'artboard' });
  assert.equal(s.state.expectedRevision, revision);
  assert.throws(() =>
    send('align-artworks', { layerIds: ids, action: 'left', reference: 'foreign' }),
  );
  assert.equal(serialize(s.snapshot()), after);
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('redo');
  assert.equal(serialize(s.snapshot()), after);
});

test('edge distribution fixes the endpoint edges, shares one Undo and is independent of click order', () => {
  for (const [action, axis, edge] of [
    ['left', 0, 0],
    ['right', 0, 2],
    ['top', 1, 0],
    ['bottom', 1, 2],
  ]) {
    const { session: s, send, ids, bounds } = fixture(),
      before = serialize(s.snapshot()),
      bb = bounds();
    send('distribute-artworks', { layerIds: [...ids].reverse(), action });
    const next = bounds();
    near(next[1][axis][edge], (bb[0][axis][edge] + bb[2][axis][edge]) / 2);
    near(next[0][axis][edge], bb[0][axis][edge]);
    near(next[2][axis][edge], bb[2][axis][edge]);
    assert.equal(s.historyView.past.length, 1);
    send('undo');
    assert.equal(serialize(s.snapshot()), before);
  }
});

test('explicit spacing handles two or more artworks, fixes the leading edge, is atomic and idempotent', () => {
  const { session: s, send, ids, bounds } = fixture(),
    before = serialize(s.snapshot()),
    bb = bounds();
  send('distribute-artwork-gaps', { layerIds: [...ids].reverse(), action: 'gap-x', spacing: 12 });
  const next = bounds();
  near(next[0][0][0], bb[0][0][0]);
  near(next[1][0][0] - next[0][0][2], 12);
  near(next[2][0][0] - next[1][0][2], 12);
  const after = serialize(s.snapshot()),
    revision = s.state.expectedRevision;
  send('distribute-artwork-gaps', { layerIds: ids, action: 'gap-x', spacing: 12 });
  assert.equal(s.state.expectedRevision, revision);
  for (const spacing of [NaN, Infinity, 100001, -100000]) {
    assert.throws(() =>
      send('distribute-artwork-gaps', { layerIds: ids, action: 'gap-x', spacing }),
    );
    assert.equal(serialize(s.snapshot()), after);
  }
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('distribute-artwork-gaps', { layerIds: ids.slice(0, 2), action: 'gap-y', spacing: 0 });
  const two = bounds();
  near(two[1][1][0] - two[0][1][2], 0);
});
