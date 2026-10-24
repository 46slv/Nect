// Synthetic, frozen native authority only; never imported by the app.
export function previewFixture() {
  const identity = {
    authority: 'native',
    integrity: 'verified',
    appId: 'com.nect.svg',
    appVersion: '0.1.1-preview.2',
    buildFlavor: 'normal',
    assetMode: 'static',
    runtimeIsolation: 'ordinary',
    target: 'x86_64-pc-windows-msvc',
    channel: 'preview',
    channelAuthority: 'native-build',
    packageKind: 'unknown',
    packageAuthority: 'unknown',
    commit: null,
    buildId: null,
  };
  const provider = {
    id: 'preview-fixture',
    configRevision: 'r1',
    targets: [identity.target],
    channels: ['preview'],
  };
  const request = {
    requestId: 'check-1',
    providerId: provider.id,
    configRevision: provider.configRevision,
    appId: identity.appId,
    currentVersion: identity.appVersion,
    target: identity.target,
    channel: identity.channel,
  };
  return {
    identity,
    provider,
    request,
    now: 1000,
    response: {
      schema: 2,
      expiresAt: 2000,
      discovery: {
        schema: 1,
        ...request,
        kind: 'candidate',
        candidate: {
          version: '0.1.1-preview.11',
          notes: 'Preview changes',
          buildId: 'fixture-build-11',
        },
      },
    },
  };
}

export function freezeFixture(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freezeFixture);
    Object.freeze(value);
  }
  return value;
}
