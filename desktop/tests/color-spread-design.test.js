import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, serialize, deserialize } from '../src/core.js';
import { proposeAutoChroma } from '../src/auto-chroma.js';
import { resolveHarmonyPalette } from '../src/harmony-wheel.js';

function fixture(version) {
  const session = new CommandSession(document()),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  if (version === 2) send('create-harmony-palette');
  else {
    for (const name of ['Primary', 'Secondary'])
      send('create-color-slot', { name, value: { hex: '#2468ab', alpha: 0.25 } });
    send('set-color-slots', {
      updates: [],
      design: {
        version: 1,
        profile: 'balanced',
        colorfulness: 0.72,
        areaAware: true,
        lockedSlotIds: [],
      },
    });
  }
  const values = () =>
    session.snapshot().colorSlots.map((slot) => ({
      slotId: slot.id,
      value: { ...slot.value },
    }));
  return { session, send, values };
}
const saved = (session) => ({
  text: serialize(session.snapshot()),
  state: session.state,
  past: structuredClone(session.historyView.past),
  future: structuredClone(session.historyView.future),
});
const options = (design) => ({
  profile: design.profile,
  colorfulness: design.colorfulness,
  areaAware: design.areaAware,
});

for (const version of [1, 2]) {
  test(`v${version} missing Spread keeps exact serialized shape and version`, () => {
    const f = fixture(version),
      before = saved(f.session),
      design = f.session.snapshot().colorDesign,
      schemaVersion = f.session.snapshot().schemaVersion;
    assert.equal(Object.hasOwn(design, 'spread'), false);
    assert.equal(schemaVersion, version === 1 ? 55 : 56);
    assert.equal(serialize(deserialize(before.text)), before.text);
    assert.equal(Object.hasOwn(deserialize(before.text).colorDesign, 'spread'), false);
    f.send('set-color-slots', { updates: f.values(), design });
    assert.deepEqual(saved(f.session), before, 'omitted Spread is not a migration or an edit');
    assert.equal(f.session.snapshot().schemaVersion, schemaVersion);
  });

  test(`v${version} Spread persists as one edit with exact Undo/Redo and serialize/reopen`, () => {
    const f = fixture(version);
    for (const spread of [0, 0.25, 0.62, 1]) {
      const before = saved(f.session),
        design = { ...f.session.snapshot().colorDesign, spread },
        schemaVersion = f.session.snapshot().schemaVersion;
      f.send('set-color-slots', { updates: f.values(), design });
      const after = serialize(f.session.snapshot());
      assert.equal(f.session.snapshot().colorDesign.spread, spread);
      assert.equal(f.session.snapshot().schemaVersion, schemaVersion, 'no schema version rewrite');
      assert.equal(f.session.state.expectedRevision, before.state.expectedRevision + 1);
      assert.equal(f.session.historyView.past.length, before.past.length + 1);
      assert.equal(serialize(deserialize(after)), after);
      assert.equal(new CommandSession(deserialize(after)).snapshot().colorDesign.spread, spread);
      f.send('undo');
      assert.equal(serialize(f.session.snapshot()), before.text);
      f.send('redo');
      assert.equal(serialize(f.session.snapshot()), after);
    }
  });

  test(`v${version} invalid own Spread fields and unknown design keys reject atomically`, () => {
    const f = fixture(version),
      before = saved(f.session),
      updates = f.values();
    updates[0].value = { hex: '#ff0033', alpha: 0.75 };
    for (const spread of [
      undefined,
      null,
      NaN,
      Infinity,
      -Infinity,
      -0.01,
      1.01,
      '0.62',
      {},
      [],
      true,
    ]) {
      const design = { ...f.session.snapshot().colorDesign, spread };
      assert.equal(Object.hasOwn(design, 'spread'), true);
      assert.throws(() => f.send('set-color-slots', { updates, design }));
      assert.deepEqual(saved(f.session), before, `rejected own Spread ${String(spread)}`);
    }
    for (const extra of [{ unknown: 0.62 }, { Spread: 0.62 }]) {
      const design = { ...f.session.snapshot().colorDesign, spread: 0.62, ...extra };
      assert.throws(() => f.send('set-color-slots', { updates, design }));
      assert.deepEqual(saved(f.session), before, 'the design schema remains otherwise closed');
    }
  });
}

test('missing Spread uses the pure proposal default 0.62 without changing coefficients', () => {
  const descriptors = [
    { id: 'primary', L: 0.5, h: 18, role: 'Primary', alpha: 0.25 },
    { id: 'secondary', L: 0.5, h: 138, role: 'Secondary', alpha: 0.75 },
    { id: 'accent', L: 0.5, h: 258, role: 'Accent', alpha: 1 },
  ];
  for (const version of [1, 2]) {
    const f = fixture(version),
      design = f.session.snapshot().colorDesign,
      settings = options(design);
    assert.deepEqual(
      proposeAutoChroma(descriptors, settings),
      proposeAutoChroma(descriptors, { ...settings, spread: 0.62 }),
    );
    if (version === 2) {
      const implicit = resolveHarmonyPalette(design.wheel, design.palette, settings),
        explicit = resolveHarmonyPalette(design.wheel, design.palette, {
          ...settings,
          spread: 0.62,
        });
      assert.deepEqual(implicit, explicit);
      assert.deepEqual(
        implicit.colors.map((row) => ({ id: row.id, value: row.value })),
        f.session.snapshot().colorSlots.map((slot) => ({ id: slot.id, value: slot.value })),
      );
    }
  }
});

for (const spread of [0, 1]) {
  test(`Add harmony color retains saved Spread ${spread} and uses it for the new literal`, () => {
    const f = fixture(2),
      design = { ...f.session.snapshot().colorDesign, spread };
    f.send('set-color-slots', { updates: f.values(), design });
    const before = f.session.snapshot(),
      beforeText = serialize(before),
      result = f.send('add-harmony-color', { kind: 'chromatic' }),
      after = f.session.snapshot(),
      entry = after.colorDesign.palette.find((item) => item.id === result.slotId),
      literal = after.colorSlots.find((slot) => slot.id === result.slotId).value,
      expected = resolveHarmonyPalette(after.colorDesign.wheel, [entry], {
        ...options(after.colorDesign),
        spread,
      }).colors[0].value,
      legacyDefault = resolveHarmonyPalette(after.colorDesign.wheel, [entry], {
        ...options(after.colorDesign),
        spread: 0.62,
      }).colors[0].value;
    assert.equal(after.colorDesign.spread, spread);
    assert.equal(after.schemaVersion, before.schemaVersion);
    assert.deepEqual(literal, expected);
    assert.notDeepEqual(
      literal,
      legacyDefault,
      'the fixture detects an accidentally dropped Spread',
    );
    assert.deepEqual(after.colorSlots.slice(0, -1), before.colorSlots);
    assert.deepEqual(after.colorDesign.palette.slice(0, -1), before.colorDesign.palette);
    assert.equal(serialize(deserialize(serialize(after))), serialize(after));
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), beforeText);
    f.send('redo');
    assert.deepEqual(f.session.snapshot(), after);
  });
}

test('Add without Spread retains old shape and generates the exact default-0.62 color', () => {
  const f = fixture(2),
    before = f.session.snapshot(),
    result = f.send('add-harmony-color', { kind: 'chromatic' }),
    after = f.session.snapshot(),
    entry = after.colorDesign.palette.find((item) => item.id === result.slotId),
    expected = resolveHarmonyPalette(after.colorDesign.wheel, [entry], {
      ...options(after.colorDesign),
      spread: 0.62,
    }).colors[0].value;
  assert.equal(Object.hasOwn(after.colorDesign, 'spread'), false);
  assert.equal(after.schemaVersion, before.schemaVersion);
  assert.deepEqual(after.colorSlots.find((slot) => slot.id === result.slotId).value, expected);
  assert.deepEqual(after.colorSlots.slice(0, -1), before.colorSlots);
});
