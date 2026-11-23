import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, exportSVG } from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { createLiveScalarWireEdit, createLiveVectorWireEdit } from '../src/live-vector-wire.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const seed = readFileSync(new URL('../examples/text-position-reuse.json', import.meta.url), 'utf8');
function fixture(createEdit = createLiveVectorWireEdit) {
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
  const edit = createEdit({
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

test('wire editing preserves locked and expression-driven targets without calling Session', () => {
  const f = fixture();
  const args = {
    ...f.scope,
    target: f.readTarget(),
    sourceId: null,
    sourcePort: null,
  };
  for (const protect of [
    (layer) => {
      layer.locked = true;
    },
    (_, node) => {
      node.locked = true;
    },
    (_, node) => {
      node.bypassed = true;
    },
    (_, __, input) => {
      input.locked = true;
    },
    (_, __, input) => {
      input.expression = 'vec(80, 100)';
    },
  ]) {
    const document = structuredClone(f.owner.snapshot());
    const [layer] = document.layers;
    const node = layer.nodes.find((entry) => entry.id === f.scope.nodeId);
    protect(layer, node, node.params.position);
    const original = structuredClone(document);
    const edit = createLiveVectorWireEdit({
      readTarget: f.readTarget,
      getOwner: () => ({ snapshot: () => document }),
      isEditBlocked: () => false,
      commit: () => assert.fail('protected target must not reach Session'),
    });
    assert.deepEqual(edit(args), { status: 'rejected' });
    assert.deepEqual(document, original);
    assert.deepEqual(f.readTarget(), args.target);
  }
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

test('named Scalar wiring shares the GUI command, keeps its literal fallback and survives reopen', () => {
  const f = fixture(createLiveScalarWireEdit);
  const gui = fixture();
  const original = f.serialize();
  const bytes = readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = { default: parseFont(Uint8Array.from(bytes).buffer) };
  const rendered = (owner) => exportSVG(owner.snapshot(), fonts);
  const connectedSvg = rendered(f.owner);
  assert.match(connectedSvg, /<path\b/);
  const read = (owner) =>
    owner.snapshot().layers[0].nodes.find((n) => n.id === 'combine-x').params.x;
  const scope = { layerId: f.scope.layerId, nodeId: 'combine-x', input: 'x' };
  const target = f.readTarget();
  const handler = createLiveEditorProtocol({ readTarget: f.readTarget, setScalarWire: f.edit });
  const call = (args) =>
    handler({ version: 1, operation: 'graph.setScalarWire', arguments: { ...scope, ...args } });
  const detach = { target, sourceId: null, sourcePort: null };
  const connect = { sourceId: 'separate-position', sourcePort: 'x' };
  assert.equal(call({ target, ...connect }).status, 'unchanged');
  assert.equal(call({ target, ...connect, sourcePort: null }).status, 'rejected');
  assert.equal(
    call({ target, sourceId: 'text-master', sourcePort: 'position' }).status,
    'rejected',
  );
  assert.equal(
    call({ ...detach, nodeId: 'text-xy', input: 'position' }).status,
    'unsupported-target',
  );
  assert.equal(f.commits(), 0);
  assert.equal(call(detach).status, 'changed');
  assert.equal(call(detach).status, 'stale-target');
  gui.owner.dispatch({
    ...gui.owner.state,
    type: 'disconnect',
    payload: { layerId: scope.layerId, nodeId: scope.nodeId, key: 'x' },
  });
  assert.equal(f.serialize(), gui.serialize());
  assert.equal(read(f.owner).value, 0);
  assert.equal(read(f.owner).source ?? null, null);
  const detachedSvg = rendered(f.owner);
  assert.notEqual(detachedSvg, connectedSvg);
  assert.equal(detachedSvg, rendered(gui.owner));
  assert.equal(call({ target: f.readTarget(), ...connect }).status, 'changed');
  assert.equal(f.serialize(), original);
  f.owner.dispatch({ ...f.owner.state, type: 'undo' });
  assert.equal(f.serialize(), gui.serialize());
  f.owner.dispatch({ ...f.owner.state, type: 'redo' });
  assert.equal(f.serialize(), original);
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(f.serialize(), deserialize),
  );
  assert.equal(read(reopened).value, 0);
  assert.equal(read(reopened).source, connect.sourceId);
  assert.equal(read(reopened).sourcePort, connect.sourcePort);
  assert.equal(rendered(reopened), connectedSvg);
});
