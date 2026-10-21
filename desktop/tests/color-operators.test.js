import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
  parameterReferenceExpression as ref,
  previewColorExpression,
  duplicateNodes,
} from '../src/core.js';
import { colorOperation, publicColor } from '../src/color-operations.js';

function fixture(type = 'Color', key = 'value') {
  const doc = document(),
    layer = doc.layers[0],
    target = node(type),
    color = node('Color'),
    scalar = node('Value');
  target.params[key].value = { hex: '#ff0000', alpha: 0 };
  color.params.value.value = { hex: '#0000ff', alpha: 1 };
  scalar.params.value.value = 0.5;
  layer.nodes.push(target, color, scalar);
  const session = new CommandSession(doc);
  const send = (type, payload = {}, envelope = session.state) =>
    session.dispatch({
      ...envelope,
      type,
      payload: { layerId: layer.id, nodeId: target.id, key, ...payload },
    });
  const live = () => session.snapshot().layers[0];
  const q = () => live().nodes.find((n) => n.id === target.id).params[key];
  const result = () => evaluate(live()).parameterValue(target.id, key);
  const apply = (expression) => send('set-parameter-expression', { expression });
  return { doc, layer, target, color, scalar, session, send, live, q, result, apply, key };
}
for (const [expression, hex, alpha] of [
  ['mix(#000000,#ffffff,0.5)', '#bcbcbc', 1],
  ['mix(#ff0000,#0000ff,0.5)', '#bc00bc', 1],
  ['mix(alpha(#ff0000,0),#0000ff,0.5)', '#0000ff', 0.5],
  ['mix(alpha(#ff0000,0.5),#0000ff,0.5)', '#9c00d5', 0.75],
  ['mix(mix(#000000,#ffffff,0.5),#ffffff,0.5)', '#e1e1e1', 1],
  ['mix(mix(#000000,#333333,0.5),#000000,0.5)', '#161616', 1],
  ['alpha(value,0.5)', '#ff0000', 0.5],
  ['alpha(value,1)', '#ff0000', 1],
  ['mix(value,#0000ff,0)', '#000000', 0],
  ['mix(value,#0000ff,1)', '#0000ff', 1],
  ['alpha(mix(alpha(#ff0000,0),alpha(#0000ff,0),0.5),1)', '#000000', 1],
])
  test(expression, () => {
    const f = fixture(),
      beforeBase = structuredClone(f.q().value);
    f.apply(expression);
    assert.equal(f.session.snapshot().schemaVersion, 43);
    for (let i = 0; i < 20; i++) assert.deepEqual(f.result(), { hex, alpha });
    assert.deepEqual(f.q().value, beforeBase, 'transparent base RGB is never overwritten');
  });

test('one common operation implementation retains floats, with separate public references as intentional boundaries', () => {
  const black = { hex: '#000000', alpha: 1 },
    gray = { hex: '#333333', alpha: 1 };
  const first = colorOperation('mix', [black, gray, 0.5]);
  assert.deepEqual(publicColor(first), { hex: '#232323', alpha: 1 });
  assert.equal(publicColor(colorOperation('mix', [first, black, 0.5])).hex, '#161616');
  assert.equal(publicColor(colorOperation('mix', [publicColor(first), black, 0.5])).hex, '#171717');
  const f = fixture();
  f.apply('mix(#000000,#333333,0.5)');
  f.send('set-parameter-expression', {
    nodeId: f.color.id,
    key: 'value',
    expression: `mix(${ref(f.target.id, 'value')},#000000,0.5)`,
  });
  assert.equal(evaluate(f.live()).parameterValue(f.color.id, 'value').hex, '#171717');
});

test('Color/Scalar references update by stable IDs, duplicate correctly and retain unrelated shape cache reuse', () => {
  const f = fixture(),
    shape = node('Rectangle');
  f.doc.layers[0].nodes.push(shape);
  f.scalar.params.value.value = 0.125;
  const s = new CommandSession(f.doc),
    send = (type, payload = {}) =>
      s.dispatch({
        ...s.state,
        type,
        payload: { layerId: f.layer.id, nodeId: f.target.id, key: 'value', ...payload },
      });
  send('set-parameter-expression', {
    expression: `mix(value,${ref(f.color.id, 'value')},${ref(f.scalar.id, 'value')} * 3)`,
  });
  const result = () => evaluate(s.snapshot().layers[0]).run(f.target.id);
  assert.deepEqual(result(), { hex: '#0000ff', alpha: 0.375 });
  send('set-parameter-base', { nodeId: f.scalar.id, value: 0.25 });
  assert.deepEqual(result(), { hex: '#0000ff', alpha: 0.75 });
  send('rename', { nodeId: f.color.id, name: 'Renamed Color' });
  const d = deserialize(serialize(s.snapshot()));
  d.layers[0].nodes.reverse();
  const copies = duplicateNodes(d.layers[0], [f.target.id, f.color.id, f.scalar.id]);
  const copied = copies.find((n) => n.type === 'Color' && n.params.value.expression);
  assert.ok(!copied.params.value.expression.includes(f.color.id));
  assert.ok(!copied.params.value.expression.includes(f.scalar.id));
  assert.deepEqual(evaluate(d.layers[0]).run(copied.id), { hex: '#0000ff', alpha: 0.75 });
  const raw = fixture();
  raw.doc.schemaVersion = 43;
  raw.layer.nodes.push(shape);
  raw.target.params.value.expression = `mix(value,${ref(raw.color.id, 'value')},${ref(raw.scalar.id, 'value')})`;
  let cached = evaluate(raw.layer);
  cached.run(shape.id);
  cached.run(raw.target.id);
  raw.color.params.value.value = { hex: '#00ff00', alpha: 1 };
  cached = evaluate(raw.layer);
  assert.deepEqual(cached.run(raw.target.id), { hex: '#00ff00', alpha: 0.5 });
  cached.run(shape.id);
  assert.ok(cached.stats.reused >= 1);
  send('delete-node', { nodeId: f.color.id });
  assert.throws(result, /missing/i);
  assert.doesNotThrow(() => deserialize(serialize(s.snapshot())));
  send('undo');
  assert.deepEqual(result(), { hex: '#0000ff', alpha: 0.75 });
});

for (const type of ['Color', 'Linear Gradient Fill', 'Radial Gradient Fill'])
  test(`${type}: one-Undo expression/base edits and JSON retain v1 and base`, () => {
    const key = type === 'Color' ? 'value' : 'startColor',
      f = fixture(type, key),
      initial = serialize(f.session.snapshot());
    f.apply('alpha(value,0.5)');
    const authored = serialize(f.session.snapshot()),
      state = f.session.state;
    f.apply('alpha(value,0.5)');
    assert.deepEqual(f.session.state, state);
    f.send('set-parameter-base', { value: { hex: '#12abcd', alpha: 0.2 } });
    assert.deepEqual(f.result(), { hex: '#12abcd', alpha: 0.5 });
    assert.equal(f.q().expression, 'alpha(value,0.5)');
    const saved = serialize(f.session.snapshot());
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), authored);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), initial);
    f.send('redo');
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), saved);
    assert.deepEqual(
      evaluate(deserialize(saved).layers[0]).parameterValue(f.target.id, key),
      f.result(),
    );
  });

test('preview, type/range/missing/cycle errors and stale Apply are neutral; connection priority retains dormant operations', () => {
  const f = fixture(),
    snapshot = () => serialize(f.session.snapshot());
  for (const expression of [
    'mix(value,1,0.5)',
    'alpha(1,0.5)',
    'mix(value,value,-0.01)',
    'mix(value,value,1.01)',
    'alpha(value,1.1)',
    'alpha(value,1/0)',
    'alpha(value,sqrt(-1))',
    'alpha(value,value)',
    'mix(#bad,value,0.5)',
    'mix(value,value,0.5,1)',
    'alpha(value)',
    `mix(value,${ref('missing-color', 'value')},0.5)`,
    `mix(value,${ref(f.scalar.id, 'value')},0.5)`,
    `alpha(value,${ref(f.color.id, 'value')})`,
    `mix(value,${ref(f.target.id, 'value')},0.5)`,
  ]) {
    const before = snapshot(),
      state = f.session.state,
      past = f.session.historyView.past.length;
    assert.throws(() => f.apply(expression));
    assert.throws(() => previewColorExpression(f.live(), f.target.id, 'value', expression));
    assert.equal(snapshot(), before);
    assert.deepEqual(f.session.state, state);
    assert.equal(f.session.historyView.past.length, past);
  }
  const before = snapshot();
  assert.deepEqual(previewColorExpression(f.live(), f.target.id, 'value', 'alpha(value,0.5)'), {
    hex: '#ff0000',
    alpha: 0.5,
  });
  assert.equal(snapshot(), before);
  const stale = f.session.state;
  f.apply('alpha(value,0.5)');
  const applied = snapshot();
  assert.throws(
    () => f.send('set-parameter-expression', { expression: 'alpha(value,0.8)' }, stale),
    /STALE/,
  );
  assert.equal(snapshot(), applied);
  f.send('connect', { sourceId: f.color.id });
  f.send('set-parameter-base', { value: { hex: '#123456', alpha: 0 } });
  assert.deepEqual(f.result(), f.color.params.value.value);
  f.send('disconnect');
  assert.deepEqual(f.result(), { hex: '#123456', alpha: 0.5 });
});

test('schema42 value/ref-only roundtrips byte-identically; new grammar requires schema43', () => {
  const f = fixture();
  f.apply('value');
  const legacy = serialize(f.session.snapshot());
  assert.equal(f.session.snapshot().schemaVersion, 42);
  assert.equal(serialize(deserialize(legacy)), legacy);
  const invalidLegacy = f.session.snapshot();
  invalidLegacy.layers[0].nodes.find((n) => n.id === f.target.id).params.value.expression =
    '(value)';
  assert.throws(() => deserialize(serialize(invalidLegacy)), /Color expression requires/);
  f.apply(ref(f.color.id, 'value'));
  assert.equal(f.session.snapshot().schemaVersion, 42);
  const doc = f.session.snapshot();
  doc.layers[0].nodes.find((n) => n.id === f.target.id).params.value.expression =
    'alpha(value,0.5)';
  assert.throws(() => deserialize(serialize(doc)), /schemaVersion 43/);
  doc.schemaVersion = 43;
  assert.doesNotThrow(() => deserialize(serialize(doc)));
});

test('Vector2 component weights and named presets retain the same typed stable references', () => {
  const f = fixture(),
    controller = node('Null');
  controller.params.position.value = [0.25, 0.75];
  f.layer.nodes.push(controller);
  const s = new CommandSession(f.doc);
  const send = (type, payload = {}) =>
    s.dispatch({
      ...s.state,
      type,
      payload: { layerId: f.layer.id, nodeId: f.target.id, key: 'value', ...payload },
    });
  send('set-parameter-expression', {
    expression: `alpha(value,${ref(controller.id, 'position', 'y')})`,
  });
  assert.deepEqual(evaluate(s.snapshot().layers[0]).run(f.target.id), {
    hex: '#ff0000',
    alpha: 0.75,
  });
  const { presetId } = send('save-node-preset', {
    nodeIds: [f.target.id, controller.id],
    name: 'Color alpha with weight',
  });
  assert.doesNotThrow(() => deserialize(serialize(s.snapshot())));
  const { nodeIds } = send('apply-node-preset', { presetId });
  const copied = s
    .snapshot()
    .layers[0].nodes.find((n) => nodeIds.includes(n.id) && n.type === 'Color');
  assert.ok(!copied.params.value.expression.includes(controller.id));
  assert.deepEqual(evaluate(s.snapshot().layers[0]).run(copied.id), {
    hex: '#ff0000',
    alpha: 0.75,
  });
});
