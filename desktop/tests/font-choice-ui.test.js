import test from 'node:test';
import assert from 'node:assert/strict';
import { validateDesktopFace } from '../src/desktop-font-text.js';
import {
  fontPickerItems,
  migrateFontFavorites,
  sameFontFace,
  selectedFontPickerId,
  windowsFontChoiceId,
  fontPickerCatalogs,
  fontFamilyGroups,
} from '../src/font-choice-ui.js';

const face = {
  family: 'Family',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const text = (patch = {}) => ({
  type: 'Text',
  text: 'ABC',
  params: { size: { value: 100 }, letterSpacing: { value: 0 }, lineSpacing: { value: 1.2 } },
  ...patch,
});
const meta = (family, weightClass, subfamily = 'Regular') => ({ family, weightClass, subfamily });

test('native discovery aliases are detached metadata, preserving strict identity, order and favorites', () => {
  const aliases = [
    'メイリオ',
    'ＭＳ ゴシック',
    'メイリオ',
    '',
    'bad\u007f',
    '\ud800',
    'x'.repeat(256),
    '𠮷',
  ];
  const input = { ...face, localizedNames: aliases };
  const bold = { ...face, name: 'Bold', weight: 700 };
  const adapted = fontPickerCatalogs({ windows: [{ ...bold, localizedNames: ['太字'] }, input] });
  const nativeRows = (catalogs) =>
    fontPickerItems(catalogs, text()).filter((row) => row.choice.source === 'windows');
  const plain = nativeRows(fontPickerCatalogs({ windows: [bold, face] }));
  const rows = nativeRows(adapted);
  assert.deepEqual(adapted.windows[1].face, face);
  validateDesktopFace(adapted.windows[1].face);
  assert.deepEqual(rows[0].metadata.localizedNames, ['メイリオ', 'ＭＳ ゴシック', '𠮷']);
  assert.equal(rows[0].key, plain[0].key);
  assert.deepEqual(
    rows.map((row) => row.key),
    plain.map((row) => row.key),
  );
  assert.equal(rows[0].label, plain[0].label);
  assert.equal(rows[0].disabled, false);
  assert.deepEqual(
    fontFamilyGroups(rows).map((g) => g.id),
    fontFamilyGroups(plain).map((g) => g.id),
  );
  assert.deepEqual(migrateFontFavorites([plain[0].key], rows), new Set([plain[0].key]));
  aliases.push('later');
  assert.equal(rows[0].metadata.localizedNames.includes('later'), false);
  for (const localizedNames of [null, 'メイリオ', [null, 1, '\udfff']]) {
    const item = fontPickerCatalogs({ windows: [{ ...face, localizedNames }] }).windows[0];
    validateDesktopFace(item.face);
    assert.deepEqual(item.face, face);
  }
  const unknown = fontPickerCatalogs({ windows: [{ ...input, surprise: true }] }).windows[0];
  assert.equal(unknown.face.surprise, true);
  assert.throws(() => validateDesktopFace(unknown.face), /Invalid Windows font face/);
  assert.equal(fontPickerItems({ windows: [unknown] }, text())[0].disabled, true);
  assert.throws(() => validateDesktopFace(input), /Invalid Windows font face/);
  for (const prototype of [null, { native: true }, Array.prototype]) {
    const malformed = Object.assign(Object.create(prototype), input);
    const item = fontPickerCatalogs({ windows: [malformed] }).windows[0];
    assert.equal(Object.getPrototypeOf(item.face), prototype);
    assert.throws(() => validateDesktopFace(item.face), /Invalid Windows font face/);
    assert.equal(fontPickerItems({ windows: [item] }, text())[0].disabled, true);
  }
  assert.equal(
    fontPickerCatalogs({
      windows: [{ ...face, localizedNames: Array.from({ length: 100 }, (_, i) => `Alias ${i}`) }],
    }).windows[0].metadata.localizedNames.length,
    64,
  );
});

test('one list preserves all exact identities and sorts family/real style deterministically', () => {
  const catalogs = {
    embedded: [
      { id: 'z', label: 'Same', metadata: meta('B', 400) },
      { id: 'a', label: 'Same', metadata: meta('A', 700, 'Bold') },
    ],
    installed: [{ id: 'a', label: 'Same', metadata: meta('A', 400) }],
    windows: [
      {
        id: windowsFontChoiceId(face),
        label: 'Family Regular',
        face,
        metadata: meta('Family', 400),
      },
    ],
  };
  const before = structuredClone(catalogs),
    rows = fontPickerItems(catalogs, text());
  assert.equal(rows.length, 4);
  assert.equal(new Set(rows.map((row) => row.id)).size, 4);
  assert.deepEqual(
    rows.map((row) => row.choice.source),
    ['installed', 'embedded', 'embedded', 'windows'],
  );
  assert.deepEqual(
    fontPickerItems({ ...catalogs, embedded: [...catalogs.embedded].reverse() }, text()),
    rows,
  );
  assert.match(rows[0].detail, /Variant/);
  assert.equal(rows[0].technicalIdentity, rows[0].choice.id);
  assert.doesNotMatch(rows[0].detail, /face a/);
  assert.deepEqual(catalogs, before);
});

test('unsupported faces remain visible with a reason, without demanding an engine choice', () => {
  const rows = fontPickerItems(
    { windows: [{ id: 'native', label: 'Family Regular', face }] },
    text({ fontFeatures: { liga: true } }),
  );
  assert.equal(rows[0].disabled, true);
  assert.match(rows[0].disabledReason, /OpenType/);
  assert.match(rows[0].detail, /OpenType/);
  const unavailable = fontPickerItems(
    {
      installed: [
        { id: 'x', label: 'X', disabledReason: 'Font discovery unavailable in this browser' },
      ],
    },
    text(),
  );
  assert.match(unavailable[0].detail, /unavailable in this browser/);
});

test('saved native selection compares every descriptor field, never a display name', () => {
  const rows = fontPickerItems(
    {
      windows: [
        { id: 'first', label: 'Same', face: { ...face, format: 2 } },
        { id: 'second', label: 'Same', face },
      ],
    },
    text(),
  );
  assert.equal(
    selectedFontPickerId(text({ osFont: { face } }), rows),
    rows.find((r) => r.choice.id === 'second').id,
  );
  assert.equal(sameFontFace(face, { ...face, color: true }), false);
  assert.notEqual(windowsFontChoiceId(face), windowsFontChoiceId({ ...face, format: 2 }));
  assert.equal(
    selectedFontPickerId(text({ osFont: { face: { ...face, name: 'Missing' } } }), rows),
    undefined,
  );
});

test('favorite migration keeps aliases and duplicate rows independently toggleable', () => {
  const rows = fontPickerItems(
    {
      embedded: [
        { id: 'default', label: 'Default' },
        { id: 'a', label: 'Same' },
      ],
      installed: [{ id: 'b', label: 'Same' }],
    },
    text(),
  );
  const favorites = migrateFontFavorites(['default', 'font:Same', 'other-hidden-id'], rows);
  assert.equal(favorites.has('other-hidden-id'), true);
  assert.equal(favorites.has('default'), false);
  assert.equal(favorites.has('font:Same'), false);
  assert.equal(favorites.has(rows.find((r) => r.choice.id === 'a').key), true);
  const b = rows.find((r) => r.choice.id === 'b');
  favorites.delete(b.key);
  assert.equal(migrateFontFavorites(favorites, rows).has(b.key), false);
  assert.equal(rows[0].metadata.localizedNames, undefined);
});

test('adapters retain installed aliases, variable flags, and exact native faces including unsupported color', () => {
  const metadata = { family: 'Alias', localizedNames: ['日本語', 'ＭＳ 書体'] };
  const catalogs = fontPickerCatalogs({
    assets: { v: { name: 'Variable', kind: 'variable-glyf-v1' } },
    fonts: { v: { nectMetadata: { family: 'Variable' } } },
    installed: [['opaque', 'Display', null, metadata]],
    windows: [face, { ...face, color: true }],
  });
  assert.deepEqual(catalogs.installed[0].metadata, metadata);
  assert.equal(catalogs.embedded.find((c) => c.id === 'v').variable, true);
  const rows = fontPickerItems(catalogs, text());
  assert.equal(rows.filter((r) => r.choice.source === 'windows').length, 2);
  assert.match(rows.find((r) => r.choice.face?.color).disabledReason, /Color/);
});

test('character picker keeps Inherit as an explicit non-name-based choice', () => {
  const rows = fontPickerItems({ embedded: [{ id: 'default', label: 'Default' }] }, text(), {
    characterIds: ['c1'],
  });
  assert.equal(rows[0].label, 'Inherit default font');
  assert.equal(rows[0].choice.id, '');
  assert.equal(rows[0].disabled, false);
  for (const patch of [{ locked: true }, { bypassed: true }]) {
    const blocked = fontPickerItems({}, text(patch), { characterIds: ['c1'] });
    assert.equal(blocked[0].disabled, true);
    assert.match(blocked[0].detail, /unlocked enabled/);
  }
});

test('installed faces are available for explicit Apply without substituting native identities', () => {
  const rows = fontPickerItems(
    {
      installed: [
        { id: 'same-id', label: 'Same', metadata: { localizedNames: ['日本語の実メタデータ'] } },
      ],
      windows: [{ id: 'same-id', label: 'Same', face }],
    },
    text(),
  );
  const installed = rows.find((row) => row.choice.source === 'installed');
  const native = rows.find((row) => row.choice.source === 'windows');
  assert.equal(installed.disabled, false);
  assert.match(installed.detail, /Included in saved document/);
  assert.doesNotMatch(installed.detail, /forbidden|illegal|Adobe|Morisawa/);
  assert.equal(native.disabled, false);
  assert.notEqual(installed.id, native.id);
  assert.deepEqual(installed.metadata.localizedNames, ['日本語の実メタデータ']);
  assert.equal(native.metadata.localizedNames, undefined);
});

test('family disclosures keep exact source identity, real child styles and unknown metadata separate', () => {
  const rows = fontPickerItems(
    {
      embedded: [
        { id: 'regular', label: 'Regular', metadata: meta('Family', 400) },
        { id: 'bold', label: 'Bold', metadata: meta('Family', 700, 'Bold') },
        { id: 'unknown-1', label: 'Unknown' },
        { id: 'unknown-2', label: 'Unknown' },
      ],
      windows: [{ id: 'native', label: 'Regular', face, metadata: meta('Family', 400) }],
    },
    text(),
  );
  const groups = fontFamilyGroups(rows);
  const named = groups.filter((g) => g.label === 'Family');
  assert.equal(named.length, 2, 'equal family labels do not merge unproven adapter identities');
  assert.notEqual(named[0].id, named[1].id);
  assert.deepEqual(
    named.map((g) => g.itemIds.length),
    [2, 1],
  );
  assert.equal(groups.filter((g) => !g.known).length, 2);
  assert.equal(new Set(groups.flatMap((g) => g.itemIds)).size, rows.length);
  assert.deepEqual(groups.flatMap((g) => g.itemIds).sort(), rows.map((r) => r.id).sort());
});

test('same-name file members remain exact choices without opaque identity in ordinary rows', () => {
  const files = [
    {
      token: 'process-only',
      sha256: 'a'.repeat(64),
      faces: [
        { faceIndex: 0, face },
        { faceIndex: 1, face },
      ],
    },
  ];
  const rows = fontPickerItems(fontPickerCatalogs({ fileNative: files }), text()).filter(
    (row) => row.choice.source === 'file-native',
  );
  assert.equal(rows.length, 2);
  assert.notEqual(rows[0].id, rows[1].id);
  assert.deepEqual(
    rows.map((row) => row.metadata.subfamily),
    [face.name, face.name],
  );
  assert.match(rows[0].detail, /Variant 1 of 2/);
  assert.match(rows[1].detail, /Variant 2 of 2/);
  for (const row of rows) {
    assert.doesNotMatch(row.detail, /a{64}|process-only/);
    assert.match(row.technicalIdentity, /a{64}/);
  }
});
