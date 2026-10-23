import { parseVersion, compareVersions } from './update-version.js';

const TARGET = 'x86_64-pc-windows-msvc';
const CHANNELS = ['stable', 'preview'];
const id = (value) =>
  typeof value === 'string' && /^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.exec(value)?.[0] === value;
const record = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const keys = (value, required, optional = []) =>
  record(value) &&
  required.every((key) => Object.hasOwn(value, key)) &&
  Reflect.ownKeys(value).every((key) => required.includes(key) || optional.includes(key));
const supportedList = (value, supported) =>
  Array.isArray(value) &&
  value.length > 0 &&
  new Set(value).size === value.length &&
  value.every((item) => supported.includes(item));
const result = (reason, state = 'unavailable') => ({ state, reason });

export function assessIdentity(identity) {
  try {
    if (
      !record(identity) ||
      identity.authority !== 'native' ||
      identity.integrity !== 'verified' ||
      identity.appId !== 'com.nect.svg'
    )
      return { eligible: false, reason: 'IDENTITY_UNTRUSTED' };
    const version = parseVersion(identity.appVersion);
    if (!version) return { eligible: false, reason: 'INVALID_CURRENT_VERSION' };
    if (
      identity.buildFlavor !== 'normal' ||
      identity.assetMode !== 'static' ||
      identity.runtimeIsolation !== 'ordinary'
    )
      return { eligible: false, reason: 'VERIFICATION_RUNTIME' };
    if (identity.target == null) return { eligible: false, reason: 'TARGET_UNKNOWN' };
    if (identity.target !== TARGET) return { eligible: false, reason: 'TARGET_UNSUPPORTED' };
    if (identity.channel == null || identity.channelAuthority !== 'native-build')
      return { eligible: false, reason: 'CHANNEL_UNKNOWN' };
    if (!CHANNELS.includes(identity.channel))
      return { eligible: false, reason: 'CHANNEL_UNSUPPORTED' };
    if (identity.channel === 'stable' && version.pre.length)
      return { eligible: false, reason: 'CHANNEL_VERSION_MISMATCH' };
    return { eligible: true, reason: 'ELIGIBLE_FOR_METADATA_ONLY' };
  } catch {
    return { eligible: false, reason: 'IDENTITY_UNTRUSTED' };
  }
}

const scope = [
  'requestId',
  'providerId',
  'configRevision',
  'appId',
  'currentVersion',
  'target',
  'channel',
];
function validProvider(provider) {
  return (
    keys(provider, ['id', 'configRevision', 'targets', 'channels']) &&
    id(provider.id) &&
    id(provider.configRevision) &&
    supportedList(provider.targets, [TARGET]) &&
    supportedList(provider.channels, CHANNELS)
  );
}
function validRequest(request, provider, identity) {
  return (
    keys(request, scope) &&
    id(request.requestId) &&
    request.providerId === provider.id &&
    request.configRevision === provider.configRevision &&
    request.appId === identity.appId &&
    request.currentVersion === identity.appVersion &&
    request.target === identity.target &&
    request.channel === identity.channel &&
    provider.targets.includes(request.target) &&
    provider.channels.includes(request.channel)
  );
}
function literal(value, limit) {
  return (
    typeof value === 'string' &&
    !/[\p{Cc}\p{Cs}]/u.test(value.replace(/[\t\r\n]/g, '')) &&
    new TextEncoder().encode(value).length <= limit
  );
}

// Test fixtures only. No provider, transport, IPC, persistence or actions are created here.
export function validateDiscovery(input) {
  let stage = 'PROVIDER_INVALID';
  try {
    const provider = input?.provider;
    if (provider == null) return result('NO_PROVIDER', 'unconfigured');
    if (!validProvider(provider)) return result('PROVIDER_INVALID');
    stage = 'IDENTITY_UNTRUSTED';
    const identity = input?.identity;
    const assessment = assessIdentity(identity);
    if (!assessment.eligible) return result(assessment.reason);
    stage = 'REQUEST_INVALID';
    const request = input?.request;
    if (!validRequest(request, provider, identity)) return result('REQUEST_INVALID');
    stage = 'MALFORMED_SOURCE';
    const response = input?.response;
    if (keys(response, ['kind']) && response.kind === 'transport-error')
      return result('PROVIDER_ERROR');
    if (response == null) return result('SOURCE_MISSING');
    if (
      !record(response) ||
      !['no-update', 'candidate'].includes(response.kind) ||
      !keys(response, [
        'schema',
        ...scope,
        'kind',
        ...(response.kind === 'candidate' ? ['candidate'] : []),
      ]) ||
      response.schema !== 1 ||
      scope.some((key) => typeof response[key] !== 'string')
    )
      return result('MALFORMED_SOURCE');
    if (
      response.kind === 'candidate' &&
      (!keys(response.candidate, ['version', 'notes'], ['buildId']) ||
        typeof response.candidate.version !== 'string')
    )
      return result('MALFORMED_SOURCE');
    if (scope.some((key) => response[key] !== request[key])) return result('SOURCE_SCOPE_MISMATCH');
    if (response.kind === 'no-update')
      return result('SOURCE_REPORTS_NONE', 'no_update_reported_by_source');
    const candidate = response.candidate;
    const version = parseVersion(candidate.version);
    if (!version) return result('INVALID_CANDIDATE_VERSION');
    if (identity.channel === 'stable' && version.pre.length) return result('PRERELEASE_ON_STABLE');
    if (
      !literal(candidate.notes, 4096) ||
      (Object.hasOwn(candidate, 'buildId') && !literal(candidate.buildId, 128))
    )
      return result('INVALID_NOTES');
    const order = compareVersions(candidate.version, identity.appVersion);
    if (order === 0) return result('NOT_NEWER_EQUAL');
    if (order < 0) return result('NOT_NEWER_OLDER');
    return {
      ...result('NEWER_METADATA', 'update_advisory'),
      advisory: {
        version: candidate.version,
        notes: candidate.notes,
        ...(Object.hasOwn(candidate, 'buildId') ? { buildId: candidate.buildId } : {}),
      },
    };
  } catch {
    return result(stage);
  }
}
