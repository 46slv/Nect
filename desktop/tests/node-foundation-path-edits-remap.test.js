import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  connect,
  deserialize,
  enableRemapPoint,
  evaluate,
  node,
  serialize,
} from '../src/core.js';

for (const [type, key, initial, changed] of [
  ['Trim Path', 'start', 0.2, 0.4],
  ['Set Path Start', 'start', 0.25, 0.5],
  ['Dash Path', 'dashLength', 30, 40],
]) {
  test(`${type} keeps saved Remap edits and suspends them after geometry changes`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      source = node('Imported Path'),
      effect = node(type),
      remap = node('Path Remap');
    source.id = 'source';
    source.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
    effect.id = 'effect';
    effect.params[key].value = initial;
    remap.id = 'remap';
    layer.nodes.push(source, effect, remap);
    connect(layer, source.id, effect.id, 'path');
    connect(layer, effect.id, remap.id, 'path');
    const input = evaluate(layer).run(effect.id),
      point = input.contours[0].points[0];
    enableRemapPoint(remap, input, point.id).params.position.value = [3, 1];
    const active = evaluate(layer),
      edited = active.run(remap.id),
      saved = serialize(doc),
      reopened = evaluate(deserialize(saved).layers[0]);
    assert.notDeepEqual(edited.contours[0].points[0].position, point.position);
    assert.deepEqual(reopened.run(remap.id), edited);
    assert.deepEqual(active.warnings, []);
    assert.deepEqual(reopened.warnings, []);
    effect.params[key].value = changed;
    const invalidated = evaluate(layer);
    invalidated.run(remap.id);
    assert.match(invalidated.warnings.join(' '), /suspended/i);
  });
}
