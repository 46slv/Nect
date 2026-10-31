import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
// Production bundle/CSP. Slow/failing disk is an explicit IPC fixture.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize } from '../src/core.js';
const d = document(),
  l = d.layers[0],
  circle = l.nodes[0],
  text = node('Text', 1200, 30),
  fill = node('Fill', 380, 30),
  out = l.nodes.find((n) => n.id === l.output);
d.name = 'ISOLATED Recovery A';
text.text = 'A';
circle.position = [30, 30];
out.position = [730, 30];
fill.params.path.source = circle.id;
out.params.path.source = fill.id;
l.nodes = [circle, fill, out, text];
const root = resolve(process.env.NECT_TEST_DIST || 'dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const p = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!p.startsWith(root + sep)) return res.writeHead(403).end();
  try {
    res.setHeader('Content-Security-Policy', config.app.security.csp);
    res.setHeader(
      'Content-Type',
      { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.ttf': 'font/ttf' }[
        extname(p)
      ] || 'application/octet-stream',
    );
    res.end(await readFile(p));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
let browser;
const errors = [],
  rows = [];
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  page.setDefaultTimeout(10000);
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript(() => {
    window.isTauri = true;
    window.recoveryWrites = [];
    window.recoveryDurable = {};
    window.diskMode = 'normal';
    window.diskActive = 0;
    window.diskMaxActive = 0;
    let next = 0;
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => 1,
      unregisterCallback: () => {},
      invoke: async (command, args) => {
        if (
          [
            'list_system_fonts',
            'list_os_outline_fonts',
            'list_desktop_font_faces',
            'list_recovery',
          ].includes(command)
        )
          return [];
        if (command === 'enable_close_guard' || command === 'release_project') return null;
        if (command === 'choose_unsaved') return 'discard';
        if (command === 'open_svg_source')
          return [
            'fixture.svg',
            '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="12" height="12" fill="red"/></svg>',
          ];
        if (command === 'begin_recovery') return `recovery-${++next}`;
        if (command === 'write_recovery') {
          const copy = structuredClone(args),
            mode = window.diskMode;
          window.recoveryWrites.push(copy);
          window.diskActive++;
          window.diskMaxActive = Math.max(window.diskMaxActive, window.diskActive);
          try {
            if (mode.startsWith('hold'))
              await new Promise((r) => {
                window.releaseDisk = r;
              });
            if (mode === 'fail' || mode === 'hold-fail') throw Error('Fixture disk write failure');
            window.recoveryDurable[copy.id] = copy;
            return Date.now();
          } finally {
            window.diskActive--;
          }
        }
        throw Error(`Unexpected recovery fixture IPC: ${command}`);
      },
    };
  });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate((s) => window.nect.load(s), serialize(d));
  const output = process.env.NECT_TEST_OUTPUT || 'test-results/recovery-failed-context';
  await mkdir(output, { recursive: true });
  const settle = () =>
    page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
  const saved = () =>
    page.waitForFunction(
      () =>
        window.nect.recoveryState.phase === 'saved' &&
        window.nect.recoveryState.savedRevision === window.nect.commandState.expectedRevision,
    );
  const snapshot = () =>
    page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      svg: window.nect.export(),
    }));
  await saved();
  const tokenA = await page.evaluate(() => window.recoveryWrites.at(-1).id);
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator('.graph .node[data-id="' + circle.id + '"] .node-head .type-tag').click();
  await page.evaluate(() => {
    window.diskMode = 'hold-fail';
  });
  const input = page.locator(
    '.inspector-body [data-node="' + circle.id + '"][data-key="radius"] [data-value]',
  );
  await input.fill('75');
  await input.press('Enter');
  await page.waitForFunction(() => typeof window.releaseDisk === 'function');
  const latestA = (await snapshot()).project;
  await page.evaluate(() => {
    window.diskMode = 'normal';
  });
  await page.getByText('Project', { exact: true }).click();
  await page.locator('[data-action="new"]').click();
  await page.waitForFunction(
    () =>
      window.nect.document.name === 'Untitled composition' &&
      window.nect.recoveryState.requestedRevision === 0,
  );
  await page.evaluate(() => window.releaseDisk());
  await saved();
  await settle();
  const failed = await page.evaluate(() => window.nect.recoveryState.failedDocuments[0]);
  assert.equal(failed.name, 'ISOLATED Recovery A');
  assert.equal(failed.phase, 'failed');
  assert.equal(await page.locator('[data-recovery-failures]').isVisible(), true);
  await page.locator('[data-recovery-failures] summary').click();
  assert.match(
    await page.locator('[data-recovery-failed-target] option').textContent(),
    /ISOLATED Recovery A/,
  );
  const B = await snapshot(),
    tokenB = await page.evaluate(() => window.recoveryWrites.at(-1).id),
    bCopy = await page.evaluate((id) => structuredClone(window.recoveryDurable[id]), tokenB);
  await page.screenshot({ path: resolve(output, '01-old-failure-visible.png') });
  await page.evaluate(() => {
    window.diskMode = 'hold';
    window.releaseDisk = null;
  });
  await page.locator('[data-recovery-retry-target]').click();
  await page.waitForFunction(() => typeof window.releaseDisk === 'function');
  assert.equal(await page.locator('[data-recovery-retry-target]').isDisabled(), true);
  await page.evaluate(() => {
    window.diskMode = 'normal';
    window.releaseDisk();
  });
  await page.waitForFunction(() => window.nect.recoveryState.failedDocuments.length === 0);
  await settle();
  assert.deepEqual(await snapshot(), B);
  assert.deepEqual(await page.evaluate((id) => window.recoveryDurable[id], tokenB), bCopy);
  assert.deepEqual(
    JSON.parse(await page.evaluate((id) => window.recoveryDurable[id].contents, tokenA)),
    latestA,
  );
  await page.screenshot({ path: resolve(output, '02-old-retry-success.png') });
  const a2 = structuredClone(d),
    b2 = structuredClone(d);
  a2.name = 'Failure A2';
  b2.name = 'Failure B2';
  await page.evaluate(() => {
    window.diskMode = 'fail';
  });
  await page.evaluate((s) => window.nect.load(s), serialize(a2));
  await page.waitForFunction(() => window.nect.recoveryState.phase === 'failed');
  await page.evaluate((s) => window.nect.load(s), serialize(b2));
  await page.waitForFunction(
    () =>
      window.nect.recoveryState.failedDocuments.length === 2 &&
      window.nect.recoveryState.phase === 'failed',
  );
  const pair = await page.evaluate(() => window.nect.recoveryState.failedDocuments);
  assert.deepEqual(
    pair.map((x) => x.name),
    ['Failure A2', 'Failure B2'],
  );
  await page.locator('[data-recovery-failures] summary').click();
  await page.locator('[data-recovery-failed-target]').selectOption(pair[0].id);
  await settle();
  assert.equal(
    await page.locator('[data-recovery-retry-target]').textContent(),
    'Retry Failure A2',
  );
  const unchanged = await snapshot();
  await page.screenshot({ path: resolve(output, '03-two-failures-selected.png') });
  await page.evaluate(() => {
    window.diskMode = 'normal';
  });
  await page.locator('[data-recovery-retry-target]').click();
  await page.waitForFunction(() => window.nect.recoveryState.failedDocuments.length === 1);
  await settle();
  assert.deepEqual(await snapshot(), unchanged);
  assert.equal(
    (await page.evaluate(() => window.nect.recoveryState.failedDocuments[0])).name,
    'Failure B2',
  );
  await page.locator('[data-recovery-retry-target]').click();
  await saved();
  await settle();
  assert.deepEqual(await snapshot(), unchanged);
  assert.equal(await page.locator('[data-recovery-failures]').isVisible(), false);
  assert.deepEqual(errors, []);
  const receipt = {
    ProductionCSP: true,
    Input: 'Headless Edge synthetic; native IPC is an explicit isolated fixture',
    OldFailureAfterNewVisible: true,
    ExplicitRetryExactLatestOriginalToken: true,
    CurrentProjectFileHistorySVGUnchanged: true,
    TwoFailuresIndividuallySelected: true,
    DuplicateRetryDisabled: true,
    NoPageErrors: true,
    MaxConcurrentWrites: await page.evaluate(() => window.diskMaxActive),
    NoRealRecoveryAccess: true,
  };
  assert.equal(receipt.MaxConcurrentWrites, 1);
  await writeFile(resolve(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  await writeFile(resolve(output, 'latest-A-project.json'), JSON.stringify(latestA, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  if (browser) await browser.close();
  await new Promise((r) => server.close(r));
}
