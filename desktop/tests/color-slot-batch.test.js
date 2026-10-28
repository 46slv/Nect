import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const original = { hex: '#123456', alpha: 0.25 };
const changed = { hex: '#aBcD09', alpha: 0.123456789 };
const transparent = { hex: '#000000', alpha: 0 };

function fixture(kind = 'document') {
  const document = blankDocument();
  const layer = document.layers[0];
  const shape = node('Rectangle');
  const directFill = node('Fill');
  const color = node('Color');
  const sourcedFill = node('Fill');
  const output = layer.nodes[0];
  layer.nodes = [shape, directFill, color, sourcedFill, output];
  connect(layer, shape.id, directFill.id, 'path');
  connect(layer, shape.id, sourcedFill.id, 'path');
  connect(layer, color.id, sourcedFill.id, 'color');
  connect(layer, directFill.id, output.id, 'path');
  const session =
    kind === 'document'
      ? new CommandSession(document)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document));
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const create = (name = 'Brand', value = original) =>
    send('create-color-slot', { name, value }).slotId;
  const ids = [create('First'), create('Second'), create('Unchanged')];
  send('bind-color-slot', {
    layerId: layer.id,
    nodeId: directFill.id,
    key: 'color',
    slotId: ids[0],
  });
  send('bind-color-slot', { layerId: layer.id, nodeId: color.id, key: 'value', slotId: ids[1] });
  const values = () => session.snapshot().colorSlots;
  const run = () => {
    const current = session.historyView.doc;
    return evaluate(current.layers[0], {}, current.imageAssets, current);
  };
  return { session, send, create, ids, values, run, layer, directFill, color, sourcedFill };
}

function stateOf(session) {
  return {
    document: serialize(session.snapshot()),
    project: session.projectSnapshot
      ? serializeCompositionProject(session.projectSnapshot())
      : null,
    state: session.state,
    projectRevision: session.projectRevision,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}
const updatesFor = (f) => [
  { slotId: f.ids[1], value: transparent },
  { slotId: f.ids[0], value: changed },
];

for (const kind of ['document', 'composition']) {
  test(`${kind}: batch updates stable shared identities in exactly one Undo/Redo step`, () => {
    const f = fixture(kind);
    const before = stateOf(f.session);
    const slots = f.values();
    const result = f.send('set-color-slots', { updates: updatesFor(f) });
    const after = stateOf(f.session);
    assert.equal(result.changed, true);
    assert.deepEqual(result.slotIds, [f.ids[1], f.ids[0]]);
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    assert.equal(after.past.length, before.past.length + 1);
    assert.equal(after.future.length, 0);
    assert.deepEqual(
      f.values().map((slot) => ({ id: slot.id, name: slot.name })),
      slots.map((slot) => ({ id: slot.id, name: slot.name })),
    );
    assert.deepEqual(
      f.values().map((slot) => slot.value),
      [changed, transparent, original],
    );
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), JSON.parse(before.document));
    if (before.project)
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), before.project);
    f.send('redo');
    assert.deepEqual(f.session.snapshot(), JSON.parse(after.document));
    if (after.project) {
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), after.project);
      const reopened = createCompositionSessionOwner(deserializeCompositionProject(after.project));
      assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after.project);
    } else
      assert.equal(
        serialize(new CommandSession(deserialize(after.document)).snapshot()),
        after.document,
      );
  });

  test(`${kind}: empty and unchanged batches preserve revision, history and pending Redo`, () => {
    const f = fixture(kind);
    f.send('set-color-slots', { updates: updatesFor(f) });
    f.send('undo');
    const before = stateOf(f.session);
    for (const updates of [
      [],
      [{ slotId: f.ids[0], value: { ...original } }],
      f.ids.map((slotId) => ({ slotId, value: { ...original } })),
    ]) {
      assert.equal(f.send('set-color-slots', { updates }).changed, false);
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    assert.deepEqual(
      f.values().map((slot) => slot.value),
      [changed, transparent, original],
    );
  });

  test(`${kind}: rejected batches preflight every row without mutation or consuming Redo`, () => {
    const f = fixture(kind);
    f.send('set-color-slots', { updates: updatesFor(f) });
    f.send('undo');
    const before = stateOf(f.session);
    const good = { slotId: f.ids[0], value: changed };
    const invalidValues = [
      null,
      '#123456',
      [],
      { hex: '#fff', alpha: 1 },
      { hex: '#gg0000', alpha: 1 },
      { hex: '#123456' },
      { hex: '#123456', alpha: -0.1 },
      { hex: '#123456', alpha: 1.1 },
      { hex: '#123456', alpha: NaN },
      { hex: '#123456', alpha: Infinity },
      { hex: '#123456', alpha: 1, extra: true },
    ];
    const sparse = [good];
    sparse.length = 2;
    const rejected = [
      undefined,
      null,
      {},
      'updates',
      [good, null],
      [good, undefined],
      sparse,
      [good, { slotId: 'missing', value: changed }],
      [good, { slotId: f.ids[0], value: original }],
      [good, { value: changed }],
      [good, { slotId: 123, value: changed }],
      Array.from({ length: 257 }, () => good),
      ...invalidValues.map((value) => [good, { slotId: f.ids[1], value }]),
    ];
    for (const updates of rejected) {
      assert.throws(() => f.send('set-color-slots', { updates }));
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    assert.deepEqual(
      f.values().map((slot) => slot.value),
      [changed, transparent, original],
    );
  });

  test(`${kind}: stale revision/session envelopes cannot batch edit or change history`, () => {
    const f = fixture(kind);
    const stale = f.session.state;
    f.send('set-color-slots', { updates: updatesFor(f) });
    const before = stateOf(f.session);
    assert.throws(
      () =>
        f.session.dispatch({
          ...stale,
          type: 'set-color-slots',
          payload: { updates: updatesFor(f) },
        }),
      /STALE_REVISION/,
    );
    assert.deepEqual(stateOf(f.session), before);
    assert.throws(
      () =>
        f.session.dispatch({
          ...f.session.state,
          sessionId: 'wrong-session',
          type: 'set-color-slots',
          payload: { updates: updatesFor(f) },
        }),
      /STALE_SESSION/,
    );
    assert.deepEqual(stateOf(f.session), before);
  });

  test(`${kind}: batch invalidates warm bound Fill and upstream Color-node evaluation`, () => {
    const f = fixture(kind);
    const fallback = f.session.snapshot().layers[0].nodes.map((node) => node.params);
    const initial = f.run();
    assert.deepEqual(initial.run(f.directFill.id).fill, original);
    assert.deepEqual(initial.run(f.sourcedFill.id).fill, original);
    const warm = f.run();
    warm.run(f.directFill.id);
    warm.run(f.sourcedFill.id);
    assert.equal(warm.stats.computed, 0);
    const updates = structuredClone(updatesFor(f));
    f.send('set-color-slots', { updates });
    const updated = f.run();
    assert.deepEqual(updated.parameterValue(f.directFill.id, 'color'), changed);
    assert.deepEqual(updated.run(f.directFill.id).fill, changed);
    assert.deepEqual(updated.run(f.color.id), transparent);
    assert.deepEqual(updated.run(f.sourcedFill.id).fill, transparent);
    assert.ok(updated.stats.computed > 0);
    assert.deepEqual(
      f.session.snapshot().layers[0].nodes.map((node) => node.params),
      fallback,
    );
    // A caller retaining its payload cannot mutate authored state after dispatch.
    updates[0].value.hex = '#ffffff';
    updates[0].value.alpha = 1;
    assert.deepEqual(f.values()[1].value, transparent);
    f.send('undo');
    assert.deepEqual(f.run().run(f.sourcedFill.id).fill, original);
    f.send('redo');
    assert.deepEqual(f.run().run(f.sourcedFill.id).fill, transparent);
  });
}

test('batch accepts the exact 256-update budget without creating/removing/reordering identities', () => {
  const f = fixture();
  for (let i = f.ids.length; i < 256; i++) f.ids.push(f.create(`Color ${i}`));
  const before = stateOf(f.session);
  const updates = [...f.ids]
    .reverse()
    .map((slotId, i) => ({ slotId, value: { hex: '#abcdef', alpha: i / 255 } }));
  const result = f.send('set-color-slots', { updates });
  assert.equal(result.changed, true);
  assert.equal(f.session.state.expectedRevision, before.state.expectedRevision + 1);
  assert.deepEqual(
    f.values().map((slot) => slot.id),
    f.ids,
  );
  assert.deepEqual(
    f.values().map((slot) => slot.value),
    [...updates].reverse().map((update) => update.value),
  );
  f.send('undo');
  assert.equal(serialize(f.session.snapshot()), before.document);
});

test('Composition batch changes the shared registry even when the bound composition is inactive', () => {
  const f = fixture('composition');
  const boundId = f.session.activeCompositionId;
  f.session.createComposition({ name: 'Unbound sibling' });
  const before = f.session.projectSnapshot();
  f.send('set-color-slots', { updates: updatesFor(f) });
  const after = f.session.projectSnapshot();
  assert.deepEqual(after.compositions, before.compositions);
  assert.notDeepEqual(after.colorSlots, before.colorSlots);
  f.send('undo');
  assert.deepEqual(f.session.projectSnapshot(), before);
  f.send('redo');
  assert.deepEqual(f.session.projectSnapshot(), after);
  f.session.switchComposition(boundId);
  assert.deepEqual(f.run().run(f.directFill.id).fill, changed);
  assert.deepEqual(f.run().run(f.sourcedFill.id).fill, transparent);
});
