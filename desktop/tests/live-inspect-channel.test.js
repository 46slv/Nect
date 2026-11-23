import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveInspectChannel } from '../src/live-inspect-channel.js';

const initial = {
  documentGeneration: 1,
  compositionId: 'one',
  sessionId: 'one',
  sessionRevision: 0,
  projectRevision: 0,
};

test('one attached channel processes only a current inspector and drops a detached callback', () => {
  let current = initial;
  const channel = {};
  const replies = [];
  const route = createLiveInspectChannel({ readTarget: () => current });
  assert.equal(channel.onmessage, undefined);
  const close = route.attach(channel, (...args) => replies.push(args));
  assert.equal(
    route.attach({}, () => {}),
    null,
  );
  const frame = (phase, requestId) => channel.onmessage({ phase, requestId });
  frame('prepare', 1);
  frame('publish', 2);
  assert.deepEqual(replies, []);
  current = { ...initial, documentGeneration: 2 };
  frame('publish', 1);
  frame('prepare', 1);
  frame('publish', 1);
  frame('prepare', 2);
  frame('publish', 2);
  frame('publish', 2);
  assert.deepEqual(replies, [
    [1, { status: 'unavailable' }],
    [2, { status: 'ok', target: current }],
  ]);
  frame('prepare', 3);
  const late = channel.onmessage;
  close();
  const closeNext = route.attach(channel, (...args) => replies.push(args));
  close();
  late({ phase: 'publish', requestId: 3 });
  frame('prepare', 1);
  frame('discard', 1);
  frame('publish', 1);
  assert.equal(replies.length, 2);
  frame('prepare', 2);
  frame('publish', 2);
  assert.equal(replies.length, 3);
  closeNext();
});

test('unavailable is emitted in publish phase only when the editor is not ready', () => {
  const channel = {};
  const replies = [];
  const route = createLiveInspectChannel({ readTarget: () => initial, canInspect: () => false });
  const close = route.attach(channel, (...args) => replies.push(args));
  channel.onmessage({ phase: 'prepare', requestId: 1 });
  assert.deepEqual(replies, []);
  channel.onmessage({ phase: 'publish', requestId: 1 });
  assert.deepEqual(replies, [[1, { status: 'unavailable' }]]);
  close();
});

test('pinned document follows revisions but never retargets on a tab transition', () => {
  let current = initial;
  const channel = {};
  const replies = [];
  const route = createLiveInspectChannel({ readTarget: () => current, pinDocument: true });
  const close = route.attach(channel, (...args) => replies.push(args));
  const frame = (phase, requestId) => channel.onmessage({ phase, requestId });
  function read(id) {
    frame('prepare', id);
    frame('publish', id);
  }
  current = { ...initial, sessionRevision: 2, projectRevision: 4 };
  read(1);
  assert.deepEqual(replies[0], [1, { status: 'ok', target: current }]);
  current = { ...initial, documentGeneration: 2, compositionId: 'two', sessionId: 'two' };
  frame('prepare', 2);
  frame('discard', 2);
  read(3);
  current = initial;
  read(4);
  assert.deepEqual(replies.slice(1), [
    [3, { status: 'unavailable' }],
    [4, { status: 'unavailable' }],
  ]);
  close();
  const closeNext = route.attach(channel, (...args) => replies.push(args));
  read(1);
  assert.deepEqual(replies.at(-1), [1, { status: 'ok', target: initial }]);
  closeNext();
});

test('an explicit document binding cannot start with no active target', () => {
  const route = createLiveInspectChannel({ readTarget: () => null, pinDocument: true });
  const channel = {};
  assert.equal(
    route.attach(channel, () => {}),
    null,
  );
  assert.equal(channel.onmessage, undefined);
});
