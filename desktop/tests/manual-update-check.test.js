import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createManualUpdateCheck } from '../src/manual-update-check.js';
import { mountManualUpdateCheck } from '../src/manual-update-view.js';
import { renderDiscovery } from '../src/build-info.js';
import { previewFixture, freezeFixture } from './helpers/preview-discovery-fixture.mjs';

const deferred = () => {
  let resolve;
  const promise = new Promise((done) => {
    resolve = done;
  });
  return { promise, resolve };
};
function harness(overrides = {}) {
  const fixture = freezeFixture(previewFixture());
  const live = {
    context: structuredClone({ provider: fixture.provider, identity: fixture.identity }),
    now: fixture.now,
  };
  let requestNumber = 0;
  const calls = { source: 0, prepare: 0 };
  const response = (request) => ({
    ...structuredClone(fixture.response),
    discovery: { ...structuredClone(fixture.response.discovery), ...request },
  });
  const controller = createManualUpdateCheck({
    provider: fixture.provider,
    identity: fixture.identity,
    readContext: () => live.context,
    clock: () => live.now,
    nextRequest: () => ({ ...fixture.request, requestId: `request-${++requestNumber}` }),
    readSource: async (request) => {
      calls.source++;
      return response(request);
    },
    prepare: async (bound) => {
      calls.prepare++;
      assert.ok(Object.isFrozen(bound));
      assert.ok(Object.isFrozen(bound.request));
    },
    ...overrides,
  });
  return { controller, live, calls, response, fixture };
}

test('production-null controller never reads adapters or offers controls', async () => {
  let calls = 0;
  const forbidden = () => {
    calls++;
    throw Error('must remain inert');
  };
  const controller = createManualUpdateCheck({
    provider: null,
    readContext: forbidden,
    nextRequest: forbidden,
    readSource: forbidden,
    clock: forbidden,
    prepare: forbidden,
  });
  const win = new Window();
  const element = win.document.createElement('div');
  const view = mountManualUpdateCheck({
    element,
    document: win.document,
    controller,
    renderDiscovery,
  });
  assert.match(element.textContent, /not configured/);
  assert.equal(element.querySelectorAll('button').length, 0);
  await controller.check();
  await controller.confirm(1);
  controller.refresh();
  controller.cancel();
  assert.equal(calls, 0);
  view.dispose();
  await win.happyDOM.close();
});

test('review is immutable, literal and requires one explicit confirmation', async () => {
  const h = harness();
  const win = new Window();
  const element = win.document.createElement('div');
  const view = mountManualUpdateCheck({
    element,
    document: win.document,
    controller: h.controller,
    renderDiscovery,
  });
  assert.equal(h.calls.source, 0);
  const review = await h.controller.check();
  assert.equal(review.state, 'update_advisory');
  review.advisory.version = 'fake';
  assert.equal(
    h.controller.getState().advisory.version,
    h.fixture.response.discovery.candidate.version,
  );
  assert.equal(h.calls.prepare, 0);
  assert.equal(element.querySelectorAll('button:not([hidden])').length, 3);
  await Promise.all([h.controller.confirm(review.reviewId), h.controller.confirm(review.reviewId)]);
  assert.equal(h.calls.prepare, 1);
  assert.equal(h.controller.getState().state, 'preparation_complete');
  assert.equal(element.querySelectorAll('button:not([hidden])').length, 1);
  await h.controller.confirm(review.reviewId);
  assert.equal(h.calls.prepare, 1);
  view.dispose();
  await win.happyDOM.close();
});

test('expiry and identity/provider/config changes revoke reviewed consent', async () => {
  for (const mutate of [
    (h) => {
      h.live.now = h.fixture.response.expiresAt;
    },
    (h) => {
      h.live.context.identity.appVersion = '0.1.1-preview.3';
    },
    (h) => {
      h.live.context.identity.target = 'different';
    },
    (h) => {
      h.live.context.identity.channel = 'stable';
    },
    (h) => {
      h.live.context.provider.configRevision = 'new';
    },
    (h) => {
      h.live.context.provider.id = 'different';
    },
  ]) {
    const h = harness();
    const review = await h.controller.check();
    mutate(h);
    const result = await h.controller.confirm(review.reviewId);
    assert.equal(result.state, 'unavailable');
    assert.equal(h.calls.prepare, 0);
    assert.equal(Object.hasOwn(result, 'advisory'), false);
  }
});

test('new requests cannot inherit old confirmation, even with identical metadata', async () => {
  const h = harness();
  const first = await h.controller.check();
  const second = await h.controller.check();
  assert.notEqual(first.reviewId, second.reviewId);
  assert.equal((await h.controller.confirm(first.reviewId)).reason, 'REVIEW_STALE');
  await h.controller.confirm(second.reviewId);
  assert.equal(h.calls.prepare, 0);
  const third = await h.controller.check();
  await h.controller.confirm(third.reviewId);
  assert.equal(h.calls.prepare, 1);
});

test('pending clicks deduplicate; cancellation and disposal suppress late results', async () => {
  for (const action of ['cancel', 'dispose']) {
    const gate = deferred();
    let sourceCalls = 0;
    const h = harness({
      readSource: async (request) => {
        sourceCalls++;
        await gate.promise;
        return h.response(request);
      },
    });
    const pending = h.controller.check();
    await h.controller.check();
    assert.equal(sourceCalls, 1);
    h.controller[action]();
    gate.resolve();
    await pending;
    assert.equal(h.controller.getState().state, action === 'dispose' ? 'disposed' : 'idle');
    assert.equal(Object.hasOwn(h.controller.getState(), 'advisory'), false);
    await h.controller.confirm(1);
    assert.equal(h.calls.prepare, 0);
  }
});

test('failure clears old advisory and retry uses fresh request and consent', async () => {
  let fail = false;
  const ids = [];
  const h = harness({
    readSource: async (request) => {
      ids.push(request.requestId);
      if (fail) throw Error('offline');
      return h.response(request);
    },
  });
  const first = await h.controller.check();
  fail = true;
  assert.equal((await h.controller.check()).state, 'unavailable');
  assert.equal(Object.hasOwn(h.controller.getState(), 'advisory'), false);
  fail = false;
  const fresh = await h.controller.check();
  assert.equal(new Set(ids).size, 3);
  assert.equal(h.calls.prepare, 0);
  assert.notEqual(first.reviewId, fresh.reviewId);
  await h.controller.confirm(fresh.reviewId);
  assert.equal(h.calls.prepare, 1);
});

test('preparation failures and cancelled late preparations never restore consent', async () => {
  let calls = 0;
  const failed = harness({
    prepare: async () => {
      calls++;
      throw Error('fixture failure');
    },
  });
  const review = await failed.controller.check();
  assert.equal((await failed.controller.confirm(review.reviewId)).reason, 'PREPARATION_FAILED');
  await failed.controller.confirm(review.reviewId);
  assert.equal(calls, 1);
  const gate = deferred();
  const h = harness({ prepare: () => gate.promise });
  const candidate = await h.controller.check();
  const pending = h.controller.confirm(candidate.reviewId);
  h.controller.cancel();
  gate.resolve();
  await pending;
  assert.equal(h.controller.getState().state, 'idle');
});

test('synchronous cancellation on pending notification prevents adapter invocation', async () => {
  for (const phase of ['checking', 'preparing']) {
    const h = harness();
    h.controller.subscribe((state) => {
      if (state.state === phase) h.controller.cancel();
    });
    const review = await h.controller.check();
    if (phase === 'preparing') await h.controller.confirm(review.reviewId);
    assert.equal(h.controller.getState().state, 'idle');
    assert.equal(phase === 'checking' ? h.calls.source : h.calls.prepare, 0);
  }
});

test('disposal is terminal before subscribers are notified', async () => {
  const h = harness();
  h.controller.subscribe((state) => {
    if (state.state === 'disposed') void h.controller.check();
  });
  h.controller.dispose();
  await Promise.resolve();
  assert.equal(h.calls.source, 0);
  assert.equal(h.controller.getState().state, 'disposed');
});

test('refresh preserves disposal, cancellation and new checks started during validation', async (t) => {
  for (const callback of ['clock', 'readContext']) {
    for (const action of ['dispose', 'cancel', 'check']) {
      await t.test(`${callback}: ${action}`, async () => {
        let armed = false;
        let newer;
        const intervene = () => {
          if (!armed) return false;
          armed = false;
          newer = h.controller[action]();
          return true;
        };
        const h = harness({
          clock: () =>
            callback === 'clock' && intervene() ? h.fixture.response.expiresAt : h.live.now,
          readContext: () =>
            callback === 'readContext' && intervene()
              ? { ...h.live.context, provider: null }
              : h.live.context,
        });
        const original = await h.controller.check();
        armed = true;
        const refreshed = h.controller.refresh();
        const expected = { dispose: 'disposed', cancel: 'idle', check: 'checking' }[action];
        assert.equal(refreshed.state, expected);
        assert.equal(h.controller.getState().state, expected);
        if (action === 'check') {
          const latest = await newer;
          assert.equal(latest.state, 'update_advisory');
          assert.ok(latest.reviewId > original.reviewId);
          await h.controller.confirm(latest.reviewId);
          assert.equal(h.calls.prepare, 1);
        } else {
          await h.controller.confirm(original.reviewId);
          assert.equal(h.calls.prepare, 0);
        }
      });
    }
  }
});

test('preparation completion revalidates expiry/context and respects cancellation during validation', async () => {
  for (const mode of ['expiry', 'context', 'cancel-in-clock']) {
    const gate = deferred();
    let clocks = 0;
    const h = harness({
      prepare: () => gate.promise,
      clock: () => {
        if (++clocks === 3 && mode === 'cancel-in-clock') h.controller.cancel();
        return h.live.now;
      },
    });
    const review = await h.controller.check();
    const pending = h.controller.confirm(review.reviewId);
    if (mode === 'expiry') h.live.now = h.fixture.response.expiresAt;
    if (mode === 'context') h.live.context.provider.configRevision = 'changed';
    gate.resolve();
    await pending;
    assert.equal(
      h.controller.getState().state,
      mode === 'cancel-in-clock' ? 'idle' : 'unavailable',
    );
    assert.equal(Object.hasOwn(h.controller.getState(), 'advisory'), false);
  }
});
