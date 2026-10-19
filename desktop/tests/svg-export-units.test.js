import assert from 'node:assert/strict';
import test from 'node:test';
import {
  displayLengthToDocumentUnits,
  svgRootAttributes,
  svgRootSizeAttributes,
} from '../src/artboard-units.js';

test('physical SVG sizes use explicit mm/cm units while viewBox keeps canonical px coordinates', () => {
  const doc = {
      width: displayLengthToDocumentUnits(100, 'mm'),
      height: displayLengthToDocumentUnits(50, 'mm'),
    },
    before = structuredClone(doc),
    mm = svgRootSizeAttributes(doc, 'mm'),
    cm = svgRootSizeAttributes(doc, 'cm');

  assert.deepEqual(mm, {
    width: '100mm',
    height: '50mm',
    viewBox: `0 0 ${doc.width} ${doc.height}`,
  });
  assert.equal(cm.width, '10cm');
  assert.equal(cm.height, '5cm');
  assert.equal(cm.viewBox, mm.viewBox);
  assert.ok(
    Math.abs(displayLengthToDocumentUnits(100, 'mm') - displayLengthToDocumentUnits(10, 'cm')) <
      1e-12,
  );
  assert.ok(
    Math.abs(displayLengthToDocumentUnits(50, 'mm') - displayLengthToDocumentUnits(5, 'cm')) <
      1e-12,
  );
  assert.equal(
    svgRootAttributes(doc, 'mm'),
    `width="100mm" height="50mm" viewBox="0 0 ${doc.width} ${doc.height}"`,
  );
  assert.deepEqual(doc, before, 'root attribute formatting does not mutate source dimensions');
});

test('default and explicit px output retain the legacy root markup', () => {
  const doc = { width: 960, height: 540 },
    expected = `width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}"`;
  assert.equal(svgRootAttributes(doc), expected);
  assert.equal(svgRootAttributes(doc, 'px'), expected);
});

test('responsive SVG root sizes use the viewBox without changing fixed-unit output', () => {
  const doc = { width: 960, height: 540 },
    before = structuredClone(doc);

  assert.equal(
    svgRootAttributes(doc, 'mm', { responsive: true }),
    'width="100%" height="100%" viewBox="0 0 960 540"',
  );
  assert.equal(
    svgRootAttributes(doc, 'mm'),
    `width="254mm" height="142.875mm" viewBox="0 0 960 540"`,
  );
  assert.deepEqual(doc, before);
  assert.throws(() => svgRootAttributes(doc, 'px', { responsive: 'yes' }));
  assert.throws(() => svgRootAttributes(doc, 'px', { precision: 3 }));
});

test('SVG physical size helpers reject invalid dimensions, units and options', () => {
  for (const value of [NaN, Infinity, -Infinity, 0, -1, 'not-a-number'])
    assert.throws(() => svgRootSizeAttributes({ width: value, height: 20 }, 'mm'));
  assert.throws(() => svgRootSizeAttributes({ width: 10, height: 0 }, 'cm'));
  for (const unit of ['in', null, 'toString'])
    assert.throws(() => svgRootSizeAttributes({ width: 10, height: 20 }, unit));
  assert.throws(() => svgRootSizeAttributes(null, 'mm'));
});
