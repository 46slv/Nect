import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { deserialize, evaluate, exportSVG } from '../src/core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
  activeCompositionDocument,
} from '../src/composition-project-model.js';
const load = (mode) =>
  deserializeCompositionProject(
    fs.readFileSync(new URL(`../examples/shared-color-${mode}.json`, import.meta.url), 'utf8'),
    deserialize,
  );
const fill = (p, i) =>
  p.compositions[i].layers[0].nodes.find((n) => n.type === 'Fill').params.color;
const svg = (p, c) => {
  const d = activeCompositionDocument(p, c.id);
  for (const l of d.layers) assert.equal(evaluate(l, {}, d.imageAssets, d).error, null);
  const result = exportSVG(d);
  assert.match(result, /<svg/);
  assert.match(result, /#cccccc/);
  assert.match(result, /#4939ba/);
  return result;
};
for (const mode of ['value', 'reference'])
  test(`${mode} example roundtrips and renders both Compositions`, () => {
    const p = load(mode);
    const reopened = deserializeCompositionProject(serializeCompositionProject(p), deserialize);
    assert.deepEqual(reopened, p);
    assert.equal(p.compositions.length, 2);
    assert.equal(p.colorSlots.length, 2);
    assert.notEqual(p.colorSlots[0].id, p.colorSlots[1].id);
    assert.deepEqual(p.colorSlots[0].value, p.colorSlots[1].value);
    for (const c of p.compositions) assert.equal(svg(p, c), svg(reopened, c));
    assert.equal(fill(p, 0).colorSlotId, p.colorSlots[0].id);
    assert.deepEqual(fill(p, 1).value, { hex: '#cccccc', alpha: 1 });
    if (mode === 'value') assert.equal(Object.hasOwn(fill(p, 1), 'colorSlotId'), false);
    else assert.equal(fill(p, 1).colorSlotId, p.colorSlots[0].id);
  });
test('changing original slot affects only reference users, not equal-color imported slot or literal', () => {
  for (const mode of ['value', 'reference']) {
    const p = load(mode);
    const before = p.compositions.map((c) => svg(p, c));
    p.colorSlots[0].value = { hex: '#808080', alpha: 1 };
    const after = p.compositions.map((c) => exportSVG(activeCompositionDocument(p, c.id)));
    assert.notEqual(after[0], before[0]);
    if (mode === 'value') assert.equal(after[1], before[1]);
    else assert.notEqual(after[1], before[1]);
    assert.deepEqual(p.colorSlots[1].value, { hex: '#cccccc', alpha: 1 });
  }
});
