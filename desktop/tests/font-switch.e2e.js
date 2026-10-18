import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize } from '../src/core.js';

// Original synthetic stored outline: tests replacement safety, not font fidelity.
function fixture(text) {
  const d = document(),
    l = d.layers[0],
    n = node('Text', 30, 30),
    fill = node('Fill', 330, 30),
    out = node('Output', 630, 30);
  d.schemaVersion = 28;
  n.text = text;
  n.osFont = {
    version: 1,
    family: 'Unavailable synthetic face',
    style: 0,
    outline: {
      family: 'Unavailable synthetic face',
      style: 0,
      text,
      size: 100,
      ascent: 80,
      points: [
        [0, 0],
        [10, 0],
        [10, 10],
        [0, 10],
      ],
      types: [0, 1, 1, 161],
    },
  };
  l.nodes = [n, fill, out];
  l.output = out.id;
  fill.params.path.source = n.id;
  out.params.path.source = fill.id;
  return { d, n };
}
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
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const inspect = async (f) => {
    await page.evaluate((s) => window.nect.load(s), serialize(f.d));
    await page.locator(`.graph .node[data-id="${f.n.id}"] .node-head strong`).click();
  };
  const japanese = fixture('日本語');
  await inspect(japanese);
  assert.match(
    await page.locator('.inspector-body .text-controls').innerText(),
    /Unavailable synthetic face/,
  );
  const before = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    revision: window.nect.commandState.expectedRevision,
  }));
  await page.locator('.inspector-body').getByRole('button', { name: 'Use DejaVu Sans', exact: true }).click();
  assert.match(await page.locator('.status').innerText(), /OS font retained:.*Missing glyphs/);
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      revision: window.nect.commandState.expectedRevision,
    })),
    before,
  );
  const latin = fixture('ABC');
  await inspect(latin);
  const old = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
  }));
  await page.locator('.inspector-body').getByRole('button', { name: 'Use DejaVu Sans', exact: true }).click();
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont,
      latin.n.id,
    ),
    undefined,
  );
  assert.match(await page.locator('.status').innerText(), /Font replaced: DejaVu Sans/);
  const next = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
  }));
  assert.notEqual(next.svg, old.svg);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
    })),
    old,
  );
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
    })),
    next,
  );
  await page.evaluate((s) => window.nect.load(s), next.doc);
  assert.equal(await page.evaluate(() => window.nect.export()), next.svg);
  assert.deepEqual(errors, []);
  await mkdir('test-results/font-switch', { recursive: true });
  await page.screenshot({ path: 'test-results/font-switch/replacement.png' });
  await writeFile(
    'test-results/font-switch/receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        actualUi: true,
        sourceAndDestinationNamed: true,
        missingCoverageRefused: true,
        failedSwitchPreservedJsonSvgAndRevision: true,
        supportedLatinSwitch: true,
        oneUndoRedo: true,
        jsonReopenExactSvg: true,
        syntheticStoredOutline: true,
        nativeFontSubstitutionChanged: false,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'Font replacement production UI: missing coverage retained; explicit DejaVu switch/Undo/reopen passed',
  );
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
