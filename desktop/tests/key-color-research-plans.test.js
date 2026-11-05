import test from 'node:test';
import assert from 'node:assert/strict';
import { researchPlans } from '../src/key-color-research-plans.js';
import { createKeyColorRecipe } from '../src/key-color-recipe.js';

test('research candidate IDs, roles, positions and coefficients match the compared plans', () => {
  assert.deepEqual(
    researchPlans.map(({ id, version, harmony, hueSteps }) => [id, version, harmony, hueSteps]),
    [
      ['research-triadic-offset', 1, 'triadic', 12],
      ['research-neutral-start', 1, 'triadic', 12],
    ],
  );
  const data = researchPlans.map((plan) =>
    plan.slots.map((s) => [
      s.id,
      s.role,
      s.position,
      s.lightnessScale,
      s.lightnessOffset,
      s.chromaScale,
      s.chromaOffset,
      s.alpha,
    ]),
  );
  const support = [
    ['neutral', 'Neutral', 0, 0, 0.94, 0, 0, 1],
    ['text', 'Text', 0, 0, 0.14, 0, 0, 1],
  ];
  assert.deepEqual(data, [
    [
      ['assort', 'Assort', 1, 1, -0.07, 0.7, 0, 1],
      ['accent', 'Accent', 2, 1, -0.16, 0.9, 0, 1],
      ...support,
    ],
    [
      ['assort', 'Assort', 1, 0.6, 0.2, 0, 0, 1],
      ['accent', 'Accent', 2, 0.4, 0.45, 0, 0, 1],
      ...support,
    ],
  ]);
  for (const preset of researchPlans) {
    const snapshot = createKeyColorRecipe({
      paletteId: 'synthetic-only',
      keyColor: { id: 'key', role: 'Identity', value: { hex: '#3273c7', alpha: 0 }, hueHint: 0 },
      preset,
      slots: preset.slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
      settings: { areaAware: true },
    });
    assert.equal(snapshot.resolved.length, 5);
    assert.deepEqual(snapshot.resolved[0].value, { hex: '#3273c7', alpha: 0 });
    assert.ok(snapshot.resolved.slice(1).every((row) => row.value.alpha === 1));
  }
});
