import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import {
  document,
  node,
  connect,
  serialize,
  parameterReferenceExpression as ref,
} from '../src/core.js';

const native = !!process.env.NECT_COLOR_CDP_URL;
const output = resolve(
  process.env.NECT_COLOR_RESULTS || `test-results/color-value/${native ? 'native' : 'browser'}`,
);
await mkdir(output, { recursive: true });
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
let server, browser, page;
const errors = [],
  results = [];
try {
  if (native) {
    browser = await chromium.connectOverCDP(process.env.NECT_COLOR_CDP_URL);
    const candidates = browser
      .contexts()
      .flatMap((c) => c.pages())
      .filter((p) => /tauri\.localhost/.test(p.url()));
    assert.equal(candidates.length, 1, 'one isolated candidate WebView2');
    [page] = candidates;
  } else {
    server = createServer(async (req, res) => {
      const p = resolve(
        root,
        '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
      );
      if (!p.startsWith(root + sep)) return res.writeHead(403).end();
      try {
        res.setHeader('Content-Security-Policy', config.app.security.csp);
        res.setHeader(
          'Content-Type',
          {
            '.html': 'text/html',
            '.js': 'text/javascript',
            '.css': 'text/css',
            '.ttf': 'font/ttf',
          }[extname(p)] || 'application/octet-stream',
        );
        res.end(await readFile(p));
      } catch {
        res.writeHead(404).end();
      }
    });
    await new Promise((r) => server.listen(0, '127.0.0.1', r));
    browser = await chromium.launch({
      executablePath:
        process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
      headless: true,
    });
    page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
    await ensureWorkspacePanel(page, 'inspector');
  }
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect);
  if (native) {
    assert.equal(await page.evaluate(() => !!window.__TAURI_INTERNALS__), true);
    // Repeated disposable runs can offer the previous synthetic recovery copy.
    const recovery = page.locator('[data-recovery-dialog]');
    try {
      await recovery.waitFor({ state: 'visible', timeout: 1000 });
      await recovery.getByRole('button', { name: 'Later', exact: true }).click();
    } catch (error) {
      if (error.name !== 'TimeoutError') throw error;
    }
  }
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const inspect = async (id) => {
    await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  };
  for (const type of ['Linear Gradient Fill', 'Radial Gradient Fill']) {
    const d = document(),
      l = d.layers[0],
      shape = node('Rectangle', 40, 40),
      source = node('Color', 40, 400),
      paint = node(type, 360, 40),
      out = node('Output', 680, 40);
    d.name = 'Color expression verification';
    d.width = 320;
    d.height = 240;
    shape.params.center.value = [160, 120];
    shape.params.size.value = [220, 160];
    source.name = 'Shared Color';
    source.params.value.value = { hex: '#123456', alpha: 0.25 };
    paint.params.startColor.value = { hex: '#abcdef', alpha: 0.6 };
    if (type === 'Linear Gradient Fill') {
      paint.params.start.value = [40, 120];
      paint.params.end.value = [280, 120];
    } else {
      paint.params.center.value = [160, 120];
      paint.params.radius.value = 120;
    }
    l.nodes = [shape, source, paint, out];
    l.output = out.id;
    connect(l, shape.id, paint.id, 'path');
    connect(l, paint.id, out.id, 'path');
    await page.evaluate((s) => window.nect.load(s), serialize(d));
    await inspect(paint.id);
    const field = (key) =>
      page.locator(`.inspector-body .field[data-node="${paint.id}"][data-key="${key}"]`);
    const q = (key) =>
      page.evaluate(
        ({ id, key }) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params[key],
        { id: paint.id, key },
      );
    const initial = await saved();
    await field('startColor').locator('[data-fx]').click();
    let input = page.getByRole('combobox', { name: 'Parameter expression' });
    await input.fill('va');
    await input.press('Tab');
    assert.equal(await input.inputValue(), 'value');
    await input.fill('va');
    await page.locator('dialog [data-cancel]').click();
    assert.equal(await page.locator('dialog [data-cancel]').count(), 0);
    assert.equal(await saved(), initial, 'Cancel leaves authored document unchanged');
    await field('startColor').locator('[data-fx]').click();
    input = page.getByRole('combobox', { name: 'Parameter expression' });
    await input.fill('value');
    await page.locator('dialog [data-confirm]').click();
    await page.waitForFunction(
      ({ id }) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).params.startColor
          .expression === 'value',
      { id: paint.id },
    );
    const beforeBase = await saved();
    const alpha = field('startColor').locator('[data-color="alpha"]');
    await alpha.fill('0.35');
    await alpha.press('Enter');
    assert.equal((await q('startColor')).value.alpha, 0.35);
    await field('startColor')
      .locator('output')
      .filter({ hasText: /#abcdef.*0.35/ })
      .waitFor();
    assert.match(await field('startColor').locator('output').innerText(), /#abcdef.*0.35/);
    await page.locator('[data-action="undo"]').click();
    assert.equal(await saved(), beforeBase, 'one Undo restores base with expression retained');
    await field('endColor').locator('[data-fx]').click();
    input = page.getByRole('combobox', { name: 'Parameter expression' });
    await input.fill('Shared');
    await page.getByRole('option').filter({ hasText: 'Shared Color' }).first().waitFor();
    await input.press('Tab');
    assert.equal(await input.inputValue(), ref(source.id, 'value'));
    await page.locator('dialog [data-confirm]').click();
    await field('endColor')
      .locator('output')
      .filter({ hasText: /#123456.*0.25/ })
      .waitFor();
    assert.match(await field('endColor').locator('output').innerText(), /#123456.*0.25/);
    const beforeInvalid = await saved();
    await field('endColor').locator('[data-fx]').click();
    input = page.getByRole('combobox', { name: 'Parameter expression' });
    await input.fill('value + 1');
    await page.locator('dialog [data-confirm]').click();
    assert.equal(await saved(), beforeInvalid);
    assert.equal(await page.locator('dialog [data-confirm]').count(), 1);
    await page.locator('dialog [data-cancel]').click();
    const file = resolve(output, `${type.startsWith('Linear') ? 'linear' : 'radial'}.json`);
    await writeFile(file, await saved());
    const contents = await readFile(file, 'utf8');
    const svg = await page.evaluate(() => window.nect.export());
    await page.evaluate((s) => window.nect.load(s), contents);
    assert.deepEqual(JSON.parse(await saved()), JSON.parse(contents));
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
    await inspect(paint.id);
    await page.screenshot({
      path: resolve(output, `${type.startsWith('Linear') ? 'linear' : 'radial'}.png`),
    });
    results.push({
      type,
      value: true,
      typedCompletionReference: true,
      cancel: true,
      baseEditOneUndo: true,
      invalidDraftAtomic: true,
      diskJsonReloadAndSvg: true,
    });
  }
  assert.deepEqual(errors, []);
  const receipt = {
    nativeWebView2: native,
    url: page.url(),
    productionCsp: true,
    syntheticInput: true,
    results,
    pageErrors: errors,
    nativeSaveOpenDialogs: 'NOT_RUN: node_repl Windows controller is not exposed in this task',
    fullSuite: 'NOT_RUN',
  };
  await writeFile(resolve(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} catch (error) {
  if (page) {
    await page.screenshot({ path: resolve(output, 'failure.png') });
    await writeFile(
      resolve(output, 'failure.json'),
      JSON.stringify(
        {
          error: error.message,
          errors,
          document: await savedForFailure(),
          text: await page.locator('body').innerText(),
        },
        null,
        2,
      ),
    );
  }
  throw error;
} finally {
  // Closing a CDP connection detaches; the launching task owns the native process.
  await browser?.close();
  if (server) await new Promise((r) => server.close(r));
}
async function savedForFailure() {
  return page.evaluate(() => window.nect?.document);
}
