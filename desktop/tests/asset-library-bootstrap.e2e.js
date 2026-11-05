import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';

// Real browser and app DOM; all native folder IPC is synthetic and in memory.
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const output = resolve('test-results/asset-bootstrap');
await mkdir(output, { recursive: true });
try {
  for (const mode of ['search', 'cancel-first', 'list-first']) {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    const errors = [];
    page.on('pageerror', (error) => errors.push(error.message));
    await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());
    await page.addInitScript(() => {
      window.isTauri = true;
      window.assetCalls = [];
      const bootstrap = new Promise((resolve) => {
        window.resolveAssetBootstrap = resolve;
      });
      const picker = new Promise((resolve) => {
        window.resolveAssetPicker = resolve;
      });
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command, args) => {
          window.assetCalls.push({ command, args });
          if (command === 'list_asset_folders') return bootstrap;
          if (command === 'register_asset_folder') return picker;
          if (command === 'refresh_asset_folder')
            return {
              root: { id: 'synthetic', name: 'Synthetic' },
              generation: '1',
              items: [
                { relativePath: 'demo.svg', bytes: 77 },
                { relativePath: 'other.svg', bytes: 77 },
              ],
              warnings: [],
            };
          if (command === 'open_asset_source')
            return {
              bytes: [
                ...new TextEncoder().encode(
                  '<svg width="20" height="20"><rect width="20" height="20" fill="#3273c7"/></svg>',
                ),
              ],
              sourceLocation: `Synthetic/${args.relativePath}`,
            };
          if (command.startsWith('list_')) return [];
          if (command === 'enable_close_guard' || command === 'release_project') return null;
          if (command === 'choose_unsaved') return 'discard';
          throw Error('Unexpected synthetic IPC ' + command);
        },
      };
    });
    await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
    await page.waitForFunction(() => window.nect);
    const before = await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    }));
    await page.locator('[data-workspace-panel="assets"]').click();
    const panel = page.locator('[data-asset-library]');
    await panel.waitFor();
    await page.waitForFunction(() =>
      window.assetCalls.some((item) => item.command === 'list_asset_folders'),
    );
    const search = panel.locator('[data-asset-search]');
    if (mode === 'search') await search.fill('demo');
    else {
      await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
      await page.waitForFunction(() =>
        window.assetCalls.some((item) => item.command === 'register_asset_folder'),
      );
    }
    if (mode !== 'cancel-first')
      await page.evaluate(() =>
        window.resolveAssetBootstrap([{ id: 'synthetic', name: 'Synthetic' }]),
      );
    if (mode !== 'search') await page.evaluate(() => window.resolveAssetPicker(null));
    if (mode === 'cancel-first')
      await page.evaluate(() =>
        window.resolveAssetBootstrap([{ id: 'synthetic', name: 'Synthetic' }]),
      );
    await page.waitForFunction(
      () => document.querySelector('[aria-label="Asset folder"]')?.value === 'synthetic',
    );
    await page.waitForFunction(
      (count) =>
        document.querySelector('[data-asset-status]')?.textContent.includes(`${count} assets`),
      mode === 'search' ? 1 : 2,
    );
    await panel.locator('[data-path="demo.svg"] img[src]').waitFor();
    assert.equal(await panel.getByLabel('Asset folder', { exact: true }).isEnabled(), true);
    assert.equal(
      await panel.getByRole('button', { name: 'Refresh', exact: true }).isEnabled(),
      true,
    );
    if (mode === 'search') {
      assert.equal(await search.inputValue(), 'demo');
      assert.equal(await panel.locator('[data-path]').count(), 1);
      await page.locator('[data-workspace-panel="layers"]').click();
      await page.locator('[data-workspace-panel="assets"]').click();
      assert.equal(await search.inputValue(), 'demo');
      assert.equal(
        await panel.getByLabel('Asset folder', { exact: true }).inputValue(),
        'synthetic',
      );
      await page.screenshot({
        path: resolve(output, 'search-after-bootstrap.png'),
        fullPage: true,
      });
    }
    assert.deepEqual(
      await page.evaluate(() => ({
        project: window.nect.projectSnapshot(),
        state: window.nect.commandState,
      })),
      before,
    );
    assert.deepEqual(errors, []);
    await page.close();
  }
  console.log(
    'Assets bootstrap: search and both picker-cancel orderings passed in sandboxed Chrome',
  );
} finally {
  await browser.close();
}
