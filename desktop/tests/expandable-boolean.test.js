import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  point,
  connect,
  evaluate,
  pathArea,
  validate,
  serialize,
  deserialize,
  parameter,
  parameters,
  booleanPendingInputKey,
  CommandSession,
} from '../src/core.js';
import { queryShapeSources } from '../src/source-navigation.js';

function scene() {
  const doc = document(),
    layer = doc.layers[0],
    sources = Array.from({ length: 3 }, () => node('Rectangle')),
    boolean = node('Boolean'),
    output = node('Output');
  sources[0].params.center.value = [50, 50];
  sources[1].params.center.value = [100, 50];
  sources[2].params.center.value = [50, 50];
  sources[0].params.size.value = [100, 100];
  sources[1].params.size.value = [100, 100];
  sources[2].params.size.value = [50, 100];
  layer.nodes = [...sources, boolean, output];
  layer.output = output.id;
  output.params.path.source = boolean.id;
  return { doc, layer, sources, boolean, output };
}
function sessionFixture() {
  const f = scene(),
    session = new CommandSession(f.doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: f.layer.id, ...payload },
      }),
    live = () => session.snapshot().layers[0].nodes.find((n) => n.id === f.boolean.id);
  return { ...f, session, dispatch, live };
}
const snapshot = (session) => serialize(session.snapshot());

function wireAB(layer, boolean, sources) {
  connect(layer, sources[0].id, boolean.id, 'a');
  connect(layer, sources[1].id, boolean.id, 'b');
}

test('legacy A/B Boolean JSON is preserved until an additional socket is used', () => {
  const { doc, layer, boolean, sources } = scene(),
    original = serialize(doc);
  assert.equal(doc.schemaVersion, 24);
  wireAB(layer, boolean, sources);
  const bytes = serialize(doc),
    loaded = deserialize(bytes);
  assert.equal(loaded.schemaVersion, 24);
  assert.equal(serialize(loaded), bytes);
  const persisted = loaded.layers[0].nodes.find((n) => n.id === boolean.id);
  assert.equal(persisted.booleanInputOrder, undefined);
  assert.deepEqual(Object.keys(persisted.params), ['a', 'b']);
  assert.equal(original.includes('booleanInputOrder'), false);
});

test('all Boolean modes fold three ordered inputs with legacy subtraction semantics', () => {
  const expected = { union: 15000, subtract: 2500, intersect: 2500, difference: 10000 };
  for (const [operation, area] of Object.entries(expected)) {
    const { layer, boolean, sources } = scene();
    boolean.operation = operation;
    wireAB(layer, boolean, sources);
    connect(layer, sources[2].id, boolean.id, booleanPendingInputKey);
    const result = evaluate(layer);
    assert.equal(result.error, null, operation);
    assert.ok(Math.abs(pathArea(result.path) - area) < 0.01, operation);
    assert.equal(boolean.params[booleanPendingInputKey].source, sources[2].id);
    assert.equal(boolean.booleanInputOrder.at(-1), parameters(boolean).at(-1)[0]);
  }
});

test('shared connect command materializes C, bumps schema only then, and preserves Undo/Redo keys', () => {
  const { session, dispatch, live, sources, boolean } = sessionFixture();
  dispatch('connect', { sourceId: sources[0].id, nodeId: boolean.id, key: 'a' });
  dispatch('connect', { sourceId: sources[1].id, nodeId: boolean.id, key: 'b' });
  assert.equal(session.snapshot().schemaVersion, 24);
  assert.equal(live().booleanInputOrder, undefined);
  assert.equal(parameter(live(), booleanPendingInputKey).type, 'Path');
  const legacyBytes = snapshot(session);
  assert.equal(serialize(deserialize(legacyBytes)), legacyBytes);

  dispatch('connect', {
    sourceId: sources[2].id,
    nodeId: boolean.id,
    key: booleanPendingInputKey,
  });
  const after = snapshot(session),
    order = [...live().booleanInputOrder];
  assert.equal(session.snapshot().schemaVersion, 40);
  assert.deepEqual(order.slice(0, 3), ['a', 'b', booleanPendingInputKey]);
  assert.equal(order.length, 4);
  assert.equal(live().params[order[3]].source, undefined);
  assert.equal(serialize(deserialize(after)), after);

  dispatch('undo');
  assert.equal(session.snapshot().schemaVersion, 24);
  assert.equal(live().booleanInputOrder, undefined);
  assert.equal(snapshot(session), legacyBytes);
  dispatch('redo');
  assert.equal(snapshot(session), after);
  assert.deepEqual(live().booleanInputOrder, order);
});

test('locking the virtual next socket persists a stable ordered slot and Undo/Redo preserves it', () => {
  const { session, dispatch, live, sources, boolean } = sessionFixture();
  dispatch('connect', { sourceId: sources[0].id, nodeId: boolean.id, key: 'a' });
  dispatch('connect', { sourceId: sources[1].id, nodeId: boolean.id, key: 'b' });
  dispatch('set-parameter-lock', {
    nodeId: boolean.id,
    key: booleanPendingInputKey,
    locked: true,
  });
  const after = snapshot(session),
    order = [...live().booleanInputOrder];
  assert.equal(session.snapshot().schemaVersion, 40);
  assert.equal(live().params[booleanPendingInputKey].locked, true);
  assert.equal(order.length, 4);
  dispatch('undo');
  assert.equal(live().booleanInputOrder, undefined);
  assert.equal(session.snapshot().schemaVersion, 24);
  dispatch('redo');
  assert.equal(snapshot(session), after);
  assert.deepEqual(live().booleanInputOrder, order);
});

test('open contour connection fails without changing Boolean sockets, project, or history', () => {
  const f = scene(),
    open = node('Curve');
  open.params.closed.value = false;
  open.points = [point(0, 0), point(100, 0), point(100, 100)];
  f.layer.nodes.push(open);
  const session = new CommandSession(f.doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: f.layer.id, ...payload },
      }),
    live = () => session.snapshot().layers[0].nodes.find((n) => n.id === f.boolean.id);
  dispatch('connect', { sourceId: f.sources[0].id, nodeId: f.boolean.id, key: 'a' });
  dispatch('connect', { sourceId: f.sources[1].id, nodeId: f.boolean.id, key: 'b' });
  const before = snapshot(session),
    state = session.state,
    history = session.historyView.past.length;
  assert.throws(
    () =>
      dispatch('connect', {
        sourceId: open.id,
        nodeId: f.boolean.id,
        key: booleanPendingInputKey,
      }),
    /Boolean requires closed contours/,
  );
  assert.equal(snapshot(session), before);
  assert.deepEqual(session.state, state);
  assert.equal(session.historyView.past.length, history);
  assert.equal(live().booleanInputOrder, undefined);
});

test('schema 40 validation rejects a missing final spare without allocating a socket ID', () => {
  const { doc, layer, boolean, sources } = scene();
  wireAB(layer, boolean, sources);
  connect(layer, sources[2].id, boolean.id, booleanPendingInputKey);
  const spare = boolean.booleanInputOrder.at(-1);
  delete boolean.params[spare];
  boolean.booleanInputOrder.pop();
  assert.equal(doc.schemaVersion, 24);
  doc.schemaVersion = 40;
  const expanded = JSON.stringify(doc);
  assert.throws(() => validate(doc), /retain an empty spare socket/);
  assert.equal(JSON.stringify(doc), expanded);
});

test('source navigation follows extra Boolean inputs and ignores the final empty spare', () => {
  const doc = document(),
    layer = doc.layers[0],
    texts = Array.from({ length: 3 }, (_, i) => {
      const t = node('Text');
      t.id = `text-${i}`;
      t.text = `T${i}`;
      return t;
    }),
    boolean = node('Boolean');
  layer.nodes = [...texts, boolean, node('Output')];
  layer.nodes.at(-1).params.path.source = boolean.id;
  boolean.params.a.source = texts[0].id;
  boolean.params.b.source = texts[1].id;
  boolean.booleanInputOrder = ['a', 'b', booleanPendingInputKey, 'input_spare'];
  boolean.params[booleanPendingInputKey] = { type: 'Path', value: null, source: texts[2].id };
  boolean.params.input_spare = { type: 'Path', value: null };
  doc.schemaVersion = 40;
  const result = queryShapeSources(doc, { layerId: layer.id, nodeId: boolean.id }, 7);
  assert.equal(result.complete, true);
  assert.deepEqual(result.candidates.map((candidate) => candidate.nodeId).sort(), [
    'text-0',
    'text-1',
    'text-2',
  ]);
});
