import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const workflow = await readFile(
  new URL('../../.github/workflows/verify-windows.yml', import.meta.url),
  'utf8',
);
const { scripts } = JSON.parse(await readFile(new URL('../package.json', import.meta.url), 'utf8'));
const lane = (name) => {
  const match = workflow.match(
    new RegExp(`^  ${name}:\\r?\\n([\\s\\S]*?)(?=^  [a-z][a-z-]*:|$(?![\\s\\S]))`, 'm'),
  );
  assert.ok(match, `Missing lane ${name}`);
  return match[1];
};
const native = lane('native-frontend');
const frontend = lane('frontend');
const browser = lane('browser');
const gate = lane('windows');

test('Windows keeps native, frontend and Chrome lanes independent', () => {
  assert.ok(
    native.indexOf('Build Windows app without packaging') < native.indexOf('Run native Rust tests'),
  );
  assert.doesNotMatch(
    native,
    /Run frontend unit and DOM tests|Run actual Chrome interaction tests/,
  );
  assert.match(
    frontend,
    /strategy:\r?\n      fail-fast: false\r?\n      matrix:\r?\n        shard: \[1, 2\]/,
  );
  assert.doesNotMatch(frontend, /cargo test|Run actual Chrome interaction tests/);
  assert.match(browser, /matrix:\s+shard: \[1, 2, 3\]/);
  assert.match(browser, /BROWSER_SHARD: \$\{\{ matrix.shard \}\}/);
  assert.match(browser, /node scripts\/browser-suite-runner.mjs --shard "\$env:BROWSER_SHARD\/3"/);
  for (const body of [native, frontend, browser]) assert.doesNotMatch(body, /needs:/);
});

test('Windows frontend bounds concurrency without changing the full test selection', () => {
  assert.equal(scripts.test, 'node --test --test-concurrency=1 tests/*.test.js');
  assert.equal(scripts['test:ci:windows'], 'node --test --test-concurrency=2 tests/*.test.js');
  assert.equal(
    scripts['test:ci:windows'],
    scripts.test.replace('--test-concurrency=1', '--test-concurrency=2'),
  );
  for (const hook of ['pretest', 'posttest', 'pretest:ci:windows', 'posttest:ci:windows']) {
    assert.equal(scripts[hook], undefined, `Unexpected test lifecycle hook: ${hook}`);
  }
  assert.match(frontend, /FRONTEND_SHARD: \$\{\{ matrix.shard \}\}/);
  const shardCommand = scripts['test:ci:windows'].replace(
    ' tests/*.test.js',
    ` "--test-shard=$env:FRONTEND_SHARD/2" 'tests/*.test.js'`,
  );
  assert.ok(frontend.split(/\r?\n/).includes(`        run: ${shardCommand}`));
  assert.doesNotMatch(browser, /test:ci:windows/);
});

test('all lanes provision dependencies and fonts without new permissions or actions', () => {
  for (const body of [native, frontend, browser]) {
    assert.match(body, /runs-on: windows-2022/);
    assert.match(body, /node-version: 24/);
    assert.match(body, /run: npm ci/);
    assert.match(body, /prepare-windows-japanese-fonts.ps1 -InstallIfMissing/);
    assert.match(body, /persist-credentials: false/);
  }
  assert.match(workflow, /permissions:\r?\n  contents: read/);
  assert.doesNotMatch(workflow, /pull_request_target:|secrets:|continue-on-error:/);
});

test('required windows aggregate rejects every non-success lane result', () => {
  assert.match(gate, /if: \$\{\{ always\(\) \}\}/);
  assert.match(gate, /needs: \[native-frontend, frontend, browser\]/);
  assert.match(gate, /NATIVE_FRONTEND_RESULT: \$\{\{ needs.native-frontend.result \}\}/);
  assert.match(gate, /FRONTEND_RESULT: \$\{\{ needs.frontend.result \}\}/);
  assert.match(gate, /BROWSER_RESULT: \$\{\{ needs.browser.result \}\}/);
  assert.match(
    gate,
    /if \(\$env:NATIVE_FRONTEND_RESULT -cne 'success' -or \$env:FRONTEND_RESULT -cne 'success' -or \$env:BROWSER_RESULT -cne 'success'\) \{/,
  );
  assert.match(gate, /throw "Windows verification incomplete:/);
});

test('lane artifact names are distinct and preserve the same evidence allowlist', () => {
  assert.match(native, /name: nect-desktop-windows-verification-\$\{\{ github.sha \}\}/);
  assert.match(browser, /name: nect-desktop-windows-browser-verification-\$\{\{ github.sha \}\}/);
  const paths = (body) =>
    body.match(/          path: \|\r?\n([\s\S]*?)          if-no-files-found: ignore/)[1];
  assert.equal(paths(native), paths(browser));
  assert.match(
    frontend,
    /name: nect-desktop-windows-frontend-\$\{\{ github.sha \}\}-shard-\$\{\{ matrix.shard \}\}/,
  );
  assert.match(browser, /-shard-\$\{\{ matrix.shard \}\}/);
  for (const body of [native, frontend, browser]) assert.match(body, /if: always\(\)/);
});

test('Chrome preserves failure continuation, production CSP entry and owned server cleanup', () => {
  assert.match(
    browser,
    /if: \$\{\{ !cancelled\(\) && steps.dependencies.outcome == 'success' \}\}/,
  );
  assert.match(browser, /finally \{ Stop-Process -Id \$server.Id -ErrorAction SilentlyContinue \}/);
  assert.match(
    browser,
    /if \(\$env:BROWSER_SHARD -eq '1'\) \{\s+npm run test:csp[\s\S]*?else \{\s+npm run build/,
  );
  assert.equal([...browser.matchAll(/npm run test:csp/g)].length, 1);
  assert.match(browser, /if \(\$LASTEXITCODE -ne 0\) \{ exit \$LASTEXITCODE \}/);
});
