// Editable production example; all geometry/paint comes from Nect's model/export.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import {
  blankDocument,
  node,
  point,
  connect,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';

export function fireworksRing(alternate = false) {
  const doc = blankDocument();
  Object.assign(doc, {
    schemaVersion: 47,
    name: `花火の輪 — ${alternate ? 'Alternate' : 'Baseline'}`,
    width: 600,
    height: 600,
  });
  const layer = doc.layers[0];
  Object.assign(layer, { id: 'fireworks-layer', name: '花火の輪', allowEmptyOutput: false });
  const source = node('Curve', 40, 60),
    repeat = node('Geometry Repeater', 350, 60),
    fill = node('Fill', 660, 60),
    stroke = node('Stroke', 970, 60),
    out = node('Output', 1280, 60);
  [source, repeat, fill, stroke, out].forEach((n, i) => {
    n.id = [
      'fireworks-diamond',
      'fireworks-repeater',
      'fireworks-fill',
      'fireworks-stroke',
      'fireworks-output',
    ][i];
  });
  source.name = 'Diamond 12 × 28';
  source.params.closed.value = true;
  source.points = [
    [300, 166],
    [306, 180],
    [300, 194],
    [294, 180],
  ].map(([x, y], i) => {
    const p = point(x, y);
    p.id = `fireworks-vertex-${i}`;
    p.handleType = 'corner';
    return p;
  });
  repeat.name = '24 copies · 15° · radius 120';
  repeat.operation = 'ae-2d';
  for (const [key, value] of Object.entries({
    count: 24,
    offset: 0,
    anchor: [300, 180],
    position: [120 * Math.sin(Math.PI / 12), 120 * (1 - Math.cos(Math.PI / 12))],
    scale: [100, 100],
    rotation: 15,
    startOpacity: 100,
    endOpacity: 100,
    copiesAbove: true,
    alternateScale: alternate ? 125 : 100,
    alternateFill: alternate,
    alternateStroke: alternate,
    alternateColor: { hex: '#E85D3F', alpha: 1 },
    alternateStrokeColor: { hex: '#18324A', alpha: 1 },
  }))
    repeat.params[key].value = value;
  fill.params.color.value = { hex: '#F5D76E', alpha: 1 };
  stroke.params.color.value = { hex: '#39234A', alpha: 1 };
  stroke.params.width.value = 1.5;
  layer.nodes = [source, repeat, fill, stroke, out];
  layer.output = out.id;
  for (const [a, b] of [
    [source, repeat],
    [repeat, fill],
    [fill, stroke],
    [stroke, out],
  ])
    connect(layer, a.id, b.id, 'path');
  return { doc, layer, source, repeat, fill, stroke, out };
}

const sha = (bytes) => createHash('sha256').update(bytes).digest('hex');
const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-8, `${a} != ${b}`);
export function verifyFireworksRing() {
  const baseline = fireworksRing(),
    variant = fireworksRing(true),
    paths = [];
  let maxRadiusError = 0,
    maxCenterDelta = 0;
  for (const f of [baseline, variant]) {
    assert.equal(f.layer.nodes.filter((n) => n.type === 'Curve').length, 1);
    assert.deepEqual(deserialize(serialize(f.doc)), f.doc);
    const evaluation = evaluate(f.layer);
    assert.equal(evaluation.error, null);
    const value = evaluation.path;
    assert.equal(value.contours.length, 24);
    assert.equal(new Set(value.contours.map((c) => c.group)).size, 24);
    const svg = exportSVG(f.doc),
      elements = [...svg.matchAll(/<g opacity="([^"]+)"><path([^>]+)\/><\/g>/g)];
    assert.equal(elements.length, 24);
    value.contours.forEach((c, i) => {
      const theta = (i * Math.PI) / 12,
        cos = Math.cos(theta),
        sin = Math.sin(theta);
      const expectedCenter = [300 + 120 * sin, 300 - 120 * cos];
      const center = [0, 1].map((axis) => c.points.reduce((s, p) => s + p.position[axis], 0) / 4);
      center.forEach((v, axis) => near(v, expectedCenter[axis]));
      maxRadiusError = Math.max(
        maxRadiusError,
        Math.abs(Math.hypot(center[0] - 300, center[1] - 300) - 120),
      );
      assert.equal(c.closed, true);
      assert.equal(c.repeaterOpacity, 1);
      const odd = f === variant && i % 2 === 1,
        scale = odd ? 1.25 : 1;
      [
        [0, -14],
        [6, 0],
        [0, 14],
        [-6, 0],
      ].forEach(([x, y], j) => {
        near(c.points[j].position[0], center[0] + (x * cos - y * sin) * scale);
        near(c.points[j].position[1], center[1] + (x * sin + y * cos) * scale);
        assert.deepEqual(c.points[j].handleA, [0, 0]);
        assert.deepEqual(c.points[j].handleB, [0, 0]);
      });
      assert.deepEqual(c.repeaterFill, odd ? { hex: '#e85d3f', alpha: 1 } : undefined);
      assert.deepEqual(c.repeaterStroke, odd ? { hex: '#18324a', alpha: 1 } : undefined);
      assert.equal(elements[i][1], '1');
      for (const attribute of [
        `fill="${odd ? '#E85D3F' : '#F5D76E'}"`,
        `stroke="${odd ? '#18324A' : '#39234A'}"`,
        'stroke-width="1.5"',
        'fill-opacity="1"',
        'stroke-opacity="1"',
      ])
        assert.ok(elements[i][2].toLowerCase().includes(attribute.toLowerCase()), attribute);
    });
    paths.push(value);
    assert.equal(exportSVG(deserialize(serialize(f.doc))), svg);
  }
  paths[0].contours.forEach((c, i) => {
    assert.equal(c.id, paths[1].contours[i].id);
    c.points.forEach((p, j) => assert.equal(p.id, paths[1].contours[i].points[j].id));
    for (let axis = 0; axis < 2; axis++) {
      const a = c.points.reduce((s, p) => s + p.position[axis], 0) / 4;
      const b = paths[1].contours[i].points.reduce((s, p) => s + p.position[axis], 0) / 4;
      maxCenterDelta = Math.max(maxCenterDelta, Math.abs(a - b));
      near(a, b);
    }
  });
  assert.deepEqual(baseline.source, variant.source);
  const session = new CommandSession(variant.doc),
    before = serialize(session.snapshot());
  session.dispatch({
    ...session.state,
    type: 'translate-points',
    payload: {
      layerId: variant.layer.id,
      nodeId: variant.source.id,
      pointIds: [variant.source.points[0].id],
      delta: [0, -2],
    },
  });
  const changed = evaluate(session.snapshot().layers[0]).path;
  changed.contours.forEach((c, i) => {
    const a = paths[1].contours[i].points[0].position,
      b = c.points[0].position;
    near(Math.hypot(a[0] - b[0], a[1] - b[1]), i % 2 ? 2.5 : 2);
    c.points.slice(1).forEach((p, j) => assert.deepEqual(p, paths[1].contours[i].points[j + 1]));
  });
  assert.equal(session.historyView.past.length, 1);
  session.dispatch({ ...session.state, type: 'undo', payload: {} });
  assert.equal(serialize(session.snapshot()), before);
  return {
    baseline,
    variant,
    receipt: {
      canvas: [600, 600],
      copies: 24,
      stepDegrees: 15,
      center: [300, 300],
      radius: 120,
      sourceSize: [12, 28],
      oddSize: [15, 35],
      strokeWidth: 1.5,
      maxRadiusError,
      maxCenterDelta,
      sourcePointEditPropagatedToCopies: 24,
      sourceUndoExact: true,
      JSONAndSVGReloadExact: true,
      stableDerivedIDs: true,
    },
  };
}

export async function writeFireworksRing(output) {
  // Refuse every existing directory/file; never overwrite an earlier artwork.
  await fs.mkdir(output);
  const result = verifyFireworksRing(),
    files = [];
  for (const [name, f] of [
    ['baseline', result.baseline],
    ['variant', result.variant],
  ])
    for (const [extension, contents] of [
      ['json', serialize(f.doc)],
      ['svg', exportSVG(f.doc)],
    ]) {
      const file = `fireworks-ring-${name}.${extension}`;
      await fs.writeFile(path.join(output, file), contents, { flag: 'wx' });
      files.push({ file, bytes: Buffer.byteLength(contents), SHA256: sha(contents) });
    }
  await fs.writeFile(
    path.join(output, 'model-receipt.json'),
    JSON.stringify({ ...result.receipt, files }, null, 2),
    { flag: 'wx' },
  );
  return { ...result.receipt, files };
}
if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) {
  const output = process.argv[2];
  if (!output) throw Error('Pass a new, absolute output directory');
  assert.ok(path.isAbsolute(output));
  console.log(JSON.stringify(await writeFireworksRing(output), null, 2));
}
