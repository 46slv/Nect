import test from 'node:test';
import assert from 'node:assert/strict';
import { compareModelLightnessOrder } from '../src/key-color-lightness-order.js';

const clamp = (x) => Math.min(1, Math.max(0, x));
function fixture(aBase, aDelta, bBase, bDelta) {
  const make = (id, base, delta) => ({ id, role: id.toUpperCase(), base, delta });
  const rows = [make('a', aBase, aDelta), make('b', bBase, bDelta)];
  return {
    intent: {
      keyColor: { id: 'key', role: 'Identity' },
      preset: {
        slots: rows.map((r) => ({
          id: r.id,
          role: r.role,
          lightnessScale: 0,
          lightnessOffset: r.base,
        })),
      },
      slots: rows.map((r) => ({ id: r.id, role: r.role, deltaL: r.delta })),
    },
    resolved: [
      { id: 'key', role: 'Identity', active: true, L: 0.5 },
      ...rows.map((r) => ({
        id: r.id,
        role: r.role,
        active: true,
        baseL: r.base,
        L: clamp(r.base + r.delta),
      })),
    ],
  };
}
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

test('all five exact numeric classifications use the clamped zero-offset baseline', () => {
  for (const [args, state] of [
    [[0.2, 0, 0.8, 0], 'maintained'],
    [[0.2, 0.6, 0.8, -0.6], 'reversed'],
    [[0.2, 0.3, 0.8, -0.3], 'equalized'],
    [[0.4, 0, 0.4, 0], 'both equal'],
    [[0.4, 0.1, 0.4, 0], 'separated'],
    [[-0.2, 0.3, -0.1, 0], 'separated'],
  ]) {
    const pair = compareModelLightnessOrder(fixture(...args))[0];
    assert.equal(pair.state, state);
    if (args[0] < 0) assert.equal(pair.a.base, 0);
  }
});

test('IDs determine stable order, frozen input is not mutated, and dormant rows are excluded', () => {
  const value = fixture(0.2, 0.1, 0.8, -0.1);
  value.intent.slots.push({ id: 'dormant', role: 'Dormant', deltaL: 0 });
  value.resolved.push({ id: 'dormant', role: 'Dormant', active: false, baseL: 0.3, L: 0.3 });
  const original = JSON.stringify(value),
    first = compareModelLightnessOrder(freeze(value));
  assert.equal(first.length, 1);
  assert.deepEqual([first[0].a.id, first[0].b.id], ['a', 'b']);
  assert.equal(JSON.stringify(value), original);
  const reordered = JSON.parse(original);
  reordered.intent.slots.reverse();
  reordered.intent.preset.slots.reverse();
  reordered.resolved.reverse();
  assert.deepEqual(compareModelLightnessOrder(reordered), first);
  const oneActive = JSON.parse(original);
  oneActive.intent.preset.slots = oneActive.intent.preset.slots.slice(0, 1);
  oneActive.resolved.find((row) => row.id === 'b').active = false;
  assert.deepEqual(compareModelLightnessOrder(oneActive), []);
});

test('inconsistent, missing, duplicate, nonfinite or mismatched metadata returns unavailable', () => {
  const changes = [
    (s) => s.intent.slots.push({ ...s.intent.slots[0] }),
    (s) => s.resolved.pop(),
    (s) => {
      s.intent.preset.slots[0].role = 'wrong';
    },
    (s) => {
      s.resolved[1].baseL = NaN;
    },
    (s) => {
      s.resolved[1].baseL = 0.7;
    },
    (s) => {
      s.resolved[1].L = 0.9;
    },
    (s) => {
      s.resolved[1].active = false;
    },
    (s) => {
      s.intent.slots[0].id = '';
    },
    (s) => {
      s.intent.preset.slots = null;
    },
  ];
  for (const change of changes) {
    const value = fixture(0.2, 0, 0.8, 0);
    change(value);
    assert.equal(compareModelLightnessOrder(value), null);
  }
});
