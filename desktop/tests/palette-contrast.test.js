import test from 'node:test';
import assert from 'node:assert/strict';
import { analyzePaletteContrast, NORMAL_TEXT_CONTRAST_THRESHOLD } from '../src/palette-contrast.js';

const slot = (id, hex, alpha = 1) => ({ id, name: id, value: { hex, alpha } });
const entry = (id, role) => ({ id, kind: 'neutral', L: 0.5, role });
function fixture(text = '#000000', background = '#FFFFFF') {
  return {
    schemaVersion: 56,
    colorSlots: [slot('text', text), slot('background', background)],
    colorDesign: {
      version: 2,
      palette: [entry('text', 'Text'), entry('background', 'Background')],
    },
  };
}
function freezeTree(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freezeTree);
    Object.freeze(value);
  }
  return value;
}

test('semantic normal-text black/white ratio is 21 in either direction', () => {
  for (const doc of [fixture(), fixture('#FFFFFF', '#000000')]) {
    const report = analyzePaletteContrast(doc);
    assert.equal(report.mode, 'semantic-normal-text-contrast-v1');
    assert.equal(report.threshold, 4.5);
    assert.equal(report.threshold, NORMAL_TEXT_CONTRAST_THRESHOLD);
    assert.equal(report.status, 'pass');
    assert.equal(report.reason, null);
    assert.deepEqual(report.issues, []);
    assert.deepEqual(report.pairs, [
      {
        textId: 'text',
        backgroundId: 'background',
        backgroundRole: 'Background',
        ratio: 21,
        status: 'pass',
        reason: null,
      },
    ]);
  }
});

test('identical literal colors remain separate IDs with ratio 1', () => {
  const report = analyzePaletteContrast(fixture('#314159', '#314159'));
  assert.equal(report.status, 'fail');
  assert.equal(report.pairs[0].ratio, 1);
  assert.notEqual(report.pairs[0].textId, report.pairs[0].backgroundId);
});

test('source-over text alpha resolves in encoded sRGB before luminance, without quantizing', () => {
  const doc = fixture();
  doc.colorSlots[0].value.alpha = 0.5;
  const report = analyzePaletteContrast(doc);
  const expected = 1.05 / (((0.5 + 0.055) / 1.055) ** 2.4 + 0.05);
  assert.ok(Math.abs(report.pairs[0].ratio - expected) < 1e-12);
  assert.equal(report.status, 'fail');
  doc.colorSlots[0].value.alpha = 0;
  assert.equal(analyzePaletteContrast(doc).pairs[0].ratio, 1);
});

test('normal-text 4.5 threshold compares unrounded ratios immediately below and above', () => {
  for (const target of [4.499, 4.501]) {
    const doc = fixture('#FFFFFF', '#000000');
    // Inverse sRGB for a white foreground composited onto black.
    doc.colorSlots[0].value.alpha = 1.055 * ((target - 1) * 0.05) ** (1 / 2.4) - 0.055;
    const report = analyzePaletteContrast(doc);
    assert.ok(Math.abs(report.pairs[0].ratio - target) < 1e-12);
    assert.equal(report.status, target >= 4.5 ? 'pass' : 'fail');
    assert.equal(report.pairs[0].ratio.toFixed(2), '4.50');
  }
});

test('sRGB low-channel luminance uses the linear branch', () => {
  const report = analyzePaletteContrast(fixture('#0A0A0A', '#000000'));
  assert.ok(Math.abs(report.pairs[0].ratio - (10 / 255 / 12.92 + 0.05) / 0.05) < 1e-12);
});

test('all Text × Background candidates retain authored IDs and explicit role labels', () => {
  const doc = fixture();
  doc.colorSlots.push(slot('text-2', '#000000'), slot('background-2', '#000000'));
  doc.colorDesign.palette.push(entry('text-2', 'Text'), entry('background-2', 'Background'));
  const report = analyzePaletteContrast(doc);
  assert.equal(report.status, 'fail');
  assert.deepEqual(
    report.pairs.map(({ textId, backgroundId, backgroundRole, ratio }) => [
      textId,
      backgroundId,
      backgroundRole,
      ratio,
    ]),
    [
      ['text', 'background', 'Background', 21],
      ['text', 'background-2', 'Background', 1],
      ['text-2', 'background', 'Background', 21],
      ['text-2', 'background-2', 'Background', 1],
    ],
  );
});

test('transparent or translucent backgrounds are unknown; no invented white or stacking', () => {
  for (const alpha of [0, 0.5, 0.999999999]) {
    const doc = fixture();
    doc.colorSlots[1].value.alpha = alpha;
    const report = analyzePaletteContrast(doc);
    assert.equal(report.status, 'unknown');
    assert.equal(report.reason, 'unresolved-pairs');
    assert.equal(report.pairs[0].reason, 'non-opaque-background');
    assert.equal(report.pairs[0].ratio, null);
  }
});

test('missing and invalid background values report unknown reasons', () => {
  const missing = fixture();
  missing.colorSlots.pop();
  const absent = analyzePaletteContrast(missing);
  assert.equal(absent.status, 'unknown');
  assert.equal(absent.pairs[0].reason, 'missing-background-color');
  assert.equal(absent.issues[0].code, 'dangling-color-slot');
  for (const value of [null, { hex: '#fff', alpha: 1 }, { hex: '#FFFFFF', alpha: NaN }]) {
    const doc = fixture();
    doc.colorSlots[1].value = value;
    const report = analyzePaletteContrast(doc);
    assert.equal(report.status, 'unknown');
    assert.equal(report.pairs[0].ratio, null);
    assert.equal(report.pairs[0].reason, 'invalid-background-color');
  }
});

test('missing or invalid Text also remains unknown rather than passing', () => {
  const missing = fixture();
  missing.colorSlots.shift();
  assert.equal(analyzePaletteContrast(missing).pairs[0].reason, 'missing-text-color');
  for (const alpha of [-1, 2, Infinity, NaN, '1', undefined]) {
    const doc = fixture();
    doc.colorSlots[0].value.alpha = alpha;
    const report = analyzePaletteContrast(doc);
    assert.equal(report.status, 'unknown');
    assert.equal(report.pairs[0].reason, 'invalid-text-color');
  }
});

test('no explicit semantic pair is not-applicable, never PASS or inferred from slot names', () => {
  const noDesign = fixture();
  delete noDesign.colorDesign;
  const v1 = fixture();
  v1.colorDesign = { version: 1 };
  const onlyText = fixture();
  onlyText.colorDesign.palette = [entry('text', 'Text')];
  const onlyBackground = fixture();
  onlyBackground.colorDesign.palette = [entry('background', 'Background')];
  const onlyPrimary = fixture();
  onlyPrimary.colorDesign.palette = [entry('text', 'Primary')];
  const empty = fixture();
  empty.colorDesign.palette = [];
  for (const doc of [noDesign, v1, onlyText, onlyBackground, onlyPrimary, empty]) {
    const report = analyzePaletteContrast(doc);
    assert.equal(report.status, 'not-applicable');
    assert.deepEqual(report.pairs, []);
  }
});

test('one unknown candidate prevents an overall PASS while retaining measurable pairs', () => {
  const doc = fixture();
  doc.colorSlots.push(slot('background-2', '#FFFFFF', 0.5));
  doc.colorDesign.palette.push(entry('background-2', 'Background'));
  const report = analyzePaletteContrast(doc);
  assert.equal(report.status, 'unknown');
  assert.deepEqual(
    report.pairs.map((pair) => pair.status),
    ['pass', 'unknown'],
  );
  assert.deepEqual(
    report.pairs.map((pair) => pair.ratio),
    [21, null],
  );
});

test('duplicate, dangling, sparse and malformed entries are never silently ignored', () => {
  const mutations = [
    (d) => d.colorSlots.push({ ...d.colorSlots[0] }),
    (d) => d.colorDesign.palette.push({ ...d.colorDesign.palette[0] }),
    (d) => d.colorDesign.palette.push(entry('missing', 'Primary')),
    (d) => d.colorDesign.palette.push(entry('text-2', 'Surface')),
    (d) => (d.colorDesign.palette[0].role = 'text'),
    (d) => (d.colorDesign.palette[0].kind = 'unknown'),
    (d) => (d.colorDesign.palette[0].id = ''),
    (d) => (d.colorDesign.palette = Array(1)),
    (d) => (d.colorSlots = Array(1)),
    (d) => (d.colorSlots[0].value.extra = true),
    (d) => (d.colorDesign.version = 3),
    (d) => (d.colorDesign.palette = null),
    (d) => (d.colorSlots = {}),
  ];
  for (const mutate of mutations) {
    const doc = fixture();
    mutate(doc);
    const report = analyzePaletteContrast(doc);
    assert.equal(report.status, 'unknown');
    assert.equal(report.reason, 'invalid-input');
    assert.ok(report.issues.length > 0);
  }
  for (const doc of [null, [], 'palette'])
    assert.equal(analyzePaletteContrast(doc).status, 'unknown');
});

test('256-slot/palette bounds allow at most 16384 pairs and reject either overflow', () => {
  const doc = fixture();
  doc.colorSlots = Array.from({ length: 256 }, (_, i) => slot(`slot-${i}`, '#000000'));
  doc.colorDesign.palette = doc.colorSlots.map((row, i) =>
    entry(row.id, i < 128 ? 'Text' : 'Background'),
  );
  assert.equal(analyzePaletteContrast(doc).pairs.length, 16384);
  const tooManySlots = structuredClone(doc);
  tooManySlots.colorSlots.push(slot('overflow', '#FFFFFF'));
  assert.equal(analyzePaletteContrast(tooManySlots).reason, 'invalid-input');
  doc.colorDesign.palette.push(entry('overflow', 'Text'));
  assert.equal(analyzePaletteContrast(doc).reason, 'invalid-input');
});

test('report reads frozen input without mutating authored state or sharing mutable result state', () => {
  const doc = freezeTree(fixture());
  const original = JSON.stringify(doc);
  const first = analyzePaletteContrast(doc);
  first.pairs[0].status = 'changed';
  assert.equal(analyzePaletteContrast(doc).pairs[0].status, 'pass');
  assert.equal(JSON.stringify(doc), original);
});
