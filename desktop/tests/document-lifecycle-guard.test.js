import test from 'node:test';
import assert from 'node:assert/strict';
import { DocumentLifecycle } from '../src/document-lifecycle.js';

function deferred() {
  let resolve, reject;
  const promise = new Promise((yes, no) => {
    resolve = yes;
    reject = no;
  });
  return { promise, resolve, reject };
}
function fixture() {
  let text = 'saved',
    draft = false;
  const choices = [],
    writes = [];
  const lifecycle = new DocumentLifecycle({
    read: () => text,
    hasDraft: () => draft,
    choose: (action) => {
      const pending = deferred();
      choices.push({ action, ...pending });
      return pending.promise;
    },
    write: (request) => {
      const pending = deferred();
      writes.push({ request, ...pending });
      return pending.promise;
    },
  });
  return {
    lifecycle,
    choices,
    writes,
    edit: (value) => {
      text = value;
    },
    draft: (value) => {
      draft = value;
    },
    read: () => text,
  };
}
async function dirtyFixture() {
  const f = fixture();
  await f.lifecycle.replace({ binding: { token: 'current', path: 'current.json' }, saved: true });
  f.edit('unsaved artwork');
  f.lifecycle.observe();
  return f;
}

test('pending chooser: edits reject Save/Discard without writing', async (t) => {
  for (const choice of ['save', 'discard']) {
    await t.test(choice, async () => {
      const f = await dirtyFixture(),
        d = f.lifecycle,
        binding = d.binding;
      const pending = d.guard('new');
      f.edit('newer artwork');
      f.choices[0].resolve(choice);
      assert.equal(await pending, false);
      assert.equal(f.read(), 'newer artwork');
      assert.equal(d.binding, binding);
      assert.equal(d.saved, 'saved');
      assert.equal(d.dirty, true);
      assert.equal(f.writes.length, 0);
      assert.equal(d.transitioning, false);
    });
  }
});

test('pending chooser: replacement invalidates choices even with identical bytes', async (t) => {
  for (const choice of ['save', 'discard']) {
    await t.test(choice, async () => {
      const f = await dirtyFixture(),
        d = f.lifecycle;
      const pending = d.guard('close');
      await d.replace({ binding: { token: 'replacement', path: 'other.json' }, saved: true });
      const generation = d.generation,
        revision = d.revision;
      f.choices[0].resolve(choice);
      assert.equal(await pending, false);
      assert.equal(f.read(), 'unsaved artwork');
      assert.equal(d.binding.token, 'replacement');
      assert.equal(d.generation, generation);
      assert.equal(d.revision, revision);
      assert.equal(d.dirty, false);
      assert.equal(f.writes.length, 0);
      assert.equal(d.transitioning, false);
    });
  }
});

test('pending chooser: a new draft blocks Save/Discard without writes', async (t) => {
  for (const choice of ['save', 'discard']) {
    await t.test(choice, async () => {
      const f = await dirtyFixture(),
        d = f.lifecycle,
        revision = d.revision;
      const pending = d.guard('open');
      f.draft(true);
      f.choices[0].resolve(choice);
      assert.equal(await pending, false);
      assert.equal(d.revision, revision);
      assert.equal(f.read(), 'unsaved artwork');
      assert.equal(d.dirty, true);
      assert.equal(f.writes.length, 0);
      assert.equal(d.transitioning, false);
    });
  }
});

test('one guarded transition owns the pending chooser', async () => {
  const f = await dirtyFixture(),
    d = f.lifecycle;
  const first = d.guard('open');
  assert.equal(await d.guard('close'), false);
  assert.equal(f.choices.length, 1);
  assert.equal(f.choices[0].action, 'open');
  f.choices[0].resolve('cancel');
  assert.equal(await first, false);
  const retry = d.guard('new');
  assert.equal(f.choices.length, 2);
  assert.equal(f.choices[1].action, 'new');
  f.choices[1].resolve('discard');
  assert.equal(await retry, true);
  assert.equal(f.writes.length, 0);
  assert.equal(d.dirty, true, 'permission to transition does not itself replace the document');
});

test('chooser failure releases the transition gate and preserves an ordinary retry', async () => {
  const f = await dirtyFixture(),
    d = f.lifecycle;
  const pending = d.guard('close');
  f.choices[0].reject(Error('chooser unavailable'));
  await assert.rejects(pending, /chooser unavailable/);
  assert.equal(d.transitioning, false);
  assert.equal(d.binding.token, 'current');
  assert.equal(d.dirty, true);
  const retry = d.guard('close');
  f.choices[1].resolve('cancel');
  assert.equal(await retry, false);
  assert.equal(f.writes.length, 0);
});

test('Save cancellation or failure cannot authorize the guarded transition', async (t) => {
  for (const failure of [false, true]) {
    await t.test(failure ? 'failure' : 'cancel', async () => {
      const f = await dirtyFixture(),
        d = f.lifecycle;
      const pending = d.guard('close');
      f.choices[0].resolve('save');
      await Promise.resolve();
      assert.equal(f.writes.length, 1);
      assert.equal(f.writes[0].request.contents, 'unsaved artwork');
      if (failure) f.writes[0].reject(Error('write unavailable'));
      else f.writes[0].resolve(null);
      if (failure) await assert.rejects(pending, /write unavailable/);
      else assert.equal(await pending, false);
      assert.equal(d.binding.token, 'current');
      assert.equal(d.saved, 'saved');
      assert.equal(d.dirty, true);
      assert.equal(d.saving, false);
      assert.equal(d.transitioning, false);
    });
  }
});

test('clean transitions bypass the chooser; active drafts still block', async () => {
  const f = fixture(),
    d = f.lifecycle;
  await d.replace({ binding: { token: 'current' }, saved: true });
  assert.equal(await d.guard('open'), true);
  f.draft(true);
  await assert.rejects(d.guard('close'), /Finish or cancel/);
  assert.equal(f.choices.length, 0);
  assert.equal(f.writes.length, 0);
  assert.equal(d.transitioning, false);
  assert.equal(d.binding.token, 'current');
  assert.equal(d.dirty, false);
});

test('successful guarded Save marks captured contents clean and permits transition', async () => {
  const f = await dirtyFixture(),
    d = f.lifecycle;
  const generation = d.generation;
  const pending = d.guard('close');
  f.choices[0].resolve('save');
  await Promise.resolve();
  assert.equal(f.writes.length, 1);
  f.writes[0].resolve({ token: 'current', path: 'current.json' });
  assert.equal(await pending, true);
  assert.equal(d.generation, generation);
  assert.equal(f.read(), 'unsaved artwork');
  assert.equal(d.saved, 'unsaved artwork');
  assert.equal(d.dirty, false);
  assert.equal(d.saving, false);
  assert.equal(d.transitioning, false);
});
