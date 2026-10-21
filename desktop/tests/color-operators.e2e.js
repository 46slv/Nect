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
  process.env.NECT_COLOR_RESULTS || `test-results/color-operators/${native ? 'native' : 'browser'}`,
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
    d.name = 'Color operations v1 verification';
    d.width = 320;
    d.height = 240;
    shape.params.center.value = [160, 120];
    shape.params.size.value = [220, 160];
    source.name = 'Shared Color';
    source.params.value.value = { hex: '#ff0000', alpha: 0.5 };
    paint.params.startColor.value = { hex: '#ff0000', alpha: 0 };
    paint.params.endColor.value = { hex: '#0000ff', alpha: 1 };
    if (type === 'Linear Gradient Fill') {
      paint.params.start.value = [40, 120];
      paint.params.end.value = [280, 120];
    } else {
      paint.params.center.value = [160, 120];
      paint.params.radius.value = 120;
    }
    const weight = node('Value', 40, 650);
    weight.name = 'Mix Weight';
    weight.params.value.value = 0.5;
    l.nodes = [shape, source, weight, paint, out];
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
    const commandState = () => page.evaluate(() => window.nect.commandState);
    const edit = async (key, text, prefix = null) => {
      await field(key).locator('[data-fx]').click();
      const input = page.getByRole('combobox', { name: 'Parameter expression' });
      if (prefix) {
        await input.fill(prefix);
        await input.press('Tab');
        assert.match(await input.inputValue(), /^(mix|alpha)\(\)$/);
      }
      await input.fill(text);
      assert.equal(await input.getAttribute('aria-invalid'), 'false');
      await page.locator('dialog [data-confirm]').click();
      await page.waitForFunction(
        ({ id, key, text }) =>
          window.nect.document.layers[0].nodes.find((n) => n.id === id).params[key].expression ===
          text,
        { id: paint.id, key, text },
      );
    };
    const result = async (key, hex, alpha) => {
      await field(key)
        .locator('output')
        .filter({ hasText: new RegExp(hex + '.*' + alpha) })
        .waitFor();
      assert.match(await field(key).locator('output').innerText(), new RegExp(hex + '.*' + alpha));
      const value = await page.evaluate(
        ({ id, key }) => window.nect.evaluate().parameterValue(id, key),
        { id: paint.id, key },
      );
      assert.deepEqual(value, { hex, alpha });
    };
    await edit('startColor', 'value');
    await result('startColor', '#ff0000', 0);
    assert.equal(await page.evaluate(() => window.nect.document.schemaVersion), 42);
    await edit('endColor', ref(source.id, 'value'));
    await result('endColor', '#ff0000', 0.5);
    await edit('startColor', 'alpha(value,0.5)', 'al');
    await result('startColor', '#ff0000', 0.5);
    const authored = await saved();
    await field('startColor').locator('[data-color="alpha"]').fill('0.25');
    await field('startColor').locator('[data-color="alpha"]').press('Enter');
    assert.equal((await q('startColor')).value.alpha, 0.25);
    await result('startColor', '#ff0000', 0.5);
    await page.locator('[data-action="undo"]').click();
    assert.equal(await saved(), authored);
    await edit(
      'endColor',
      `mix(value,${ref(source.id, 'value')},${ref(weight.id, 'value')})`,
      'mi',
    );
    await result('endColor', '#9c00d5', 0.75);
    await inspect(source.id);
    const shared = page.locator(
      `.inspector-body .field[data-node="${source.id}"][data-key="value"] [data-color="alpha"]`,
    );
    await shared.fill('1');
    await shared.press('Tab');
    await inspect(paint.id);
    await result('endColor', '#bc00bc', 1);
    await inspect(weight.id);
    const number = page.locator(
      `.inspector-body .field[data-node="${weight.id}"][data-key="value"] [data-value]`,
    );
    await number.fill('0.25');
    await number.press('Tab');
    await inspect(paint.id);
    await result('endColor', '#8900e1', 1);
    const beforeNested = await saved(),
      revision = await commandState();
    const nested = 'mix(mix(#000000,#333333,0.5),#000000,0.5)';
    await edit('startColor', nested);
    await result('startColor', '#161616', 1);
    assert.deepEqual((await q('startColor')).value, { hex: '#ff0000', alpha: 0 });
    assert.equal((await commandState()).expectedRevision, revision.expectedRevision + 1);
    const final = await saved();
    await page.locator('[data-action="undo"]').click();
    assert.equal(await saved(), beforeNested);
    await page.locator('[data-action="redo"]').click();
    assert.equal(await saved(), final);
    const noOp = await commandState();
    await edit('startColor', nested);
    assert.deepEqual(await commandState(), noOp);
    const beforeInvalid = await saved();
    await field('startColor').locator('[data-fx]').click();
    let input = page.getByRole('combobox', { name: 'Parameter expression' });
    await input.fill('mix(value,#000000,2)');
    assert.equal(await input.getAttribute('aria-invalid'), 'true');
    assert.match(await input.evaluate((el) => el.validationMessage), /0 to 1/);
    assert.equal(await saved(), beforeInvalid);
    await page.locator('dialog [data-confirm]').click();
    assert.equal(await saved(), beforeInvalid);
    await page.locator('dialog [data-cancel]').click();
    assert.equal(await saved(), beforeInvalid);
    const file = resolve(output, `${type.startsWith('Linear') ? 'linear' : 'radial'}.json`),
      svg = await page.evaluate(() => window.nect.export());
    assert.match(svg, /#161616/i);
    assert.match(svg, /#8900e1/i);
    const canvasColors = await page
      .locator('.artboard stop')
      .evaluateAll((stops) => stops.map((stop) => stop.getAttribute('stop-color')?.toLowerCase()));
    assert.ok(canvasColors.includes('#161616'));
    assert.ok(canvasColors.includes('#8900e1'));
    await writeFile(file, await saved());
    await page.evaluate((text) => window.nect.load(text), await readFile(file, 'utf8'));
    assert.deepEqual(JSON.parse(await saved()), JSON.parse(beforeInvalid));
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
    await inspect(paint.id);
    await result('startColor', '#161616', 1);
    await result('endColor', '#8900e1', 1);
    await page.screenshot({
      path: resolve(output, `${type.startsWith('Linear') ? 'linear' : 'radial'}.png`),
    });
    results.push({
      type,
      valueAndStableColorReference: true,
      alphaReplacementHiddenBase: true,
      linearPremultipliedMix: true,
      scalarAndColorDependencyUpdates: true,
      nested161616Not171717: true,
      oneUndoRedo: true,
      noOpAndInvalidCancelNeutral: true,
      diskJsonReloadSvg: true,
    });
  }
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(output, 'receipt.json'),
    JSON.stringify(
      {
        renderer: native ? 'Tauri Windows WebView2' : 'browser',
        input: 'Playwright synthetic pointer/keyboard',
        results,
        errors,
        notRun: [
          'native OS Save/Open dialogs',
          'cold-process file reopen',
          'physical keyboard/IME',
        ],
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify({ pass: results.length, results, errors }));
} catch (error) {
  if (page) await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw error;
} finally {
  await browser?.close();
  if (server) await new Promise((r) => server.close(r));
}
