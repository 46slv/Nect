import test from 'node:test';
import assert from 'node:assert/strict';
import { createNumericControlBehavior } from '../src/numeric-control-behavior.js';

function fixture(normalizeValues) {
  const calls = [],
    state = { valid: true, value: 10, fail: false };
  const behavior = createNumericControlBehavior({
    normalizeValues,
    valid: () => state.valid,
    read: () => [state.value],
    step: () => 1,
    bounds: () => [-100, 100],
    begin: () => calls.push('begin'),
    preview: ([value]) => {
      if (state.fail) throw Error('domain refused');
      state.value = value;
      calls.push('preview');
    },
    commit: () => calls.push('commit'),
    cancel: () => {
      state.value = 10;
      calls.push('cancel');
    },
  });
  return { behavior, state, calls };
}

test('pending click preserves native editing; a changed gesture commits once', () => {
  const f = fixture();
  assert.deepEqual(f.behavior.move(3), { started: false });
  assert.deepEqual(f.calls, []);
  f.behavior.move(14);
  assert.equal(f.state.value, 20);
  assert.equal(f.behavior.finish(), true);
  assert.equal(f.behavior.finish(), false);
  assert.deepEqual(f.calls, ['begin', 'preview', 'commit']);
});
test('returning to the original value cancels without committing', () => {
  const f = fixture();
  f.behavior.move(14);
  f.behavior.move(0);
  assert.equal(f.behavior.finish(), false);
  assert.equal(f.state.value, 10);
  assert.deepEqual(f.calls, ['begin', 'preview', 'preview', 'cancel']);
});
test('a stale target/revision cancels once and late events cannot commit', () => {
  const f = fixture();
  f.behavior.move(14);
  f.state.valid = false;
  assert.deepEqual(f.behavior.move(24), { cancelled: true });
  f.behavior.move(34);
  f.behavior.finish();
  f.behavior.cancel();
  assert.equal(f.state.value, 10);
  assert.deepEqual(f.calls, ['begin', 'preview', 'cancel']);
});
test('a refused preview cannot replace the last accepted owner value', () => {
  const f = fixture();
  f.behavior.move(14);
  f.state.fail = true;
  assert.throws(() => f.behavior.move(24), /domain refused/);
  assert.equal(f.state.value, 20);
  assert.equal(f.behavior.finish(), true);
  assert.deepEqual(f.calls, ['begin', 'preview', 'commit']);
});

test('normalization receives the raw request and participates in no-op completion', () => {
  const f = fixture((values, context) => {
    assert.deepEqual(values, [20]);
    assert.deepEqual(context, { baseline: [10], bounds: [-100, 100], requestedDelta: 10 });
    context.baseline[0] = 999; // Request context cannot mutate the owned raw baseline.
    return [10];
  });
  f.behavior.move(14);
  assert.equal(f.behavior.finish(), false);
  assert.deepEqual(f.calls, ['begin', 'cancel']);
  assert.equal(f.state.value, 10);
});

test('normalized values drive change tracking and raw return-to-origin', () => {
  const f = fixture((values, { baseline, requestedDelta }) =>
    requestedDelta === 0 ? baseline : [20],
  );
  f.behavior.move(14);
  f.behavior.move(24);
  assert.deepEqual(
    f.calls,
    ['begin', 'preview'],
    'equivalent normalized values do not preview again',
  );
  f.behavior.move(0);
  assert.equal(f.behavior.finish(), false);
  assert.deepEqual(f.calls, ['begin', 'preview', 'preview', 'cancel']);
  assert.equal(f.state.value, 10);
});

test('invalid normalization retains the last accepted preview', () => {
  let invalid;
  const f = fixture((values) => invalid ?? values);
  f.behavior.move(14);
  for (invalid of [[NaN], new Array(1), [], [10, 20]])
    assert.throws(() => f.behavior.move(24), /Invalid normalized numeric values/);
  assert.equal(f.state.value, 20);
  assert.equal(f.behavior.finish(), true);
  assert.deepEqual(f.calls, ['begin', 'preview', 'commit']);
});
