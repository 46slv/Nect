import test from 'node:test';
import assert from 'node:assert/strict';
import { setCharacterOffsets, setCharacterStyles } from '../src/character-identity.js';
import {
  OPENTYPE_SCHEMA_VERSION,
  standardLigatureSupport,
  textFeatureOptions,
  textFeatureReason,
  validateTextFeatures,
} from '../src/opentype-features.js';

const face = () => ({
  tables: {
    gsub: {
      scripts: [{ tag: 'latn', script: { defaultLangSys: { featureIndexes: [0] } } }],
      features: [{ tag: 'liga', feature: { lookupListIndexes: [0] } }],
      lookups: [{ lookupType: 4, lookupFlag: 0, subtables: [{ substFormat: 1 }] }],
    },
  },
});
const text = (patch = {}) => ({ type: 'Text', text: 'office ffi', ...patch });

test('only the verified Latin default-language ligature lookup is advertised', () => {
  assert.equal(standardLigatureSupport(face()), true);
  assert.equal(standardLigatureSupport({}), false);
  for (const mutate of [
    (g) => (g.scripts[0].tag = 'DFLT'),
    (g) => (g.features[0].tag = 'dlig'),
    (g) => (g.lookups[0].lookupType = 7),
    (g) => (g.lookups[0].lookupFlag = 8),
    (g) => (g.lookups[0].subtables[0].substFormat = 2),
    (g) => g.features[0].feature.lookupListIndexes.push(99),
  ]) {
    const font = face();
    mutate(font.tables.gsub);
    assert.equal(standardLigatureSupport(font), false);
  }
});

test('legacy options stay absent and only explicit liga is passed to the shaper', () => {
  assert.deepEqual(textFeatureOptions(face(), text()), {});
  for (const liga of [false, true])
    assert.deepEqual(textFeatureOptions(face(), text({ fontFeatures: { liga } })), {
      features: { liga, rlig: false },
    });
  assert.throws(() => textFeatureOptions({}, text({ fontFeatures: { liga: true } })), /lookup/);
});

test('unsupported tags, malformed settings and older schemas reject explicitly', () => {
  for (const fontFeatures of [
    null,
    [],
    {},
    { liga: 1 },
    { dlig: true },
    { liga: true, onum: true },
  ])
    assert.throws(() => validateTextFeatures(text({ fontFeatures })), /liga/);
  assert.throws(
    () => validateTextFeatures(text({ fontFeatures: { liga: true } }), OPENTYPE_SCHEMA_VERSION - 1),
    /schemaVersion/,
  );
});

test('unsupported layout and script routes remain explicit, including inactive character records', () => {
  for (const patch of [
    { type: 'Rectangle' },
    { osFont: {} },
    { variation: {} },
    { writingMode: 'vertical-rl' },
    { textWrap: { width: 100 } },
    { pathAttachment: {} },
    { characterArrangement: {} },
    { characterPositions: setCharacterOffsets('office ffi', undefined, { c1: [1, 0] }) },
    { characterPositions: setCharacterStyles('office ffi', undefined, { c1: { size: 20 } }) },
    { text: '\u0627\u0644' },
    { text: '\u65e5\u672c' },
  ]) {
    assert.ok(textFeatureReason(text(patch)));
    assert.throws(() => validateTextFeatures(text({ ...patch, fontFeatures: { liga: true } })));
  }
  assert.equal(textFeatureReason(text({ text: 'office\nAV 123' })), null);
  assert.throws(
    () =>
      validateTextFeatures(text({ fontFeatures: { liga: false } }), 38, {
        kind: 'variable-glyf-v1',
      }),
    /static/,
  );
});
