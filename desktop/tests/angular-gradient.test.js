import test from 'node:test';
import assert from 'node:assert/strict';
import {
  createAngularGradientFill,
  sampleAngularGradient,
  renderAngularGradientFill,
} from '../src/angular-gradient.js';
import {
  blankDocument,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
} from '../src/core.js';
const color = (hex, alpha = 1) => ({ hex, alpha });
const inputs = () => ({
  center: [100, 100],
  angle: -90,
  clockwise: true,
  startColor: color('#ff0000'),
  color1: color('#00ff00'),
  position1: 1 / 3,
  color2: color('#0000ff'),
  position2: 2 / 3,
  endColor: color('#ff0000'),
});
const geometry = {
  contours: [
    {
      id: 'c',
      closed: true,
      points: [
        [0, 0],
        [200, 0],
        [200, 200],
        [0, 200],
      ].map((position, i) => ({ id: 'p' + i, position, handleA: [0, 0], handleB: [0, 0] })),
    },
  ],
};
test('four stops, alpha and center are copied and sampled without mutation', () => {
  const o = inputs(),
    before = structuredClone(o),
    f = createAngularGradientFill(o);
  for (const [t, c] of [
    [0, o.startColor],
    [o.position1, o.color1],
    [o.position2, o.color2],
    [1, o.endColor],
  ])
    assert.deepEqual(sampleAngularGradient(f, t), c);
  f.center[0] = 20;
  f.color1.hex = '#ffffff';
  assert.deepEqual(o, before);
});
test('encoded-sRGB interpolation is premultiplied by alpha', () => {
  const f = createAngularGradientFill({ ...inputs(), color1: color('#0000ff', 0) });
  assert.deepEqual(sampleAngularGradient(f, 1 / 6), color('#ff0000', 0.5));
  assert.deepEqual(
    sampleAngularGradient(
      createAngularGradientFill({
        ...inputs(),
        startColor: color('#ff0000', 0),
        color1: color('#0000ff', 0),
      }),
      1 / 6,
    ),
    color('#000000', 0),
  );
});
test('malformed stops, colors and coordinate/direction options are rejected', () => {
  for (const changes of [
    { position1: 0 },
    { position1: 0.9 },
    { position2: 1 },
    { angle: 361 },
    { center: Array(2) },
    { clockwise: 1 },
    { startColor: color('#ffffff\n') },
    { color1: color('#ffffff', NaN) },
  ])
    assert.throws(() => createAngularGradientFill({ ...inputs(), ...changes }));
  assert.throws(() => sampleAngularGradient(createAngularGradientFill(inputs()), NaN));
});
test('SVG uses bounded vector sectors, local IDs and no raster/foreignObject fallback', () => {
  const f = createAngularGradientFill(inputs()),
    before = JSON.stringify(geometry),
    r = renderAngularGradientFill(f, 'gradient-safe', geometry);
  assert.equal((r.definition.match(/<path /g) || []).length, 720);
  assert.match(r.definition, /<pattern .*patternUnits="userSpaceOnUse"/);
  assert.match(r.definition, /viewBox="0 0 200 200"/);
  assert.equal(r.paint, 'url(#gradient-safe)');
  assert.doesNotMatch(r.definition, /<image|foreignObject|javascript:|data:/);
  assert.equal(JSON.stringify(geometry), before);
  assert.throws(() => renderAngularGradientFill(f, 'x" onload="alert(1)', geometry));
  assert.notEqual(
    renderAngularGradientFill({ ...f, clockwise: false }, 'g', geometry).definition,
    r.definition,
  );
});
function fixture() {
  const doc = blankDocument(),
    l = doc.layers[0],
    circle = node('Circle'),
    grad = node('Angular Gradient Fill'),
    stroke = node('Stroke'),
    out = l.nodes[0];
  circle.params.center.value = [480, 270];
  circle.params.radius.value = 180;
  l.nodes.unshift(circle, grad, stroke);
  connect(l, circle.id, grad.id, 'path');
  connect(l, grad.id, stroke.id, 'path');
  connect(l, stroke.id, out.id, 'path');
  return { doc, l, grad };
}
test('core connects paint, keeps solid Stroke, saves/reopens and exports the shared pattern', () => {
  const { doc, l } = fixture(),
    r = evaluate(l, {}, {}, doc);
  assert.equal(r.error, null);
  assert.equal(r.path.fill.kind, 'angular-gradient');
  assert.ok(r.path.stroke);
  const saved = serialize(doc);
  assert.equal(serialize(deserialize(saved)), saved);
  const svg = exportSVG(doc);
  assert.match(svg, /<pattern /);
  assert.match(svg, /stroke-width=/);
  assert.equal((svg.match(/<pattern /g) || []).length, 1);
});
test('invalid stop edits roll back atomically; angle edits support Undo/Redo', () => {
  const { doc, l, grad } = fixture(),
    s = new CommandSession(doc),
    before = serialize(s.snapshot());
  const edit = (key, value) =>
    s.dispatch({
      ...s.state,
      type: 'set-parameter',
      payload: { layerId: l.id, nodeId: grad.id, key, value },
    });
  assert.throws(() => edit('position1', 0.9));
  assert.equal(serialize(s.snapshot()), before);
  edit('angle', 45);
  const after = serialize(s.snapshot());
  assert.notEqual(after, before);
  s.dispatch({ ...s.state, type: 'undo' });
  assert.equal(serialize(s.snapshot()), before);
  s.dispatch({ ...s.state, type: 'redo' });
  assert.equal(serialize(s.snapshot()), after);
});
