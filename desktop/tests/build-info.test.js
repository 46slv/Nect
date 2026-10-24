import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { Window } from 'happy-dom';
import { ensureBuildInfo, identityLabel, renderDiscovery } from '../src/build-info.js';
import { validateDiscovery } from '../src/update-discovery.js';

test('native query success and failure are lazy, cached, read-only IPC', () => {
  for (const mode of ['success', 'failure']) {
    const run = spawnSync(
      process.execPath,
      [fileURLToPath(new URL('./helpers/build-info-native-fixture.mjs', import.meta.url)), mode],
      { encoding: 'utf8', timeout: 10000 },
    );
    assert.equal(run.status, 0, run.stdout + run.stderr);
    assert.match(run.stdout, /one read-only query/);
    if (mode === 'success') console.log(run.stdout.trim());
  }
});

test('native CI commit display and honest fallbacks use the same read-only query', () => {
  for (const mode of [
    'missing-commit',
    'invalid-commit',
    'newline-commit',
    'invalid-identity',
    'browser-identity',
    'dev-server',
  ]) {
    const run = spawnSync(
      process.execPath,
      [fileURLToPath(new URL('./helpers/build-info-native-fixture.mjs', import.meta.url)), mode],
      { encoding: 'utf8', timeout: 10000 },
    );
    assert.equal(run.status, 0, run.stdout + run.stderr);
    assert.match(run.stdout, /one read-only query/);
  }
});

test('build info labels honest runtime states', () => {
  const identity = {
    authority: 'native',
    integrity: 'verified',
    appVersion: '0.1.0',
    buildFlavor: 'normal',
    assetMode: 'static',
    runtimeIsolation: 'ordinary',
  };
  for (const [patch, label] of [
    [{}, 'Native build'],
    [{ assetMode: 'dev-server' }, 'Native development build'],
    [{ runtimeIsolation: 'acceptance-isolated' }, 'Isolated native build'],
    [{ buildFlavor: 'recovery-acceptance-test' }, 'Recovery acceptance test'],
    [{ integrity: 'invalid' }, 'Native identity invalid'],
    [{ authority: 'browser' }, 'Browser preview'],
    [{ authority: 'unavailable' }, 'Native identity query failed'],
    [{ buildFlavor: 'unknown' }, 'Native runtime unknown'],
  ])
    assert.equal(identityLabel({ ...identity, ...patch }), label);
});

test('advisory notes and labels stay literal without HTML, Markdown or links', () => {
  const window = new Window();
  const document = window.document,
    element = document.createElement('div');
  const notes =
    '<script>window.executed=true</script>\n<a href="https://invalid">x</a> **bold** https://invalid';
  renderDiscovery(
    element,
    {
      state: 'update_advisory',
      advisory: { version: '0.2.0', notes, buildId: '<img src=x onerror=alert(1)>' },
    },
    document,
  );
  assert.equal(element.querySelector('pre').textContent, notes);
  assert.equal(element.querySelectorAll('script,a,img,strong,button').length, 0);
  assert.equal(window.executed, undefined);
  assert.match(element.textContent, /^Newer version reported by source/);
  renderDiscovery(element, { state: 'no_update_reported_by_source' }, document);
  assert.equal(element.textContent, 'No update reported by this source');
  renderDiscovery(element, validateDiscovery({ provider: null }), document);
  assert.equal(element.textContent, 'Update checks not configured');
  window.happyDOM.abort();
});

test('Build menu is read-only, browser honest and idempotent', async () => {
  const window = new Window(),
    document = window.document;
  const app = document.createElement('main');
  app.innerHTML = '<header><b>Nect</b></header><section data-document>authored document</section>';
  document.body.append(app);
  ensureBuildInfo(app, document);
  ensureBuildInfo(app, document);
  assert.equal(app.querySelectorAll('[data-build-info]').length, 1);
  const menu = app.querySelector('[data-build-info]');
  assert.equal(menu.querySelector('summary').textContent, 'Build');
  menu.open = true;
  menu.dispatchEvent(new window.Event('toggle'));
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.match(menu.textContent, /Browser preview/);
  assert.equal(menu.querySelector('[data-build-commit]').textContent, 'Commit unknown');
  assert.equal(menu.querySelector('[data-build-commit]').getAttribute('title'), null);
  assert.match(menu.textContent, /Update checks not configured/);
  assert.equal(app.querySelector('[data-document]').textContent, 'authored document');
  assert.equal(menu.querySelectorAll('button,input,a').length, 0);
  window.happyDOM.abort();
});

test('production has one read-only native query and fixed null provider without update effects', () => {
  const ui = readFileSync(new URL('../src/build-info.js', import.meta.url), 'utf8');
  const native = readFileSync(
    new URL('../src-tauri/src/build_identity.rs', import.meta.url),
    'utf8',
  );
  const discovery = readFileSync(new URL('../src/update-discovery.js', import.meta.url), 'utf8');
  assert.match(ui, /const provider = null;/);
  assert.match(native, /pub fn query_build_identity\(identity: tauri::State<'_, BuildIdentity>\)/);
  assert.match(native, /identity\.inner\(\)\.clone\(\)/);
  for (const source of [ui, native, discovery])
    assert.doesNotMatch(
      source,
      /fetch\(|XMLHttpRequest|WebSocket|localStorage|sessionStorage|setInterval|setTimeout|std::fs|std::env::var|restart\(|apply_update|tauri_plugin_updater/,
    );
  const main = readFileSync(new URL('../src-tauri/src/main.rs', import.meta.url), 'utf8');
  assert.match(main, /storage\.0\.is_some\(\),/);
  assert.match(main, /build_identity::query_build_identity,/);
});
