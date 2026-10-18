import test from 'node:test';
import assert from 'node:assert/strict';
import { initialLayerId } from '../src/initial-layer.js';
const l = (id, visible, locked, source) => ({
  id,
  visible,
  locked,
  output: id + '-out',
  nodes: [{ id: id + '-out', params: { path: { source } } }],
});
test('open chooses a visible editable populated composition without changing authored state', () => {
  const doc = {
      layers: [
        l('source', false, true, 'x'),
        l('empty', true, false),
        l('qr', true, false, 'image'),
      ],
    },
    before = JSON.stringify(doc);
  assert.equal(initialLayerId(doc), 'qr');
  assert.equal(JSON.stringify(doc), before);
  doc.layers[2].locked = true;
  assert.equal(initialLayerId(doc), 'empty');
  doc.layers[1].locked = true;
  assert.equal(initialLayerId(doc), 'empty');
  doc.layers.forEach((l) => (l.visible = false));
  assert.equal(initialLayerId(doc), 'source');
});
test('solo/hide/guide state determines the initial selection without revealing hidden sources', () => {
  const doc = { layers: [l('first', true, false, 'x'), l('solo', true, false, 'y')] };
  doc.layers[1].solo = true;
  assert.equal(initialLayerId(doc), 'solo');
  doc.layers[1].visible = false;
  assert.equal(initialLayerId(doc), 'first');
  delete doc.layers[1].solo;
  doc.layers[1].visible = true;
  doc.layers[0].guide = true;
  assert.equal(initialLayerId(doc), 'solo');
});
