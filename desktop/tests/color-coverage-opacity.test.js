import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, connect, exportScene } from '../src/core.js';
import { colorCoveragePasses } from '../src/color-coverage-passes.js';
import { opacityCoverageDocument } from './helpers/opacity-coverage-fixture.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    repeater = node('Geometry Repeater'),
    fill = node('Fill');
  doc.width = 64;
  doc.height = 48;
  doc.schemaVersion = 37;
  shape.params.center.value = [24, 24];
  shape.params.size.value = [32, 24];
  repeater.operation = 'ae-2d';
  repeater.params.count.value = 2;
  repeater.params.position.value = [16, 0];
  repeater.params.startOpacity.value = 25;
  repeater.params.endOpacity.value = 75;
  layer.nodes = [shape, repeater, fill, output];
  connect(layer, shape.id, repeater.id, 'path');
  connect(layer, repeater.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, scene: exportScene(doc) };
}

test('solid fill-only repeater groups retain their production opacity without mutation', () => {
  const { doc, scene } = fixture(),
    before = JSON.stringify(scene),
    actual = colorCoveragePasses(scene, doc);
  assert.equal(actual.length, 2);
  assert.deepEqual(
    scene[0].value.contours.map((contour) => contour.repeaterOpacity),
    [0.25, 0.75],
  );
  assert.match(actual[0].svg, /<g opacity="0\.25">/);
  assert.match(actual[1].svg, /<g opacity="0\.75">/);
  assert.equal(JSON.stringify(scene), before);
});

test('partial group opacity refuses stroke, alternate paint, invalid or ungrouped metadata', () => {
  const { doc, scene } = fixture();
  for (const mutate of [
    (value) => (value.stroke = { hex: '#000000', alpha: 1 }),
    (value) => (value.contours[0].repeaterFill = { hex: '#000000', alpha: 1 }),
    (value) => (value.contours[0].repeaterOpacity = -0.1),
    (value) => (value.contours[0].repeaterOpacity = NaN),
    (value) => value.contours.forEach((contour) => delete contour.group),
    (value) => value.contours.push({ ...structuredClone(value.contours[0]), repeaterOpacity: 0.5 }),
  ]) {
    const invalid = structuredClone(scene);
    mutate(invalid[0].value);
    assert.throws(() => colorCoveragePasses(invalid, doc), /coverage|attribution/i);
  }
});

test('equal color literals retain distinct slot IDs with partial opacity', () => {
  const doc = opacityCoverageDocument();
  doc.colorSlots[1].value.hex = doc.colorSlots[0].value.hex;
  const passes = colorCoveragePasses(exportScene(doc), doc);
  assert.deepEqual(new Set(passes.map((pass) => pass.slotId)), new Set(['red', 'blue']));
});
