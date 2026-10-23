import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  disconnect,
  evaluate,
  validate,
  serialize,
  deserialize,
  parameters,
  CommandSession,
  JOIN_SLOT_LIMIT,
  duplicateNodes,
} from '../src/core.js';

function scene(count = 4) {
  const doc = document(),
    layer = doc.layers[0],
    join = node('Join Geometry'),
    out = node('Output');
  const sources = Array.from({ length: count }, () => node('Rectangle'));
  layer.nodes = [...sources, join, out];
  layer.output = out.id;
  out.params.path.source = join.id;
  return { doc, layer, join, sources };
}
function sessionFixture(count = 4) {
  const f = scene(count),
    session = new CommandSession(f.doc);
  return {
    ...f,
    session,
    dispatch: (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: f.layer.id, ...payload } }),
    live: () => session.snapshot().layers[0].nodes.find((n) => n.id === f.join.id),
  };
}
const snap = (s) => serialize(s.snapshot());

test('default, empty and one-input Join are valid editable states; ordered slots append and compact without retargeting', () => {
  const { doc, layer, join, sources } = scene();
  assert.deepEqual(join.joinInputOrder, ['a', 'b']);
  assert.match(evaluate(layer).error, /Connect a path to Join Geometry/);
  assert.doesNotThrow(() => validate(doc));
  connect(layer, sources[0].id, join.id, 'a');
  assert.equal(evaluate(layer).error, null);
  assert.equal(evaluate(layer).path.contours.length, 1);
  connect(layer, sources[1].id, join.id, 'b');
  const third = join.joinInputOrder.at(-1);
  connect(layer, sources[2].id, join.id, third);
  const fourth = join.joinInputOrder.at(-1);
  connect(layer, sources[3].id, join.id, fourth);
  const spare = join.joinInputOrder.at(-1);
  disconnect(join, third);
  assert.deepEqual(join.joinInputOrder, ['a', 'b', fourth, spare]);
  assert.equal(join.params[fourth].source, sources[3].id);
  assert.equal(join.params[third], undefined);
  disconnect(join, 'a');
  assert.equal(join.params.a.source, undefined);
  connect(layer, sources[0].id, join.id, 'a');
  assert.deepEqual(
    parameters(join).map(([key]) => key),
    join.joinInputOrder,
  );
  assert.equal(evaluate(layer).path.contours.length, 3);
  assert.equal(serialize(deserialize(serialize(doc))), serialize(doc));
});

test('100 direct sources and the 256 group boundary work; 257th connection rolls back all state and history', () => {
  const { session, dispatch, live, sources, join } = sessionFixture(1);
  for (let i = 0; i < 256; i++) {
    const key = i === 0 ? 'a' : live().joinInputOrder.at(-1);
    dispatch('connect', { sourceId: sources[0].id, nodeId: join.id, key });
    if (i === 99) assert.equal(evaluate(session.snapshot().layers[0]).path.contours.length, 100);
  }
  const before = snap(session),
    state = session.state,
    history = session.historyView.past.length;
  assert.equal(evaluate(session.snapshot().layers[0]).path.contours.length, 256);
  assert.throws(
    () =>
      dispatch('connect', {
        sourceId: sources[0].id,
        nodeId: join.id,
        key: live().joinInputOrder.at(-1),
      }),
    /256 independent groups/,
  );
  assert.equal(snap(session), before);
  assert.deepEqual(session.state, state);
  assert.equal(session.historyView.past.length, history);
});

test('spares, lock holes, disconnect, source deletion and duplicate node retain keys through Undo/Redo', () => {
  const { session, dispatch, live, sources, join } = sessionFixture();
  dispatch('connect', { sourceId: sources[0].id, nodeId: join.id, key: 'b' });
  const third = live().joinInputOrder.at(-1);
  dispatch('set-parameter-lock', { nodeId: join.id, key: third, locked: true });
  const fourth = live().joinInputOrder.at(-1);
  const before = snap(session);
  dispatch('connect', { sourceId: sources[1].id, nodeId: join.id, key: fourth });
  const after = snap(session);
  dispatch('undo');
  assert.equal(snap(session), before);
  dispatch('redo');
  assert.equal(snap(session), after);
  assert.throws(
    () => dispatch('connect', { sourceId: sources[2].id, nodeId: join.id, key: third }),
    /locked/,
  );
  assert.equal(snap(session), after);
  dispatch('set-parameter-lock', { nodeId: join.id, key: third, locked: false });
  assert.equal(live().params[third], undefined);
  assert.equal(live().params[fourth].source, sources[1].id);
  dispatch('delete-node', { nodeId: sources[1].id });
  assert.equal(live().params[fourth], undefined);
  const doc = session.snapshot(),
    layer = doc.layers[0],
    original = layer.nodes.find((n) => n.id === join.id);
  const copies = duplicateNodes(layer, [sources[0].id, join.id]);
  const copy = copies.find((n) => n.type === 'Join Geometry');
  assert.deepEqual(copy.joinInputOrder, original.joinInputOrder);
  assert.equal(copy.params.b.source, copies.find((n) => n.type === 'Rectangle').id);
  validate(doc);
});

test('legacy a/b migration is deterministic and geometry/IDs/fingerprints are unchanged', () => {
  const { doc, layer, join, sources } = scene();
  join.params.a.source = sources[0].id;
  join.params.b.source = sources[1].id;
  delete join.joinInputOrder;
  doc.schemaVersion = 2;
  const before = evaluate(layer).path,
    original = serialize(doc);
  const migrated = deserialize(original),
    again = deserialize(original);
  assert.equal(migrated.schemaVersion, 23);
  assert.equal(serialize(migrated), serialize(again));
  assert.deepEqual(evaluate(migrated.layers[0]).path, before);
  const bytes = serialize(migrated);
  validate(migrated);
  assert.equal(serialize(migrated), bytes);
  assert.equal(serialize(deserialize(bytes)), bytes);
});

test('malformed and older-schema expandable inputs fail closed', () => {
  for (const mutate of [
    (d, j) => {
      j.joinInputOrder = ['a', 'a'];
    },
    (d, j) => {
      j.joinInputOrder = ['b', 'a'];
    },
    (d, j) => {
      delete j.joinInputOrder;
    },
    (d, j) => {
      j.joinInputOrder.push('input_missing');
    },
    (d, j) => {
      j.params.a.extra = true;
    },
    (d, j) => {
      j.params.a.type = 'Scalar';
    },
    (d, j) => {
      d.schemaVersion = 22;
    },
    (d, j) => {
      j.joinInputOrder = Array(JOIN_SLOT_LIMIT + 1).fill('a');
    },
  ]) {
    const { doc, join } = scene();
    mutate(doc, join);
    assert.throws(() => deserialize(serialize(doc)));
  }
});

test('later-socket cycle, wrong types and composition lock reject without spare allocation', () => {
  const { doc, layer, join, sources } = scene();
  connect(layer, sources[0].id, join.id, 'b');
  const spare = join.joinInputOrder.at(-1),
    transform = node('Transform'),
    color = node('Color');
  layer.nodes.push(transform, color);
  connect(layer, join.id, transform.id, 'path');
  const before = serialize(doc);
  assert.throws(() => connect(layer, transform.id, join.id, spare), /[Cc]ycl/);
  assert.equal(serialize(doc), before);
  assert.throws(() => connect(layer, color.id, join.id, spare), /Cannot connect/);
  assert.equal(serialize(doc), before);
  layer.locked = true;
  const session = new CommandSession(doc),
    snapshot = snap(session);
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'connect',
        payload: { layerId: layer.id, nodeId: join.id, sourceId: sources[1].id, key: spare },
      }),
    /locked/,
  );
  assert.equal(snap(session), snapshot);
});

test('100 distinct procedural sources connect directly in explicit order', () => {
  const { doc, layer, join, sources } = scene(100);
  for (let i = 0; i < sources.length; i++) {
    sources[i].params.center.value = [i, 0];
    connect(layer, sources[i].id, join.id, i === 0 ? 'a' : join.joinInputOrder.at(-1));
  }
  validate(doc);
  const result = evaluate(layer);
  assert.equal(result.error, null);
  assert.equal(result.path.contours.length, 100);
  assert.equal(join.joinInputOrder.filter((k) => join.params[k].source).length, 100);
});

test('upstream changes that newly overflow Join roll back transactionally', () => {
  const { doc, layer, join, sources } = scene(1);
  const repeater = node('Geometry Repeater'),
    grouped = node('Join Geometry');
  layer.nodes.push(repeater, grouped);
  repeater.params.count.value = 128;
  connect(layer, sources[0].id, grouped.id, 'a');
  connect(layer, grouped.id, repeater.id, 'path');
  connect(layer, repeater.id, join.id, 'a');
  connect(layer, repeater.id, join.id, 'b');
  const session = new CommandSession(doc),
    before = snap(session),
    state = session.state;
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: repeater.id, key: 'count', value: 129 },
      }),
    /256 independent groups/,
  );
  assert.equal(snap(session), before);
  assert.deepEqual(session.state, state);
});

test('contour and point budgets fail explicitly at their existing independent boundaries', () => {
  const f = scene(1),
    source = f.sources[0];
  source.type = 'Imported Path';
  source.params = {};
  source.rawPath = 'M0 0L1 0L1 1Z '.repeat(2500);
  for (let i = 0; i < 4; i++)
    connect(f.layer, source.id, f.join.id, i === 0 ? 'a' : f.join.joinInputOrder.at(-1));
  assert.equal(evaluate(f.layer).path.contours.length, 10000);
  connect(f.layer, source.id, f.join.id, f.join.joinInputOrder.at(-1));
  assert.match(evaluate(f.layer).error, /geometry budget/);
  const g = scene(1),
    path = g.sources[0];
  path.type = 'Imported Path';
  path.params = {};
  path.rawPath =
    'M0 0' +
    Array.from({ length: 9999 }, (_, i) => `L${i % 100} ${Math.floor(i / 100)}`).join('') +
    'Z';
  for (let i = 0; i < 10; i++)
    connect(g.layer, path.id, g.join.id, i === 0 ? 'a' : g.join.joinInputOrder.at(-1));
  assert.equal(
    evaluate(g.layer).path.contours.reduce((s, c) => s + c.points.length, 0),
    100000,
  );
  connect(g.layer, path.id, g.join.id, g.join.joinInputOrder.at(-1));
  assert.match(evaluate(g.layer).error, /geometry budget/);
});

test('structural slot cap retains a valid final connection without allocating another spare', () => {
  const { doc, layer, join, sources } = scene(1);
  for (let i = 2; i < JOIN_SLOT_LIMIT; i++) {
    const key = `input_cap_${i}`;
    join.joinInputOrder.push(key);
    join.params[key] = {
      type: 'Path',
      value: null,
      expression: '',
      locked: i < JOIN_SLOT_LIMIT - 1,
    };
  }
  validate(doc);
  connect(layer, sources[0].id, join.id, join.joinInputOrder.at(-1));
  assert.equal(join.joinInputOrder.length, JOIN_SLOT_LIMIT);
  assert.equal(evaluate(layer).error, null);
  assert.equal(serialize(deserialize(serialize(doc))), serialize(doc));
});

test('bypass remains strictly a even when an additional input has geometry', () => {
  const { layer, join, sources } = scene(1);
  connect(layer, sources[0].id, join.id, 'b');
  const extra = join.joinInputOrder.at(-1);
  connect(layer, sources[0].id, join.id, extra);
  join.bypassed = true;
  assert.match(evaluate(layer).error, /[Cc]onnect|missing|[Ii]nput/);
  connect(layer, sources[0].id, join.id, 'a');
  assert.deepEqual(evaluate(layer).run(join.id), evaluate(layer).run(sources[0].id));
});
