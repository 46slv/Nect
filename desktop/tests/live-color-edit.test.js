import test from 'node:test';
import assert from 'node:assert/strict';
import { createLiveColorEdit } from '../src/live-color-edit.js';

function fixture() {
  const frames = new Map(),
    calls = [],
    notices = [];
  let sequence = 0,
    value = '#111111',
    committed = '#111111',
    candidate = committed,
    active = false,
    previews = 0,
    settled = 0;
  const state = { sessionId: 'a', expectedRevision: 0 };
  const owner = {
    state,
    dispatch({ type, payload }) {
      calls.push(type);
      if (type === 'begin-gesture') active = true;
      if (type === 'update-gesture') candidate = payload.update.updates[0].value.hex;
      if (type === 'commit-gesture') {
        if (candidate !== committed) state.expectedRevision++;
        committed = candidate;
        active = false;
      }
      if (type === 'cancel-gesture') active = false;
      return { gestureId: 'g', expectedPreviewRevision: calls.length };
    },
    previewSnapshot() {
      return { document: { colorSlots: [{ id: 'stable', value: { hex: candidate, alpha: 1 } }] } };
    },
  };
  const controller = createLiveColorEdit({
    app: {
      ownerDocument: {
        defaultView: {
          requestAnimationFrame(fn) {
            frames.set(++sequence, fn);
            return sequence;
          },
          cancelAnimationFrame(id) {
            frames.delete(id);
          },
        },
      },
    },
    getSession: () => owner,
    getDocument: () => ({ colorSlots: [{ id: 'stable' }] }),
    proposal: () => ({ updates: [{ slotId: 'stable', value: { hex: value, alpha: 1 } }] }),
    onPreview: () => previews++,
    onSettled: () => settled++,
    notify: (message) => notices.push(message),
  });
  return {
    controller,
    state,
    calls,
    notices,
    set value(next) {
      value = next;
    },
    get committed() {
      return committed;
    },
    get frames() {
      return frames.size;
    },
    get active() {
      return active;
    },
    get previews() {
      return previews;
    },
    get settled() {
      return settled;
    },
    frame() {
      const callbacks = [...frames.values()];
      frames.clear();
      for (const fn of callbacks) fn();
    },
  };
}

test('many input events coalesce to one detached latest-value preview', () => {
  const f = fixture();
  for (const value of ['#222222', '#333333', '#444444']) {
    f.value = value;
    f.controller.schedule();
  }
  assert.equal(f.frames, 1);
  assert.equal(f.calls.length, 0);
  f.frame();
  assert.equal(f.previews, 1);
  assert.equal(f.controller.document.colorSlots[0].value.hex, '#444444');
  assert.equal(f.committed, '#111111');
  assert.equal(f.state.expectedRevision, 0);
  assert.equal(f.controller.metrics().requested, 3);
  assert.equal(f.controller.metrics().evaluated, 1);
});

test('commit flushes the newest pending value once without waiting for its frame', () => {
  const f = fixture();
  f.value = '#222222';
  f.controller.schedule();
  f.frame();
  f.value = '#333333';
  f.controller.schedule();
  f.controller.commit();
  assert.equal(f.frames, 0);
  assert.equal(f.committed, '#333333');
  assert.equal(f.state.expectedRevision, 1);
  assert.equal(f.controller.document, null);
  assert.equal(f.controller.active, false);
  assert.equal(f.calls.filter((type) => type === 'commit-gesture').length, 1);
});

test('cancel discards both an evaluated preview and the pending final frame', () => {
  const f = fixture();
  f.value = '#222222';
  f.controller.schedule();
  f.frame();
  f.value = '#333333';
  f.controller.schedule();
  f.controller.cancel();
  assert.equal(f.frames, 0);
  assert.equal(f.active, false);
  assert.equal(f.committed, '#111111');
  assert.equal(f.state.expectedRevision, 0);
  assert.equal(f.controller.document, null);
  f.frame();
  assert.equal(f.committed, '#111111');
});

test('a stale envelope cancels before applying or committing any queued value', () => {
  const f = fixture();
  f.controller.schedule();
  f.state.expectedRevision++;
  f.controller.commit();
  assert.equal(f.committed, '#111111');
  assert.equal(f.calls.length, 0);
  assert.match(f.notices[0], /stale/);
  assert.equal(f.controller.active, false);
});

test('no-op edit can finish without changing the authored revision', () => {
  const f = fixture();
  f.controller.schedule();
  f.controller.commit();
  assert.equal(f.state.expectedRevision, 0);
  assert.equal(f.controller.active, false);
});
