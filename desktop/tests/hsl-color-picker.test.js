import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
  hexToHsl,
  hslToHex,
  updateHexHsl,
  renderHslFields,
  canonicalizeHexSaturation,
} from '../src/hsl-color-picker.js';

const characterization = JSON.parse(
  readFileSync(
    new URL('./fixtures/hex-hsl-derived-characterization.json', import.meta.url),
    'utf8',
  ),
);

test('sRGB hex roundtrips through HSL without changing the document color', () => {
  for (const hex of [
    '#000000',
    '#ffffff',
    '#808080',
    '#ff0000',
    '#00ff00',
    '#0000ff',
    '#254360',
    '#a44799',
    '#ffca42',
  ]) {
    const { h, s, l } = hexToHsl(hex);
    assert.equal(hslToHex(h, s, l), hex);
  }
});

test('HSL input edits one channel and leaves the other coordinates intact', () => {
  assert.equal(updateHexHsl('#ff0000', 'h', '120'), '#00ff00');
  assert.equal(updateHexHsl('#ff0000', 's', '0'), '#808080');
  assert.equal(updateHexHsl('#ff0000', 'l', '0'), '#000000');
  assert.equal(hslToHex(360, 100, 50), '#ff0000');
});

test('HEX-derived saturation roundoff does not block valid H/L edits', () => {
  assert.equal(updateHexHsl('#000011', 'h', '120'), '#001100');
  assert.equal(updateHexHsl('#000011', 'l', '50'), '#0000ff');
  assert.equal(updateHexHsl('#000011', 's', '100'), '#000011');
  assert.equal(updateHexHsl('#808080', 'h', '120'), '#808080');
});

test('only characterized HEX-derived overshoot is canonicalized; user HSL stays strict', () => {
  const { maxS, ulpAt100, worstInputs } = characterization.node;
  assert.equal(characterization.node.inputs, 0x1000000);
  assert.equal(characterization.chrome.results.maxS, maxS);
  assert.equal(canonicalizeHexSaturation(maxS), 100);
  assert.equal(canonicalizeHexSaturation(0), 0);
  assert.equal(canonicalizeHexSaturation(37.123456789), 37.123456789);
  for (const bad of [NaN, Infinity, -Infinity, -Number.MIN_VALUE, maxS + ulpAt100, 101])
    assert.throws(() => canonicalizeHexSaturation(bad), /Internal HEX saturation/);
  for (const hex of worstInputs) {
    const { h, s, l } = hexToHsl(hex);
    assert.equal(s, 100, hex);
    assert.equal(hslToHex(h, s, l), hex);
  }
  assert.throws(() => hslToHex(120, maxS, 50), /HSL/);
  assert.throws(() => updateHexHsl('#000011', 's', String(maxS)), /HSL/);
});

test('invalid HSL never converts to a color', () => {
  const invalid = [
    ['h', ''],
    ['h', 'not a number'],
    ['h', '361'],
    ['h', '-1'],
    ['s', '101'],
    ['s', '100.00000000000003'],
    ['s', '-0.1'],
    ['s', 'NaN'],
    ['l', 'Infinity'],
    ['l', '101'],
    ['rgb', '20'],
  ];
  for (const hex of ['#336699', '#000011'])
    for (const [channel, value] of invalid)
      assert.throws(() => updateHexHsl(hex, channel, value), /HSL|value/);
  assert.throws(() => hslToHex(120, 100.00000000000003, 50), /HSL/);
  assert.throws(() => hexToHsl('rgb(0,0,0)'), /sRGB/);
});

test('default visible controls are H, S and L with distinct scoped attributes', () => {
  const local = renderHslFields('#ff0000', { name: 'Fill', base: true });
  assert.match(local, /data-color="hsl-h" data-color-base/);
  assert.match(local, /data-color="hsl-s" data-color-base/);
  assert.match(local, /data-color="hsl-l" data-color-base/);
  assert.match(local, /value="0"/);
  assert.match(local, /value="100"/);
  assert.match(local, /value="50"/);

  const shared = renderHslFields('#254360', {
    mode: 'shared',
    name: 'Color <A>',
    id: 'slot-1',
  });
  assert.match(shared, /data-shared-color-edit="hsl-h"/);
  assert.match(shared, /data-shared-color-id="slot-1"/);
  assert.match(shared, /Color &lt;A&gt; HSL/);
  assert.doesNotMatch(shared, /data-color-base/);
});
