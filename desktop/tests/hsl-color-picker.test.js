import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createHslColorPicker, hexToHsl, hslToHex } from '../src/hsl-color-picker.js';

test('sRGB colors round-trip through UI-only HSL coordinates', () => {
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
  assert.equal(hslToHex(360, 100, 50), '#ff0000');
  assert.throws(() => hexToHsl('rgb(0,0,0)'), /sRGB/);
  for (const args of [
    [-1, 20, 50],
    [361, 20, 50],
    [0, 101, 50],
    [0, 50, -1],
    [0, NaN, 50],
  ])
    assert.throws(() => hslToHex(...args), /HSL/);
});

test('swatch popup defaults to HSL with hue control, saturation/lightness plane and HEX fallback', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<button id="swatch">Color</button>';
  const commits = [],
    errors = [];
  const target = { nodeId: 'a', key: 'color' };
  const picker = createHslColorPicker({
    document,
    onCommit: (value) => {
      commits.push(value);
      return true;
    },
    onError: (error) => errors.push(error.message),
  });
  const popup = picker.open({
    anchor: document.querySelector('#swatch'),
    label: 'Fill color',
    color: { hex: '#ff0000', alpha: 0.4 },
    context: target,
  });
  assert.equal(document.querySelectorAll('.hsl-color-popover').length, 1);
  assert.equal(popup.getAttribute('role'), 'dialog');
  assert.ok(popup.querySelector('[data-hsl-plane]'));
  assert.ok(popup.querySelector('[data-hsl-hue]'));
  assert.equal(popup.querySelectorAll('[data-hsl-number]').length, 3);
  assert.equal(popup.querySelector('[data-hsl-hex]').value, '#FF0000');
  const hue = popup.querySelector('[data-hsl-number="h"]');
  hue.value = '120';
  hue.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(commits.length, 1);
  assert.deepEqual(commits[0], { target, hex: '#00ff00' });
  assert.equal(popup.querySelector('[data-hsl-hex]').value, '#00FF00');
  assert.deepEqual(errors, []);
  // No repeated authoring for a no-op value.
  hue.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(commits.length, 1);
  hue.value = '400';
  hue.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(commits.length, 1);
  assert.equal(hue.value, '120');
  const hexInput = popup.querySelector('[data-hsl-hex]');
  hexInput.value = '#123456';
  hexInput.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(commits.at(-1).hex, '#123456');
  assert.equal(commits.at(-1).target, target);
  assert.equal(commits.length, 2);
  popup.querySelector('[data-hsl-close]').click();
  assert.equal(picker.element, null);
});

test('invalid input and Escape do not author a Color change', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<button id="swatch">Color</button><button id="outside">Away</button>';
  const commits = [];
  const picker = createHslColorPicker({
    document,
    onCommit: ({ hex }) => commits.push(hex),
  });
  const open = () =>
    picker.open({
      anchor: document.querySelector('#swatch'),
      label: 'Paint',
      color: { hex: '#2040ff', alpha: 1 },
      context: { nodeId: 'b' },
    });
  let popup = open();
  const hex = popup.querySelector('[data-hsl-hex]');
  hex.value = 'bad';
  hex.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(hex.value, '#2040FF');
  popup.querySelector('[data-hsl-number="s"]').value = '99';
  popup.dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
  );
  assert.equal(picker.element, null);
  assert.deepEqual(commits, []);
  popup = open();
  document.querySelector('#outside').click();
  assert.equal(picker.element, null);
  assert.deepEqual(commits, []);
});

test('plane gesture changes saturation/lightness once at pointer release', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<button id="swatch">Color</button>';
  const commits = [];
  const picker = createHslColorPicker({
    document,
    onCommit: ({ hex }) => {
      commits.push(hex);
      return true;
    },
  });
  const popup = picker.open({
    anchor: document.querySelector('#swatch'),
    label: 'Fill',
    color: { hex: '#ff0000', alpha: 1 },
    context: {},
  });
  const plane = popup.querySelector('[data-hsl-plane]');
  plane.getBoundingClientRect = () => ({
    left: 0,
    top: 0,
    right: 180,
    bottom: 132,
    width: 180,
    height: 132,
  });
  plane.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      bubbles: true,
      button: 0,
      pointerId: 9,
      isPrimary: true,
      clientX: 90,
      clientY: 66,
    }),
  );
  assert.equal(commits.length, 0);
  document.dispatchEvent(
    new win.PointerEvent('pointerup', {
      bubbles: true,
      button: 0,
      pointerId: 9,
      isPrimary: true,
    }),
  );
  assert.equal(commits.length, 1);
  assert.equal(commits[0], '#bf4040');
});
