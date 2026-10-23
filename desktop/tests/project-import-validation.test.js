import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const project = () => migrateLegacyDocumentToCompositionProject(blankDocument());
const send = (owner, type, payload) => owner.dispatch({ ...owner.state, type, payload });

for (const invalidIndex of [0, 1]) {
  test(`project import validates source Composition ${invalidIndex + 1} before changing history`, () => {
    const owner = createCompositionSessionOwner(project());
    const added = owner.createComposition({ name: 'Existing second' });
    owner.renameComposition(added, 'Future name');
    const future = owner.projectSnapshot();
    send(owner, 'undo');
    const before = owner.projectSnapshot(),
      command = owner.state,
      history = owner.historyState;
    assert.equal(history.canUndo, true);
    assert.equal(history.canRedo, true);
    const source = project(),
      second = project().compositions[0];
    second.id = 'second-source';
    source.compositions.push(second);
    source.compositions[invalidIndex].layers[0].nodes[0].type = 'Not a Nect node';
    const unchangedSource = structuredClone(source);
    assert.throws(
      () => send(owner, 'import-project-compositions', { sourceProject: source }),
      /Invalid node/,
    );
    assert.deepEqual(source, unchangedSource);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, command);
    assert.deepEqual(owner.historyState, history);
    assert.equal(owner.activeCompositionId, added);
    send(owner, 'redo');
    assert.deepEqual(owner.projectSnapshot(), future);
    send(owner, 'undo');
    assert.deepEqual(owner.projectSnapshot(), before);
    source.compositions[invalidIndex].layers[0].nodes[0].type = 'Output';
    const result = send(owner, 'import-project-compositions', { sourceProject: source });
    assert.equal(result.importedCompositionIds.length, 2);
    assert.equal(owner.projectSnapshot().compositions.length, before.compositions.length + 2);
    send(owner, 'undo');
    assert.deepEqual(owner.projectSnapshot(), before);
  });
}
