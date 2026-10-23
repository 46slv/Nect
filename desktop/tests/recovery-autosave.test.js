import test from 'node:test';
import assert from 'node:assert/strict';
import { RecoveryAutosave } from '../src/recovery-autosave.js';
const tick = () => new Promise((resolve) => setTimeout(resolve, 0));
const snapshot = (contents, revision = 0, generation = 0, dirty = true) => ({
  contents,
  revision,
  generation,
  dirty,
  name: 'Embedded document',
});
test('each idle committed revision requests a save immediately without a debounce timer', async () => {
  const writes = [];
  const a = new RecoveryAutosave({
    begin: async () => 'doc-1',
    write: async (s) => {
      writes.push(s);
      return 42;
    },
  });
  a.observe(snapshot('baseline'));
  await a.flush();
  assert.equal(writes.length, 0);
  for (let revision = 1; revision <= 3; revision++) {
    a.observe(snapshot(`operation-${revision}`, revision));
    await a.flush();
    assert.equal(writes.length, revision);
    assert.equal(a.state.savedRevision, revision);
  }
  assert.deepEqual(
    writes.map((s) => s.revision),
    [1, 2, 3],
  );
  const embedded = JSON.stringify({ image: 'base64'.repeat(20000), font: 'ttf'.repeat(30000) });
  a.observe(snapshot(embedded, 4));
  await a.flush();
  assert.equal(writes.at(-1).contents, embedded);
});
test('slow disk serializes immutable requests and automatically drains the latest queued commit', async () => {
  let release,
    active = 0,
    maxActive = 0;
  const writes = [],
    states = [];
  const a = new RecoveryAutosave({
    begin: async () => 'doc-1',
    write: async (s) => {
      active++;
      maxActive = Math.max(maxActive, active);
      writes.push({ ...s });
      if (s.revision === 1)
        await new Promise((r) => {
          release = r;
        });
      active--;
      return s.revision;
    },
    status: (s) => states.push({ ...s }),
  });
  a.observe(snapshot('baseline'));
  a.observe(snapshot('one', 1));
  await tick();
  for (let r = 2; r <= 7; r++) a.observe(snapshot(`operation-${r}`, r));
  assert.equal(writes.length, 1);
  assert.equal(writes[0].contents, 'one');
  assert.equal(a.state.requestedRevision, 7);
  assert.equal(a.state.savedRevision, null);
  release();
  await tick();
  await a.flush();
  assert.equal(maxActive, 1);
  assert.deepEqual(
    writes.map((s) => s.revision),
    [1, 7],
  );
  assert.equal(writes.at(-1).contents, 'operation-7');
  assert.equal(a.state.savedRevision, 7);
  assert.ok(
    !states.some((s) => s.phase === 'saved' && s.requestedRevision === 7 && s.savedRevision === 1),
  );
});
test('document switching retains old queued work and serializes writes across windows/documents', async () => {
  let release,
    next = 0,
    active = 0,
    maxActive = 0;
  const writes = [];
  const a = new RecoveryAutosave({
    begin: async () => `doc-${++next}`,
    write: async (s) => {
      writes.push({ ...s });
      active++;
      maxActive = Math.max(maxActive, active);
      if (writes.length === 1)
        await new Promise((r) => {
          release = r;
        });
      active--;
      return 1;
    },
  });
  a.observe(snapshot('old baseline'));
  a.observe(snapshot('old one', 1));
  await tick();
  a.observe(snapshot('old latest', 2));
  a.observe(snapshot('new imported', 0, 1));
  a.observe(snapshot('new latest', 1, 1));
  release();
  await a.flush();
  assert.equal(maxActive, 1);
  assert.equal(writes.filter((s) => s.id === 'doc-1').at(-1).contents, 'old latest');
  assert.equal(writes.filter((s) => s.id === 'doc-2').at(-1).contents, 'new latest');
  assert.equal(a.current.generation, 1);
  assert.equal(a.state.savedRevision, 1);
});
test('failed revision retains the last saved revision; retry saves the current latest state', async () => {
  let fail = false;
  const writes = [];
  const a = new RecoveryAutosave({
    begin: async () => 'doc',
    write: async (s) => {
      if (fail) throw Error('Disk unavailable');
      writes.push(s);
      return 1;
    },
  });
  a.observe(snapshot('baseline'));
  a.observe(snapshot('one', 1));
  await a.flush();
  fail = true;
  a.observe(snapshot('two', 2));
  assert.equal(await a.flush(), false);
  assert.equal(a.state.phase, 'failed');
  assert.equal(a.state.failedRevision, 2);
  assert.equal(a.state.savedRevision, 1);
  assert.match(a.state.error, /Disk unavailable/);
  fail = false;
  assert.equal(await a.retry(), true);
  assert.equal(a.state.savedRevision, 2);
  assert.equal(writes.at(-1).contents, 'two');
});
test('restore is acknowledged only after a durable successor; synchronous failures remain retryable', async () => {
  let fail = true;
  const acknowledged = [];
  const a = new RecoveryAutosave({
    begin: () => {
      if (fail) throw Error('Disk unavailable');
      return Promise.resolve('successor');
    },
    write: async () => 12,
    acknowledge: async (id) => acknowledged.push(id),
  });
  a.observe(snapshot('restored'));
  assert.equal(await a.preserve('old-copy'), false);
  assert.deepEqual(acknowledged, []);
  fail = false;
  assert.equal(await a.retry(), true);
  assert.deepEqual(acknowledged, ['old-copy']);
});
test('manual save cleanliness during I/O is not lost and status callbacks cannot stop persistence', async () => {
  let release;
  const writes = [];
  const a = new RecoveryAutosave({
    begin: async () => 'doc',
    status: () => {
      throw Error('View unavailable');
    },
    write: async (s) => {
      writes.push(s);
      if (writes.length === 1)
        await new Promise((r) => {
          release = r;
        });
      return 1;
    },
  });
  a.observe(snapshot('baseline'));
  a.observe(snapshot('one', 1));
  await tick();
  a.observe(snapshot('one', 1, 0, false));
  release();
  await a.flush();
  assert.deepEqual(
    writes.map((s) => s.recoverable),
    [true, false],
  );
  assert.equal(a.state.savedRevision, 1);
});
