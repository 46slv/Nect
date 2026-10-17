import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  bypassContract,
  connect,
  deserialize,
  document,
  evaluate,
  exportSVG,
  node,
  pathArea,
  serialize,
  setNodeBypassed,
} from '../src/core.js';

function fixture({
  modifier = 'Halftone',
  paint = 'solid',
  color = '#000000',
  width = 16,
  height = 16,
  left = 0,
  top = 0,
  rawPath,
  strokeOnly = false,
  grouped = false,
  image = false,
} = {}) {
  const doc = document(),
    layer = doc.layers[0],
    shape = node(image ? 'Image' : rawPath !== undefined ? 'Imported Path' : 'Rectangle'),
    effect = node(modifier),
    out = node('Output');
  if (rawPath !== undefined) shape.rawPath = rawPath;
  if (!image && rawPath === undefined) {
    shape.params.center.value = [left + width / 2, top + height / 2];
    shape.params.size.value = [width, height];
  }
  const nodes = [shape];
  let painted = shape;
  if (grouped) {
    const joined = node('Join Geometry');
    joined.params.a.source = shape.id;
    joined.params.b.source = shape.id;
    nodes.push(joined);
    painted = joined;
  }
  if (strokeOnly) {
    shape.type = 'Curve';
    shape.params = structuredClone(node('Curve').params);
    shape.points = structuredClone(node('Curve').points);
    shape.params.closed.value = false;
  }
  const paintNode = node(
    strokeOnly
      ? 'Stroke'
      : paint === 'linear'
        ? 'Linear Gradient Fill'
        : paint === 'radial'
          ? 'Radial Gradient Fill'
          : 'Fill',
  );
  paintNode.params.path.source = painted.id;
  if (paint === 'solid' && !strokeOnly) paintNode.params.color.value = { hex: color, alpha: 1 };
  if (paint === 'linear') {
    paintNode.params.start.value = [left, top + height / 2];
    paintNode.params.end.value = [left + width, top + height / 2];
    paintNode.params.startColor.value = { hex: '#a0a0a0', alpha: 1 };
    paintNode.params.endColor.value = { hex: '#e0e0e0', alpha: 1 };
  }
  if (paint === 'radial') {
    paintNode.params.center.value = [left + width / 2, top + height / 2];
    paintNode.params.radius.value = Math.max(width, height) / 2;
    paintNode.params.startColor.value = { hex: '#000000', alpha: 1 };
    paintNode.params.endColor.value = { hex: '#ffffff', alpha: 1 };
  }
  nodes.push(paintNode, effect, out);
  effect.params.path.source = paintNode.id;
  effect.params.pitch.value = 16;
  out.params.path.source = effect.id;
  layer.nodes = nodes;
  layer.output = out.id;
  return { doc, layer, shape, paintNode, effect, out };
}

const area = (path) => (path.contours.length ? pathArea(path) : 0);
const coldRun = (layer, id) => evaluate(layer).run(id);

test('Halftone and Ordered Dither preserve all-white emptiness and cover small and nonaligned black regions', () => {
  for (const modifier of ['Halftone', 'Ordered Dither']) {
    const white = fixture({ modifier, color: '#ffffff' });
    assert.equal(coldRun(white.layer, white.effect.id).contours.length, 0, `${modifier} white`);

    const black = fixture({ modifier, color: '#000000' });
    const blackPath = coldRun(black.layer, black.effect.id);
    assert.equal(blackPath.fill.hex, '#000000');
    assert.equal(blackPath.fill.alpha, 1);
    assert.equal(blackPath.stroke, undefined);
    assert.ok(Math.abs(area(blackPath) - 256) < 0.001, `${modifier} covers black source`);

    const tiny = fixture({ modifier, color: '#000000', width: 2, height: 2, left: 2, top: 3 });
    assert.ok(
      Math.abs(area(coldRun(tiny.layer, tiny.effect.id)) - 4) < 0.001,
      `${modifier} tiny source`,
    );

    const misaligned = fixture({
      modifier,
      color: '#000000',
      width: 13,
      height: 19,
      left: 3,
      top: 5,
    });
    assert.ok(
      Math.abs(area(coldRun(misaligned.layer, misaligned.effect.id)) - 247) < 0.001,
      `${modifier} nonaligned source`,
    );
  }
});

test('valid empty filled geometry stays empty through luminance modifiers', () => {
  for (const modifier of ['Halftone', 'Ordered Dither']) {
    const empty = fixture({ modifier, rawPath: '' }),
      result = coldRun(empty.layer, empty.effect.id);
    assert.deepEqual(result.contours, [], modifier);
    assert.deepEqual(result.fill, { hex: '#000000', alpha: 1 }, modifier);
    assert.ok(result.unstable, `${modifier} remains derived geometry`);
  }
});

test('gradient halftone varies dot area and ordered dither varies Bayer coverage density', () => {
  const gradient = fixture({ modifier: 'Halftone', paint: 'linear', width: 64, height: 16 }),
    dots = coldRun(gradient.layer, gradient.effect.id),
    dotAreas = dots.contours.map((contour) => area({ contours: [contour] }));
  assert.equal(dotAreas.length, 4);
  assert.ok(dotAreas[0] > dotAreas[1] && dotAreas[1] > dotAreas[2] && dotAreas[2] > dotAreas[3]);
  assert.ok(dots.unstable);

  const dark = fixture({ modifier: 'Ordered Dither', color: '#404040', width: 128, height: 128 }),
    light = fixture({ modifier: 'Ordered Dither', color: '#c0c0c0', width: 128, height: 128 });
  const darkArea = area(coldRun(dark.layer, dark.effect.id)),
    lightArea = area(coldRun(light.layer, light.effect.id));
  assert.ok(darkArea > lightArea, `dark Bayer coverage ${darkArea} > light ${lightArea}`);
  assert.ok(lightArea > 0 && darkArea < 128 * 128);
});

test('compound holes remain empty, with no black leakage across the clipped mask', () => {
  const donut = fixture({
    modifier: 'Ordered Dither',
    rawPath: 'M0 0H64V64H0Z M16 16V48H48V16Z',
  });
  const result = coldRun(donut.layer, donut.effect.id);
  assert.ok(Math.abs(area(result) - (64 * 64 - 32 * 32)) < 0.001);
  assert.equal(result.contours.length, 2, 'one outer boundary and one hole boundary');
});

test('unsupported open, grouped, stroke-only and image sources fail with specific diagnostics', () => {
  const open = fixture({ modifier: 'Halftone' });
  open.shape.type = 'Curve';
  open.shape.params = structuredClone(node('Curve').params);
  open.shape.params.closed.value = false;
  open.shape.points = [
    {
      id: 'open-a',
      position: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleA: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleB: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleType: 'corner',
    },
    {
      id: 'open-b',
      position: { type: 'Vector2', value: [64, 0], expression: '', locked: false },
      handleA: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleB: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleType: 'corner',
    },
  ];
  assert.throws(() => coldRun(open.layer, open.effect.id), { code: 'LUMINANCE_MASK_OPEN_PATH' });

  const grouped = fixture({ modifier: 'Halftone', grouped: true });
  assert.throws(() => coldRun(grouped.layer, grouped.effect.id), {
    code: 'LUMINANCE_MASK_UNSUPPORTED',
  });

  const stroke = fixture({ modifier: 'Halftone', strokeOnly: true });
  assert.throws(() => coldRun(stroke.layer, stroke.effect.id), /stroke-only/);

  const image = fixture({ modifier: 'Ordered Dither', image: true });
  assert.throws(() => coldRun(image.layer, image.effect.id), /image inputs are unsupported/);
});

test('same-type bypass, deterministic identity, JSON round-trip, shared undo/redo and budget rollback work', () => {
  const f = fixture({ modifier: 'Halftone', paint: 'radial', width: 64, height: 64 });
  const effect = f.effect,
    direct = coldRun(f.layer, effect.id);
  assert.ok(direct.unstable);
  assert.deepEqual(coldRun(f.layer, effect.id), direct, 'cold evaluation has deterministic IDs');
  const contract = bypassContract(effect);
  assert.deepEqual(contract, {
    supported: true,
    input: 'path',
    type: 'PaintedPath',
    reason: 'Pass path through unchanged; keep all settings and connections.',
  });
  const original = coldRun(f.layer, f.paintNode.id);
  setNodeBypassed(f.layer, effect.id, true);
  f.doc.schemaVersion = 10;
  assert.deepEqual(coldRun(f.layer, effect.id), original);
  const saved = serialize(f.doc),
    restored = deserialize(saved);
  assert.deepEqual(coldRun(restored.layers[0], effect.id), original);

  const session = new CommandSession(f.doc),
    layerId = f.layer.id,
    dispatch = (type, payload) =>
      session.dispatch({ ...session.state, type, payload: { layerId, ...payload } });
  dispatch('set-node-bypassed', { nodeId: effect.id, bypassed: false });
  const before = serialize(session.snapshot()),
    beforePath = coldRun(session.snapshot().layers[0], effect.id);
  dispatch('set-parameter', { nodeId: effect.id, key: 'pitch', value: 8 });
  const after = serialize(session.snapshot()),
    afterPath = coldRun(session.snapshot().layers[0], effect.id);
  assert.notDeepEqual(afterPath, beforePath);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), after);
  assert.deepEqual(coldRun(session.snapshot().layers[0], effect.id), afterPath);

  const bounded = fixture({ modifier: 'Ordered Dither', color: '#000000' }),
    boundedSession = new CommandSession(bounded.doc),
    boundedDispatch = (payload) =>
      boundedSession.dispatch({
        ...boundedSession.state,
        type: 'set-parameter',
        payload: { layerId: bounded.layer.id, nodeId: bounded.shape.id, ...payload },
      }),
    stable = serialize(boundedSession.snapshot());
  assert.throws(() => boundedDispatch({ key: 'size', value: [2048, 2048] }), /4096/);
  assert.equal(
    serialize(boundedSession.snapshot()),
    stable,
    'budget failure rolls back the source edit',
  );
});
