import test from 'node:test';
import assert from 'node:assert/strict';
import { hexToHsl, hslToHex, updateHexHsl, renderHslFields } from '../src/hsl-color-picker.js';

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

test('invalid HSL never converts to a color', () => {
  for (const [channel, value] of [
    ['h', ''],
    ['h', 'not a number'],
    ['h', '361'],
    ['h', '-1'],
    ['s', '101'],
    ['s', '-0.1'],
    ['l', 'Infinity'],
    ['l', '101'],
    ['rgb', '20'],
  ])
    assert.throws(() => updateHexHsl('#336699', channel, value), /HSL|value/);
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
