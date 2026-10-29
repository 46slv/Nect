import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, CommandSession, validate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { collectIds, remapIds } from '../src/project-import-remap.js';

function groupedProject() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    first = node('Value'),
    second = node('Value');
  layer.nodes.push(first, second);
  const session = new CommandSession(doc);
  session.dispatch({
    ...session.state,
    type: 'create-node-group',
    payload: { layerId: layer.id, name: first.id, nodeIds: [second.id, first.id] },
  });
  const grouped = session.snapshot();
  grouped.layers[0].nodeGroups.push({ id: 'empty-group', name: 'Empty', nodeIds: [] });
  validate(grouped);
  return migrateLegacyDocumentToCompositionProject(grouped);
}

for (const collision of [false, true]) {
  test(`grouped Composition import ${collision ? 'with' : 'without'} collisions validates, is atomic and round-trips`, () => {
    const source = groupedProject(),
      current = collision
        ? structuredClone(source)
        : migrateLegacyDocumentToCompositionProject(blankDocument()),
      sourceBefore = structuredClone(source),
      currentBefore = structuredClone(current),
      owner = createCompositionSessionOwner(current),
      before = owner.projectSnapshot(),
      history = owner.historyState,
      sourceLayer = source.compositions[0].layers[0],
      prepared = prepareProjectCompositionsImport(current, source),
      importedLayer = prepared.project.compositions.at(-1).layers[0];
    const suffix = collision ? '-import-1' : '';
    assert.deepEqual(
      importedLayer.nodeGroups,
      sourceLayer.nodeGroups.map((group) => ({
        ...group,
        id: group.id + suffix,
        nodeIds: group.nodeIds.map((id) => id + suffix),
      })),
    );
    assert.deepEqual(current, currentBefore);
    assert.deepEqual(source, sourceBefore);
    const result = owner.dispatch({
      ...owner.state,
      type: 'import-project-compositions',
      payload: { sourceProject: source },
    });
    assert.equal(result.changed, true);
    assert.deepEqual(owner.snapshot().layers[0], importedLayer);
    validate(owner.snapshot());
    const after = owner.projectSnapshot();
    assert.deepEqual(after.compositions.slice(0, before.compositions.length), before.compositions);
    assert.equal(owner.historyState.revision, history.revision + 1);
    const json = serializeCompositionProject(after),
      reopened = createCompositionSessionOwner(deserializeCompositionProject(json));
    assert.equal(serializeCompositionProject(reopened.projectSnapshot()), json);
    reopened.switchComposition(result.importedCompositionIds[0]);
    validate(reopened.snapshot());
    assert.deepEqual(reopened.snapshot().layers[0].nodeGroups, importedLayer.nodeGroups);
    owner.dispatch({ ...owner.state, type: 'undo' });
    assert.deepEqual(owner.projectSnapshot(), before);
    owner.dispatch({ ...owner.state, type: 'redo' });
    assert.deepEqual(owner.projectSnapshot(), after);
    assert.deepEqual(source, sourceBefore);
  });
}

test('group members only remap local node IDs; unrelated arrays and optional groups stay intact', () => {
  const layers = [
    {
      id: 'layer',
      nodes: [{ id: 'node' }, { id: 'second' }],
      nodeGroups: [
        { id: 'group', name: 'node', nodeIds: ['second', 'node', 'other-layer-node', 'layer'] },
      ],
      metadata: { nodeIds: ['node'], tags: ['node'], nodeGroups: [{ nodeIds: ['node'] }] },
    },
    { id: 'other-layer', nodes: [{ id: 'other-layer-node' }] },
    { id: 'empty', nodes: [], nodeGroups: [] },
  ];
  const before = structuredClone(layers),
    copy = remapIds(layers, collectIds(layers), collectIds(layers));
  assert.deepEqual(copy[0].nodeGroups[0].nodeIds, [
    'second-import-1',
    'node-import-1',
    'other-layer-node',
    'layer',
  ]);
  assert.equal(copy[0].nodeGroups[0].id, 'group-import-1');
  assert.equal(copy[0].nodeGroups[0].name, 'node');
  assert.deepEqual(copy[0].metadata, layers[0].metadata);
  assert.equal(Object.hasOwn(copy[1], 'nodeGroups'), false);
  assert.deepEqual(copy[2].nodeGroups, []);
  assert.deepEqual(layers, before);
});
