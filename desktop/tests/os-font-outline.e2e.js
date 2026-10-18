// Production Chromium UI with an explicit native IPC stub. Native GDI+ output
// can be supplied by the Windows Rust smoke; this is not WebView2 acceptance.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize } from '../src/core.js';
const raw = process.env.NECT_OS_OUTLINE_SMOKE_OUTPUT
  ? JSON.parse(await readFile(process.env.NECT_OS_OUTLINE_SMOKE_OUTPUT, 'utf8'))
  : {
      family: 'Test OS family',
      style: 0,
      text: 'ABC',
      size: 100,
      ascent: 80,
      points: [
        [0, 0],
        [10, 0],
        [10, 10],
        [0, 10],
      ],
      types: [0, 1, 1, 161],
    };
const doc = document(),
  layer = doc.layers[0],
  text = node('Text', 100, 80),
  fill = node('Fill', 420, 80);
text.text = raw.text;
text.params.size.value = raw.size;
text.params.position.value = [60, 160];
fill.params.path.source = text.id;
layer.nodes.push(text, fill);
layer.nodes.find((n) => n.id === layer.output).params.path.source = fill.id;
layer.nodes.find((n) => n.id === layer.output).position = [740, 80];
const config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const root = resolve('dist'),
  types = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.ttf': 'font/ttf',
  };
const server = createServer(async (req, res) => {
  const path = resolve(
    root,
    '.' + (req.url === '/' ? '/index.html' : new URL(req.url, 'http://localhost').pathname),
  );
  if (!path.startsWith(root + sep)) {
    res.writeHead(403).end();
    return;
  }
  try {
    res.setHeader('Content-Security-Policy', config.app.security.csp);
    res.setHeader('Content-Type', types[extname(path)] || 'application/octet-stream');
    res.end(await readFile(path));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROME_PATH ||
      'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1500, height: 1000 } }),
    errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.addInitScript(
    ({ raw }) => {
      window.isTauri = true;
      window.osNativeCalls = [];
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command, args) => {
          window.osNativeCalls.push(command);
          if (command === 'list_system_fonts') return [];
          if (command === 'list_os_outline_fonts')
            return [{ family: raw.family, styles: [raw.style] }];
          if (command === 'outline_os_text') {
            if (args.family !== raw.family || args.text !== raw.text || args.size !== raw.size)
              throw Error('Unexpected native intent');
            return structuredClone(raw);
          }
          if (command === 'read_system_font')
            throw Error('OS outlines must not request font bytes');
          if (['enable_close_guard', 'release_project'].includes(command)) return null;
          throw Error('Unexpected native command ' + command);
        },
      };
    },
    { raw },
  );
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((contents) => window.nect.load(contents), serialize(doc));
  await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
  await page.locator('.inspector-body [data-os-text-outline]').click();
  const dialog = page.getByRole('dialog', { name: 'OS font outlines' });
  await dialog.getByRole('combobox').fill(raw.family);
  await dialog.getByRole('option').first().click();
  await mkdir('test-results/os-font-outline', { recursive: true });
  await page.screenshot({ path: 'test-results/os-font-outline/picker.png' });
  await dialog.getByRole('button', { name: 'Convert to paths', exact: true }).click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id)?.type === 'Multi Path',
    text.id,
  );
  const after = await page.evaluate(() => window.nect.document);
  assert.equal(Object.keys(after.fontAssets || {}).length, 0);
  const baked = after.layers[0].nodes.find((n) => n.id === text.id);
  assert.ok(baked.points.length > 0);
  assert.ok(!(await page.evaluate(() => window.osNativeCalls)).includes('read_system_font'));
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('Control+z');
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).type,
      text.id,
    ),
    'Text',
  );
  await page.keyboard.press('Control+Shift+z');
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).type,
      text.id,
    ),
    'Multi Path',
  );
  await page.screenshot({ path: 'test-results/os-font-outline/converted.png' });
  assert.deepEqual(errors, []);
  const receipt = {
    family: raw.family,
    nativePoints: raw.points.length,
    contours: baked.contours.length,
    vertices: baked.points.length,
    fontAssets: 0,
    undoRedo: true,
    productionCsp: true,
    nativeIpc: 'stub using separately verified OS geometry',
    pageErrors: errors,
  };
  await writeFile('test-results/os-font-outline/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((resolve) => server.close(resolve));
}
