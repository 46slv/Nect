import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

function fixture(options = {}) {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  project.compositions[0].id = 'first';
  project.compositions[0].name = 'Composition 1';
  const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'second';
  other.name = 'Composition 1';
  project.compositions.push(other);
  return createCompositionSessionOwner(project, options);
}
function rename(owner, id, name, overrides = {}) {
  return owner.dispatch({
    ...owner.state,
    type: 'rename-composition',
    payload: {
      compositionId: id,
      name,
      expectedName: owner.projectSnapshot().compositions.find((c) => c.id === id)?.name,
      expectedProjectRevision: owner.projectRevision,
      ...overrides,
    },
  });
}
function send(owner, type) {
  return owner.dispatch({ ...owner.state, type });
}

test('typed Composition rename preserves identity and is one project Undo/Redo', () => {
  const owner = fixture(),
    before = owner.projectSnapshot(),
    revision = owner.projectRevision;
  const result = rename(owner, 'second', '横型 · 日本語');
  const after = owner.projectSnapshot();
  assert.equal(result.changed, true);
  assert.equal(owner.projectRevision, revision + 1);
  assert.equal(owner.activeCompositionId, 'first');
  assert.deepEqual(after.compositions[0], before.compositions[0]);
  assert.deepEqual(after.compositions[1], { ...before.compositions[1], name: '横型 · 日本語' });
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), after);
});

test('no-op and stale Composition rename keep project, revision and pending Redo', () => {
  const owner = fixture();
  rename(owner, 'second', 'Variant');
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.equal(rename(owner, 'second', 'Composition 1').changed, false);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  for (const override of [
    { expectedName: 'old' },
    { expectedProjectRevision: owner.projectRevision - 1 },
    { expectedProjectRevision: undefined },
  ]) {
    assert.throws(() => rename(owner, 'second', 'Wrong', override), /STALE/);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.historyState, history);
  }
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions[1].name, 'Variant');
});

test('reentrant project edits are retained and the stale outer rename is rejected', () => {
  let owner,
    armed = false,
    nested;
  owner = fixture({
    fonts: () => {
      if (armed) {
        armed = false;
        owner.renameComposition('first', 'Nested');
        nested = owner.projectSnapshot();
      }
      return {};
    },
  });
  armed = true;
  assert.throws(() => rename(owner, 'second', 'Outer'), /STALE/);
  assert.deepEqual(owner.projectSnapshot(), nested);
  assert.equal(owner.projectSnapshot().compositions[1].name, 'Composition 1');
});
