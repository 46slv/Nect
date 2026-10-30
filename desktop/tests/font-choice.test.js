import test from 'node:test';
import assert from 'node:assert/strict';
import { fontChoiceCatalog, fontChoiceCapability } from '../src/font-choice.js';

const face = {
  family: 'Same name',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const text = (patch = {}) => ({
  type: 'Text',
  text: '日本語\nABC',
  params: {
    size: { value: 100 },
    letterSpacing: { value: 0 },
    lineSpacing: { value: 1.2 },
  },
  ...patch,
});
const windows = { source: 'windows', face };

test('catalog retains opaque identity and same-name faces across all adapters', () => {
  const input = {
    embedded: [{ id: 'opaque:1', label: 'Same name' }],
    installed: [
      { id: 'opaque:1', label: 'Same name' },
      { id: 'opaque:2', label: 'Same name' },
    ],
    windows: [{ id: 'native-token', label: 'Same name', face }],
  };
  const before = structuredClone(input),
    rows = fontChoiceCatalog(input);
  assert.equal(rows.length, 4);
  assert.equal(new Set(rows.map((row) => row.key)).size, 4);
  assert.deepEqual(
    rows.map((row) => row.id),
    ['opaque:1', 'opaque:1', 'opaque:2', 'native-token'],
  );
  assert.deepEqual(input, before);
  assert.throws(() => fontChoiceCatalog({ installed: [...input.installed, input.installed[0]] }));
  assert.throws(() => fontChoiceCatalog({ embedded: [{ label: 'No identity' }] }));
});

test('routing selects prepared atomic replacement without clearing OS Text first', () => {
  for (const source of ['embedded', 'installed']) {
    assert.equal(
      fontChoiceCapability(text({ osFont: {} }), { source }).route,
      'replace-os-text-font',
    );
    assert.match(
      fontChoiceCapability(text({ osFont: {} }), { source, variable: true }).reason,
      /static/,
    );
  }
  assert.equal(fontChoiceCapability(text(), { source: 'installed' }).route, 'adopt-font');
  assert.equal(fontChoiceCapability(text(), { source: 'embedded' }).route, 'set-text-settings');
  assert.equal(
    fontChoiceCapability(text(), { source: 'embedded' }, { characterIds: ['c1'] }).route,
    'set-character-style',
  );
  assert.equal(
    fontChoiceCapability(text(), { source: 'embedded', variable: true }).route,
    'set-variable-font',
  );
  assert.equal(fontChoiceCapability(text(), windows).route, 'set-os-font-text');
});

test('modern native route retains multiline, vertical, wrap and axis capability', () => {
  for (const patch of [
    {},
    { writingMode: 'vertical-rl' },
    { textWrap: { width: 400 } },
    { writingMode: 'vertical-rl', textWrap: { height: 400, policy: 'japanese-basic-v1' } },
    {
      osFont: {
        version: 3,
        face,
        axes: [
          {
            tag: 'wght',
            name: 'Weight',
            min: 100,
            max: 900,
            defaultValue: 400,
            baseValue: 400,
            value: 600,
            hidden: false,
          },
        ],
        outline: { ligatures: true },
      },
    },
  ])
    assert.deepEqual(fontChoiceCapability(text(patch), windows), {
      route: 'set-os-font-text',
      reason: null,
    });
});

test('native route refuses authored overrides and explicit liga without mutating intent', () => {
  for (const patch of [
    { variation: {} },
    { pathAttachment: {} },
    { leading: { mode: 'manual', value: 120 } },
    { characterArrangement: {} },
    ...['offsets', 'styles', 'transforms', 'kerningPairs'].map((key) => ({
      characterPositions: { [key]: { c1: {} } },
    })),
    { fontFeatures: { liga: false } },
    { fontFeatures: { liga: true } },
  ]) {
    const n = text(patch),
      before = structuredClone(n),
      result = fontChoiceCapability(n, windows);
    assert.equal(result.route, null);
    assert.ok(result.reason);
    assert.deepEqual(n, before);
  }
  const n = text();
  n.params.size.expression = '100';
  assert.match(fontChoiceCapability(n, windows).reason, /driven/);
  assert.match(fontChoiceCapability(text(), windows, { characterIds: ['c1'] }).reason, /whole/);
  assert.match(
    fontChoiceCapability(text(), { ...windows, face: { ...face, color: true } }).reason,
    /Color/,
  );
});

test('replacement preflight explains known saved ligature-on layout limits before Apply', () => {
  const choice = { source: 'embedded' };
  for (const patch of [{}, { writingMode: 'vertical-rl' }, { textWrap: { width: 400 } }]) {
    const n = text({ ...patch, osFont: { outline: { ligatures: true } } }),
      before = structuredClone(n);
    assert.equal(fontChoiceCapability(n, choice).route, null);
    assert.match(fontChoiceCapability(n, choice).reason, /ligatures require/i);
    assert.deepEqual(n, before);
  }
  const latin = text({ text: 'office ffi', osFont: { outline: { ligatures: true } } }),
    vertical = text({ writingMode: 'vertical-rl', osFont: { outline: { ligatures: false } } });
  assert.equal(fontChoiceCapability(latin, choice).route, 'replace-os-text-font');
  assert.equal(fontChoiceCapability(vertical, choice).route, 'replace-os-text-font');
});
