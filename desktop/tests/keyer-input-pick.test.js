import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, serialize, deserialize, exportSVG, node } from '../src/core.js';
import {
  keyerInputPickContext,
  sampleKeyerInput,
  planKeyerInputPick,
} from '../src/keyer-input-pick.js';
import { keyerPreviewPoint } from '../src/keyer-input-picker.js';
import { keyerPickFixture } from './fixtures/keyer-input-pick.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

test('input is immediately before selected Keyer, with earlier effects, straight RGB and immutable assets', () => {
  const f = keyerPickFixture(),
    before = serialize(f.doc),
    ctx = keyerInputPickContext(f.doc, f.layer.id, f.keyer.id);
  assert.equal(sampleKeyerInput(ctx, [0.1, 0.1]).state, 'transparent');
  assert.deepEqual(sampleKeyerInput(ctx, [1.9, 0.9]), {
    state: 'color',
    x: 1,
    y: 0,
    hex: '#0c2238',
  });
  assert.equal(sampleKeyerInput(ctx, [2.1, 0.1]).state, 'transparent');
  assert.deepEqual(sampleKeyerInput(ctx, [3.9, 0.1]), {
    state: 'color',
    x: 3,
    y: 0,
    hex: '#ff0000',
  });
  const raw = keyerInputPickContext(f.doc, f.layer.id, f.first.id);
  assert.equal(sampleKeyerInput(raw, [0.1, 0.1]).hex, '#00ff00');
  assert.equal(serialize(f.doc), before);
  assert.notEqual(ctx.asset.data, f.doc.imageAssets['image-pick'].data);
});
test('floor uses half-open boundaries without clamping or applying alpha0', () => {
  const f = keyerPickFixture(),
    ctx = keyerInputPickContext(f.doc, f.layer.id, f.keyer.id);
  for (const point of [
    [-1e-12, 0],
    [4, 0],
    [0, 2],
    [5, 5],
    [0, -1e-12],
  ])
    assert.equal(sampleKeyerInput(ctx, point).state, 'outside');
  assert.equal(sampleKeyerInput(ctx, [4 - 1e-10, 2 - 1e-10]).hex, '#505050');
  assert.equal(planKeyerInputPick(f.doc, ctx, [2, 0]).state, 'transparent');
  for (const point of [[NaN, 0], [Infinity, 0], [], null])
    assert.throws(() => sampleKeyerInput(ctx, point));
});
test('one common Color command, one Undo/Redo/no-op, original alpha and JSON/SVG preserved', () => {
  const f = keyerPickFixture(),
    s = new CommandSession(f.doc),
    ctx = keyerInputPickContext(s.snapshot(), f.layer.id, f.keyer.id),
    before = s.snapshot();
  const plan = planKeyerInputPick(s.snapshot(), ctx, [1.2, 0.3]);
  assert.equal(plan.type, 'set-parameter');
  s.dispatch({ ...s.state, type: plan.type, payload: plan.payload });
  assert.equal(s.historyView.past.length, 1);
  const doc = s.snapshot(),
    live = doc.layers[0].nodes.find((n) => n.id === f.keyer.id);
  assert.deepEqual(live.params.keyColor.value, { hex: '#0c2238', alpha: 1 });
  assert.deepEqual(doc.imageAssets, before.imageAssets);
  assert.deepEqual(deserialize(serialize(doc)), doc);
  assert.equal(exportSVG(deserialize(serialize(doc))), exportSVG(doc));
  s.dispatch({ ...s.state, type: 'undo' });
  assert.deepEqual(s.snapshot(), before);
  s.dispatch({ ...s.state, type: 'redo' });
  assert.deepEqual(s.snapshot(), doc);
  const repeat = planKeyerInputPick(
      doc,
      keyerInputPickContext(doc, f.layer.id, f.keyer.id),
      [1.2, 0.3],
    ),
    state = s.state;
  s.dispatch({ ...s.state, type: repeat.type, payload: repeat.payload });
  assert.deepEqual(s.state, state);
  assert.equal(s.historyView.past.length, 1);
});
test('driven base uses the existing Color base editor without detaching expression or changing saved alpha', () => {
  const f = keyerPickFixture();
  f.keyer.params.keyColor.expression = 'value';
  f.keyer.params.keyColor.value.alpha = 0.3;
  const s = new CommandSession(f.doc),
    ctx = keyerInputPickContext(s.snapshot(), f.layer.id, f.keyer.id),
    plan = planKeyerInputPick(s.snapshot(), ctx, [1, 0]);
  assert.equal(plan.type, 'set-parameter-base');
  s.dispatch({ ...s.state, type: plan.type, payload: plan.payload });
  const q = s.snapshot().layers[0].nodes.find((n) => n.id === f.keyer.id).params.keyColor;
  assert.equal(q.expression, 'value');
  assert.equal(q.value.alpha, 0.3);
  assert.equal(q.value.hex, '#0c2238');
});
test('target/input/image replacement and locks reject before a common command can mutate history', () => {
  for (const mutate of [
    (f) => {
      f.keyer.params.path.source = f.image.id;
    },
    (f) => {
      f.first.params.tolerance.value = 0.5;
    },
    (f) => {
      f.doc.imageAssets['image-pick'].data =
        f.doc.imageAssets['image-pick'].data.slice(0, -4) + 'AAAA';
    },
    (f) => {
      f.layer.locked = true;
    },
    (f) => {
      f.keyer.locked = true;
    },
    (f) => {
      f.keyer.params.keyColor.locked = true;
    },
    (f) => {
      f.layer.nodes = f.layer.nodes.filter((n) => n.id !== f.keyer.id);
    },
  ]) {
    const f = keyerPickFixture(),
      ctx = keyerInputPickContext(f.doc, f.layer.id, f.keyer.id);
    mutate(f);
    const before = JSON.stringify(f.doc);
    assert.throws(() => planKeyerInputPick(f.doc, ctx, [1, 0]));
    assert.equal(JSON.stringify(f.doc), before);
  }
});
test('unsupported source is rejected; placed image size does not alter intrinsic samples', () => {
  const f = keyerPickFixture(),
    ctx = keyerInputPickContext(f.doc, f.layer.id, f.keyer.id);
  assert.equal(ctx.input.size[0], 284);
  assert.equal(ctx.asset.width, 4);
  f.image.params.position.value = [-200, 300];
  f.image.params.size.value = [10, 500];
  assert.equal(
    sampleKeyerInput(keyerInputPickContext(f.doc, f.layer.id, f.keyer.id), [1, 0]).hex,
    '#0c2238',
  );
  f.keyer.params.path.source = null;
  assert.throws(() => keyerInputPickContext(f.doc, f.layer.id, f.keyer.id), /Connect/);
  f.layer.nodes.push(node('Composition Reference'));
  f.keyer.params.path.source = f.layer.nodes.at(-1).id;
  assert.throws(() => keyerInputPickContext(f.doc, f.layer.id, f.keyer.id));
});

test('a valid same-dimension image reload invalidates captured pixels without touching the replacement', () => {
  const f = keyerPickFixture(),
    ctx = keyerInputPickContext(f.doc, f.layer.id, f.keyer.id);
  f.doc.imageAssets['image-pick'] = {
    ...f.doc.imageAssets['image-pick'],
    data: encodeImage(encodeRgbaPng(4, 2, new Uint8Array(32).fill(255))),
  };
  const before = serialize(f.doc);
  assert.throws(() => planKeyerInputPick(f.doc, ctx, [1, 0]), /input changed/);
  assert.equal(serialize(f.doc), before);
});
test('preview screen CTM inverse supports rotated/nonuniform placement; singular/unavailable mappings reject', () => {
  const old = globalThis.DOMPoint;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
    }
  };
  try {
    // Forward transform: screen=[100-3*y,50+2*x].
    const svg = {
      getScreenCTM: () => ({
        inverse: () => ({ a: 0, b: -1 / 3, c: 1 / 2, d: 0, e: -25, f: 100 / 3 }),
      }),
    };
    const point = keyerPreviewPoint(svg, [98.5, 53]);
    assert.ok(Math.abs(point[0] - 1.5) < 1e-10);
    assert.ok(Math.abs(point[1] - 0.5) < 1e-10);
    for (const getScreenCTM of [
      () => null,
      () => ({ is2D: false }),
      () => ({ inverse: () => ({ a: NaN }) }),
    ])
      assert.throws(() => keyerPreviewPoint({ getScreenCTM }, [1, 1]));
  } finally {
    globalThis.DOMPoint = old;
  }
});
