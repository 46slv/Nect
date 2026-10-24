import test from 'node:test';
import assert from 'node:assert/strict';
import { assessIdentity, validateDiscovery } from '../src/update-discovery.js';
import { parseVersion, compareVersions, validateVersionSet } from '../src/update-version.js';

const identity = Object.freeze({
  authority: 'native',
  integrity: 'verified',
  appId: 'com.nect.svg',
  appVersion: '0.1.0',
  buildFlavor: 'normal',
  assetMode: 'static',
  runtimeIsolation: 'ordinary',
  target: 'x86_64-pc-windows-msvc',
  channel: 'stable',
  channelAuthority: 'native-build',
  packageKind: 'unknown',
  packageAuthority: 'unknown',
  commit: null,
  buildId: null,
});
const provider = Object.freeze({
  id: 'fixture',
  configRevision: 'r1',
  targets: Object.freeze([identity.target]),
  channels: Object.freeze(['stable', 'preview']),
});
function fixture(patch = {}) {
  const current = patch.identity ?? identity;
  const request = {
    requestId: 'request-1',
    providerId: provider.id,
    configRevision: provider.configRevision,
    appId: current.appId,
    currentVersion: current.appVersion,
    target: current.target,
    channel: current.channel,
  };
  return {
    identity: current,
    provider,
    request,
    response: {
      schema: 1,
      ...request,
      kind: 'candidate',
      candidate: { version: '0.2.0', notes: '' },
    },
    ...patch,
  };
}
const reason = (input, expected) => assert.equal(validateDiscovery(input).reason, expected);
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

test('null provider dominates all identity and positive fixture states without mutation', () => {
  for (const current of [identity, { ...identity, authority: 'browser' }, null]) {
    const input = freeze(fixture({ identity: current, provider: null }));
    const before = JSON.stringify(input);
    assert.deepEqual(validateDiscovery(input), { state: 'unconfigured', reason: 'NO_PROVIDER' });
    assert.equal(JSON.stringify(input), before);
  }
});

test('first-match guards do not inspect later failing inputs', () => {
  const later = {
    get identity() {
      throw Error('later identity');
    },
    get request() {
      throw Error('later request');
    },
    get response() {
      throw Error('later response');
    },
  };
  const input = Object.create(null, Object.getOwnPropertyDescriptors(later));
  input.provider = null;
  reason(input, 'NO_PROVIDER');
  input.provider = {};
  reason(input, 'PROVIDER_INVALID');
  const f = fixture();
  reason(
    {
      ...f,
      identity: { ...identity, authority: 'browser' },
      request: null,
      response: { kind: 'transport-error' },
    },
    'IDENTITY_UNTRUSTED',
  );
  reason({ ...f, request: null, response: { kind: 'transport-error' } }, 'REQUEST_INVALID');
});

test('identity failures are first-match, conservative and exact', () => {
  for (const [patch, expected] of [
    [{ authority: 'browser' }, 'IDENTITY_UNTRUSTED'],
    [{ authority: 'unavailable' }, 'IDENTITY_UNTRUSTED'],
    [{ authority: undefined }, 'IDENTITY_UNTRUSTED'],
    ...['invalid', 'unknown', undefined].map((integrity) => [{ integrity }, 'IDENTITY_UNTRUSTED']),
    [{ appId: 'other' }, 'IDENTITY_UNTRUSTED'],
    [{ appVersion: '0.1' }, 'INVALID_CURRENT_VERSION'],
    ...['recovery-acceptance-test', 'unknown', undefined].map((buildFlavor) => [
      { buildFlavor },
      'VERIFICATION_RUNTIME',
    ]),
    ...['dev-server', 'unknown', undefined].map((assetMode) => [
      { assetMode },
      'VERIFICATION_RUNTIME',
    ]),
    ...['acceptance-isolated', 'unknown', undefined].map((runtimeIsolation) => [
      { runtimeIsolation },
      'VERIFICATION_RUNTIME',
    ]),
    [{ target: null }, 'TARGET_UNKNOWN'],
    [{ target: undefined }, 'TARGET_UNKNOWN'],
    ...['windows-x64', 'X86_64-pc-windows-msvc', 'x86_64-pc-windows-gnu', 'unknown'].map(
      (target) => [{ target }, 'TARGET_UNSUPPORTED'],
    ),
    [{ channel: null }, 'CHANNEL_UNKNOWN'],
    [{ channel: undefined }, 'CHANNEL_UNKNOWN'],
    [{ channelAuthority: 'unknown' }, 'CHANNEL_UNKNOWN'],
    [{ channelAuthority: undefined }, 'CHANNEL_UNKNOWN'],
    ...['Stable', 'nightly', 'unknown'].map((channel) => [{ channel }, 'CHANNEL_UNSUPPORTED']),
    [{ appVersion: '0.1.0-beta.1' }, 'CHANNEL_VERSION_MISMATCH'],
  ]) {
    const current = freeze({ ...identity, ...patch });
    assert.deepEqual(assessIdentity(current), { eligible: false, reason: expected });
    reason(fixture({ identity: current }), expected);
  }
  assert.deepEqual(assessIdentity(identity), {
    eligible: true,
    reason: 'ELIGIBLE_FOR_METADATA_ONLY',
  });
  assert.equal(
    assessIdentity({ ...identity, authority: 'browser', appVersion: 'invalid', target: null })
      .reason,
    'IDENTITY_UNTRUSTED',
  );
  assert.equal(
    assessIdentity({ ...identity, appVersion: 'invalid', buildFlavor: 'unknown' }).reason,
    'INVALID_CURRENT_VERSION',
  );
});

test('strict complete SemVer and arbitrary precision precedence', () => {
  for (const value of [
    '',
    null,
    1,
    'v0.1.0',
    '0.1',
    '0.1.0 ',
    '0.1.0\n',
    ' 0.1.0',
    '00.1.0',
    '0.01.0',
    '0.1.00',
    '0.1.0-01',
    '0.1.0-beta.01',
    '0.1.0-',
    '0.1.0+',
    '0.1.0-a..b',
    '0.1.0+a..b',
    '0.1.0+é',
    '0.1.0+one+two',
    `0.1.0+${'a'.repeat(123)}`,
  ])
    assert.equal(parseVersion(value), null, String(value));
  for (const value of ['0.2.0+001', '0.1.0+1', '0.1.0--', `0.1.0+${'a'.repeat(122)}`])
    assert.ok(parseVersion(value), value);
  for (const [a, b, expected] of [
    ['0.1.0+001', '0.1.0+1', 0],
    ['0.10.0', '0.9.0', 1],
    ['0.2.0-beta.11', '0.2.0-beta.2', 1],
    ['9007199254740993.0.0', '9007199254740992.0.0', 1],
    ['0.9007199254740993.0', '0.9007199254740992.0', 1],
    ['0.1.9007199254740993', '0.1.9007199254740992', 1],
    ['0.1.0-beta.9007199254740993', '0.1.0-beta.9007199254740992', 1],
    ['1.0.0', '1.0.0-rc.1', 1],
    ['1.0.0-a', '1.0.0-a.1', -1],
    ['1.0.0-1', '1.0.0-a', -1],
    ['1.0.0-a', '1.0.0-b', -1],
  ]) {
    assert.equal(compareVersions(a, b), expected);
    assert.equal(compareVersions(b, a), -expected || 0);
  }
});

test('version set rejects malformed and exact drift in every source', () => {
  const versions = { cargo: '0.1.0', npm: '0.1.0', tauriBase: '0.1.0', tauriEffective: '0.1.0' };
  assert.deepEqual(validateVersionSet(freeze(versions)), { ok: true, reason: 'CONSISTENT' });
  for (const key of Object.keys(versions)) {
    assert.deepEqual(validateVersionSet({ ...versions, [key]: '0.1.0+001' }), {
      ok: false,
      reason: 'VERSION_DRIFT',
    });
    assert.deepEqual(validateVersionSet({ ...versions, [key]: '0.1' }), {
      ok: false,
      reason: 'INVALID_VERSION',
    });
  }
  assert.equal(validateVersionSet(null).reason, 'INVALID_VERSION');
});

test('provider schema, supported unique arrays and request audience are closed', () => {
  for (const bad of [
    {},
    false,
    [],
    { ...provider, id: '_bad' },
    { ...provider, id: 'x'.repeat(129) },
    { ...provider, configRevision: '' },
    { ...provider, configRevision: 1 },
    { ...provider, channels: [] },
    { ...provider, channels: ['stable', 'stable'] },
    { ...provider, channels: ['unknown'] },
    { ...provider, targets: ['unknown'] },
    { ...provider, targets: [identity.target, identity.target] },
    { ...provider, targets: [] },
    { ...provider, targets: Array(1) },
    { ...provider, channels: Array(1) },
    { ...provider, url: 'https://invalid' },
  ])
    reason(fixture({ provider: bad }), 'PROVIDER_INVALID');
  const f = fixture();
  for (const key of Object.keys(f.request)) {
    for (const value of [undefined, null, 7, ...(key === 'requestId' ? [] : ['different'])])
      reason({ ...f, request: { ...f.request, [key]: value } }, 'REQUEST_INVALID');
  }
  reason({ ...f, request: { ...f.request, requestId: '_bad' } }, 'REQUEST_INVALID');
  reason({ ...f, request: { ...f.request, extra: true } }, 'REQUEST_INVALID');
  reason(fixture({ identity: { ...identity, channel: null } }), 'CHANNEL_UNKNOWN');
  reason(fixture({ identity: { ...identity, target: null } }), 'TARGET_UNKNOWN');
});

test('negative source must match all seven scope values exactly', () => {
  const f = fixture();
  const negative = { schema: 1, ...f.request, kind: 'no-update' };
  assert.deepEqual(validateDiscovery(freeze({ ...f, response: negative })), {
    state: 'no_update_reported_by_source',
    reason: 'SOURCE_REPORTS_NONE',
  });
  for (const key of Object.keys(f.request)) {
    for (const response of [negative, f.response])
      reason({ ...f, response: { ...response, [key]: 'mismatch' } }, 'SOURCE_SCOPE_MISMATCH');
  }
});

test('transport, missing, malformed and conflicting source remain distinct', () => {
  const f = fixture();
  reason({ ...f, response: { kind: 'transport-error' } }, 'PROVIDER_ERROR');
  for (const response of [null, undefined]) reason({ ...f, response }, 'SOURCE_MISSING');
  for (const response of [
    {},
    [],
    'text',
    { ...f.response, schema: 2 },
    { ...f.response, schema: '1' },
    { ...f.response, extra: 1 },
    { ...f.response, requestId: null },
    { ...f.response, kind: 'no-update' },
    { ...f.response, candidate: undefined },
    { ...f.response, candidate: { version: '0.2.0' } },
    { ...f.response, candidate: { version: 2, notes: '' } },
    { ...f.response, candidate: { ...f.response.candidate, url: 'https://invalid' } },
    { ...f.response, kind: 'transport-error' },
  ])
    reason({ ...f, response }, 'MALFORMED_SOURCE');
  for (const key of ['schema', ...Object.keys(f.request), 'kind']) {
    const response = { ...f.response };
    delete response[key];
    reason({ ...f, response }, 'MALFORMED_SOURCE');
  }
});

test('candidate precedence ignores provenance and metadata, preview compares numerically', () => {
  for (const [current, candidate, expected] of [
    ['0.1.0', '0.1.0+changed', 'NOT_NEWER_EQUAL'],
    ['0.1.0+001', '0.1.0+1', 'NOT_NEWER_EQUAL'],
    ['0.2.0', '0.1.0', 'NOT_NEWER_OLDER'],
    ['0.9.0', '0.10.0', 'NEWER_METADATA'],
    ['0.1.0', '0.2.0+001', 'NEWER_METADATA'],
    ['0.1.0', '0.2.0-beta.1', 'PRERELEASE_ON_STABLE'],
    ['9007199254740992.0.0', '9007199254740993.0.0', 'NEWER_METADATA'],
  ]) {
    const f = fixture({ identity: { ...identity, appVersion: current, commit: 'old' } });
    f.response.candidate = { version: candidate, notes: '', buildId: 'new commit' };
    reason(freeze(f), expected);
  }
  for (const [current, next] of [
    ['0.2.0-beta.2', '0.2.0-beta.11'],
    ['0.2.0-beta.9007199254740992', '0.2.0-beta.9007199254740993'],
  ]) {
    const f = fixture({ identity: { ...identity, channel: 'preview', appVersion: current } });
    f.response.candidate.version = next;
    reason(freeze(f), 'NEWER_METADATA');
  }
  for (const value of ['v0.2.0', '0.2', '0.02.0', '0.2.0-beta.01', `0.2.0+${'a'.repeat(123)}`]) {
    const f = fixture();
    f.response.candidate.version = value;
    reason(f, 'INVALID_CANDIDATE_VERSION');
  }
});

test('notes and optional build label are byte bounded literal strings', () => {
  for (const [notes, expected] of [
    ['', 'NEWER_METADATA'],
    ['x'.repeat(4096), 'NEWER_METADATA'],
    ['x'.repeat(4097), 'INVALID_NOTES'],
    ['あ'.repeat(1365), 'NEWER_METADATA'],
    ['あ'.repeat(1366), 'INVALID_NOTES'],
    ['a\t\n\r', 'NEWER_METADATA'],
    ['<script>alert(1)</script> https://invalid **literal**', 'NEWER_METADATA'],
    ['a\0b', 'INVALID_NOTES'],
    ['\u007f', 'INVALID_NOTES'],
    ['\u0085', 'INVALID_NOTES'],
    [1, 'INVALID_NOTES'],
    [null, 'INVALID_NOTES'],
    [{}, 'INVALID_NOTES'],
  ]) {
    const f = fixture();
    f.response.candidate.notes = notes;
    reason(freeze(f), expected);
  }
  for (const [buildId, expected] of [
    ['x'.repeat(128), 'NEWER_METADATA'],
    ['あ'.repeat(43), 'INVALID_NOTES'],
    ['x'.repeat(129), 'INVALID_NOTES'],
    ['\0', 'INVALID_NOTES'],
    [null, 'INVALID_NOTES'],
    [undefined, 'INVALID_NOTES'],
    [9, 'INVALID_NOTES'],
  ]) {
    const f = fixture();
    f.response.candidate.buildId = buildId;
    reason(freeze(f), expected);
  }
});

test('invalid JavaScript inputs never escape as uncaught exceptions', () => {
  const hostile = new Proxy(
    {},
    {
      get() {
        throw Error('hostile');
      },
      ownKeys() {
        throw Error('hostile');
      },
    },
  );
  for (const input of [
    undefined,
    null,
    false,
    2,
    'x',
    [],
    hostile,
    { provider: hostile },
    fixture({ request: hostile }),
    fixture({ response: hostile }),
  ])
    assert.doesNotThrow(() => validateDiscovery(input));
  assert.doesNotThrow(() => assessIdentity(hostile));
  assert.doesNotThrow(() => validateVersionSet(hostile));
});
