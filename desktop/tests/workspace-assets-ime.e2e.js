import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';

// The browser performs the focus and pointer operations. Folder IPC is a
// disposable in-memory fixture; synthetic composition is not physical IME.
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());
await page.addInitScript(() => {
  window.isTauri = true;
  window.assetCalls = [];
  const bootstrap = new Promise((resolve) => {
    window.resolveAssetBootstrap = resolve;
  });
  const refresh = new Promise((resolve) => {
    window.resolveAssetRefresh = resolve;
  });
  window.__TAURI_INTERNALS__ = {
    transformCallback: () => 1,
    unregisterCallback: () => {},
    invoke: async (command, args) => {
      window.assetCalls.push(command);
      if (command === 'list_asset_folders') return bootstrap;
      if (command === 'refresh_asset_folder') return refresh;
      if (command === 'open_asset_source')
        return {
          bytes: [
            ...new TextEncoder().encode(
              '<svg width="20" height="20"><rect width="20" height="20" fill="#3273c7"/></svg>',
            ),
          ],
          sourceLocation: `Synthetic/${args.relativePath}`,
        };
      if (command === 'register_asset_folder') return null;
      if (command.startsWith('list_')) return [];
      if (command === 'enable_close_guard' || command === 'release_project') return null;
      if (command === 'choose_unsaved') return 'discard';
      throw Error('Unexpected synthetic IPC ' + command);
    },
  };
});

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await page.locator('[data-workspace-panel="assets"]').click();
  const panel = page.locator('[data-asset-library]');
  const search = panel.locator('[data-asset-search]');
  await page.waitForFunction(() => window.assetCalls.includes('list_asset_folders'));
  const initial = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  async function protect(input, value, duringComposition = async () => {}) {
    await input.fill(value);
    await input.evaluate((element) =>
      element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
    );
    await input.click();
    for (const key of ['Tab', 'Shift+Tab']) {
      await page.keyboard.press(key);
      assert.equal(
        await input.evaluate((element) => element === document.activeElement),
        true,
        key,
      );
    }
    await duringComposition();
    assert.equal(await input.evaluate((element) => element === document.activeElement), true);
    assert.equal(await input.inputValue(), value);
    const targets = [
      page.locator('[data-workspace-panel="assets"]'),
      page.locator('[data-workspace-panel="colors"]'),
      page.locator('[data-workspace-panel="layers"]'),
      page.locator('.viewer-tabs [data-viewer-mode="viewers"]'),
      panel.getByRole('button', { name: 'Close assets' }),
      panel.getByRole('button', { name: 'Add folder', exact: true }),
    ];
    for (const target of targets) {
      await target.click();
      await target.evaluate((element) => element.click());
      assert.equal(await input.evaluate((element) => element === document.activeElement), true);
      assert.equal(await input.inputValue(), value);
      assert.equal(await panel.isVisible(), true);
      assert.equal(
        await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
        'true',
      );
      assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');
      assert.deepEqual(await page.evaluate(() => window.nect.commandState), initial.state);
      assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), initial.project);
      assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
    }
    await page.evaluate(async () => {
      const { closeAssetLibrary } = await import('/src/asset-library.js');
      closeAssetLibrary();
    });
    assert.equal(await panel.isVisible(), true);
    assert.equal(await input.evaluate((element) => element === document.activeElement), true);
    await input.evaluate((element) => {
      element.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'Escape', bubbles: true, isComposing: true }),
      );
      element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
    });
  }

  await protect(search, 'demo', async () => {
    await page.evaluate(() =>
      window.resolveAssetBootstrap([{ id: 'synthetic', name: 'Synthetic' }]),
    );
    await page.waitForFunction(() => window.assetCalls.includes('refresh_asset_folder'));
    assert.equal(await search.evaluate((element) => element === document.activeElement), true);
    await search.fill('not the final query');
    await search.fill('demo');
    await page.evaluate(() =>
      window.resolveAssetRefresh({
        root: { id: 'synthetic', name: 'Synthetic' },
        generation: '1',
        items: [{ relativePath: 'demo.svg', bytes: 77 }],
        warnings: [],
      }),
    );
    await panel.locator('[data-path="demo.svg"] img[src]').waitFor();
  });
  await page.locator('[data-workspace-panel="colors"]').click();
  await panel.waitFor({ state: 'hidden' });
  assert.equal(
    await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
    'false',
  );
  await page.locator('[data-workspace-panel="assets"]').click();
  assert.equal(await search.inputValue(), 'demo');
  await panel.locator('[data-path="demo.svg"]').click();
  await panel.getByText('Asset details', { exact: true }).click();
  const author = panel.getByLabel('Asset author');
  await protect(author, '編集中');
  await page.keyboard.press('Tab');
  assert.equal(await author.evaluate((element) => element === document.activeElement), false);
  assert.equal(await author.inputValue(), '編集中');
  assert.equal(
    await page.evaluate(() => window.assetCalls.includes('register_asset_folder')),
    false,
  );
  await panel.getByRole('button', { name: 'Close assets' }).click();
  assert.equal(await panel.count(), 0);
  await page.locator('[data-workspace-panel="colors"]').click();
  await page.locator('[data-shared-color-add]').click();
  assert.deepEqual(errors, []);
  console.log('Assets: synthetic IME, real pointer/keyboard, normal return and cleanup passed');
} finally {
  await browser.close();
}
