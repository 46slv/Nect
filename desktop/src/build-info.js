import { desktop, queryBuildIdentity } from './storage.js';
import { validateDiscovery } from './update-discovery.js';

// Production has no provider, settings, fixture injection or transport.
const provider = null;
let query;
const frontendVersion = typeof __NECT_APP_VERSION__ === 'string' ? __NECT_APP_VERSION__ : 'unknown';

export function identityLabel(identity) {
  if (identity?.authority === 'browser') return 'Browser preview';
  if (identity?.authority !== 'native') return 'Native identity query failed';
  if (identity.integrity !== 'verified') return 'Native identity invalid';
  if (identity.buildFlavor === 'recovery-acceptance-test') return 'Recovery acceptance test';
  if (identity.runtimeIsolation === 'acceptance-isolated') return 'Isolated native build';
  if (identity.assetMode === 'dev-server') return 'Native development build';
  if (
    identity.buildFlavor === 'normal' &&
    identity.assetMode === 'static' &&
    identity.runtimeIsolation === 'ordinary'
  )
    return 'Native build';
  return 'Native runtime unknown';
}

export function renderDiscovery(element, discovery, document) {
  const labels = {
    unconfigured: 'Update checks not configured',
    no_update_reported_by_source: 'No update reported by this source',
    update_advisory: 'Newer version reported by source',
    unavailable: 'Update information unavailable',
  };
  element.replaceChildren();
  const status = document.createElement('p');
  status.textContent = labels[discovery.state] ?? labels.unavailable;
  element.append(status);
  if (discovery.state !== 'update_advisory' || !discovery.advisory) return;
  for (const key of ['version', 'notes', 'buildId']) {
    if (!Object.hasOwn(discovery.advisory, key)) continue;
    const text = document.createElement(key === 'notes' ? 'pre' : 'p');
    text.textContent = discovery.advisory[key];
    element.append(text);
  }
}

function readIdentity() {
  if (!query)
    query = desktop
      ? queryBuildIdentity().catch(() => ({ authority: 'unavailable', integrity: 'unknown' }))
      : Promise.resolve({ authority: 'browser', integrity: 'unknown' });
  return query;
}

export function ensureBuildInfo(app, document) {
  const header = app.querySelector('header');
  if (!header || header.querySelector('[data-build-info]')) return;
  const menu = document.createElement('details');
  menu.className = 'command-menu build-info';
  menu.dataset.buildInfo = '';
  const summary = document.createElement('summary');
  summary.textContent = 'Build';
  const content = document.createElement('div');
  content.className = 'command-menu-items';
  const runtime = document.createElement('p');
  runtime.textContent = desktop ? 'Native identity' : 'Browser preview';
  const version = document.createElement('p');
  version.textContent = `Frontend ${frontendVersion}`;
  const commit = document.createElement('p');
  commit.dataset.buildCommit = '';
  commit.textContent = 'Commit unknown';
  const updates = document.createElement('div');
  renderDiscovery(updates, validateDiscovery({ provider }), document);
  content.append(runtime, version, commit, updates);
  menu.append(summary, content);
  menu.addEventListener('toggle', () => {
    if (!menu.open) return;
    header.querySelectorAll('.command-menu[open]').forEach((other) => {
      if (other !== menu) other.open = false;
    });
    void readIdentity().then((identity) => {
      runtime.textContent = identityLabel(identity);
      version.textContent =
        identity?.authority === 'native'
          ? `Version ${identity.appVersion}`
          : `Frontend ${frontendVersion}`;
      const nativeCommit =
        identity?.authority === 'native' &&
        identity.integrity === 'verified' &&
        typeof identity.commit === 'string' &&
        identity.commit.length === 40 &&
        /^[0-9a-f]{40}$/.test(identity.commit)
          ? identity.commit
          : null;
      commit.textContent = nativeCommit ? `Commit ${nativeCommit.slice(0, 8)}` : 'Commit unknown';
      if (nativeCommit) commit.title = nativeCommit;
      else commit.removeAttribute('title');
      renderDiscovery(updates, validateDiscovery({ identity, provider }), document);
    });
  });
  const group = document.createElement('div');
  group.className = 'header-end';
  group.setAttribute('role', 'group');
  group.setAttribute('aria-label', 'Document and application');
  const title = header.querySelector('[data-document-title]');
  const help = header.querySelector('[data-action="help"]');
  if (title) group.append(title);
  if (help) group.append(help);
  group.append(menu);
  header.append(group);
}
