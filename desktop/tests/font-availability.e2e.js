import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
// Production UI/CSP with explicit catalog-failure IPC fixtures; no native claim.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { CommandSession, document, node, serialize } from '../src/core.js';
import { desktopTextIntent } from '../src/desktop-font-text.js';
const face = {
  family: 'Catalog fixture',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const d = document(),
  l = d.layers[0],
  n = node('Text', 30, 30),
  fill = node('Fill', 380, 30);
n.text = 'ABC';
l.nodes = [n, fill, l.nodes.find((n) => n.id === l.output)];
fill.params.path.source = n.id;
l.nodes[2].params.path.source = fill.id;
l.nodes[2].position = [730, 30];
const session = new CommandSession(d);
session.dispatch({
  ...session.state,
  type: 'set-os-font-text',
  payload: {
    layerId: l.id,
    nodeId: n.id,
    outline: {
      ...desktopTextIntent(n, face),
      engine: 'directwrite-v1',
      ascent: 80,
      advanceWidth: 20,
      lineCount: 1,
      glyphCount: 3,
      points: [
        [0, 0],
        [20, 0],
        [20, 10],
        [0, 10],
      ],
      types: [0, 1, 1, 129],
    },
  },
});
const root = resolve('dist'),
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
let browser, page;
const errors = [];
const output = 'test-results/font-availability';
await mkdir(output, { recursive: true });
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript((face) => {
    window.isTauri = true;
    window.catalogMode = 'available';
    window.catalogCalls = [];
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => 1,
      unregisterCallback: () => {},
      invoke: async (command) => {
        window.catalogCalls.push(command);
        if (command === 'list_system_fonts' || command === 'list_recovery') return [];
        if (command === 'enable_close_guard' || command === 'acknowledge_recovery') return null;
        if (command === 'begin_recovery') return 'fixture-recovery';
        if (command === 'write_recovery') return Date.now();
        if (command === 'list_os_outline_fonts') {
          if (window.catalogMode === 'family-error') throw Error('Legacy catalog failed');
          return [{ family: face.family, styles: [0] }];
        }
        if (command === 'list_desktop_font_faces') {
          if (window.catalogMode === 'face-error') throw Error('DirectWrite catalog failed');
          if (window.catalogMode === 'missing') return [];
          if (window.catalogMode === 'format-changed') return [{ ...face, format: 2 }];
          return [face];
        }
        throw Error(`Unexpected IPC in catalog UI test: ${command}`);
      },
    };
  }, face);
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), serialize(session.snapshot()));
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator(`.graph .node[data-id="${n.id}"] .node-head strong`).click();
  const status = page.locator('.inspector-body .text-controls output[role="status"]');
  const before = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    revision: window.nect.commandState.expectedRevision,
  }));
  const refresh = async (mode) => {
    await page.evaluate((mode) => {
      window.catalogMode = mode;
    }, mode);
    await page.locator('.inspector-body [data-refresh-os-fonts]').click();
    await page.waitForFunction(
      () =>
        document.querySelector('.inspector-body .text-controls output[role="status"]')
          ?.textContent !== 'Checking font…',
    );
  };
  await refresh('available');
  assert.equal(await status.textContent(), '');
  await refresh('face-error');
  const failedStatus = await status.textContent();
  if (!/Font check failed/.test(failedStatus))
    await writeFile(
      `${output}/before-fix.json`,
      JSON.stringify(
        {
          scenario: 'Previously available face; modern catalog refresh rejects',
          observed: failedStatus,
          expected: 'Font check failed · Refresh fonts',
          unchanged: await page.evaluate(() => ({
            doc: JSON.stringify(window.nect.document),
            svg: window.nect.export(),
            revision: window.nect.commandState.expectedRevision,
          })),
        },
        null,
        2,
      ),
    );
  assert.match(failedStatus, /Font check failed/);
  assert.doesNotMatch(failedStatus, /Font unavailable/);
  await refresh('missing');
  assert.match(await status.textContent(), /Font unavailable.*saved outlines retained/);
  await refresh('format-changed');
  assert.match(await status.textContent(), /Font unavailable/);
  await refresh('family-error');
  assert.equal(await status.textContent(), '');
  await refresh('available');
  assert.equal(await status.textContent(), '');
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      revision: window.nect.commandState.expectedRevision,
    })),
    before,
  );
  assert.equal(
    await page.evaluate(
      () =>
        window.catalogCalls.filter(
          (c) => c === 'shape_desktop_text' || c === 'outline_os_text' || c === 'read_system_font',
        ).length,
    ),
    0,
  );
  assert.deepEqual(errors, []);
  await page.screenshot({ path: `${output}/result.png` });
  await writeFile(
    `${output}/receipt.json`,
    JSON.stringify(
      {
        productionCsp: true,
        ipcWasStubbed: true,
        knownFaceFailureNotReportedAvailable: true,
        missingDistinguishedFromCheckFailure: true,
        fullFaceMetadataMatch: true,
        independentCatalogFailure: true,
        refreshRecovery: true,
        jsonSvgRevisionUnchanged: true,
        noShapeOrFontByteCall: true,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'Font availability refresh failure/recovery, independent catalogs and snapshot preservation passed',
  );
} finally {
  if (browser) await browser.close();
  await new Promise((r) => server.close(r));
}
