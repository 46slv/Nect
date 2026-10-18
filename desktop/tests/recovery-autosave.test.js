import test from 'node:test';
import assert from 'node:assert/strict';
import { RecoveryAutosave } from '../src/recovery-autosave.js';
const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const snapshot = (contents, generation = 0, dirty = true) => ({
  contents,
  generation,
  dirty,
  name: 'Embedded 文書',
});
test('debounces committed changes, keeps complete embedded data and does not mark a manual file saved', async () => {
  const writes = [],
    states = [];
  const a = new RecoveryAutosave({
    begin: async () => 'doc-1',
    write: async (s) => {
      writes.push(s);
      return 42;
    },
    status: (s) => states.push(s.phase),
    delay: 10,
  });
  a.observe(snapshot('baseline'));
  await wait(20);
  assert.equal(writes.length, 0);
  a.observe(snapshot('first'));
  a.observe(snapshot(JSON.stringify({ image: 'base64'.repeat(20000), font: 'ttf'.repeat(30000) })));
  await wait(30);
  assert.equal(writes.length, 1);
  assert.equal(writes[0].contents, a.current.contents);
  assert.equal(writes[0].recoverable, true);
  assert.equal(a.state.phase, 'saved');
  a.observe(snapshot(a.current.contents, 0, false));
  await a.flush();
  assert.equal(writes.at(-1).recoverable, false);
  assert.ok(states.includes('pending') && states.includes('saving'));
});
test('serializes in-flight revisions; replacing documents gives each an independent identity', async () => {
  const writes = [];
  let next = 0,
    release;
  const a = new RecoveryAutosave({
    begin: async () => `doc-${++next}`,
    write: async (s) => {
      writes.push(s);
      if (writes.length === 1)
        await new Promise((r) => {
          release = r;
        });
      return 1;
    },
    delay: 1000,
  });
  a.observe(snapshot('zero'));
  a.observe(snapshot('one'));
  const first = a.flush();
  await wait(0);
  a.observe(snapshot('two'));
  a.observe(snapshot('new baseline', 1));
  a.observe(snapshot('new edit', 1));
  const second = a.flush();
  release();
  await first;
  await second;
  await wait(0);
  assert.equal(writes.filter((s) => s.id === 'doc-1').at(-1).contents, 'two');
  assert.equal(writes.filter((s) => s.id === 'doc-2').at(-1).contents, 'new edit');
  assert.equal(a.current.generation, 1);
});
test('failure is visible and does not acknowledge the old recovery; explicit retry succeeds', async () => {
  let fail = true;
  const acknowledged = [];
  const a = new RecoveryAutosave({
    begin: () => {
      if (fail) throw Error('Disk unavailable');
      return Promise.resolve('new-copy');
    },
    write: async () => 12,
    acknowledge: async (id) => acknowledged.push(id),
  });
  a.observe(snapshot('restored'));
  assert.equal(await a.preserve('old-copy'), false);
  assert.equal(a.state.phase, 'failed');
  assert.match(a.state.error, /Disk unavailable/);
  assert.deepEqual(acknowledged, []);
  fail = false;
  assert.equal(await a.retry(), true);
  assert.deepEqual(acknowledged, ['old-copy']);
  assert.equal(a.state.phase, 'saved');
});
