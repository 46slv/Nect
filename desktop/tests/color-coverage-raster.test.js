import test from 'node:test';
import assert from 'node:assert/strict';
import { rasterizeCoverageAlpha as rasterize } from '../src/color-coverage-raster.js';

function fakeBrowser(t, options = {}) {
  const state = {
    blobs: [],
    revoked: [],
    images: [],
    canvases: [],
    draws: [],
    reads: [],
    timers: [],
    cleared: [],
  };
  const failure = new Error('Injected browser failure');
  const replaceGlobal = (name, value) => {
    const original = Object.getOwnPropertyDescriptor(globalThis, name);
    Object.defineProperty(globalThis, name, { configurable: true, writable: true, value });
    t.after(() => {
      if (original) Object.defineProperty(globalThis, name, original);
      else delete globalThis[name];
    });
  };
  if (options.float16) {
    // Contract-test surrogate only; real precision is proved by Chrome receipts.
    class Float16Array extends Float32Array {
      static BYTES_PER_ELEMENT = 2;
      get BYTES_PER_ELEMENT() {
        return 2;
      }
    }
    replaceGlobal('Float16Array', Float16Array);
  }
  t.mock.method(URL, 'createObjectURL', (blob) => {
    state.blobs.push(blob);
    return 'blob:coverage-test';
  });
  t.mock.method(URL, 'revokeObjectURL', (url) => state.revoked.push(url));
  replaceGlobal('setTimeout', (callback, delay) => {
    const token = { callback, delay };
    state.timers.push(token);
    return token;
  });
  replaceGlobal('clearTimeout', (token) => state.cleared.push(token));
  replaceGlobal(
    'Image',
    class {
      constructor() {
        if (options.failAt === 'image-constructor') throw failure;
        state.images.push(this);
      }
      set src(value) {
        this.source = value;
        if (value === '') {
          if (options.failAt === 'image-cleanup') throw failure;
          return;
        }
        if (options.failAt === 'image-src') throw failure;
        queueMicrotask(() => {
          if (options.decode === 'timeout') state.timers[0].callback();
          else if (options.decode === 'error') this.onerror?.();
          else this.onload?.();
        });
      }
    },
  );
  replaceGlobal('document', {
    createElement(tag) {
      assert.equal(tag, 'canvas');
      if (options.failAt === 'create-canvas') throw failure;
      const canvas = {
        getContext(type, config) {
          assert.equal(type, '2d');
          assert.deepEqual(config, {
            willReadFrequently: true,
            ...(options.float16 ? { colorType: 'float16', colorSpace: 'srgb' } : {}),
          });
          if (options.failAt === 'get-context') throw failure;
          if (options.failAt === 'missing-context') return null;
          return {
            getContextAttributes: options.noAttributes
              ? undefined
              : () => options.attributes ?? { colorType: 'float16', colorSpace: 'srgb' },
            drawImage(...args) {
              state.draws.push(args);
              if (options.failAt === 'draw') throw failure;
            },
            getImageData(...args) {
              state.reads.push(args);
              if (options.failAt === 'read') throw failure;
              if (options.float16) {
                assert.deepEqual(args[4], { colorSpace: 'srgb', pixelFormat: 'rgba-float16' });
                return {
                  pixelFormat: 'rgba-float16',
                  colorSpace: 'srgb',
                  data: new globalThis.Float16Array(options.pixels ?? [0.5, 0.5, 0.5, 0.5]),
                  ...options.readback,
                };
              }
              return { data: options.pixels ?? new Uint8ClampedArray([123, 45, 67, 255]) };
            },
          };
        },
      };
      state.canvases.push(canvas);
      return canvas;
    },
  });
  return { state, failure };
}

function assertReleased(state) {
  assert.deepEqual(state.revoked, ['blob:coverage-test']);
  assert.equal(state.images.length, 1);
  assert.equal(state.images[0].source, '');
  assert.equal(state.images[0].onload, null);
  assert.equal(state.images[0].onerror, null);
  assert.deepEqual(state.cleared, state.timers);
}

test('RGBA extraction normalizes only alpha and cleans up decoded image resources', async (t) => {
  const svg = '<svg xmlns="http://www.w3.org/2000/svg"/>';
  const pixels = new Uint8ClampedArray([255, 0, 1, 0, 0, 255, 2, 128, 4, 5, 6, 255]);
  const { state } = fakeBrowser(t, { pixels });
  const alpha = await rasterize(svg, { width: 3, height: 1 });
  assert.ok(alpha instanceof Float32Array);
  assert.deepEqual([...alpha], [0, Math.fround(128 / 255), 1]);
  pixels[7] = 0;
  assert.equal(alpha[1], Math.fround(128 / 255), 'Result owns its alpha storage');
  assert.equal(state.blobs.length, 1);
  assert.equal(state.blobs[0].type, 'image/svg+xml');
  assert.equal(await state.blobs[0].text(), svg);
  assert.equal(state.canvases[0].width, 3);
  assert.equal(state.canvases[0].height, 1);
  assert.deepEqual(state.draws, [[state.images[0], 0, 0, 3, 1]]);
  assert.deepEqual(state.reads, [[0, 0, 3, 1]]);
  assert.equal(state.timers[0].delay, 10000);
  assertReleased(state);
});

test('invalid dimensions and SVGs reject before URL, image, or canvas allocation', async (t) => {
  const { state } = fakeBrowser(t);
  for (const value of [0, -1, 0.5, 257, NaN, Infinity, '1', undefined]) {
    await assert.rejects(rasterize('<svg/>', { width: value, height: 1 }), /raster request/);
    await assert.rejects(rasterize('<svg/>', { width: 1, height: value }), /raster request/);
  }
  for (const svg of [null, undefined, {}, 1, 'x'.repeat(4 * 1024 * 1024 + 1)])
    await assert.rejects(rasterize(svg, { width: 1, height: 1 }), /raster request/);
  await assert.rejects(rasterize('<svg/>', null));
  assert.deepEqual(state.blobs, []);
  assert.deepEqual(state.images, []);
  assert.deepEqual(state.canvases, []);
  assert.deepEqual(state.timers, []);
});

test('decode errors and timeout reject and release resources without a real timer wait', async (t) => {
  for (const decode of ['error', 'timeout']) {
    await t.test(decode, async (t) => {
      const { state } = fakeBrowser(t, { decode });
      await assert.rejects(
        rasterize('<svg/>', { width: 1, height: 1 }),
        decode === 'error' ? /could not be decoded/ : /timed out/,
      );
      assert.deepEqual(state.canvases, []);
      assertReleased(state);
    });
  }
});

test('canvas creation, context, draw, read, and image source failures release resources', async (t) => {
  for (const failAt of [
    'create-canvas',
    'get-context',
    'missing-context',
    'draw',
    'read',
    'image-src',
  ]) {
    await t.test(failAt, async (t) => {
      const { state, failure } = fakeBrowser(t, { failAt });
      await assert.rejects(rasterize('<svg/>', { width: 1, height: 1 }), (error) =>
        failAt === 'missing-context'
          ? /canvas is unavailable/.test(error.message)
          : error === failure,
      );
      assertReleased(state);
    });
  }
});

test('image construction failure cannot allocate or leak a blob URL', async (t) => {
  const { state, failure } = fakeBrowser(t, { failAt: 'image-constructor' });
  await assert.rejects(rasterize('<svg/>', { width: 1, height: 1 }), (error) => error === failure);
  assert.deepEqual(state.blobs, []);
  assert.deepEqual(state.images, []);
  assert.deepEqual(state.canvases, []);
  assert.deepEqual(state.revoked, []);
});

test('image source cleanup failure cannot prevent URL revocation', async (t) => {
  const { state, failure } = fakeBrowser(t, { failAt: 'image-cleanup' });
  await assert.rejects(rasterize('<svg/>', { width: 1, height: 1 }), (error) => error === failure);
  assertReleased(state);
});

test('contribution readback uses premultiplied red once, preserving transparent and antialiased pixels', async (t) => {
  const { state } = fakeBrowser(t, {
    float16: true,
    pixels: [1, 0, 0, 0, 0.5, 0.5, 0.5, 0.5, 1, 1, 1, 0.25, 0, 0, 0, 1],
  });
  const mass = await rasterize('<svg/>', { width: 4, height: 1, channel: 'premultiplied-red' });
  assert.deepEqual([...mass], [0, 0.25, 0.25, 0]);
  assertReleased(state);
});

test('unsupported contribution channels reject before browser resource allocation', async (t) => {
  const { state } = fakeBrowser(t);
  for (const channel of [null, 'red', 'luminance', 0, {}])
    await assert.rejects(rasterize('<svg/>', { width: 1, height: 1, channel }), /raster request/);
  assert.deepEqual(state.blobs, []);
  assert.deepEqual(state.images, []);
});

test('basis decode verifies actual float16 context/readback and records precision evidence', async (t) => {
  const { state } = fakeBrowser(t, { float16: true, pixels: [0.5, 0, 0, 1 / 255] });
  const result = await rasterize('<svg/>', { width: 1, height: 1, channel: 'premultiplied-red' });
  assert.equal(result[0], Math.fround(Math.fround(1 / 255) * 0.5));
  assert.deepEqual(result.coveragePrecision, {
    contextColorType: 'float16',
    contextColorSpace: 'srgb',
    pixelFormat: 'rgba-float16',
    readbackColorSpace: 'srgb',
    arrayType: 'Float16Array',
    bytesPerElement: 2,
  });
  assert.equal(Object.isFrozen(result.coveragePrecision), true);
  assertReleased(state);
});

test('unsupported actual precision never falls back or converts unorm8 and always cleans up', async (t) => {
  for (const options of [
    { noAttributes: true },
    { attributes: { colorType: 'unorm8', colorSpace: 'srgb' } },
    { attributes: { colorType: 'float16', colorSpace: 'display-p3' } },
    { readback: { pixelFormat: 'rgba-unorm8' } },
    { readback: { colorSpace: 'display-p3' } },
    { readback: { data: new Uint8ClampedArray([128, 0, 0, 1]) } },
    { readback: { data: new Float32Array([0.5, 0, 0, 0.5]) } },
    { pixels: [NaN, 0, 0, 0.5] },
    { pixels: [1, 0, 0, Infinity] },
  ])
    await t.test(JSON.stringify(options), async (t) => {
      const { state } = fakeBrowser(t, { float16: true, ...options });
      await assert.rejects(
        rasterize('<svg/>', { width: 1, height: 1, channel: 'premultiplied-red' }),
        /float16|readback/,
      );
      assertReleased(state);
    });
});

test('float16 draw/read rejection propagates while releasing images and URLs', async (t) => {
  for (const failAt of ['draw', 'read'])
    await t.test(failAt, async (t) => {
      const { state, failure } = fakeBrowser(t, { float16: true, failAt });
      await assert.rejects(
        rasterize('<svg/>', { width: 1, height: 1, channel: 'premultiplied-red' }),
        (error) => error === failure,
      );
      assertReleased(state);
    });
});
