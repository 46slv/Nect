import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize } from '../src/core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { createLiveVectorWireEdit } from '../src/live-vector-wire.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const seed = readFileSync(new URL('../examples/text-position-reuse.json', import.meta.url), 'utf8');
function fixture() {
  const owner = createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  const scope = { layerId: owner.snapshot().layers[0].id, nodeId: 'text-xy', input: 'position' };
  let blocked = false;
  let commits = 0;
  const edit = createLiveVectorWireEdit({
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => blocked,
    commit: (currentOwner, cmd) => {
      assert.equal(currentOwner, owner);
      commits++;
      try {
        return owner.dispatch(cmd);
      } catch {
        return null;
      }
    },
  });
  return {
    owner,
    readTarget,
    scope,
    serialize: () => serializeCompositionProject(owner.projectSnapshot()),
    edit,
    block: () => {
      blocked = true;
    },
    commits: () => commits,
  };
}

test('same-layer named Vector2 wiring uses Session, preserves fallback and matches GUI edits', () => {
  const f = fixture();
  const gui = fixture();
  const original = f.serialize();
  const target = f.readTarget();
  const remove = { ...f.scope, target, sourceId: null, sourcePort: null };
  const source = { sourceId: 'text-master', sourcePort: 'position' };
  assert.equal(f.edit({ ...remove, ...source }).status, 'unchanged');
  assert.equal(f.commits(), 0);
  assert.equal(f.edit(remove).status, 'changed');
  gui.owner.dispatch({
    ...gui.owner.state,
    type: 'disconnect',
    payload: { layerId: gui.scope.layerId, nodeId: 'text-xy', key: 'position' },
  });
  assert.equal(f.serialize(), gui.serialize());
  assert.equal(f.edit(remove).status, 'stale-target');
  assert.equal(f.edit({ ...remove, target: f.readTarget() }).status, 'unchanged');
  assert.equal(f.edit({ ...remove, ...source, target: f.readTarget() }).status, 'changed');
  assert.equal(f.serialize(), original);
  f.owner.dispatch({ ...f.owner.state, type: 'undo' });
  assert.equal(f.serialize(), gui.serialize());
  f.owner.dispatch({ ...f.owner.state, type: 'redo' });
  assert.equal(f.serialize(), original);
});

test('unavailable, mismatched, and cyclic sources do not alter the document or revision', () => {
  const f = fixture();
  const before = f.serialize();
  const target = f.readTarget();
  const args = { ...f.scope, target, sourceId: 'text-master', sourcePort: 'position' };
  assert.equal(f.edit({ ...args, sourceId: 'missing' }).status, 'unsupported-target');
  assert.equal(f.edit({ ...args, sourcePort: null }).status, 'rejected');
  assert.equal(f.edit({ ...args, input: 'size' }).status, 'unsupported-target');
  assert.equal(f.edit({ ...args, sourceId: null }).status, 'invalid-arguments');
  assert.equal(f.commits(), 0);
  assert.equal(f.edit({ ...args, nodeId: 'text-master', sourceId: 'text-xy' }).status, 'rejected');
  assert.equal(f.commits(), 1);
  f.block();
  assert.equal(f.edit(args).status, 'edit-in-progress');
  assert.deepEqual(f.readTarget(), target);
  assert.equal(f.serialize(), before);
});

test('fixture protocol projects only status and fresh target for a wiring command', () => {
  const f = fixture();
  const handler = createLiveEditorProtocol({ readTarget: f.readTarget, setVectorWire: f.edit });
  const packet = {
    version: 1,
    operation: 'graph.setVectorWire',
    arguments: { ...f.scope, target: f.readTarget(), sourceId: null, sourcePort: null },
  };
  assert.deepEqual(createLiveEditorProtocol({ readTarget: f.readTarget })(packet), {
    status: 'unsupported-operation',
  });
  assert.deepEqual(handler({ ...packet, arguments: { ...packet.arguments, path: '/tmp/a' } }), {
    status: 'invalid-arguments',
  });
  assert.equal(f.commits(), 0);
  assert.deepEqual(handler(packet), { status: 'changed', target: f.readTarget() });
  assert.deepEqual(handler(packet), { status: 'stale-target' });
  assert.equal(f.commits(), 1);
});
