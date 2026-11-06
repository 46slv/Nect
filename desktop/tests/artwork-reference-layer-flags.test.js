import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function fixture(kind = 'document') {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Source'),
    sink = node('Value'),
    local = node('Value'),
    curve = node('Curve');
  target.id = 'target';
  source.id = 'source';
  sink.id = 'sink';
  local.id = 'local';
  curve.id = 'curve';
  curve.points[0].id = 'point';
  curve.points[0].position.value = [12, 24];
  sink.params.value.expression = 'ref(artwork, "source", "curve", "point.position", "x")';
  target.nodes.push(local, sink);
  source.nodes.push(curve);
  doc.layers.push(source);
  doc.schemaVersion = 62;
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const read = () => {
    const current = session.snapshot();
    return evaluate(current.layers[0], {}, {}, current).parameterValue(sink.id, 'value');
  };
  return { session, send, read, source, curve, sink };
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: presentation flags preserve foreign references, history and saved data`, () => {
    const f = fixture(kind),
      original = f.session.snapshot(),
      nodes = JSON.stringify(original.layers.map((layer) => layer.nodes));
    assert.equal(f.read(), 12);
    f.send('set-layer-state', { layerId: 'source', key: 'visible', value: false });
    assert.equal(f.read(), 12);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), original);
    const future = f.session.historyView.future;
    f.send('set-layer-state', { layerId: 'source', key: 'visible', value: true });
    assert.deepEqual(f.session.historyView.future, future);
    f.send('redo');
    assert.equal(f.session.snapshot().layers[1].visible, false);
    for (const [key, value] of [
      ['locked', true],
      ['locked', false],
      ['guide', true],
      ['solo', true],
    ])
      f.send('set-layer-state', { layerId: 'source', key, value });
    f.send('clear-layer-solo');
    const result = f.session.snapshot();
    assert.equal(result.layers[1].guide, true);
    assert.equal(result.layers[1].solo, false);
    assert.equal(JSON.stringify(result.layers.map((layer) => layer.nodes)), nodes);
    assert.equal(f.read(), 12);
    assert.equal(serialize(deserialize(serialize(result))), serialize(result));
  });
}

test('reentrant flag payload cannot restore a removed foreign source over the committed edit', () => {
  const f = fixture();
  let triggered = false,
    afterDelete,
    stateAfterDelete;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        f.send('delete-node', { layerId: 'source', nodeId: 'curve' });
        afterDelete = f.session.snapshot();
        stateAfterDelete = f.session.state;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_REVISION/i);
  assert.equal(triggered, true);
  assert.deepEqual(f.session.snapshot(), afterDelete);
  assert.deepEqual(f.session.state, stateAfterDelete);
  assert.throws(() => f.read(), /missing/i);
});

test('reentrant unrelated edit cannot be overwritten by a stale flag candidate', () => {
  const f = fixture();
  let triggered = false,
    afterEdit,
    stateAfterEdit;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        f.send('set-document-settings', { key: 'width', value: 961 });
        afterEdit = f.session.snapshot();
        stateAfterEdit = f.session.state;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_REVISION/i);
  assert.equal(triggered, true);
  assert.deepEqual(f.session.snapshot(), afterEdit);
  assert.deepEqual(f.session.state, stateAfterEdit);
  assert.equal(f.session.snapshot().width, 961);
  assert.equal(f.session.snapshot().layers[1].visible, true);
});

test('a nested commit is notified once even when the stale outer command fails', () => {
  const f = fixture(),
    events = [];
  const session = new CommandSession(f.session.snapshot(), {
    onCommitted: (state) => events.push(state.expectedRevision),
  });
  let triggered = false;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        session.dispatch({
          ...session.state,
          type: 'set-document-settings',
          payload: { key: 'width', value: 961 },
        });
      }
      return 'visible';
    },
  };
  assert.throws(
    () => session.dispatch({ ...session.state, type: 'set-layer-state', payload }),
    /STALE_REVISION/i,
  );
  assert.deepEqual(events, [1]);
  assert.equal(session.snapshot().width, 961);
});

test('an observer-triggered second commit still notifies each revision once', () => {
  const f = fixture(),
    events = [];
  let session;
  session = new CommandSession(f.session.snapshot(), {
    onCommitted: (state) => {
      events.push(state.expectedRevision);
      if (state.expectedRevision === 1)
        session.dispatch({
          ...session.state,
          type: 'set-document-settings',
          payload: { key: 'width', value: 961 },
        });
    },
  });
  session.dispatch({
    ...session.state,
    type: 'set-layer-state',
    payload: { layerId: 'source', key: 'visible', value: false },
  });
  assert.deepEqual(events, [1, 2]);
  assert.equal(session.snapshot().width, 961);
  assert.equal(session.snapshot().layers[1].visible, false);
});

test('reentrant precommit callback cannot overwrite its newer edit', () => {
  const f = fixture();
  let session,
    armed = false,
    afterEdit,
    stateAfterEdit;
  session = new CommandSession(f.session.snapshot(), {
    beforeCandidateCommit: () => {
      if (!armed) return;
      armed = false;
      session.dispatch({
        ...session.state,
        type: 'set-document-settings',
        payload: { key: 'width', value: 961 },
      });
      afterEdit = session.snapshot();
      stateAfterEdit = session.state;
    },
  });
  armed = true;
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-layer-state',
        payload: { layerId: 'source', key: 'visible', value: false },
      }),
    /STALE_REVISION/i,
  );
  assert.deepEqual(session.snapshot(), afterEdit);
  assert.deepEqual(session.state, stateAfterEdit);
  assert.equal(session.snapshot().width, 961);
  assert.equal(session.snapshot().layers[1].visible, true);
});

test('a Composition switch during flag input preserves the new owner and pending Redo', () => {
  const f = fixture('composition'),
    owner = f.session,
    first = owner.activeCompositionId,
    second = owner.createComposition({ name: 'Second' });
  owner.switchComposition(first);
  owner.renameComposition(second, 'Renamed');
  f.send('undo');
  const before = owner.projectSnapshot(),
    revision = owner.projectRevision,
    futureLength = owner.historyView.future.length;
  let switchedState;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!switchedState) {
        owner.switchComposition(second);
        switchedState = owner.state;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_SESSION/i);
  assert.equal(owner.activeCompositionId, second);
  assert.deepEqual(owner.state, switchedState);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.projectRevision, revision);
  assert.equal(owner.historyView.future.length, futureLength);
  f.send('redo');
  assert.equal(owner.projectSnapshot().compositions.find((c) => c.id === second).name, 'Renamed');
});

test('a nested Composition edit keeps its committed owner envelope after the outer flag is stale', () => {
  const f = fixture('composition'),
    owner = f.session;
  f.send('copy-nodes', { layerId: 'target', nodeIds: ['local'] });
  let triggered = false,
    afterEdit,
    stateAfterEdit,
    revisionAfterEdit;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        f.send('set-document-settings', { key: 'width', value: 961 });
        afterEdit = owner.projectSnapshot();
        stateAfterEdit = owner.state;
        revisionAfterEdit = owner.projectRevision;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_REVISION/i);
  assert.deepEqual(owner.projectSnapshot(), afterEdit);
  assert.deepEqual(owner.state, stateAfterEdit);
  assert.equal(owner.projectRevision, revisionAfterEdit);
  assert.equal(f.send('paste-nodes', { layerId: 'target' }).nodeIds.length, 1);
});

test('a nested source deletion keeps the owner and its dangling reference', () => {
  const f = fixture('composition'),
    owner = f.session;
  let triggered = false,
    afterDelete,
    stateAfterDelete;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        f.send('delete-node', { layerId: 'source', nodeId: 'curve' });
        afterDelete = owner.projectSnapshot();
        stateAfterDelete = owner.state;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_REVISION/i);
  assert.deepEqual(owner.projectSnapshot(), afterDelete);
  assert.deepEqual(owner.state, stateAfterDelete);
  assert.throws(() => f.read(), /missing/i);
});

test('a nested edit followed by a Composition switch keeps the newer active session', () => {
  const f = fixture('composition'),
    owner = f.session,
    first = owner.activeCompositionId,
    second = owner.createComposition({ name: 'Second' });
  owner.switchComposition(first);
  let triggered = false,
    afterSwitch,
    stateAfterSwitch,
    revisionAfterSwitch;
  const payload = {
    layerId: 'source',
    value: false,
    get key() {
      if (!triggered) {
        triggered = true;
        f.send('set-document-settings', { key: 'width', value: 961 });
        owner.switchComposition(second);
        afterSwitch = owner.projectSnapshot();
        stateAfterSwitch = owner.state;
        revisionAfterSwitch = owner.projectRevision;
      }
      return 'visible';
    },
  };
  assert.throws(() => f.send('set-layer-state', payload), /STALE_REVISION/i);
  assert.equal(owner.activeCompositionId, second);
  assert.deepEqual(owner.projectSnapshot(), afterSwitch);
  assert.deepEqual(owner.state, stateAfterSwitch);
  assert.equal(owner.projectRevision, revisionAfterSwitch);
});

test('the owner also rejects a stale non-flag edit before the inner commit', () => {
  const f = fixture('composition'),
    owner = f.session,
    first = owner.activeCompositionId,
    second = owner.createComposition({ name: 'Second' });
  owner.switchComposition(first);
  const before = owner.projectSnapshot(),
    revision = owner.projectRevision;
  let switched = false;
  const payload = {
    layerId: 'target',
    nodeId: 'sink',
    get name() {
      if (!switched) {
        switched = true;
        owner.switchComposition(second);
      }
      return 'Renamed';
    },
  };
  assert.throws(() => f.send('rename', payload), /STALE_SESSION/i);
  assert.equal(owner.activeCompositionId, second);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.projectRevision, revision);
});

test('flag accessors are read once and cannot substitute a different field or value', () => {
  const f = fixture(),
    original = f.session.snapshot(),
    replacement = structuredClone(original.layers[1].nodes);
  replacement.find((n) => n.id === 'curve').points[0].position.value = [99, 24];
  let keyReads = 0,
    valueReads = 0;
  f.send('set-layer-state', {
    layerId: 'source',
    get key() {
      return keyReads++ === 0 ? 'visible' : 'nodes';
    },
    get value() {
      return valueReads++ === 0 ? false : replacement;
    },
  });
  assert.equal(keyReads, 1);
  assert.equal(valueReads, 1);
  assert.equal(f.session.snapshot().layers[1].visible, false);
  assert.equal(f.read(), 12);
  assert.deepEqual(f.session.snapshot().layers[1].nodes, original.layers[1].nodes);
});

test('flag target accessor is read once for the whole layer lookup', () => {
  const f = fixture();
  let reads = 0;
  f.send('set-layer-state', {
    get layerId() {
      return reads++ === 0 ? 'source' : 'target';
    },
    key: 'visible',
    value: false,
  });
  assert.equal(reads, 1);
  assert.equal(f.session.snapshot().layers[1].visible, false);
  assert.equal(f.session.snapshot().layers[0].visible, true);
  assert.equal(f.read(), 12);
});
