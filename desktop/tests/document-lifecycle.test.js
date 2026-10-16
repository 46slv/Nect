import test from 'node:test';
import assert from 'node:assert/strict';
import { DocumentLifecycle } from '../src/document-lifecycle.js';

function fixture() {
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
