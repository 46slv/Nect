import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluateImageSampleLuminanceV1 } from '../src/image-sample-luminance-v1.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { encodeImage } from '../src/images.js';
import { gridSamplesFixtureV1 } from './fixtures/grid-samples-v1.js';

const descriptor = (role = 'luminance') => ({
  kind: 'ImageLuminance',
  version: 1,
  valueType: 'Scalar',
  role,
  unit: null,
  sourceId: 'image-a',
  sourceRevision: 'revision-a',
  sampling: 'cell-average',
  edge: 'transparent',
  alpha: 'composite-over-white',
  transform: { fromFrame: 'sample-frame', to: 'image-pixel-v1', matrix: [1, 0, 0, 1, 0, 0] },
});

function request(width, height, rgba, centers, bases, maxPixelVisits = 100) {
  return {
    domain: 'Sample',
    count: centers.length,
    maxSamples: 4096,
    maxPixelVisits,
    image: { sourceId: 'image-a', sourceRevision: 'revision-a', width, height, rgba },
    footprint: { frameToken: 'sample-frame', centers, bases },
  };
}

const close = (actual, expected) => assert.ok(Math.abs(actual - expected) < 1e-12);

test('real PNG decode and explicit Grid footprint average black, white, black to one third', () => {
  const pixels = new Uint8Array([0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255]),
    asset = {
      mime: 'image/png',
      width: 3,
      height: 1,
      data: encodeImage(encodeRgbaPng(3, 1, pixels)),
    },
    grid = gridSamplesFixtureV1({
      namespaceId: 'actual-png-grid',
      frameToken: 'sample-frame',
      origin: [1.5, 0.5],
      spacing: [3, 1],
      angle: { type: 'Angle', unit: 'degree', value: 0 },
      bounds: { minI: 0, maxI: 1, minJ: 0, maxJ: 1 },
    }),
    data = request(
      asset.width,
      asset.height,
      copyImageRgba(asset),
      grid.samples.map((sample) => sample.position),
      grid.samples.map(() => [grid.grid.basisX, grid.grid.basisY]),
    ),
    before = structuredClone(data),
    result = evaluateImageSampleLuminanceV1(descriptor(), data);
  assert.deepEqual(
    { ...result, values: [] },
    {
      kind: 'Varying',
      version: 1,
      valueType: 'Scalar',
      role: 'luminance',
      unit: null,
      count: 1,
      values: [],
    },
  );
  close(result.values[0], 1 / 3);
  assert.deepEqual(data, before);
  result.values[0] = 10;
  assert.deepEqual(data, before);
});

test('linear premultiplied integration ignores hidden RGB and composites outside over white', () => {
  const transparentRedAndBlue = new Uint8Array([255, 0, 0, 0, 0, 0, 255, 255]),
    data = request(
      2,
      1,
      transparentRedAndBlue,
      [[1, 0.5]],
      [
        [
          [2, 0],
          [0, 1],
        ],
      ],
    );
  close(evaluateImageSampleLuminanceV1(descriptor(), data).values[0], 0.5361);
  close(evaluateImageSampleLuminanceV1(descriptor('darkness'), data).values[0], 0.4639);
  const black = new Uint8Array([0, 0, 0, 255]),
    partlyOutside = request(
      1,
      1,
      black,
      [[0, 0.5]],
      [
        [
          [2, 0],
          [0, 1],
        ],
      ],
    );
  close(evaluateImageSampleLuminanceV1(descriptor(), partlyOutside).values[0], 0.5);
  const outside = request(
    1,
    1,
    black,
    [[-3, 0.5]],
    [
      [
        [1, 0],
        [0, 1],
      ],
    ],
    0,
  );
  assert.equal(evaluateImageSampleLuminanceV1(descriptor(), outside).values[0], 1);
});

test('explicit affine supports nonuniform scale, axis swap and negative orientation', () => {
  const pixels = new Uint8Array([0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255]),
    scaled = request(
      3,
      1,
      pixels,
      [[0.75, 0.5]],
      [
        [
          [1.5, 0],
          [0, 1],
        ],
      ],
    ),
    scaleField = descriptor();
  scaleField.transform.matrix = [2, 0, 0, -1, 0, 1];
  close(evaluateImageSampleLuminanceV1(scaleField, scaled).values[0], 1 / 3);
  const swapped = request(
      3,
      1,
      pixels,
      [[0.5, 1.5]],
      [
        [
          [1, 0],
          [0, 3],
        ],
      ],
    ),
    swapField = descriptor();
  swapField.transform.matrix = [0, 1, 1, 0, 0, 0];
  close(evaluateImageSampleLuminanceV1(swapField, swapped).values[0], 1 / 3);
});

test('fractional pixel intersections use the full footprint rather than its center', () => {
  const pixels = new Uint8Array([0, 0, 0, 255, 255, 255, 255, 255]),
    data = request(
      2,
      1,
      pixels,
      [[1.125, 0.5]],
      [
        [
          [1.25, 0],
          [0, 1],
        ],
      ],
    );
  close(evaluateImageSampleLuminanceV1(descriptor(), data).values[0], 0.6);
});

test('population order is caller-owned and revisions cannot silently bind another image', () => {
  const pixels = new Uint8Array([0, 0, 0, 255, 255, 255, 255, 255]),
    data = request(
      2,
      1,
      pixels,
      [
        [1.5, 0.5],
        [0.5, 0.5],
      ],
      [
        [
          [1, 0],
          [0, 1],
        ],
        [
          [1, 0],
          [0, 1],
        ],
      ],
    );
  assert.deepEqual(evaluateImageSampleLuminanceV1(descriptor(), data).values, [1, 0]);
  data.image.sourceRevision = 'changed';
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), data), /matching bounded image/);
  const updated = descriptor();
  updated.sourceRevision = 'changed';
  pixels.set([255, 255, 255, 255], 0);
  assert.deepEqual(evaluateImageSampleLuminanceV1(updated, data).values, [1, 1]);
});

test('empty populations still require a matching source, frame and full capability', () => {
  const data = request(1, 1, new Uint8Array([0, 0, 0, 255]), [], [], 0),
    result = evaluateImageSampleLuminanceV1(descriptor(), data);
  assert.deepEqual(result.values, []);
  assert.equal(result.count, 0);
  assert.throws(
    () => evaluateImageSampleLuminanceV1(descriptor(), { ...data, footprint: null }),
    /frame/,
  );
  assert.throws(
    () => evaluateImageSampleLuminanceV1(descriptor(), { ...data, image: null }),
    /image source/,
  );
  const field = descriptor();
  field.transform.fromFrame = 'other';
  assert.throws(() => evaluateImageSampleLuminanceV1(field, data), /frame/);
});

test('unsupported modes, rotation, shear and invalid descriptors are rejected', () => {
  const data = request(
    1,
    1,
    new Uint8Array([0, 0, 0, 255]),
    [[0.5, 0.5]],
    [
      [
        [1, 0],
        [0, 1],
      ],
    ],
  );
  for (const changed of [
    { sampling: 'nearest' },
    { edge: 'clamp' },
    { alpha: 'ignore' },
    { role: 'Color' },
    { sourceId: '' },
    { transform: { fromFrame: 'sample-frame', to: 'other', matrix: [1, 0, 0, 1, 0, 0] } },
    {
      transform: { fromFrame: 'sample-frame', to: 'image-pixel-v1', matrix: [1, 0.1, 0, 1, 0, 0] },
    },
    { transform: { fromFrame: 'sample-frame', to: 'image-pixel-v1', matrix: [1, 0, 1, 1, 0, 0] } },
  ]) {
    assert.throws(() => evaluateImageSampleLuminanceV1({ ...descriptor(), ...changed }, data));
  }
  const badCenters = { ...data, footprint: { ...data.footprint, centers: [[Infinity, 0]] } };
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), badCenters), /finite/);
  const badBasis = {
    ...data,
    footprint: {
      ...data.footprint,
      bases: [
        [
          [0, 0],
          [0, 1],
        ],
      ],
    },
  };
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), badBasis), /axis-aligned/);
});

test('source bytes and all footprint work are bounded before any result', () => {
  const data = request(
      2,
      1,
      new Uint8Array(8),
      [[1, 0.5]],
      [
        [
          [2, 0],
          [0, 1],
        ],
      ],
      1,
    ),
    before = structuredClone(data);
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), data), /pixel-visit budget/);
  assert.deepEqual(data, before);
  for (const image of [
    { ...data.image, width: 4_000_001 },
    { ...data.image, rgba: new Uint8Array(7) },
    { ...data.image, rgba: new Uint8ClampedArray(8) },
  ])
    assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), { ...data, image }));
  if (typeof SharedArrayBuffer === 'function') {
    const image = { ...data.image, rgba: new Uint8Array(new SharedArrayBuffer(8)) };
    assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), { ...data, image }), /RGBA8/);
  }
  assert.throws(
    () => evaluateImageSampleLuminanceV1(descriptor(), { ...data, maxPixelVisits: 16_000_001 }),
    /budget/,
  );
});

test('data accessors reject without executing; unrepresentable transformed geometry rejects', () => {
  const data = request(
    1,
    1,
    new Uint8Array([0, 0, 0, 255]),
    [[0.5, 0.5]],
    [
      [
        [1, 0],
        [0, 1],
      ],
    ],
  );
  let calls = 0;
  const image = { ...data.image };
  Object.defineProperty(image, 'rgba', {
    enumerable: true,
    get() {
      calls++;
      return data.image.rgba;
    },
  });
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), { ...data, image }));
  assert.equal(calls, 0);
  const far = { ...data, footprint: { ...data.footprint, centers: [[1e308, 0]] } };
  assert.throws(() => evaluateImageSampleLuminanceV1(descriptor(), far), /representable/);
});
