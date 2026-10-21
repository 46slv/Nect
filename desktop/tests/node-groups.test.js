import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  serialize,
  deserialize,
  validate,
  evaluate,
  exportSVG,
} from '../src/core.js';
import { stageArtworkDuplicate } from '../src/artwork-duplication.js';

function fixture(extra = true) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    circle = node('Circle', 40, 50),
    fill = node('Fill', 350, 50),
    output = layer.nodes[0],
    rectangle = node('Rectangle', 40, 350),
    value = node('Value', 650, 350);
  layer.nodes = extra ? [circle, fill, output, rectangle, value] : [circle, fill, output];
  connect(layer, circle.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    snapshot = () => session.snapshot(),
    groups = () => snapshot().layers[0].nodeGroups || [],
    saved = () => serialize(snapshot());
  return {
    doc,
    layer,
    circle,
    fill,
    output,
    rectangle,
    value,
    session,
    send,
    snapshot,
    groups,
    saved,
  };
}
function reject(f, type, payload, pattern) {
  const before = f.saved(),
    state = f.session.state,
    past = f.session.historyView.past.length;
  assert.throws(() => f.send(type, payload), pattern);
  assert.equal(f.saved(), before);
  assert.deepEqual(f.session.state, state);
  assert.equal(f.session.historyView.past.length, past);
}

test('group commands preserve nodes, connections, positions, evaluated geometry and SVG, each as one Undo', () => {
  const f = fixture(),
    nodes = f.snapshot().layers[0].nodes,
    svg = exportSVG(f.snapshot()),
    geometry = evaluate(f.snapshot().layers[0]).path;
  const once = (type, payload) => {
    const before = f.saved(),
      past = f.session.historyView.past.length,
      result = f.send(type, payload),
      after = f.saved();
    assert.equal(result.changed, true);
    assert.equal(f.session.historyView.past.length, past + 1);
    assert.deepEqual(f.snapshot().layers[0].nodes, nodes);
    assert.deepEqual(evaluate(f.snapshot().layers[0]).path, geometry);
    assert.equal(exportSVG(f.snapshot()), svg);
    assert.equal(serialize(deserialize(after)), after);
    f.send('undo');
    assert.equal(f.saved(), before);
    f.send('redo');
    assert.equal(f.saved(), after);
    return result;
  };
  const a = once('create-node-group', {
    name: 'Shapes',
    nodeIds: [f.circle.id, f.fill.id],
  }).groupId;
  assert.equal(f.snapshot().schemaVersion, 44);
  once('rename-node-group', { groupId: a, name: 'Primary' });
  once('add-to-node-group', { groupId: a, nodeIds: [f.rectangle.id] });
  const b = once('create-node-group', { name: 'Controls', nodeIds: [f.value.id] }).groupId;
  once('move-to-node-group', { groupId: b, nodeIds: [f.fill.id, f.rectangle.id] });
  assert.deepEqual(f.groups().find((g) => g.id === a).nodeIds, [f.circle.id]);
  once('remove-from-node-group', { groupId: a, nodeIds: [f.circle.id, f.value.id] });
  assert.deepEqual(f.groups().find((g) => g.id === a).nodeIds, []);
  once('rename-node-group', { groupId: a, name: 'Empty retained' });
  once('ungroup-nodes', { groupId: a });
  assert.equal(f.groups().length, 1);
});

test('no-op metadata edits retain revision and Redo; old documents without groups are unchanged', () => {
  const f = fixture(),
    old = f.saved();
  assert.equal(f.snapshot().schemaVersion, 27);
  assert.equal('nodeGroups' in f.snapshot().layers[0], false);
  assert.equal(serialize(deserialize(old)), old);
  const a = f.send('create-node-group', { name: 'One', nodeIds: [f.circle.id] }).groupId;
  f.send('rename-node-group', { groupId: a, name: 'Two' });
  f.send('undo');
  const state = f.session.state,
    saved = f.saved(),
    future = f.session.historyView.future.length;
  for (const [type, payload] of [
    ['rename-node-group', { groupId: a, name: ' One ' }],
    ['add-to-node-group', { groupId: a, nodeIds: [f.circle.id] }],
    ['move-to-node-group', { groupId: a, nodeIds: [f.circle.id] }],
    ['remove-from-node-group', { groupId: a, nodeIds: [f.rectangle.id] }],
  ])
    assert.equal(f.send(type, payload).changed, false);
  assert.deepEqual(f.session.state, state);
  assert.equal(f.saved(), saved);
  assert.equal(f.session.historyView.future.length, future);
});

test('invalid names, missing/cross-layer/duplicate IDs and implicit moves reject atomically', () => {
  const f = fixture(),
    a = f.send('create-node-group', { name: 'A', nodeIds: [f.circle.id] }).groupId,
    b = f.send('create-node-group', { name: 'B', nodeIds: [f.rectangle.id] }).groupId;
  for (const name of ['', '  ', 'x'.repeat(101), null])
    reject(f, 'rename-node-group', { groupId: a, name }, /Group name/);
  for (const nodeIds of [[], [f.fill.id, 'node-missing'], [f.fill.id, f.fill.id], [f.layer.id]])
    reject(f, 'move-to-node-group', { groupId: a, nodeIds }, /distinct current nodes/);
  reject(
    f,
    'add-to-node-group',
    { groupId: a, nodeIds: [f.fill.id, f.rectangle.id] },
    /Move selected/,
  );
  reject(
    f,
    'create-node-group',
    { name: 'C', groupId: b, nodeIds: [f.rectangle.id] },
    /Move selected/,
  );
  for (const type of [
    'rename-node-group',
    'add-to-node-group',
    'move-to-node-group',
    'remove-from-node-group',
    'ungroup-nodes',
  ])
    reject(
      f,
      type,
      { groupId: 'node-group-missing', name: 'C', nodeIds: [f.fill.id] },
      /Missing node group/,
    );
  const stale = f.session.state;
  f.send('rename-node-group', { groupId: a, name: 'Current' });
  const saved = f.saved();
  assert.throws(
    () =>
      f.session.dispatch({
        ...stale,
        type: 'ungroup-nodes',
        payload: { layerId: f.layer.id, groupId: a },
      }),
    /STALE/,
  );
  assert.equal(f.saved(), saved);
});

test('locked composition rejects; locked/driven nodes can be organized without changing values', () => {
  const f = fixture();
  f.send('set-parameter-lock', { nodeId: f.circle.id, key: 'radius', locked: true });
  f.send('set-parameter-expression', {
    nodeId: f.rectangle.id,
    key: 'center',
    expression: 'value; value',
  });
  const nodes = f.snapshot().layers[0].nodes;
  const a = f.send('create-node-group', {
    name: 'Locked values',
    nodeIds: [f.circle.id, f.rectangle.id],
  }).groupId;
  assert.deepEqual(f.snapshot().layers[0].nodes, nodes);
  f.send('set-layer-state', { key: 'locked', value: true });
  reject(f, 'rename-node-group', { groupId: a, name: 'Blocked' }, /locked/);
  reject(f, 'ungroup-nodes', { groupId: a }, /locked/);
});

test('deletion keeps empty records and protected Output membership; Undo restores all members', () => {
  const f = fixture(),
    a = f.send('create-node-group', {
      name: 'Delete members',
      nodeIds: [f.circle.id, f.rectangle.id],
    }).groupId,
    before = f.saved();
  f.send('delete-nodes', { nodeIds: [f.circle.id, f.rectangle.id] });
  assert.deepEqual(f.groups().find((g) => g.id === a).nodeIds, []);
  const empty = f.saved();
  assert.equal(serialize(deserialize(empty)), empty);
  f.send('undo');
  assert.equal(f.saved(), before);
  f.send('redo');
  assert.equal(f.saved(), empty);
  const b = f.send('create-node-group', {
    name: 'Output',
    nodeIds: [f.fill.id, f.output.id],
  }).groupId;
  f.send('delete-nodes', { nodeIds: [f.fill.id, f.output.id] });
  assert.deepEqual(f.groups().find((g) => g.id === b).nodeIds, [f.output.id]);
});

test('node copies and presets stay ungrouped; composition copies remap group/member IDs', () => {
  const f = fixture(),
    a = f.send('create-node-group', {
      name: f.circle.id,
      nodeIds: [f.circle.id, f.fill.id],
    }).groupId;
  const copy = f.send('duplicate-nodes', { nodeIds: [f.circle.id] }).nodeIds[0];
  f.send('copy-nodes', { nodeIds: [f.circle.id] });
  const pasted = f.send('paste-nodes').nodeIds[0];
  const preset = f.send('save-node-preset', { nodeIds: [f.circle.id], name: 'Circle' }).presetId;
  const applied = f.send('apply-node-preset', { presetId: preset }).nodeIds;
  assert.deepEqual(f.groups()[0].nodeIds, [f.circle.id, f.fill.id]);
  assert.ok(
    [copy, pasted, ...applied].every((id) => !f.groups().some((g) => g.nodeIds.includes(id))),
  );
  const duplicated = f.send('duplicate-layer').layerId,
    layer = f.snapshot().layers.find((l) => l.id === duplicated),
    g = layer.nodeGroups[0];
  assert.notEqual(g.id, a);
  assert.equal(g.name, f.circle.id);
  assert.equal(g.nodeIds.length, 2);
  assert.ok(
    g.nodeIds.every(
      (id) => layer.nodes.some((n) => n.id === id) && !f.layer.nodes.some((n) => n.id === id),
    ),
  );
  const simple = fixture(false);
  simple.send('create-node-group', {
    name: 'Artwork',
    nodeIds: [simple.circle.id, simple.fill.id],
  });
  const staged = stageArtworkDuplicate(simple.snapshot(), simple.layer.id);
  assert.notEqual(staged.layer.nodeGroups[0].id, simple.groups()[0].id);
  assert.ok(
    staged.layer.nodeGroups[0].nodeIds.every((id) => staged.layer.nodes.some((n) => n.id === id)),
  );
});

test('malformed persisted groups refuse ambiguous membership, nesting, IDs, versions and names', () => {
  const f = fixture();
  f.send('create-node-group', { name: 'A', nodeIds: [f.circle.id] });
  for (const mutate of [
    (d) => {
      d.schemaVersion = 43;
    },
    (d, l) => {
      l.nodeGroups[0].nodeIds.push('node-missing');
    },
    (d, l) => {
      l.nodeGroups[0].nodeIds.push(f.circle.id);
    },
    (d, l) => {
      l.nodeGroups.push({ id: 'group-other', name: 'B', nodeIds: [f.circle.id] });
    },
    (d, l) => {
      l.nodeGroups[0].children = [];
    },
    (d, l) => {
      l.nodeGroups[0].id = f.circle.id;
    },
    (d, l) => {
      l.nodeGroups[0].name = ' ';
    },
    (d, l) => {
      l.nodeGroups = null;
    },
  ]) {
    const d = f.snapshot();
    mutate(d, d.layers[0]);
    assert.throws(() => validate(d));
  }
});
