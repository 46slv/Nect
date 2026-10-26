import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { Window } from 'happy-dom';
import { validatePreviewDiscovery } from '../src/update-discovery.js';
import { createPreviewFixtureCheck } from '../src/preview-update-check.js';
import { previewFixture, freezeFixture } from './helpers/preview-discovery-fixture.mjs';

function check(input, reason) {
  freezeFixture(input);
  const before = JSON.stringify(input);
  const result = validatePreviewDiscovery(input);
  assert.equal(result.reason, reason);
  assert.equal(JSON.stringify(input), before);
  if (!['NEWER_METADATA', 'SOURCE_REPORTS_NONE'].includes(reason))
    assert.equal(Object.hasOwn(result, 'advisory'), false);
  return result;
}

test('fresh preview envelope reuses native audience/version checks and stays pure', () => {
  const f = previewFixture();
  assert.deepEqual(check(f, 'NEWER_METADATA'), {
    state: 'update_advisory',
    reason: 'NEWER_METADATA',
    advisory: {
      version: '0.1.1-preview.11',
      notes: 'Preview changes',
      buildId: 'fixture-build-11',
    },
  });
});

test('expiry is mandatory, exact and applies to scoped negative reports too', () => {
  for (const kind of ['candidate', 'no-update']) {
    for (const [now, expected] of [
      [1999, kind === 'candidate' ? 'NEWER_METADATA' : 'SOURCE_REPORTS_NONE'],
      [2000, 'SOURCE_EXPIRED'],
      [2001, 'SOURCE_EXPIRED'],
    ]) {
      const f = previewFixture();
      f.now = now;
      f.response.discovery.kind = kind;
      if (kind === 'no-update') delete f.response.discovery.candidate;
      check(f, expected);
    }
  }
});

test('clock and expiry reject missing, coerced, unsafe or nonfinite values', () => {
  for (const expiresAt of [undefined, null, '2000', 0, -1, 0.5, NaN, Infinity, 2 ** 53]) {
    const f = previewFixture();
    f.response.expiresAt = expiresAt;
    check(f, 'MALFORMED_SOURCE');
  }
  for (const now of [undefined, null, '1000', -1, 0.5, NaN, Infinity, 2 ** 53]) {
    check({ ...previewFixture(), now }, 'CLOCK_INVALID');
  }
  const f = previewFixture();
  Object.defineProperty(f, 'now', {
    get() {
      throw Error('clock unavailable');
    },
  });
  assert.deepEqual(validatePreviewDiscovery(f), { state: 'unavailable', reason: 'CLOCK_INVALID' });
});

test('null provider and early guards dominate response and clock without accessing later getters', () => {
  const later = {
    get identity() {
      throw Error('identity');
    },
    get request() {
      throw Error('request');
    },
    get response() {
      throw Error('response');
    },
    get now() {
      throw Error('now');
    },
  };
  for (const provider of [null, undefined, {}]) {
    const f = Object.create(null, Object.getOwnPropertyDescriptors(later));
    f.provider = provider;
    assert.equal(
      validatePreviewDiscovery(f).reason,
      provider == null ? 'NO_PROVIDER' : 'PROVIDER_INVALID',
    );
  }
  const f = previewFixture();
  f.identity.authority = 'browser';
  check(f, 'IDENTITY_UNTRUSTED');
  check({ ...previewFixture(), provider: null }, 'NO_PROVIDER');
  check({ ...previewFixture(), request: null }, 'REQUEST_INVALID');
});

test('preview contract refuses unknown native channel/target and stable channel', () => {
  for (const [key, value, expected] of [
    ['channel', null, 'CHANNEL_UNKNOWN'],
    ['target', null, 'TARGET_UNKNOWN'],
    ['channel', 'unknown', 'CHANNEL_UNSUPPORTED'],
    ['target', 'unknown', 'TARGET_UNSUPPORTED'],
  ]) {
    const f = previewFixture();
    f.identity[key] = f.request[key] = f.response.discovery[key] = value;
    check(f, expected);
  }
  const f = previewFixture();
  f.identity.channel = f.request.channel = f.response.discovery.channel = 'stable';
  f.identity.appVersion = f.request.currentVersion = f.response.discovery.currentVersion = '0.1.0';
  f.provider.channels = ['stable'];
  check(f, 'CHANNEL_UNSUPPORTED');
});

test('every audience field must match even for a negative report', () => {
  for (const kind of ['candidate', 'no-update']) {
    for (const key of Object.keys(previewFixture().request)) {
      const f = previewFixture();
      f.response.discovery.kind = kind;
      if (kind === 'no-update') delete f.response.discovery.candidate;
      f.response.discovery[key] += '-foreign';
      check(f, 'SOURCE_SCOPE_MISMATCH');
    }
  }
});

test('same build, changed commit/build metadata and older versions never advise', () => {
  for (const [version, expected] of [
    ['0.1.1-preview.2', 'NOT_NEWER_EQUAL'],
    ['0.1.1-preview.2+other', 'NOT_NEWER_EQUAL'],
    ['0.1.1-preview.1', 'NOT_NEWER_OLDER'],
    ['0.1.0', 'NOT_NEWER_OLDER'],
  ]) {
    const f = previewFixture();
    f.identity.commit = 'a'.repeat(40);
    f.response.discovery.candidate.version = version;
    f.response.discovery.candidate.buildId = 'a-new-commit-is-not-a-new-version';
    check(f, expected);
  }
  const f = previewFixture();
  f.response.discovery.candidate.version = '0.1.1-preview.9007199254740993';
  f.identity.appVersion =
    f.request.currentVersion =
    f.response.discovery.currentVersion =
      '0.1.1-preview.9007199254740992';
  check(f, 'NEWER_METADATA');
});

test('closed envelope and inner schema reject malformed metadata and URLs', () => {
  const edits = [
    (f) => {
      f.response.schema = 1;
    },
    (f) => {
      delete f.response.expiresAt;
    },
    (f) => {
      f.response.url = 'https://invalid';
    },
    (f) => {
      f.response.discovery = null;
    },
    (f) => {
      f.response.discovery = { kind: 'transport-error' };
    },
    (f) => {
      f.response.discovery.schema = 2;
    },
    (f) => {
      f.response.discovery.candidate.url = 'https://invalid';
    },
    (f) => {
      f.response.discovery.kind = 'no-update';
    },
    (f) => {
      delete f.response.discovery.requestId;
    },
  ];
  for (const edit of edits) {
    const f = previewFixture();
    edit(f);
    check(f, 'MALFORMED_SOURCE');
  }
  const f = previewFixture();
  f.response.discovery.candidate.version = 'v0.2.0';
  check(f, 'INVALID_CANDIDATE_VERSION');
  const g = previewFixture();
  g.response.discovery.candidate.notes = '\0';
  check(g, 'INVALID_NOTES');
});

test('missing source and local fixture failure remain distinct; expiry cannot bless an error', () => {
  check({ ...previewFixture(), response: null }, 'SOURCE_MISSING');
  check(
    { ...previewFixture(), response: { kind: 'transport-error' }, now: undefined },
    'PROVIDER_ERROR',
  );
  check(
    { ...previewFixture(), response: { kind: 'transport-error', extra: true } },
    'MALFORMED_SOURCE',
  );
  for (const input of [null, 0, [], 'bad'])
    assert.doesNotThrow(() => validatePreviewDiscovery(input));
});

function mount(f, options = {}) {
  const window = new Window();
  const document = window.document;
  const element = document.createElement('section');
  document.body.innerHTML = '<main data-document>Unchanged document</main>';
  document.body.append(element);
  let sequence = 0;
  const controller = createPreviewFixtureCheck({
    element,
    document,
    identity: f.identity,
    provider: f.provider,
    nextRequest: () => ({ ...f.request, requestId: `check-${++sequence}` }),
    readFixture: (request) => ({
      ...f.response,
      discovery: { ...f.response.discovery, ...request },
    }),
    clock: () => f.now,
    ...options,
  });
  return { window, document, element, controller };
}

test('fixture UI has no automatic check; user failure/retry uses a fresh audience and literal notes', async () => {
  const f = freezeFixture(previewFixture());
  let calls = 0;
  const seen = [];
  const notes = '<script>executed=true</script> **notes** https://invalid';
  const ui = mount(f, {
    readFixture: async (request) => {
      calls++;
      seen.push(request.requestId);
      assert.ok(Object.isFrozen(request));
      if (calls === 1) throw Error('local fixture failure');
      return {
        ...f.response,
        discovery: {
          ...f.response.discovery,
          ...request,
          candidate: { ...f.response.discovery.candidate, notes },
        },
      };
    },
  });
  try {
    assert.equal(calls, 0);
    const button = ui.element.querySelector('button');
    button.click();
    await new Promise((resolve) => setImmediate(resolve));
    assert.equal(ui.controller.getState().reason, 'PROVIDER_ERROR');
    assert.equal(button.textContent, 'Retry preview check');
    assert.equal(button.disabled, false);
    button.click();
    await new Promise((resolve) => setImmediate(resolve));
    assert.equal(ui.controller.getState().reason, 'NEWER_METADATA');
    assert.deepEqual(seen, ['check-1', 'check-2']);
    assert.equal(ui.element.querySelector('pre').textContent, notes);
    assert.equal(ui.element.querySelectorAll('a,script,img,input').length, 0);
    assert.match(ui.element.textContent, /fixture-build-11/);
    assert.equal(ui.element.querySelectorAll('button:not([hidden])').length, 2);
    assert.equal(ui.element.querySelectorAll('button')[1].textContent, 'Cancel');
    assert.equal(ui.document.querySelector('[data-document]').textContent, 'Unchanged document');
    const detached = ui.controller.getState();
    detached.advisory.notes = 'changed';
    assert.equal(ui.controller.getState().advisory.notes, notes);
  } finally {
    ui.controller.dispose();
    await ui.window.happyDOM.abort();
  }
});

test('pending fixture clears old advisory, deduplicates clicks and expires at completion', async () => {
  const f = previewFixture();
  let finish,
    calls = 0;
  const ui = mount(f, {
    readFixture: (request) => {
      calls++;
      const response = { ...f.response, discovery: { ...f.response.discovery, ...request } };
      if (calls === 3) return { ...response, expiresAt: 4000 };
      return calls === 1
        ? response
        : new Promise((resolve) => {
            finish = () => resolve(response);
          });
    },
  });
  try {
    await ui.controller.check();
    assert.ok(ui.element.querySelector('pre'));
    const pending = ui.controller.check();
    assert.equal(ui.element.querySelector('pre'), null);
    assert.equal(ui.element.getAttribute('aria-busy'), 'true');
    assert.equal(ui.element.querySelector('button').disabled, true);
    await ui.controller.check();
    assert.equal(calls, 2);
    f.now = 2000;
    finish();
    await pending;
    assert.equal(ui.controller.getState().reason, 'SOURCE_EXPIRED');
    assert.match(ui.element.textContent, /Preview metadata expired/);
    assert.equal(ui.element.querySelector('pre'), null);
    assert.equal(ui.element.getAttribute('aria-busy'), 'false');
    assert.equal((await ui.controller.check()).reason, 'NEWER_METADATA');
  } finally {
    ui.controller.dispose();
    await ui.window.happyDOM.abort();
  }
});

test('late disposed results cannot publish and old request responses fail before retry', async () => {
  const f = previewFixture();
  let finish;
  const ui = mount(f, {
    readFixture: () =>
      new Promise((resolve) => {
        finish = resolve;
      }),
  });
  const pending = ui.controller.check();
  ui.controller.dispose();
  finish(f.response);
  await pending;
  assert.equal(ui.element.querySelector('pre'), null);
  assert.equal(ui.element.querySelector('button').disabled, true);
  await ui.window.happyDOM.abort();

  let calls = 0;
  const retry = mount(f, {
    readFixture: (request) => ({
      ...f.response,
      discovery: {
        ...f.response.discovery,
        ...request,
        requestId: ++calls === 1 ? 'stale' : request.requestId,
      },
    }),
  });
  try {
    assert.equal((await retry.controller.check()).reason, 'SOURCE_SCOPE_MISMATCH');
    assert.equal((await retry.controller.check()).reason, 'NEWER_METADATA');
  } finally {
    retry.controller.dispose();
    await retry.window.happyDOM.abort();
  }
});

test('unconfigured/ineligible fixtures cannot call a source; invalid clock stays retryable', async () => {
  for (const provider of [null, {}]) {
    const f = previewFixture();
    f.provider = provider;
    const ui = mount(f, {
      readFixture() {
        assert.fail('must not read source');
      },
    });
    try {
      assert.equal(
        (await ui.controller.check()).reason,
        provider == null ? 'NO_PROVIDER' : 'PROVIDER_INVALID',
      );
    } finally {
      ui.controller.dispose();
      await ui.window.happyDOM.abort();
    }
  }
  let clockReads = 0;
  const ui = mount(previewFixture(), {
    clock() {
      if (++clockReads === 1) throw Error('clock');
      return 1000;
    },
  });
  try {
    assert.equal((await ui.controller.check()).reason, 'CLOCK_INVALID');
    assert.equal(ui.element.querySelector('button').disabled, false);
    assert.equal((await ui.controller.check()).reason, 'NEWER_METADATA');
  } finally {
    ui.controller.dispose();
    await ui.window.happyDOM.abort();
  }
});

test('fixture request factory failure and native isolation refuse before reading a source', async () => {
  for (const mode of ['request', 'isolated', 'browser', 'channel']) {
    const f = previewFixture();
    if (mode === 'isolated') f.identity.runtimeIsolation = 'acceptance-isolated';
    if (mode === 'browser') f.identity.authority = 'browser';
    if (mode === 'channel') f.identity.channel = null;
    let calls = 0;
    const ui = mount(f, {
      nextRequest() {
        if (mode === 'request') throw Error('fixture request');
        return f.request;
      },
      readFixture() {
        calls++;
        return f.response;
      },
    });
    try {
      assert.equal(
        (await ui.controller.check()).reason,
        {
          request: 'REQUEST_INVALID',
          isolated: 'VERIFICATION_RUNTIME',
          browser: 'IDENTITY_UNTRUSTED',
          channel: 'CHANNEL_UNKNOWN',
        }[mode],
      );
      assert.equal(calls, 0);
    } finally {
      ui.controller.dispose();
      await ui.window.happyDOM.abort();
    }
  }
});

test('fixture client is outside the production module graph and has no updater effects', () => {
  const root = new URL('../src/main.js', import.meta.url),
    visited = new Set();
  function visit(url) {
    if (visited.has(url.href)) return;
    visited.add(url.href);
    const source = readFileSync(url, 'utf8');
    for (const match of source.matchAll(/(?:from\s*|import\s*\()\s*['"]([^'"]+\.js)['"]/g)) {
      if (match[1].startsWith('.')) visit(new URL(match[1], url));
    }
  }
  visit(root);
  assert.equal(visited.has(new URL('../src/preview-update-check.js', import.meta.url).href), false);
  const production = readFileSync(new URL('../src/build-info.js', import.meta.url), 'utf8');
  assert.match(production, /const provider = null;/);
  for (const name of ['preview-update-check.js', 'update-discovery.js']) {
    const source = readFileSync(new URL(`../src/${name}`, import.meta.url), 'utf8');
    assert.doesNotMatch(
      source,
      /fetch\(|XMLHttpRequest|WebSocket|localStorage|sessionStorage|setInterval|setTimeout|Date\.now|invoke\(|restart\(|apply_update|tauri_plugin_updater/,
    );
  }
});
