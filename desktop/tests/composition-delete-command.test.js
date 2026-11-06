import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

function fixture(options = {}, prepare = () => {}) {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  project.compositions[0].id = 'first';
  project.compositions[0].name = 'First';
  const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  other.id = 'second';
  other.name = 'Second';
  project.compositions.push(other);
  prepare(project);
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

for (const method of ['typed', 'direct']) {
  test(`${method} deletion retains a reentrant project edit`, () => {
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
    assert.throws(
      () => (method === 'typed' ? remove(owner, 'second') : owner.deleteComposition('second')),
      /STALE/,
    );
    assert.deepEqual(owner.projectSnapshot(), nested);
    assert.equal(owner.projectSnapshot().compositions.length, 2);
  });
}

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

for (const method of ['typed', 'direct']) {
  test(`${method} deletion refuses a Text draft started by the fonts callback`, () => {
    let owner,
      layerId,
      textId,
      draft,
      armed = false;
    owner = fixture(
      {
        fonts: () => {
          if (armed) {
            armed = false;
            draft = owner.dispatch({
              ...owner.state,
              type: 'begin-text-draft',
              payload: { layerId, nodeId: textId },
            });
            draft = owner.dispatch({
              ...owner.state,
              type: 'stage-text-draft',
              payload: {
                draftId: draft.draftId,
                expectedDraftRevision: draft.draftRevision,
                action: 'edit',
                previousRaw: draft.text,
                nextRaw: 'Unsaved nested edit',
              },
            });
          }
          return {};
        },
      },
      (project) => {
        const layer = project.compositions[0].layers[0],
          text = node('Text');
        layerId = layer.id;
        textId = text.id;
        layer.nodes.push(text);
      },
    );
    owner.renameComposition('second', 'Temporary');
    step(owner, 'undo');
    const before = owner.projectSnapshot(),
      state = owner.state,
      history = owner.historyState;
    armed = true;
    assert.throws(
      () => (method === 'typed' ? remove(owner, 'first') : owner.deleteComposition('first')),
      /finish|active/i,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.deepEqual(owner.historyState, history);
    assert.equal(owner.hasTextDrafts, true);
    assert.equal(
      owner.textDraftSnapshot(draft.draftId).document.layers[0].nodes.find((n) => n.id === textId)
        .text,
      'Unsaved nested edit',
    );
    owner.dispatch({
      ...owner.state,
      type: 'cancel-text-draft',
      payload: { draftId: draft.draftId },
    });
    step(owner, 'redo');
    assert.equal(owner.projectSnapshot().compositions[1].name, 'Temporary');
  });
}

for (const method of ['typed', 'direct']) {
  test(`${method} deletion checks authority after the final busy callback`, () => {
    let owner,
      armed = false,
      calls = 0,
      nested;
    owner = fixture({
      busy: () => {
        if (armed && ++calls === 2) {
          armed = false;
          owner.renameComposition('first', 'Nested from busy');
          nested = owner.projectSnapshot();
        }
        return false;
      },
    });
    armed = true;
    assert.throws(
      () => (method === 'typed' ? remove(owner, 'second') : owner.deleteComposition('second')),
      /STALE/,
    );
    assert.ok(nested);
    assert.deepEqual(owner.projectSnapshot(), nested);
    assert.equal(owner.projectSnapshot().compositions.length, 2);
  });
}
