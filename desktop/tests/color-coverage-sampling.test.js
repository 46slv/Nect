import test from 'node:test';
import assert from 'node:assert/strict';
import {
  coverageSampleSize,
  sampleColorCoverage as sample,
} from '../src/color-coverage-sampling.js';

const pass = (slotId, svg = '<svg/>') => ({ slotId, svg });
const bounds = { width: 1, height: 1 };
const deferred = () => {
  let resolve;
  const promise = new Promise((done) => (resolve = done));
  return { promise, resolve };
};

test('sampling dimensions preserve bounded aspect ratio without upscaling', () => {
  assert.deepEqual(coverageSampleSize(100, 50, 1), { width: 100, height: 50 });
  assert.deepEqual(coverageSampleSize(1000, 500, 1), { width: 256, height: 128 });
  assert.deepEqual(coverageSampleSize(500, 1000, 1), { width: 128, height: 256 });
  assert.deepEqual(coverageSampleSize(100000, 0.01, 256), { width: 256, height: 1 });
  assert.deepEqual(coverageSampleSize(0.01, 100000, 256), { width: 1, height: 256 });
});

test('invalid dimensions and pass counts reject before allocating buffers', () => {
  for (const dimension of [0, -1, NaN, Infinity, '100', undefined, 100001]) {
    assert.throws(() => coverageSampleSize(dimension, 1, 1), /sampling bounds/);
    assert.throws(() => coverageSampleSize(1, dimension, 1), /sampling bounds/);
  }
  for (const count of [-1, 1.5, 257, NaN, Infinity, '1', undefined])
    assert.throws(() => coverageSampleSize(1, 1, count), /sampling bounds/);
});

test('aggregate alpha budget accepts the boundary and rejects many individually bounded passes', async () => {
  assert.deepEqual(coverageSampleSize(256, 256, 64), { width: 256, height: 256 });
  assert.throws(() => coverageSampleSize(256, 256, 65), /sampling budget/);
  let calls = 0;
  await assert.rejects(
    sample(
      ['c1'],
      Array.from({ length: 65 }, () => pass('c1')),
      { width: 256, height: 256 },
      () => {
        calls++;
        return [];
      },
    ),
    /sampling budget/,
  );
  assert.equal(calls, 0);
});

test('empty scenes need no rasterizer calls and do not count artboard background', async () => {
  const result = await sample(['c2', 'c1'], [], { width: 1000, height: 500 }, () => {
    assert.fail('Empty scenes must not rasterize');
  });
  assert.deepEqual(result.slots, [
    { id: 'c2', contribution: 0, percent: 0 },
    { id: 'c1', contribution: 0, percent: 0 },
  ]);
  assert.deepEqual(result.other, { id: null, contribution: 0, percent: 0 });
  assert.equal(result.paintedPixelEquivalent, 0);
  assert.equal(result.samplePixels, 256 * 128);
  assert.deepEqual(result.sampleSize, { width: 256, height: 128 });
  assert.equal(result.approximate, true);
  assert.equal(result.mode, 'source-over-visible-contribution-v1');
});

test('known and Other passes compose in paint order while preserving stable IDs', async () => {
  const masks = {
    back: [1, 1, 1, 1],
    front: [1, 0.5, 0, 0],
    unlinked: [0, 0, 1, 0],
    missing: [0, 0, 0, 0.5],
  };
  const calls = [];
  const result = await sample(
    ['front-id', 'back-id', 'same-display-value-distinct-id'],
    [
      pass('back-id', 'back'),
      pass('front-id', 'front'),
      pass(null, 'unlinked'),
      pass('missing-id', 'missing'),
    ],
    { width: 4, height: 1 },
    async (svg, size) => {
      calls.push({ svg, size });
      return masks[svg];
    },
  );
  assert.deepEqual(
    calls,
    Object.keys(masks).map((svg) => ({ svg, size: { width: 4, height: 1 } })),
  );
  assert.deepEqual(result.slots, [
    { id: 'front-id', contribution: 1.5, percent: 38 },
    { id: 'back-id', contribution: 1, percent: 25 },
    { id: 'same-display-value-distinct-id', contribution: 0, percent: 0 },
  ]);
  assert.deepEqual(result.other, { id: null, contribution: 1.5, percent: 37 });
  assert.equal(result.paintedPixelEquivalent, 4);
});

test('invalid request shapes, Color registries, and source slots reject before decoding', async () => {
  let calls = 0;
  const rasterize = () => {
    calls++;
    return [1];
  };
  for (const args of [
    [null, [], bounds, rasterize],
    [[], null, bounds, rasterize],
    [[], [], bounds, null],
    [[], [], bounds, rasterize, null],
    [[], [], null, rasterize],
    [['same', 'same'], [], bounds, rasterize],
    [[''], [], bounds, rasterize],
    [[3], [], bounds, rasterize],
    [new Array(1), [], bounds, rasterize],
    [Array.from({ length: 257 }, (_, i) => `c${i}`), [], bounds, rasterize],
    [['c1'], [pass({})], bounds, rasterize],
    [['c1'], [pass('invalid slot')], bounds, rasterize],
    [['c1'], [pass('')], bounds, rasterize],
    [['c1'], [null], bounds, rasterize],
    [['c1'], new Array(1), bounds, rasterize],
    [['c1'], [{ slotId: 'c1', svg: 1 }], bounds, rasterize],
  ])
    await assert.rejects(sample(...args));
  assert.equal(calls, 0);
});

test('SVG per-pass and aggregate budgets reject before decoding', async () => {
  const maxSvg = 'x'.repeat(4 * 1024 * 1024);
  let calls = 0;
  const rasterize = () => {
    calls++;
    return [1];
  };
  await assert.rejects(sample(['c1'], [pass('c1', `${maxSvg}x`)], bounds, rasterize), /paint pass/);
  await assert.rejects(
    sample(
      ['c1'],
      Array.from({ length: 5 }, () => pass('c1', maxSvg)),
      bounds,
      rasterize,
    ),
    /SVG budget/,
  );
  assert.equal(calls, 0);
  const allowed = await sample(
    ['c1'],
    Array.from({ length: 4 }, () => pass('c1', maxSvg)),
    bounds,
    rasterize,
  );
  assert.equal(calls, 4);
  assert.equal(allowed.slots[0].percent, 100);
});

test('malformed alpha rejects without coercion or Float32 rounding into validity', async () => {
  for (const alpha of [
    null,
    undefined,
    [],
    [1, 0],
    ['1'],
    ['0.5'],
    [NaN],
    [Infinity],
    [-0.01],
    [1.01],
    new Float64Array([1 + Number.EPSILON]),
    new Uint8Array([1]),
    new Array(1),
  ]) {
    let calls = 0;
    await assert.rejects(
      sample(['c1'], [pass('c1'), pass('c1')], bounds, () => {
        calls++;
        return alpha;
      }),
    );
    assert.equal(calls, 1, 'Malformed masks must stop before the next decode');
  }
});

test('valid Array, Float32Array, and Float64Array masks retain normalized alpha', async () => {
  for (const alpha of [[0.5], new Float32Array([0.5]), new Float64Array([0.5])]) {
    const result = await sample(['c1'], [pass('c1')], bounds, async () => alpha);
    assert.equal(result.slots[0].contribution, 0.5);
    assert.equal(result.slots[0].percent, 100);
    assert.equal(alpha[0], 0.5);
  }
});

test('a stale sample never starts decoding, including an empty scene', async () => {
  for (const passes of [[], [pass('c1')]])
    await assert.rejects(
      sample(
        ['c1'],
        passes,
        bounds,
        () => assert.fail('Stale decode'),
        () => false,
      ),
      /stale/,
    );
});

test('document changes during a pending decode discard the result and remaining passes', async () => {
  const pending = deferred();
  let current = true;
  let calls = 0;
  const result = sample(
    ['c1'],
    [pass('c1'), pass('c1')],
    bounds,
    () => {
      calls++;
      return pending.promise;
    },
    () => current,
  );
  assert.equal(calls, 1);
  current = false;
  pending.resolve([1]);
  await assert.rejects(result, /stale/);
  assert.equal(calls, 1);
});

test('pending sampling snapshots caller-owned IDs, pass objects, SVGs, and bounds', async () => {
  const first = deferred();
  const ids = ['c1', 'c2'];
  const passes = [pass('c1', 'first'), pass('c2', 'second')];
  const inputBounds = { width: 2, height: 1 };
  const calls = [];
  const result = sample(ids, passes, inputBounds, (svg, size) => {
    calls.push({ svg, size: { ...size } });
    size.width = 999; // A decoder must not rewrite subsequent sampling dimensions.
    return calls.length === 1 ? first.promise : [0, 1];
  });
  ids.reverse();
  ids.push('new');
  passes[0].slotId = 'c2';
  passes[1].slotId = 'c1';
  passes[1].svg = 'rewritten';
  passes.push(pass('c1', 'new'));
  inputBounds.width = 10;
  first.resolve([1, 0]);
  const measured = await result;
  assert.deepEqual(calls, [
    { svg: 'first', size: { width: 2, height: 1 } },
    { svg: 'second', size: { width: 2, height: 1 } },
  ]);
  assert.deepEqual(measured.slots, [
    { id: 'c1', contribution: 1, percent: 50 },
    { id: 'c2', contribution: 1, percent: 50 },
  ]);
  assert.deepEqual(measured.sampleSize, { width: 2, height: 1 });
});

test('decoder-reused alpha buffers are copied before the next rasterization', async () => {
  for (const shared of [[0, 0], new Float32Array(2), new Float64Array(2)]) {
    let calls = 0;
    const result = await sample(
      ['back', 'front'],
      [pass('back'), pass('front')],
      { width: 2, height: 1 },
      () => {
        shared[0] = calls === 0 ? 1 : 0;
        shared[1] = calls === 0 ? 0 : 1;
        calls++;
        return shared;
      },
    );
    assert.deepEqual(result.slots, [
      { id: 'back', contribution: 1, percent: 50 },
      { id: 'front', contribution: 1, percent: 50 },
    ]);
    assert.equal(calls, 2);
  }
});

test('decoder failures propagate without returning partial coverage or starting later passes', async () => {
  for (const asynchronous of [false, true]) {
    const failure = new Error('Decoder failed');
    let calls = 0;
    await assert.rejects(
      sample(['c1'], [pass('c1'), pass('c1'), pass('c1')], bounds, () => {
        calls++;
        if (calls === 1) return [1];
        if (asynchronous) return Promise.reject(failure);
        throw failure;
      }),
      (error) => error === failure,
    );
    assert.equal(calls, 2);
  }
});

const gradientPass = (startSlotId = 'start', endSlotId = 'end') => ({
  slotId: null,
  svg: 'full',
  split: { startSlotId, endSlotId, startSvg: 'start' },
});

test('sampling decodes full alpha and weighted stop mass as one paint with shared backdrop occlusion', async () => {
  const masks = { back: [1, 1], full: [0.5, 0.5], start: [0.375, 0.125] };
  const inputs = [pass('back', 'back'), gradientPass()];
  const before = JSON.stringify(inputs);
  const result = await sample(
    ['back', 'start', 'end'],
    inputs,
    { width: 2, height: 1 },
    (svg) => masks[svg],
  );
  assert.deepEqual(
    result.slots.map((s) => s.percent),
    [50, 25, 25],
  );
  assert.equal(result.paintedPixelEquivalent, 2);
  assert.equal(JSON.stringify(inputs), before);
});

test('auxiliary stop masks count toward both aggregate SVG and alpha storage budgets', async () => {
  let calls = 0;
  const rasterize = () => {
    calls++;
    return [];
  };
  await assert.rejects(
    sample(
      ['start', 'end'],
      Array.from({ length: 33 }, () => gradientPass()),
      { width: 256, height: 256 },
      rasterize,
    ),
    /sampling budget/,
  );
  const large = 'x'.repeat(4 * 1024 * 1024);
  const input = {
    ...gradientPass(),
    svg: large,
    split: { ...gradientPass().split, startSvg: large },
  };
  await assert.rejects(
    sample(['start', 'end'], [input, input, input], bounds, rasterize),
    /SVG budget/,
  );
  assert.equal(calls, 0);
});

test('stop inputs are snapshotted and borrowed full/stop masks copied before reuse', async () => {
  const pending = deferred();
  const input = gradientPass();
  const shared = new Float32Array([0.5]);
  const seen = [];
  const measured = sample(['start', 'end'], [input, pass('end', 'front')], bounds, (svg) => {
    seen.push(svg);
    if (seen.length === 1) return pending.promise;
    shared[0] = seen.length === 2 ? 0.125 : 0;
    return shared;
  });
  input.split.startSlotId = 'end';
  input.split.startSvg = 'changed';
  pending.resolve(shared);
  const result = await measured;
  assert.deepEqual(seen, ['full', 'start', 'front']);
  assert.deepEqual(
    result.slots.map((s) => s.percent),
    [25, 75],
  );
});

test('one-quantum stop overshoot is clamped to actual paint alpha, gross mismatch rejects', async () => {
  const quantized = await sample(['start', 'end'], [gradientPass()], bounds, (svg) => [
    svg === 'full' ? 0.5 : 0.5 + 1 / 255,
  ]);
  assert.equal(quantized.slots[0].percent, 100);
  assert.equal(quantized.paintedPixelEquivalent, 0.5);
  await assert.rejects(
    sample(['start', 'end'], [gradientPass()], bounds, (svg) => [svg === 'full' ? 0.5 : 0.9]),
    /exceeds paint alpha/,
  );
});

test('malformed and stale auxiliary masks stop the entire measurement', async () => {
  for (const split of [
    null,
    [],
    {},
    { startSlotId: 7, endSlotId: 'end', startSvg: 'start' },
    { startSlotId: 'start', endSlotId: 'invalid id', startSvg: 'start' },
  ]) {
    await assert.rejects(
      sample(['start'], [{ slotId: null, svg: 'full', split }], bounds, () =>
        assert.fail('Must preflight'),
      ),
    );
  }
  await assert.rejects(
    sample(['start', 'end'], [gradientPass()], bounds, (svg) => (svg === 'full' ? [1] : [NaN])),
  );
  const pending = deferred();
  let current = true,
    calls = 0;
  const result = sample(
    ['start', 'end'],
    [gradientPass(), pass('end')],
    bounds,
    () => (++calls === 1 ? [1] : pending.promise),
    () => current,
  );
  await Promise.resolve();
  await Promise.resolve();
  current = false;
  pending.resolve([0.5]);
  await assert.rejects(result, /stale/);
  assert.ok(calls <= 2);
});
