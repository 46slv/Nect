import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { deserialize, evaluate, exportSVG } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { DocumentLifecycle } from '../src/document-lifecycle.js';

function fixture({ release = async () => {} } = {}) {
  let text = 'a',
    draft = false,
    choice = 'cancel',
    pending;
  const requests = [];
  const lifecycle = new DocumentLifecycle({
    read: () => text,
    write: (request) => {
      requests.push(request);
      return new Promise((resolve, reject) => {
        pending = { resolve, reject };
      });
    },
    choose: async () => choice,
    hasDraft: () => draft,
    release,
  });
  return {
    lifecycle,
    requests,
    edit: (value) => {
      text = value;
    },
    draft: (value) => {
      draft = value;
    },
    choose: (value) => {
      choice = value;
    },
    finish: (value = { token: '1', path: 'a.json' }) => pending.resolve(value),
    fail: () => pending.reject(Error('write failed')),
  };
}

test('save captures a revision; later editing remains dirty, Undo to saved content becomes clean', async () => {
  const f = fixture(),
    d = f.lifecycle;
  const saving = d.save();
  f.edit('b');
  assert.equal(d.dirty, true);
  f.finish();
  assert.equal(await saving, true);
  assert.equal(d.savedRevision, 0);
  assert.equal(d.dirty, true);
  f.edit('a');
  assert.equal(d.dirty, false);
  assert.equal(d.revision, 2);
  assert.equal(d.binding.path, 'a.json');
});

test('cancel, failure, stale results and Save As preserve current binding and dirtiness', async () => {
  const f = fixture(),
    d = f.lifecycle;
  d.replace({ binding: { token: 'old' }, saved: true });
  f.edit('b');
  let p = d.save(true);
  assert.equal(f.requests[0].binding, null);
  f.finish(null);
  assert.equal(await p, false);
  assert.equal(d.binding.token, 'old');
  assert.equal(d.dirty, true);
  p = d.save();
  f.fail();
  await assert.rejects(p, /write failed/);
  assert.equal(d.dirty, true);
  assert.equal(d.binding.token, 'old');
  p = d.save();
  f.edit('other document');
  d.replace();
  f.finish();
  assert.equal(await p, false);
  assert.equal(d.binding, null);
  assert.equal(d.dirty, true);
});

test('transition Save/Discard/Cancel and in-flight editing never silently discard', async () => {
  const f = fixture(),
    d = f.lifecycle;
  assert.equal(await d.guard('close'), false);
  f.choose('discard');
  assert.equal(await d.guard('new'), true);
  f.choose('save');
  const transition = d.guard('close');
  await Promise.resolve();
  f.edit('late edit');
  f.finish();
  assert.equal(await transition, false);
  assert.equal(d.dirty, true);
  f.draft(true);
  await assert.rejects(d.guard('open'), /Finish or cancel/);
  await assert.rejects(d.save(), /Finish or cancel/);
});

test('file-only pending work blocks saving and is rechecked across an asynchronous transition', async () => {
  let text = 'saved',
    pending = false,
    choose,
    finishWrite;
  const d = new DocumentLifecycle({
    read: () => text,
    write: () => new Promise((resolve) => (finishWrite = resolve)),
    choose: () => new Promise((resolve) => (choose = resolve)),
    hasLifecycleDraft: () => pending,
  });
  await d.replace({ saved: true });
  pending = true;
  assert.equal(d.hasDraft(), false, 'file-only work does not become a general editor draft');
  await assert.rejects(d.save(), /Finish or cancel/);
  await assert.rejects(d.guard('new'), /Finish or cancel/);
  pending = false;
  text = 'changed';
  const discarding = d.guard('new');
  pending = true;
  choose('discard');
  assert.equal(await discarding, false, 'work starting during the choice blocks replacement');
  pending = false;
  const saving = d.guard('new');
  choose('save');
  await Promise.resolve();
  assert.equal(typeof finishWrite, 'function');
  pending = true;
  finishWrite({ token: 'saved' });
  assert.equal(await saving, false, 'work starting during Save blocks replacement');
  assert.equal(d.saved, 'changed', 'the completed save still records its actual snapshot');
});

test('close approval is captured before the guard promise exposes a later edit', async () => {
  const f = fixture(),
    d = f.lifecycle;
  f.choose('discard');
  let approved;
  const transition = d.guard('close', { onApproved: () => (approved = d.observe()) });
  queueMicrotask(() => f.edit('later'));
  assert.equal(await transition, true);
  assert.equal(approved, 'a');
  assert.equal(d.observe(), 'later');
});

test('replace and Save As retire only obsolete bindings; cancellation/failure retain the save target', async () => {
  const released = [],
    f = fixture({ release: async ({ token }) => released.push(token) }),
    d = f.lifecycle;
  await d.replace({ binding: { token: 'old' }, saved: true });
  let saving = d.save(true);
  f.finish(null);
  assert.equal(await saving, false);
  saving = d.save(true);
  f.fail();
  await assert.rejects(saving);
  assert.deepEqual(released, []);
  saving = d.save();
  f.finish({ token: 'old' });
  assert.equal(await saving, true);
  assert.deepEqual(released, []);
  saving = d.save(true);
  f.finish({ token: 'save-as' });
  assert.equal(await saving, true);
  assert.deepEqual(released, ['old']);
  await d.replace({ binding: { token: 'opened' }, saved: true });
  assert.deepEqual(released, ['old', 'save-as']);
  await d.releaseUnused({ token: 'opened' });
  assert.deepEqual(released, ['old', 'save-as']);
  await d.replace();
  assert.deepEqual(released, ['old', 'save-as', 'opened']);
});

test('stale save tokens and rejected Open tokens are released without retiring the active document', async () => {
  const released = [],
    f = fixture({ release: async ({ token }) => released.push(token) }),
    d = f.lifecycle;
  await d.replace({ binding: { token: 'original' }, saved: true });
  const saving = d.save(true);
  await d.replace({ binding: { token: 'current' }, saved: true });
  f.finish({ token: 'stale-save-as' });
  assert.equal(await saving, false);
  await d.releaseUnused({ token: 'rejected-open' });
  await d.releaseUnused({ token: 'current' });
  assert.deepEqual(released, ['original', 'stale-save-as', 'rejected-open']);
  assert.equal(d.binding.token, 'current');
  assert.equal(d.dirty, false);
});

test('cleanup failure preserves accepted state and retries; concurrent retirement drains all tokens', async () => {
  const released = [];
  let fail = true,
    resume;
  const f = fixture({
      release: async ({ token }) => {
        if (fail) throw Error('temporary host failure');
        if (token === 'first')
          await new Promise((resolve) => {
            resume = resolve;
          });
        released.push(token);
      },
    }),
    d = f.lifecycle;
  await d.replace({ binding: { token: 'old' }, saved: true });
  await d.replace({ binding: { token: 'current' }, saved: true });
  assert.equal(d.binding.token, 'current');
  assert.match(d.cleanupError.message, /temporary/);
  assert.equal(d.pendingReleases.size, 1);
  fail = false;
  assert.equal(await d.flushReleases(), true);
  assert.deepEqual(released, ['old']);
  const first = d.releaseUnused({ token: 'first' });
  const second = d.releaseUnused({ token: 'second' });
  resume();
  await Promise.all([first, second]);
  assert.deepEqual(released, ['old', 'first', 'second']);
  assert.equal(d.pendingReleases.size, 0);
  assert.equal(d.cleanupError, null);
});

test('a bound Blend edit survives Close and Save As cancellation, then saves to a new project', async () => {
  const original = readFileSync(
    new URL('../examples/blend-distance.json', import.meta.url),
    'utf8',
  );
  const originalBinding = { token: 'original', path: 'blend-distance.json' },
    newBinding = { token: 'copy', path: 'blend-distance-edited.json' },
    files = new Map([[originalBinding.path, original]]),
    writes = [],
    released = [];
  const load = (text) =>
    createCompositionSessionOwner(deserializeCompositionProject(text, deserialize));
  const session = load(original);
  const shapeCount = (document) => {
    const result = evaluate(document.layers[0], {}, document.imageAssets, document);
    assert.equal(result.error, null);
    return result.path.children.length;
  };
  const read = () => serializeCompositionProject(session.projectSnapshot());
  let allowSaveAs = false;
  const lifecycle = new DocumentLifecycle({
    read,
    choose: async (action) => {
      assert.equal(action, 'closing Nect');
      return 'cancel';
    },
    write: async (request) => {
      writes.push(request);
      const target = request.binding ?? (allowSaveAs ? newBinding : null);
      if (!target) return null;
      files.set(target.path, request.contents);
      return target;
    },
    release: async ({ token }) => released.push(token),
  });
  await lifecycle.replace({ binding: originalBinding, saved: true });
  const before = session.snapshot(),
    layer = before.layers[0],
    blend = layer.nodes.find((node) => node.type === 'Blend');
  assert.equal(shapeCount(before), 16);
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: { layerId: layer.id, nodeId: blend.id, key: 'distance', value: 60 },
  });
  const edited = read(),
    editedSVG = exportSVG(session.snapshot());
  assert.equal(shapeCount(session.snapshot()), 11);
  assert.equal(lifecycle.dirty, true);

  assert.equal(
    await lifecycle.guard('closing Nect', {
      onApproved: () => assert.fail('a canceled Close must not be approved'),
    }),
    false,
  );
  assert.equal(writes.length, 0);
  assert.equal(await lifecycle.save(true), false);
  assert.equal(writes.length, 1);
  assert.equal(writes[0].binding, null);
  assert.equal(read(), edited);
  assert.equal(lifecycle.dirty, true);
  assert.deepEqual(lifecycle.binding, originalBinding);
  assert.deepEqual(released, []);
  assert.deepEqual([...files], [[originalBinding.path, original]]);

  allowSaveAs = true;
  assert.equal(await lifecycle.save(true), true);
  assert.equal(writes.length, 2);
  assert.equal(writes[1].binding, null);
  assert.deepEqual(lifecycle.binding, newBinding);
  assert.equal(lifecycle.dirty, false);
  assert.deepEqual(released, ['original']);
  assert.equal(files.get(originalBinding.path), original);
  const reopened = load(files.get(newBinding.path)).snapshot();
  assert.equal(
    reopened.layers[0].nodes.find((node) => node.id === blend.id).params.distance.value,
    60,
  );
  assert.equal(shapeCount(reopened), 11);
  assert.equal(exportSVG(reopened), editedSVG);
});
