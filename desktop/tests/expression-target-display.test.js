import test from 'node:test';
import assert from 'node:assert/strict';
import { expressionTargetDisplay } from '../src/expression-target-display.js';

test('Artwork context distinguishes duplicate names without changing reference identity', () => {
  const document = {
    layers: [
      { id: 'local', name: 'Local', nodes: [{ id: 'curve-local', name: 'Curve' }] },
      {
        id: 'artwork-1234',
        name: 'が',
        nodes: [
          { id: 'curve-1', name: 'Curve' },
          { id: 'curve-2', name: 'Curve' },
        ],
      },
      { id: 'artwork-2234', name: 'か\u3099', nodes: [{ id: 'curve-3', name: 'Curve' }] },
      { id: 'artwork-3', name: 'が · …1234', nodes: [] },
      { id: 'artwork-empty', name: '', nodes: [] },
      { id: 'artwork-named', name: 'Untitled', nodes: [] },
    ],
  };
  const display = expressionTargetDisplay(document, 'local');
  const point = {
    layerId: 'artwork-1234',
    layerName: 'が',
    nodeId: 'curve-1',
    name: 'Curve',
    key: 'point-1.position',
    pointOrdinal: 1,
    label: 'Curve · Point 1 · X',
    expression: 'ref(artwork, "artwork-1234", "curve-1", "point-1.position", "x")',
  };
  const otherPoint = {
    ...point,
    layerId: 'artwork-2234',
    layerName: 'か\u3099',
    nodeId: 'curve-3',
    expression: 'ref(artwork, "artwork-2234", "curve-3", "point-1.position", "x")',
  };
  const one = display.picker(point),
    two = display.picker(otherPoint);
  assert.notEqual(one.label.normalize('NFC'), two.label.normalize('NFC'));
  assert.notEqual(one.label.split(' · Curve')[0], 'が · …1234');
  assert.equal(one.id, point.expression);
  assert.equal(one.detail, 'curve-1');
  assert.equal(two.detail, '');
  assert.match(one.searchText, /artwork-1234.*curve-1.*point-1/);
  const completion = display.completion(point);
  assert.equal(completion.expression, point.expression);
  assert.ok(completion.contextLabel);
  assert.equal(completion.contextSearchTerms[0], 'artwork-1234');
  const empty = display.completion({ ...point, layerId: 'artwork-empty', layerName: '' }),
    named = display.completion({ ...point, layerId: 'artwork-named', layerName: 'Untitled' });
  assert.match(empty.contextLabel, /^Untitled/);
  assert.notEqual(empty.contextLabel, named.contextLabel);

  const local = {
    ...point,
    layerId: 'local',
    nodeId: 'curve-local',
    expression: 'ref("curve-local", "point-1.position", "x")',
  };
  assert.equal(display.completion(local), local);
  assert.equal(display.picker(local).label, local.label);
  assert.equal(display.picker(local).id, local.expression);
});
