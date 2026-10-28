import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir, unlink } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';
const root = resolve('dist'),
  out = resolve('test-results/asset-catalog');
const config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
await mkdir(out + '/A', { recursive: true });
await mkdir(out + '/B', { recursive: true });
await writeFile(
  out + '/A/old.svg',
  '<svg width="20" height="20"><rect width="10" height="10"/></svg>',
);
await writeFile(
  out + '/B/other.svg',
  '<svg width="20" height="20"><circle cx="10" cy="10" r="5"/></svg>',
);
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
const results = {};
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const url = `http://127.0.0.1:${server.address().port}`;
  for (const mode of ['browser-refresh', 'native-reselection']) {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    page.setDefaultTimeout(15000);
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    try {
      if (mode === 'native-reselection')
        await page.addInitScript(() => {
          window.isTauri = true;
          window.catalogRoots = [];
          window.nextAssetFolder = 'folder-a';
          window.catalogGeneration = 0;
          window.lastCatalogRefresh = null;
          const roots = {
            'folder-a': { id: 'folder-a', name: 'A' },
            'folder-b': { id: 'folder-b', name: 'B' },
          };
          window.__TAURI_INTERNALS__ = {
            transformCallback: () => 1,
            unregisterCallback: () => {},
            invoke: async (command, args) => {
              if (command === 'list_asset_folders') return window.catalogRoots;
              if (command === 'register_asset_folder') {
                const selected = roots[window.nextAssetFolder];
                if (!window.catalogRoots.some((r) => r.id === selected.id))
                  window.catalogRoots.push(selected);
                return { roots: window.catalogRoots, selectedRootId: selected.id };
              }
              if (command === 'refresh_asset_folder') {
                window.lastCatalogRefresh = args.rootId;
                return {
                  root: roots[args.rootId],
                  generation: String(++window.catalogGeneration),
                  items: [],
                  warnings: [],
                };
              }
              if (command.startsWith('list_')) return [];
              if (command === 'enable_close_guard' || command === 'release_project') return null;
              if (command === 'choose_unsaved') return 'discard';
              throw Error('Unexpected IPC ' + command);
            },
          };
        });
      await page.goto(url);
      await page.waitForFunction(() => window.nect);
      await activateCommand(page, 'asset-library');
      const panel = page.locator('[data-asset-library]'),
        select = page.getByLabel('Asset folder', { exact: true });
      await panel.getByRole('button', { name: 'Add folder', exact: true }).waitFor();
      const before = await page.evaluate(() => JSON.stringify(window.nect.document));
      if (mode === 'browser-refresh') {
        const pick = async (action, path) => {
          const pending = page.waitForEvent('filechooser');
          await panel.getByRole('button', { name: action, exact: true }).click();
          await (await pending).setFiles(path);
          await panel
            .getByRole('button', { name: 'Refresh', exact: true })
            .waitFor({ state: 'visible' });
          await page.waitForFunction(
            () => !document.querySelector('[data-asset-library] select').disabled,
          );
        };
        await pick('Add folder', out + '/A');
        const id = await select.inputValue();
        assert.equal(await select.locator('option').count(), 1);
        for (let i = 0; i < 20; i++) {
          if (i === 2) {
            await unlink(out + '/A/old.svg');
            await writeFile(
              out + '/A/new.svg',
              '<svg width="30" height="20"><rect width="25" height="15" fill="#0000ff"/></svg>',
            );
          }
          await pick('Refresh', out + '/A');
          assert.equal(await select.inputValue(), id, `Refresh ${i} retargeted root identity`);
          assert.equal(await select.locator('option').count(), 1, `Refresh ${i} duplicated root`);
          await panel
            .getByRole('button', { name: i < 2 ? 'old.svg' : 'new.svg', exact: true })
            .waitFor();
          if (i >= 2)
            assert.equal(
              await panel.getByRole('button', { name: 'old.svg', exact: true }).count(),
              0,
            );
        }
        await pick('Add folder', out + '/B');
        assert.equal(await select.locator('option').count(), 2);
        const state = await select.evaluate((el) => ({
          value: el.value,
          options: [...el.options].map((o) => [o.value, o.textContent]),
        }));
        const pending = page.waitForEvent('filechooser');
        await panel.getByRole('button', { name: 'Refresh', exact: true }).click();
        await pending;
        await page.locator('[data-asset-folder-input]').dispatchEvent('cancel');
        assert.deepEqual(
          await select.evaluate((el) => ({
            value: el.value,
            options: [...el.options].map((o) => [o.value, o.textContent]),
          })),
          state,
        );
        await panel.getByRole('button', { name: 'other.svg', exact: true }).waitFor();
        results[mode] = {
          passed: true,
          refreshes: 20,
          stableRootId: id,
          rootCount: 2,
          changedContents: true,
          cancelUnchanged: true,
        };
      } else {
        for (const id of ['folder-a', 'folder-b', 'folder-a']) {
          await page.evaluate((id) => {
            window.nextAssetFolder = id;
          }, id);
          await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
          await page.waitForFunction(
            () => !document.querySelector('[data-asset-library] select').disabled,
          );
          assert.equal(await select.inputValue(), id, 'Native registration selected another root');
          assert.equal(await page.evaluate(() => window.lastCatalogRefresh), id);
        }
        assert.equal(await select.locator('option').count(), 2);
        results[mode] = {
          passed: true,
          selected: 'folder-a',
          rootCount: 2,
          desktopIoIsExplicitStub: true,
        };
      }
      assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
      assert.deepEqual(errors, []);
      console.log('PASS catalog ' + mode);
    } catch (error) {
      results[mode] = { passed: false, error: error.message };
      console.error('FAIL catalog ' + mode + ': ' + error.message);
      process.exitCode = 1;
    } finally {
      await page.close();
    }
  }
  await writeFile(out + '/results.json', JSON.stringify(results, null, 2));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
