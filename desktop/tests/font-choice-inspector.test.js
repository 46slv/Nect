import test from 'node:test';
import assert from 'node:assert/strict';
import { fontChoiceControls, fontImportControls, textOutlineActions,
  textFontLabel } from '../src/font-choice-inspector.js';

test('both editable Text forms expose one ordinary Font entry without engine/conversion labels', () => {
  for (const node of [{ id: 'text', font: 'default' },
    { id: 'native', osFont: { family: 'Family', face: { name: 'Regular' } } }]) {
    const html = fontChoiceControls(node);
    assert.equal((html.match(/data-change-font=/g) || []).length, 1);
    assert.match(html, />Font<|aria-label="Font"/);
    assert.match(html, /Change font…/);
    assert.doesNotMatch(html, /OS font|Adobe|paths|Import|TTF|engine|Default font/);
  }
});

test('manual static/variable loading is under Advanced and conversion stays in Text actions', () => {
  const node = { id: 'text', font: 'default' };
  const advanced = fontImportControls(node), actions = textOutlineActions(node);
  assert.match(advanced, /^<details[^>]*><summary>Advanced font import<\/summary>/);
  assert.match(advanced, /data-load-font=/); assert.match(advanced, /data-load-variable-font=/);
  assert.doesNotMatch(advanced, /data-bake-text-outline|Adobe/);
  assert.match(actions, /<summary>Text actions<\/summary>/);
  assert.match(actions, /data-bake-text-outline=/); assert.match(actions, /Undo restores Text/);
  assert.doesNotMatch(actions, /data-change-font|data-load-font/);
});

test('native variable-import limitation and unavailable browser discovery remain explicit', () => {
  const html = fontImportControls({ id: 'native', osFont: {} });
  assert.match(html, /data-load-variable-font="native" disabled/);
  assert.match(html, /cannot preserve/); assert.match(html, /not this browser/);
  assert.match(fontImportControls({ id: 'text' }, { desktop: true }), /Refresh available fonts/);
});

test('font names and stable node identities are escaped, unavailable saved font is not guessed', () => {
  assert.equal(textFontLabel({ font: 'missing' }), 'Unavailable font');
  const html = fontChoiceControls({ id: '\"<id>', font: 'asset' },
    { assets: { asset: { name: '<script> & font' } }, status: '<status>' });
  assert.doesNotMatch(html, /<script>|<status>/);
  assert.match(html, /&lt;script&gt; &amp; font/); assert.match(html, /&quot;&lt;id&gt;/);
});
