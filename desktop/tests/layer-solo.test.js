import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  validate,
} from '../src/core.js';
import { layerIsVisible } from '../src/layer-visibility.js';
function fixture() {
  const d = document();
  d.layers = [];
  for (const [i, color] of ['#ff0000', '#0000ff', '#00ff00'].entries()) {
    const l = document().layers[0],
      shape = node('Rectangle'),
      fill = node('Fill'),
      out = node('Output');
    l.name = ['Red', 'Blue', 'Hidden'][i];
    l.visible = i < 2;
    shape.params.center.value = [25 + i * 60, 25];
    shape.params.size.value = [40, 40];
    fill.params.color.value = { hex: color, alpha: 0.5 };
    l.nodes = [shape, fill, out];
    l.output = out.id;
    connect(l, shape.id, fill.id, 'path');
    connect(l, fill.id, out.id, 'path');
    d.layers.push(l);
  }
  const s = new CommandSession(d),
    send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  return { d, s, send };
}
test('multiple Solo filters preview/export without rewriting Hide; Undo and saved JSON retain it', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot()),
    hidden = f.d.layers.map((l) => l.visible);
  const set = (i, value) =>
    f.send('set-layer-state', { layerId: f.d.layers[i].id, key: 'solo', value });
  set(1, true);
  let d = f.s.snapshot();
  assert.equal(d.schemaVersion, 33);
  assert.deepEqual(
    d.layers.map((l) => layerIsVisible(d, l)),
    [false, true, false],
  );
  assert.doesNotMatch(exportSVG(d), /#ff0000/);
  assert.match(exportSVG(d), /#0000ff/);
  set(0, true);
  d = f.s.snapshot();
  assert.deepEqual(
    d.layers.map((l) => layerIsVisible(d, l)),
    [true, true, false],
  );
  set(2, true);
  d = f.s.snapshot();
  assert.equal(layerIsVisible(d, d.layers[2]), false, 'Hide has priority');
  assert.deepEqual(
    d.layers.map((l) => l.visible),
    hidden,
  );
  assert.equal(exportSVG(deserialize(serialize(d))), exportSVG(d));
  f.send('clear-layer-solo');
  d = f.s.snapshot();
  assert.deepEqual(
    d.layers.map((l) => layerIsVisible(d, l)),
    hidden,
  );
  f.send('undo');
  assert.equal(f.s.snapshot().layers.filter((l) => l.solo).length, 3);
  f.send('redo');
  assert.equal(f.s.snapshot().layers.filter((l) => l.solo).length, 0);
  for (let i = 0; i < 4; i++) f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
});
test('Solo only filters top-level compositions; referenced hidden source still evaluates unchanged', () => {
  const f = fixture(),
    d = f.d,
    source = d.layers[2],
    target = d.layers[0],
    ref = node('Composition Reference');
  ref.compositionReference = { compositionId: source.id };
  target.nodes.push(ref);
  connect(target, ref.id, target.output, 'path');
  d.schemaVersion = 31;
  const s = new CommandSession(d);
  s.dispatch({
    ...s.state,
    type: 'set-layer-state',
    payload: { layerId: target.id, key: 'solo', value: true },
  });
  const after = s.snapshot();
  assert.deepEqual(after.layers[2], source);
  assert.match(exportSVG(after), /#00ff00/);
  assert.doesNotMatch(exportSVG(after), /#0000ff/);
  const old = structuredClone(after);
  old.schemaVersion = 32;
  assert.throws(() => validate(old), /layer state/);
});
