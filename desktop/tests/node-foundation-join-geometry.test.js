import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  parameters,
  serialize,
  CommandSession,
  connect,
  evaluate,
  setNodeBypassed,
  deserialize,
  JOIN_SLOT_LIMIT,
} from '../src/core.js';
import { JOIN_GEOMETRY } from '../src/builtin-node-registry.js';
import { joinGeometryDeclaration } from '../src/join-geometry-declaration.js';
import { evaluateJoinGeometry } from '../src/join-geometry-binding.js';
import { joinSlotsProtocol } from '../src/join-slots-protocol.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { createNodeRegistry } from '../src/node-foundation.js';

test('dynamic slot snapshots are pure, bounded at 1024 and reject malformed shapes', () => {
  const join = node('Join Geometry');
  for (let i = 0; i < JOIN_SLOT_LIMIT - 2; i++) {
    const key = `input_${i}`;
    join.joinInputOrder.push(key);
    join.params[key] = { type: 'Path', value: null, expression: '', locked: false };
  }
  const before = JSON.stringify(join),
    snapshot = JOIN_GEOMETRY.snapshot(join),
    view = JOIN_GEOMETRY.interface(join.operation, snapshot);
  assert.equal(snapshot.slots.length, 1024);
  assert.equal(view.active.length, 1024);
  assert.equal(view.resultType, 'Path');
  assert.equal(JSON.stringify(join), before);
  assert.ok(Object.isFrozen(snapshot));
  assert.ok(Object.isFrozen(snapshot.slots));
  assert.ok(Object.isFrozen(snapshot.slots[0]));
  assert.ok(Object.isFrozen(view));
  assert.deepEqual(Object.keys(snapshot.slots[0]).sort(), ['connected', 'id', 'locked', 'type']);
  assert.equal(parameters(join).length, 1024);
  assert.equal(JSON.stringify(join), before);
  join.joinInputOrder.push('input_overflow');
  join.params.input_overflow = { type: 'Path', value: null, expression: '', locked: false };
  assert.throws(() => JOIN_GEOMETRY.snapshot(join), /Invalid slot/);
  const n = node('Join Geometry');
  n.joinInputOrder[1] = 'a';
  assert.throws(() => JOIN_GEOMETRY.snapshot(n), /Invalid slot/);
  const missing = node('Join Geometry');
  delete missing.params.b;
  assert.throws(() => JOIN_GEOMETRY.snapshot(missing), /Invalid slot/);
  const badMode = Object.freeze({ nodeId: 'node', mode: 'unknown', slots: snapshot.slots });
  assert.throws(() => JOIN_GEOMETRY.interface('subtract', badMode), /Invalid slot/);
  const wrong = Object.freeze({ nodeId: 'node', mode: 'painted', slots: snapshot.slots });
  assert.throws(() => JOIN_GEOMETRY.interface('subtract', wrong), /Invalid slot/);
});

test('slot protocol and shape resolver are trusted, validated and captured', () => {
  const binding = new Map([['builtin.join-geometry.v1', { evaluate: evaluateJoinGeometry }]]);
  const protocol = { ...joinSlotsProtocol },
    host = {
      ...nectNodeHost,
      types: new Map(nectNodeHost.types),
      shapeProtocols: new Map([['nect.join-slots', protocol]]),
    };
  const registry = createNodeRegistry([structuredClone(joinGeometryDeclaration)], binding, host),
    resolved = registry.get('Join Geometry'),
    n = node('Join Geometry'),
    shape = resolved.snapshot(n);
  protocol.resolve = () => {
    throw Error('changed');
  };
  host.shapeProtocols.clear();
  host.types.clear();
  assert.equal(resolved.interface('subtract', shape).resultType, 'Path');
  const bad = structuredClone(joinGeometryDeclaration);
  bad.shape.protocol = 'unknown';
  assert.throws(() => createNodeRegistry([bad], binding, nectNodeHost), /protocol/);
  const version = structuredClone(joinGeometryDeclaration);
  version.shape.revision = 2;
  assert.throws(() => createNodeRegistry([version], binding, nectNodeHost), /protocol/);
  const badType = structuredClone(joinGeometryDeclaration);
  badType.parameters[0].type = 'Unknown';
  assert.throws(() => createNodeRegistry([badType], binding, nectNodeHost), /Unsupported/);
  const missingBinding = structuredClone(joinGeometryDeclaration);
  missingBinding.implementationId = 'unknown';
  assert.throws(() => createNodeRegistry([missingBinding], binding, nectNodeHost), /Unavailable/);
});

test('slot mutations remain Session-owned and failed connect retains redo', () => {
  const doc = document(),
    layer = doc.layers[0],
    rect = node('Rectangle'),
    join = node('Join Geometry');
  layer.nodes.push(rect, join);
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  send('set-parameter-lock', { nodeId: join.id, key: 'b', locked: true });
  send('undo');
  const before = serialize(session.snapshot()),
    history = JSON.stringify(session.historyView),
    revision = session.state.expectedRevision;
  assert.throws(
    () => send('connect', { sourceId: 'missing', nodeId: join.id, key: 'a' }),
    /Missing/,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(JSON.stringify(session.historyView), history);
  assert.equal(session.state.expectedRevision, revision);
  send('redo');
  const locked = session.snapshot().layers[0].nodes.find((n) => n.id === join.id);
  assert.equal(locked.params.b.locked, true);
  assert.equal(locked.joinInputOrder.length, 3);
});

test('connected empty and geometry limits keep the existing operation boundary', () => {
  const slot = Object.freeze({ id: 'a', type: 'Path', connected: true, locked: false });
  const context = Object.freeze({
    nodeId: 'join-fixed',
    shape: Object.freeze({ nodeId: 'join-fixed', mode: 'path', slots: Object.freeze([slot]) }),
  });
  const result = evaluateJoinGeometry('subtract', () => ({ contours: [] }), context);
  assert.deepEqual(result.contours, []);
  assert.equal(result.unstable, true);
  const contour = { id: 'c', closed: false, points: [] };
  assert.throws(
    () =>
      evaluateJoinGeometry('subtract', () => ({ contours: Array(10001).fill(contour) }), context),
    /geometry budget/,
  );
  assert.throws(
    () =>
      evaluateJoinGeometry(
        'subtract',
        () => ({ contours: [{ ...contour, points: Array(100001).fill({}) }] }),
        context,
      ),
    /geometry budget/,
  );
  const grouped = (count) => ({
    contours: Array.from({ length: count }, (_, i) => ({
      id: `c${i}`,
      closed: false,
      group: `g${i}`,
      points: [],
    })),
  });
  assert.equal(evaluateJoinGeometry('subtract', () => grouped(256), context).contours.length, 256);
  assert.throws(
    () => evaluateJoinGeometry('subtract', () => grouped(257), context),
    /256 independent groups/,
  );
});

test('legacy a/b codec migration preserves source keys and deterministic spare', () => {
  const doc = document(),
    layer = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    join = node('Join Geometry');
  layer.nodes.push(a, b, join);
  connect(layer, a.id, join.id, 'a');
  connect(layer, b.id, join.id, 'b');
  const spare = join.joinInputOrder.at(-1);
  delete join.params[spare];
  delete join.joinInputOrder;
  doc.schemaVersion = 22;
  const loaded = deserialize(serialize(doc)),
    restored = loaded.layers[0].nodes.find((n) => n.id === join.id);
  assert.deepEqual(restored.joinInputOrder, ['a', 'b', 'input_legacy_spare']);
  assert.equal(restored.params.a.source, a.id);
  assert.equal(restored.params.b.source, b.id);
});

test('Session preserves stable extra-key edges through lock, compact, undo and redo', () => {
  const doc = document(),
    layer = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    c = node('Rectangle'),
    join = node('Join Geometry');
  layer.nodes.push(a, b, c, join);
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    live = () => session.snapshot().layers[0].nodes.find((n) => n.id === join.id);
  send('connect', { sourceId: a.id, nodeId: join.id, key: 'a' });
  send('connect', { sourceId: b.id, nodeId: join.id, key: 'b' });
  const extra = live().joinInputOrder.at(-1);
  send('connect', { sourceId: c.id, nodeId: join.id, key: extra });
  send('set-parameter-lock', { nodeId: join.id, key: extra, locked: true });
  const locked = serialize(session.snapshot()),
    revision = session.state.expectedRevision;
  assert.throws(() => send('disconnect', { nodeId: join.id, key: extra }), /locked/);
  assert.equal(serialize(session.snapshot()), locked);
  assert.equal(session.state.expectedRevision, revision);
  send('set-parameter-lock', { nodeId: join.id, key: extra, locked: false });
  send('disconnect', { nodeId: join.id, key: extra });
  assert.equal(live().params[extra], undefined);
  assert.equal(live().params.a.source, a.id);
  assert.equal(live().params.b.source, b.id);
  send('undo');
  assert.equal(live().params[extra].source, c.id);
  send('redo');
  assert.equal(live().params[extra], undefined);
  assert.equal(live().params.b.source, b.id);
});

test('painted Join bypass preserves input result without constructing new group', () => {
  const doc = document(),
    layer = doc.layers[0],
    rect = node('Rectangle'),
    fill = node('Fill'),
    join = node('Join Geometry');
  layer.nodes.push(rect, fill, join);
  connect(layer, rect.id, fill.id, 'path');
  connect(layer, fill.id, join.id, 'a');
  assert.equal(join.paintMode, true);
  const source = evaluate(layer).run(fill.id);
  setNodeBypassed(layer, join.id, true);
  assert.deepEqual(evaluate(layer).run(join.id), source);
});

test('production Join distinguishes no connection from connected zero-point Curve', () => {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    join = node('Join Geometry');
  curve.points = [];
  layer.nodes.push(curve, join);
  assert.throws(() => evaluate(layer).run(join.id), /Connect a path to Join Geometry/);
  connect(layer, curve.id, join.id, 'a');
  const result = evaluate(layer).run(join.id);
  assert.equal(result.contours.length, 1);
  assert.equal(result.contours[0].points.length, 0);
  assert.equal(result.unstable, true);
});

test('synthetic Join binding preserves source-key character routes and cloned grouped metadata', () => {
  const slots = Object.freeze([
    Object.freeze({ id: 'a', type: 'Path', connected: true, locked: false }),
    Object.freeze({ id: 'b', type: 'Path', connected: true, locked: false }),
  ]);
  const context = Object.freeze({
    nodeId: 'join-fixed',
    shape: Object.freeze({ nodeId: 'join-fixed', mode: 'path', slots }),
  });
  const matrix = [1, 0, 0, 1, 0, 0],
    meta = { tag: 'fixture' },
    point = { id: 'p', position: [1, 2], handleA: [0, 0], handleB: [0, 0], meta };
  const a = {
    contours: [{ id: 'c1', closed: false, group: 'g', meta, points: [point] }],
    characterOccurrences: [{ route: 'text-a', matrix, reason: 'prior' }],
  };
  const b = {
    contours: [{ id: 'c2', closed: false, group: 'g', points: [] }],
    characterOccurrences: [{ route: 'text-b', matrix, reason: null }],
  };
  const before = JSON.stringify([a, b]),
    out = evaluateJoinGeometry('subtract', (id) => (id === 'a' ? a : b), context);
  assert.equal(JSON.stringify([a, b]), before);
  assert.equal(out.contours.length, 2);
  assert.notEqual(out.contours[0].meta, meta);
  assert.deepEqual(out.contours[0].meta, meta);
  assert.match(out.contours[0].id, /:g0:c0$/);
  assert.match(out.contours[1].id, /:g1:c0$/);
  assert.match(out.contours[0].points[0].id, /:g0:c0:p0$/);
  assert.deepEqual(
    out.characterOccurrences.map((x) => x.route),
    ['text-a/join-fixed:a', 'text-b/join-fixed:b'],
  );
  assert.equal(out.characterOccurrences[0].reason, 'prior');
  assert.equal(out.characterOccurrences[1].reason, null);
});
