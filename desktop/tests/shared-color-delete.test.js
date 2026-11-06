import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, CommandSession, deserialize, node, serialize } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';

const value = { hex: '#12aBcD', alpha: 0.5 };
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
const captured = (owner) => ({
  snapshot: owner.projectSnapshot ? owner.projectSnapshot() : owner.snapshot(),
  state: { ...owner.state },
  past: structuredClone(owner.historyView.past),
  future: structuredClone(owner.historyView.future),
});

function documentFixture() {
  const doc = blankDocument();
  const color = node('Color');
  doc.layers[0].nodes.unshift(color);
  const owner = new CommandSession(doc);
  const used = send(owner, 'create-color-slot', { name: 'Used', value }).slotId;
  const unused = send(owner, 'create-color-slot', { name: 'Unused', value }).slotId;
  send(owner, 'bind-color-slot', {
    layerId: doc.layers[0].id,
    nodeId: color.id,
    key: 'value',
    slotId: used,
  });
  return { owner, used, unused, color, layerId: doc.layers[0].id };
}

test('delete an unused shared color as one undoable edit without changing other identities', () => {
  const { owner, used, unused } = documentFixture();
  const before = owner.snapshot();
  const revision = owner.state.expectedRevision;
  send(owner, 'delete-color-slot', { slotId: unused });
  const after = owner.snapshot();
  assert.equal(owner.state.expectedRevision, revision + 1);
  assert.deepEqual(
    after.colorSlots,
    before.colorSlots.filter((slot) => slot.id !== unused),
  );
  assert.equal(after.colorSlots[0].id, used);
  assert.deepEqual(deserialize(serialize(after)), after);
  send(owner, 'undo');
  assert.deepEqual(owner.snapshot(), before);
  send(owner, 'redo');
  assert.deepEqual(owner.snapshot(), after);
});

test('bindings and saved node presets reject deletion without changing the envelope or Redo', () => {
  const { owner, used, color, layerId } = documentFixture();
  send(owner, 'set-color-slot', { slotId: used, value: { hex: '#112233', alpha: 1 } });
  send(owner, 'undo');
  const before = captured(owner);
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: used }), /use|bound|link/i);
  assert.deepEqual(captured(owner), before);

  send(owner, 'save-node-preset', { layerId, nodeIds: [color.id], name: 'Saved color' });
  send(owner, 'unbind-color-slot', { layerId, nodeId: color.id, key: 'value' });
  const presetBefore = captured(owner);
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: used }), /preset/i);
  assert.deepEqual(captured(owner), presetBefore);
});

test('an inactive Composition binding rejects before replacing the current Session or Redo', () => {
  const f = documentFixture();
  const project = migrateLegacyDocumentToCompositionProject(f.owner.snapshot());
  const owner = createCompositionSessionOwner(project);
  owner.createComposition({ name: 'Second' });
  send(owner, 'set-document-settings', { key: 'width', value: owner.snapshot().width + 1 });
  send(owner, 'undo');
  const before = captured(owner);
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: f.used }), /use|bound|link/i);
  assert.deepEqual(captured(owner), before);
  send(owner, 'redo');
  assert.equal(owner.snapshot().width, before.snapshot.compositions[1].width + 1);
  assert.doesNotThrow(() =>
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
});

test('prepared deletion rejects an inactive binding before calling the bridge', async () => {
  const f = documentFixture();
  const owner = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(f.owner.snapshot()),
  );
  owner.createComposition({ name: 'Second' });
  send(owner, 'set-document-settings', { key: 'width', value: owner.snapshot().width + 1 });
  send(owner, 'undo');
  const before = captured(owner);
  let bridgeUsed = false;
  await assert.rejects(
    owner.prepareCommand(
      { ...owner.state, type: 'delete-color-slot', payload: { slotId: f.used } },
      {
        prepareDocument() {
          bridgeUsed = true;
        },
      },
    ),
    /use|bound|link/i,
  );
  assert.equal(bridgeUsed, false);
  assert.deepEqual(captured(owner), before);
});

test('retained recipe and legacy palette references prevent deletion; unrelated slots remain editable', () => {
  const doc = retainedRecipeFixture();
  for (const n of doc.layers[0].nodes)
    for (const q of Object.values(n.params || {})) delete q.colorSlotId;
  const owner = new CommandSession(doc);
  const before = captured(owner);
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: 'key' }), /palette|recipe/i);
  assert.deepEqual(captured(owner), before);
  send(owner, 'delete-color-slot', { slotId: 'other' });
  assert.equal(
    owner.snapshot().colorSlots.some((slot) => slot.id === 'other'),
    false,
  );
  send(owner, 'undo');
  assert.deepEqual(owner.snapshot(), before.snapshot);
});

test('legacy palette membership and locks are protected independently of a recipe', () => {
  const doc = retainedRecipeFixture();
  delete doc.keyColorRecipe;
  for (const n of doc.layers[0].nodes)
    for (const q of Object.values(n.params || {})) delete q.colorSlotId;
  const owner = new CommandSession(doc);
  const before = captured(owner);
  for (const slotId of ['key', 'dormant']) {
    assert.throws(() => send(owner, 'delete-color-slot', { slotId }), /palette|locked/i);
    assert.deepEqual(captured(owner), before);
  }
});

test('live and detached recipe slots stay protected until the retained recipe is explicitly cleared', () => {
  const owner = new CommandSession(blankDocument());
  send(owner, 'initialize-live-key-color-recipe', {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'delete-test-palette',
    intent: {
      keyColor: {
        id: 'delete-test-key',
        role: 'Identity',
        value: { ...value, alpha: 1 },
        hueHint: 17,
      },
      preset: {
        id: 'test-only-coefficients',
        version: 1,
        harmony: 'triadic',
        hueSteps: 12,
        slots: [
          {
            id: 'delete-test-derived',
            role: 'Fixture',
            position: 1,
            lightnessScale: 0,
            lightnessOffset: 0.8,
            chromaScale: 0,
            chromaOffset: 0,
            alpha: 1,
          },
        ],
      },
      slots: [{ id: 'delete-test-derived', role: 'Fixture', deltaL: 0, deltaRatioCmax: 0 }],
      settings: { areaAware: false },
    },
    allocations: [
      { slotId: 'delete-test-key', name: 'Key' },
      { slotId: 'delete-test-derived', name: 'Derived' },
    ],
  });
  const live = captured(owner);
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: 'delete-test-key' }), /recipe/i);
  assert.deepEqual(captured(owner), live);
  send(owner, 'detach-live-key-color-recipe', {
    expectedPaletteRevision: owner.snapshot().keyColorRecipe.committedSnapshot.revision,
  });
  assert.throws(() => send(owner, 'delete-color-slot', { slotId: 'delete-test-key' }), /recipe/i);
  send(owner, 'clear-key-color-recipe-snapshot');
  send(owner, 'delete-color-slot', { slotId: 'delete-test-key' });
  assert.equal(
    owner.snapshot().colorSlots.some((slot) => slot.id === 'delete-test-key'),
    false,
  );
});

test('unrelated metadata is preserved and does not count as a Color binding', () => {
  const f = documentFixture();
  const doc = f.owner.snapshot();
  doc.layers[0].nodes.find((item) => item.id === f.color.id).metadata = {
    colorSlotId: f.unused,
  };
  const owner = new CommandSession(doc);
  send(owner, 'delete-color-slot', { slotId: f.unused });
  assert.deepEqual(
    owner.snapshot().layers[0].nodes.find((item) => item.id === f.color.id).metadata,
    {
      colorSlotId: f.unused,
    },
  );
  assert.doesNotThrow(() => deserialize(serialize(owner.snapshot())));
});

test('the last unused color can be removed and the old schema is not downgraded', () => {
  const owner = new CommandSession(blankDocument());
  const slotId = send(owner, 'create-color-slot', { name: 'Only', value }).slotId;
  const schemaVersion = owner.snapshot().schemaVersion;
  send(owner, 'delete-color-slot', { slotId });
  assert.deepEqual(owner.snapshot().colorSlots, []);
  assert.equal(owner.snapshot().schemaVersion, schemaVersion);
  assert.doesNotThrow(() => deserialize(serialize(owner.snapshot())));
  send(owner, 'undo');
  assert.equal(owner.snapshot().colorSlots[0].id, slotId);
});

test('missing IDs and stale envelopes reject without consuming Redo', () => {
  const f = documentFixture();
  const owner = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(f.owner.snapshot()),
  );
  send(owner, 'set-document-settings', { key: 'width', value: owner.snapshot().width + 1 });
  send(owner, 'undo');
  const before = captured(owner);
  assert.throws(
    () => send(owner, 'delete-color-slot', { slotId: 'missing' }),
    /no longer available/i,
  );
  assert.deepEqual(captured(owner), before);
  assert.throws(
    () =>
      owner.dispatch({
        ...owner.state,
        sessionId: 'stale-session',
        type: 'delete-color-slot',
        payload: { slotId: f.unused },
      }),
    /STALE_SESSION/,
  );
  assert.deepEqual(captured(owner), before);
});
