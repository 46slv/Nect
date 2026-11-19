import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const workflow = await readFile(
  new URL('../../.github/workflows/verify-windows.yml', import.meta.url),
  'utf8',
);
const lane = (name) => {
  const match = workflow.match(
    new RegExp(`^  ${name}:\\r?\\n([\\s\\S]*?)(?=^  [a-z][a-z-]*:|$(?![\\s\\S]))`, 'm'),
  );
  assert.ok(match, `Missing lane ${name}`);
  return match[1];
};
const native = lane('native-frontend');
const browser = lane('browser');
const gate = lane('windows');

test('Windows keeps native/frontend serial and all Chrome work in an independent lane', () => {
  assert.ok(
    native.indexOf('Build Windows app without packaging') < native.indexOf('Run native Rust tests'),
  );
  assert.ok(
    native.indexOf('Run native Rust tests') < native.indexOf('Run frontend unit and DOM tests'),
  );
  assert.match(native, /run: npm test/);
  assert.doesNotMatch(native, /Run actual Chrome interaction tests/);
  assert.match(browser, /npm run test:e2e/);
  assert.doesNotMatch(browser, /needs:|cargo test|run: npm test/);
  assert.doesNotMatch(native, /needs:/);
});

test('both lanes provision dependencies and fonts without new permissions or actions', () => {
  for (const body of [native, browser]) {
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
  assert.match(gate, /needs: \[native-frontend, browser\]/);
  assert.match(gate, /NATIVE_FRONTEND_RESULT: \$\{\{ needs.native-frontend.result \}\}/);
  assert.match(gate, /BROWSER_RESULT: \$\{\{ needs.browser.result \}\}/);
  assert.match(
    gate,
    /if \(\$env:NATIVE_FRONTEND_RESULT -cne 'success' -or \$env:BROWSER_RESULT -cne 'success'\) \{/,
  );
  assert.match(gate, /throw "Windows verification incomplete:/);
});

test('lane artifact names are distinct and preserve the same evidence allowlist', () => {
  assert.match(native, /name: nect-desktop-windows-verification-\$\{\{ github.sha \}\}/);
  assert.match(browser, /name: nect-desktop-windows-browser-verification-\$\{\{ github.sha \}\}/);
  const paths = (body) =>
    body.match(/          path: \|\r?\n([\s\S]*?)          if-no-files-found: ignore/)[1];
  assert.equal(paths(native), paths(browser));
  for (const body of [native, browser]) assert.match(body, /if: always\(\)/);
});

test('Chrome preserves failure continuation, production CSP entry and owned server cleanup', () => {
  assert.match(
    browser,
    /if: \$\{\{ !cancelled\(\) && steps.dependencies.outcome == 'success' \}\}/,
  );
  assert.match(browser, /finally \{ Stop-Process -Id \$server.Id -ErrorAction SilentlyContinue \}/);
  assert.match(browser, /if \(\$LASTEXITCODE -ne 0\) \{ exit \$LASTEXITCODE \}/);
});
