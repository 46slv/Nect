import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

function fixture(options = {}) {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  project.compositions[0].id = 'first';
  project.compositions[0].name = 'First';
  const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'second';
  other.name = 'Second';
  project.compositions.push(other);
  return createCompositionSessionOwner(project, options);
}
function remove(owner, compositionId, overrides = {}) {
  return owner.dispatch({
    ...owner.state,
    type: 'delete-composition',
    payload: {
      compositionId,
      expectedName: owner.projectSnapshot().compositions.find((item) => item.id === compositionId)
        ?.name,
      expectedProjectRevision: owner.projectRevision,
      ...overrides,
    },
  });
}
function step(owner, type) {
  return owner.dispatch({ ...owner.state, type });
}

test('typed Composition deletion keeps the remaining project and is one Undo/Redo', () => {
  const owner = fixture(),
    before = owner.projectSnapshot(),
    revision = owner.projectRevision;
  const result = remove(owner, 'first');
  assert.equal(result.changed, true);
  assert.equal(result.sessionId, owner.state.sessionId);
  assert.equal(owner.activeCompositionId, 'second');
  assert.equal(owner.projectRevision, revision + 1);
  assert.deepEqual(owner.projectSnapshot().compositions, [before.compositions[1]]);
  const after = owner.projectSnapshot();
  step(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  step(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), after);
});

test('stale, missing and final-Composition deletion preserve the full envelope and Redo', () => {
  const owner = fixture();
  remove(owner, 'second');
  const single = owner.projectSnapshot();
  owner.renameComposition('first', 'Temporary');
  step(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.deepEqual(before, single);
  for (const [id, override] of [
    ['first', {}],
    ['missing', {}],
    ['first', { expectedName: 'Stale' }],
    ['first', { expectedProjectRevision: owner.projectRevision - 1 }],
  ]) {
    assert.throws(() => remove(owner, id, override), /last|STALE|Unknown/i);
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyState, history);
  }
  step(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions[0].name, 'Temporary');
});

test('reentrant project edit is retained and stale outer deletion is rejected', () => {
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
  assert.throws(() => remove(owner, 'second'), /STALE/);
  assert.deepEqual(owner.projectSnapshot(), nested);
  assert.equal(owner.projectSnapshot().compositions.length, 2);
});

test('a payload getter cannot delete after a reentrant Composition switch', () => {
  const owner = fixture(),
    before = owner.projectSnapshot(),
    history = owner.historyState;
  const payload = {
    compositionId: 'second',
    expectedProjectRevision: owner.projectRevision,
    get expectedName() {
      owner.switchComposition('second');
      return 'Second';
    },
  };
  assert.throws(
    () => owner.dispatch({ ...owner.state, type: 'delete-composition', payload }),
    /STALE_SESSION/,
  );
  assert.equal(owner.activeCompositionId, 'second');
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
});

test('busy deletion preserves the document and pending Redo', () => {
  let busy = false;
  const owner = fixture({ busy: () => busy });
  owner.renameComposition('second', 'Temporary');
  step(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  busy = true;
  assert.throws(() => remove(owner, 'second'), /finish|active/i);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  busy = false;
  step(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions[1].name, 'Temporary');
});
