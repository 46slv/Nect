import test from 'node:test';
import assert from 'node:assert/strict';
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
