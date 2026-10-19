import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { deflate } from 'pako';
import {
  CommandSession,
  History,
  connect,
  deserialize,
  document,
  evaluate,
  exportSVG,
  node,
  pathArea,
  pathBounds,
  serialize,
  setNodeBypassed,
  validate,
} from '../src/core.js';

const SIGNATURE = Uint8Array.from([137, 80, 78, 71, 13, 10, 26, 10]);
const modes = ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'];

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = crc & 1 ? 0xedb88320 ^ (crc >>> 1) : crc >>> 1;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function concat(...parts) {
  const result = new Uint8Array(parts.reduce((total, part) => total + part.length, 0));
  let offset = 0;
  for (const part of parts) {
    result.set(part, offset);
    offset += part.length;
  }
  return result;
}

function chunk(tag, data) {
  const tagBytes = new TextEncoder().encode(tag),
    body = concat(tagBytes, data),
    result = new Uint8Array(body.length + 8),
    view = new DataView(result.buffer);
  view.setUint32(0, data.length);
  result.set(body, 4);
  view.setUint32(4 + body.length, crc32(body));
  return result;
}

function pngAsset(width, height, rgba, { colorType = 6 } = {}) {
  const channels = colorType === 6 ? 4 : colorType === 2 ? 3 : 1,
    header = new Uint8Array(13),
    view = new DataView(header.buffer),
    rows = new Uint8Array(height * (width * channels + 1));
  view.setUint32(0, width);
  view.setUint32(4, height);
  header[8] = 8;
  header[9] = colorType;
  for (let y = 0; y < height; y++) {
    const stride = width * channels,
      start = y * (stride + 1);
    rows[start] = 0;
    if (colorType === 6) rows.set(rgba.subarray(y * stride, (y + 1) * stride), start + 1);
    else if (colorType === 2) rows.set(rgba.subarray(y * stride, (y + 1) * stride), start + 1);
    else rows.fill(0, start + 1, start + 1 + stride);
  }
  const bytes = concat(
    SIGNATURE,
    chunk('IHDR', header),
    chunk('IDAT', deflate(rows)),
    chunk('IEND', new Uint8Array()),
  );
  return {
    name: 'pixels.png',
    mime: 'image/png',
    width,
    height,
    data: Buffer.from(bytes).toString('base64'),
  };
}

function fixture({
  mode = 'Halftone',
  width = 1,
  height = 1,
  rgba = Uint8Array.from([0, 0, 0, 255]),
  position = [0, 0],
  size = [16, 16],
  pitch = 16,
  gridOrigin = [0, 0],
  connectImage = true,
  asset = pngAsset(width, height, rgba),
} = {}) {
  const doc = document(),
    layer = doc.layers[0],
    image = node('Image'),
    effect = node(mode),
    output = node('Output');
  doc.imageAssets = { 'image-test': asset };
  image.asset = 'image-test';
  image.params.position.value = position;
  image.params.size.value = size;
  effect.params.pitch.value = pitch;
  if (effect.params.dotSize) effect.params.dotSize.value = pitch;
  effect.params.gridOrigin.value = gridOrigin;
  output.params.path.source = effect.id;
  layer.nodes = [image, effect, output];
  layer.output = output.id;
  if (connectImage) connect(layer, image.id, effect.id, 'path');
  return { doc, layer, image, effect, output };
}

function evaluated(f) {
  const result = evaluate(f.layer, {}, f.doc.imageAssets);
  if (result.error) throw Error(result.error);
  return result.run(f.effect.id);
}

function area(path) {
  return path.contours.length ? pathArea(path) : 0;
}

test('Image-typed inputs connect to all luminance effects and promote the saved tone contract to schema 21', () => {
  for (const mode of modes) {
    const f = fixture({ mode, size: [16, 16] });
    validate(f.doc);
    assert.equal(f.doc.schemaVersion, mode === 'Line Screen' ? 24 : 36, mode);
    assert.equal(f.effect.params.path.source, f.image.id);
    assert.equal(area(evaluated(f)), 256, `${mode} black image geometry`);

    const oldVersion = structuredClone(f.doc);
    oldVersion.schemaVersion = 16;
    assert.equal(
      validate(oldVersion).schemaVersion,
      mode === 'Line Screen' ? 21 : 36,
      `${mode} legacy edge promotion`,
    );
    const reopened = deserialize(serialize(oldVersion));
    assert.equal(reopened.schemaVersion, mode === 'Line Screen' ? 21 : 36);
    assert.equal(
      area(evaluate(reopened.layers[0], {}, reopened.imageAssets).run(f.effect.id)),
      256,
    );
  }
});

test('Image stays rejected by unrelated fill, stroke, boolean, transform, remap, and vector output inputs', () => {
  const { layer, image } = fixture({ connectImage: false });
  for (const [type, key] of [
    ['Fill', 'path'],
    ['Stroke', 'path'],
    ['Boolean', 'a'],
    ['Transform', 'path'],
    ['Path Remap', 'path'],
    ['Convert to Path', 'path'],
    ['Output', 'path'],
  ]) {
    const target = node(type);
    layer.nodes.push(target);
    assert.throws(() => connect(layer, image.id, target.id, key), /Cannot connect Image to/);
  }

  const invalid = fixture({ connectImage: false });
  const invalidTarget = node('Fill');
  invalid.layer.nodes.push(invalidTarget);
  invalidTarget.params.path.source = invalid.image.id;
  assert.throws(() => deserialize(serialize(invalid.doc)), /Cannot connect Image to/);

  const invalidConversion = fixture({ connectImage: false }),
    converter = node('Convert to Path');
  invalidConversion.layer.nodes.push(converter);
  converter.params.path.source = invalidConversion.image.id;
  assert.throws(
    () => deserialize(serialize(invalidConversion.doc)),
    /Cannot connect Image to Path/,
  );
});

test('Image → Halftone → Convert to Path → Transform → Fill composes and saves as schema 21', () => {
  const f = fixture({ size: [16, 16], pitch: 8 }),
    converter = node('Convert to Path'),
    transform = node('Transform'),
    fill = node('Fill');
  f.layer.nodes.splice(f.layer.nodes.indexOf(f.output), 0, converter, transform, fill);
  connect(f.layer, f.effect.id, converter.id, 'path');
  connect(f.layer, converter.id, transform.id, 'path');
  transform.params.offset.value = [6, 4];
  connect(f.layer, transform.id, fill.id, 'path');
  fill.params.color.value = { hex: '#147c72', alpha: 1 };
  connect(f.layer, fill.id, f.output.id, 'path');

  validate(f.doc);
  assert.equal(f.doc.schemaVersion, 36);
  const result = evaluate(f.layer, {}, f.doc.imageAssets),
    marks = result.run(f.effect.id),
    converted = result.run(converter.id);
  assert.equal(result.error, null);
  assert.ok(marks.contours.length);
  assert.equal(Object.hasOwn(converted, 'fill'), false);
  assert.equal(Object.hasOwn(converted, 'stroke'), false);
  assert.deepEqual(converted.contours, marks.contours);
  assert.deepEqual(result.path.fill, fill.params.color.value);

  const reopened = deserialize(serialize(f.doc)),
    cold = evaluate(reopened.layers[0], {}, reopened.imageAssets);
  assert.equal(reopened.schemaVersion, 36);
  assert.equal(cold.error, null);
  assert.deepEqual(cold.path, result.path);
});

test('Image Output forwards its Image-typed descriptor to a luminance input', () => {
  const f = fixture({ connectImage: false }),
    forwarded = node('Image Output');
  forwarded.params.path.source = f.image.id;
  f.layer.nodes.splice(f.layer.nodes.indexOf(f.effect), 0, forwarded);
  connect(f.layer, forwarded.id, f.effect.id, 'path');
  validate(f.doc);
  assert.equal(f.doc.schemaVersion, 36);
  assert.equal(area(evaluated(f)), 256);
});

test('nonaligned opaque-black image marks cover and stay clipped to the placed rectangle', () => {
  for (const mode of modes) {
    const f = fixture({
      mode,
      position: [3, 5],
      size: [13, 19],
      pitch: 16,
    });
    const imageValue = evaluate(f.layer, {}, f.doc.imageAssets).run(f.image.id),
      result = evaluated(f);
    assert.deepEqual(imageValue.position, [3, 5]);
    assert.deepEqual(imageValue.size, [13, 19]);
    assert.ok(Math.abs(area(result) - 247) < 0.001, mode);
    assert.deepEqual(
      pathBounds(result).map((axis) => [axis[0], axis[2]]),
      [
        [3, 16],
        [5, 24],
      ],
    );
  }
});

test('scaling samples nearest texels; alpha over white changes tone without creating a contour mask', () => {
  const twoPixels = Uint8Array.from([0, 0, 0, 255, 255, 255, 255, 255]);
  for (const mode of modes) {
    const scaled = fixture({
        mode,
        width: 2,
        height: 1,
        rgba: twoPixels,
        size: [16, 8],
        pitch: 8,
      }),
      scaledOutput = evaluated(scaled);
    assert.ok(Math.abs(area(scaledOutput) - 64) < 0.001, `${mode} maps left black texel`);
    assert.deepEqual(
      pathBounds(scaledOutput).map((axis) => [axis[0], axis[2]]),
      [
        [0, 8],
        [0, 8],
      ],
    );

    const transparent = fixture({
      mode,
      rgba: Uint8Array.from([0, 0, 0, 0]),
      position: [3, 5],
      size: [13, 19],
      pitch: 16,
    });
    assert.equal(area(evaluated(transparent)), 0, `${mode} transparent black is white paper`);
    transparent.effect.params.invert.value = true;
    const inverted = evaluated(transparent);
    assert.ok(Math.abs(area(inverted) - 247) < 0.001, `${mode} alpha is not a contour mask`);
  }
});

test('same-ID same-size asset replacement, Undo/Redo and cold JSON reopen invalidate image geometry', () => {
  const f = fixture({ mode: 'Ordered Dither' }),
    history = new History(f.doc),
    sourceId = f.image.id,
    imageAssetId = f.image.asset,
    black = area(evaluate(history.doc.layers[0], {}, history.doc.imageAssets).run(f.effect.id));
  const whiteAsset = pngAsset(1, 1, Uint8Array.from([255, 255, 255, 255]));
  history.apply((doc) => {
    doc.imageAssets[imageAssetId] = { ...whiteAsset };
  });
  const white = area(evaluate(history.doc.layers[0], {}, history.doc.imageAssets).run(f.effect.id));
  assert.equal(black, 256);
  assert.equal(white, 0);
  assert.equal(
    history.doc.layers[0].nodes.find((item) => item.id === sourceId).asset,
    imageAssetId,
  );

  history.undo();
  assert.equal(
    area(evaluate(history.doc.layers[0], {}, history.doc.imageAssets).run(f.effect.id)),
    black,
  );
  history.redo();
  const reopened = deserialize(serialize(history.doc));
  assert.equal(reopened.schemaVersion, 36);
  assert.equal(
    area(evaluate(reopened.layers[0], {}, reopened.imageAssets).run(f.effect.id)),
    white,
  );
  const json = serialize(reopened);
  assert.doesNotMatch(json, /"(?:rgba|pixels|cachedRgba)"\s*:/i);
  assert.match(json, /"data":\s*"[A-Za-z0-9+/]+=*"/);
});

test('image bypass is disallowed and an already-bypassed effect cannot accept an Image edge', () => {
  for (const mode of modes) {
    const f = fixture({ mode, connectImage: false });
    f.doc.schemaVersion = 10;
    setNodeBypassed(f.layer, f.effect.id, true);
    assert.throws(
      () => connect(f.layer, f.image.id, f.effect.id, 'path'),
      /cannot be connected while bypassed/,
    );
    assert.equal(f.effect.params.path.source, undefined, 'connection rolls back atomically');
    f.effect.params.path.source = f.image.id;
    assert.throws(() => validate(f.doc), /cannot be bypassed as PaintedPath/);
  }
});

test('unreferenced unsupported PNGs are not decoded and image failures return no partial output', () => {
  const unused = pngAsset(1, 1, new Uint8Array(), { colorType: 3 }),
    f = fixture();
  f.doc.imageAssets['image-unused'] = unused;
  validate(f.doc);
  assert.equal(evaluate(f.layer, {}, f.doc.imageAssets).error, null);

  f.image.asset = 'image-unused';
  f.doc.imageAssets['image-unused'] = unused;
  const result = evaluate(f.layer, {}, f.doc.imageAssets);
  assert.match(result.error, /Unsupported PNG encoding/);
  assert.equal(result.path, null);
});

test('image luminance budget failures roll back parameter edits and browser SVG matches CLI SVG', async () => {
  const largePlaced = fixture({ mode: 'Halftone', size: [65, 65], pitch: 16 }),
    session = new CommandSession(largePlaced.doc),
    before = serialize(session.snapshot());
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: {
          layerId: largePlaced.layer.id,
          nodeId: largePlaced.effect.id,
          key: 'pitch',
          value: 1,
        },
      }),
    /4096-cell limit/,
  );
  assert.equal(serialize(session.snapshot()), before, 'rejected budget edit keeps prior document');

  const exportFixture = fixture({
    mode: 'Line Screen',
    size: [13, 19],
    position: [3, 5],
    pitch: 16,
  });
  validate(exportFixture.doc);
  const browserSvg = exportSVG(exportFixture.doc),
    dir = await fs.mkdtemp(path.join(os.tmpdir(), 'nect-image-luminance-'));
  try {
    const input = path.join(dir, 'input.json'),
      output = path.join(dir, 'output.svg');
    await fs.writeFile(input, serialize(exportFixture.doc));
    execFileSync(
      process.execPath,
      [fileURLToPath(new URL('../cli/export.js', import.meta.url)), input, output],
      {
        cwd: fileURLToPath(new URL('..', import.meta.url)),
      },
    );
    assert.equal(await fs.readFile(output, 'utf8'), browserSvg);
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
});

test('vector luminance output and default fingerprint stay on the unchanged vector path', () => {
  const doc = document(),
    layer = doc.layers[0],
    rectangle = node('Rectangle'),
    fill = node('Fill'),
    effect = node('Halftone'),
    output = node('Output');
  rectangle.id = 'vector-rect';
  fill.id = 'vector-fill';
  effect.id = 'vector-effect';
  output.id = 'vector-output';
  rectangle.params.center.value = [8, 8];
  rectangle.params.size.value = [16, 16];
  fill.params.path.source = rectangle.id;
  fill.params.color.value = { hex: '#000000', alpha: 1 };
  effect.params.path.source = fill.id;
  output.params.path.source = effect.id;
  layer.nodes = [rectangle, fill, effect, output];
  layer.output = output.id;
  const result = evaluate(layer).run(effect.id);
  assert.equal(area(result), 256);
  assert.equal(
    result.fingerprint,
    'sha256:85550975e61ac8b2b1fef571d2356555fdec3924cefa07f82ffeb4978322e3d0',
  );
  assert.equal(validate(doc).schemaVersion, 36);
});
