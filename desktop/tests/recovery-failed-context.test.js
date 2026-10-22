import test from 'node:test';
import assert from 'node:assert/strict';
import { RecoveryAutosave } from '../src/recovery-autosave.js';
const tick = () => new Promise((r) => setTimeout(r, 0));
const snap = (generation, revision, name = String(generation)) => ({
  generation,
  revision,
  name,
  dirty: true,
  contents: JSON.stringify({ name, revision }),
});
const deferred = () => {
  let resolve;
  const promise = new Promise((r) => (resolve = r));
  return { promise, resolve };
};
function fixture(handler = async () => {}) {
  let next = 0,
    active = 0,
    maxActive = 0;
  const writes = [],
    durable = new Map(),
    states = [];
  const a = new RecoveryAutosave({
    begin: async () => 'token-' + ++next,
    status: (s) => states.push({ ...s }),
    write: async (w) => {
      writes.push({ ...w });
      active++;
      maxActive = Math.max(maxActive, active);
      try {
        await handler(w);
        durable.set(w.id, { ...w });
        return writes.length;
      } finally {
        active--;
      }
    },
  });
  return { a, writes, durable, states, max: () => maxActive };
}
test('old failure arriving after switch remains explicit and retry writes A latest without changing B', async () => {
  const hold = deferred();
  let fail = true;
  const f = fixture(async (w) => {
      if (w.name === 'A' && w.revision === 1) {
        await hold.promise;
        if (fail) throw Error('A disk failure');
      }
    }),
    { a } = f;
  a.observe(snap(1, 0, 'A'));
  await a.flush();
  const tokenA = a.current.token;
  a.observe(snap(1, 1, 'A'));
  await tick();
  a.observe(snap(2, 0, 'B'));
  hold.resolve();
  await a.flush();
  const failed = a.failedDocuments[0],
    b = f.durable.get(a.current.token);
  assert.equal(failed.name, 'A');
  assert.equal(failed.failedRevision, 1);
  assert.equal(failed.savedRevision, 0);
  assert.equal(failed.generation, 1);
  assert.equal(a.state.phase, 'saved');
  assert.equal(f.durable.get(tokenA).revision, 0);
  fail = false;
  assert.equal(await a.retry(failed.id, failed.failedRevision), true);
  assert.equal(f.durable.get(tokenA).revision, 1);
  assert.deepEqual(f.durable.get(a.current.token), b);
  assert.deepEqual(a.failedDocuments, []);
  assert.equal(f.max(), 1);
});
test('A and B failures are individually selectable; fixing one leaves the other available', async () => {
  const bad = new Set(['A', 'B']),
    f = fixture(async (w) => {
      if (bad.has(w.name)) throw Error(w.name + ' failure');
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  assert.equal(await a.flush(), false);
  a.observe(snap(2, 1, 'B'));
  assert.equal(await a.flush(), false);
  const [A, B] = a.failedDocuments;
  assert.deepEqual([A.name, B.name], ['A', 'B']);
  bad.delete('A');
  assert.equal(await a.retry(A.id, A.failedRevision), true);
  assert.deepEqual(
    a.failedDocuments.map((x) => x.id),
    [B.id],
  );
  assert.equal(a.state.phase, 'failed');
  bad.delete('B');
  assert.equal(await a.retry(B.id, B.failedRevision), true);
  assert.deepEqual(a.failedDocuments, []);
});
test('a newer edit/success retires the old failure and stale retry cannot overwrite it', async () => {
  let fail = true;
  const f = fixture(async () => {
      if (fail) throw Error('disk');
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const old = a.failedDocuments[0];
  fail = false;
  a.observe(snap(1, 2, 'A'));
  await a.flush();
  const before = f.writes.length;
  assert.equal(await a.retry(old.id, old.failedRevision), false);
  assert.equal(f.writes.length, before);
  assert.equal(f.durable.get(a.current.token).revision, 2);
  assert.deepEqual(a.failedDocuments, []);
});
test('retry during a newer pending edit joins its write and uses the latest snapshot', async () => {
  let fail = true,
    hold = deferred();
  const f = fixture(async (w) => {
      if (fail) throw Error('disk');
      if (w.revision === 2) await hold.promise;
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const old = a.failedDocuments[0];
  fail = false;
  a.observe(snap(1, 2, 'A'));
  await tick();
  const retry = a.retry(old.id, old.failedRevision);
  hold.resolve();
  assert.equal(await retry, true);
  assert.deepEqual(
    f.writes.map((w) => w.revision),
    [1, 2],
  );
  assert.equal(f.durable.get(a.current.token).revision, 2);
});
test('multiple document switches retain identities and explicit retries preserve C', async () => {
  const bad = new Set(['A', 'B']),
    f = fixture(async (w) => {
      if (bad.has(w.name)) throw Error('disk');
    }),
    { a } = f;
  a.observe(snap(1, 3, 'A'));
  await a.flush();
  a.observe(snap(2, 7, 'B'));
  await a.flush();
  a.observe(snap(3, 0, 'C'));
  await a.flush();
  const [A, B] = a.failedDocuments,
    c = f.durable.get(a.current.token);
  assert.notEqual(A.id, B.id);
  bad.clear();
  await a.retry(B.id, B.failedRevision);
  await a.retry(A.id, A.failedRevision);
  assert.deepEqual([...f.durable.values()].map((w) => [w.name, w.revision]).sort(), [
    ['A', 3],
    ['B', 7],
    ['C', 0],
  ]);
  assert.deepEqual(f.durable.get(a.current.token), c);
  assert.deepEqual(a.failedDocuments, []);
});
test('missing targets and stale failure revisions do not issue writes', async () => {
  const f = fixture(async () => {
      throw Error('disk');
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const old = a.failedDocuments[0];
  a.observe(snap(1, 2, 'A'));
  await a.flush();
  const n = f.writes.length;
  assert.equal(await a.retry('absent', 1), false);
  assert.equal(await a.retry(old.id, old.failedRevision), false);
  assert.equal(f.writes.length, n);
  assert.equal(a.failedDocuments[0].failedRevision, 2);
});
test('duplicate explicit retry joins one in-flight request', async () => {
  let fail = true;
  const hold = deferred(),
    f = fixture(async () => {
      if (fail) throw Error('disk');
      await hold.promise;
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const old = a.failedDocuments[0];
  fail = false;
  const one = a.retry(old.id, old.failedRevision);
  await tick();
  const two = a.retry(old.id, old.failedRevision);
  assert.equal(a.failedDocuments[0].phase, 'saving');
  hold.resolve();
  assert.deepEqual(await Promise.all([one, two]), [true, true]);
  assert.equal(f.writes.length, 2);
  assert.equal(f.max(), 1);
});
test('older revisions/generations are rejected without changing the latest document or durable content', async () => {
  const f = fixture(),
    { a } = f;
  a.observe(snap(2, 4, 'B'));
  await a.flush();
  const n = f.writes.length;
  assert.equal(a.observe(snap(2, 3, 'B')), false);
  assert.equal(a.observe(snap(1, 9, 'A')), false);
  await a.flush();
  assert.equal(a.current.name, 'B');
  assert.equal(f.writes.length, n);
  assert.equal(f.durable.get(a.current.token).revision, 4);
});
test('repeated retry failure stays available without an automatic retry loop', async () => {
  const f = fixture(async () => {
      throw Error('disk');
    }),
    { a } = f;
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const old = a.failedDocuments[0];
  assert.equal(await a.retry(old.id, old.failedRevision), false);
  await tick();
  assert.equal(f.writes.length, 2);
  assert.equal(a.failedDocuments[0].id, old.id);
  assert.equal(a.failedDocuments[0].failedRevision, 1);
});
test('failed native token allocation survives switching and obtains its own token on explicit retry', async () => {
  let fail = true,
    next = 0;
  const writes = [];
  const a = new RecoveryAutosave({
    begin: async () => {
      if (fail) throw Error('begin');
      return 'token-' + ++next;
    },
    write: async (w) => {
      writes.push(w);
      return 1;
    },
  });
  a.observe(snap(1, 1, 'A'));
  await a.flush();
  const A = a.failedDocuments[0];
  fail = false;
  a.observe(snap(2, 1, 'B'));
  await a.flush();
  await a.retry(A.id, A.failedRevision);
  assert.deepEqual(
    writes.map((w) => [w.id, w.name]),
    [
      ['token-1', 'B'],
      ['token-2', 'A'],
    ],
  );
  assert.deepEqual(a.failedDocuments, []);
});
