import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';

const base = process.env.BASE_URL || 'http://127.0.0.1:5173';
const out = resolve('test-results/asset-folder-favorites');
const first = resolve(out, 'first/assets');
const second = resolve(out, 'second/assets');
await mkdir(first, { recursive: true });
await mkdir(second, { recursive: true });
await writeFile(
  first + '/alpha.svg',
  '<svg width="20" height="20"><rect width="20" height="20" fill="#bf2020"/></svg>',
);
await writeFile(
  second + '/beta.svg',
  '<svg width="20" height="20"><circle cx="10" cy="10" r="8" fill="#203fbf"/></svg>',
);
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
  chromiumSandbox: true,
});
const errors = [];
const results = {};
const listen = (page) => page.on('pageerror', (error) => errors.push(error.message));
const openPanel = async (page) => {
  await page.locator('[data-workspace-panel="assets"]').click();
  const panel = page.locator('[data-asset-library]');
  await panel.waitFor({ state: 'visible' });
  return panel;
};
const ready = async (panel) =>
  panel
    .getByLabel('Asset folder', { exact: true })
    .waitFor({ state: 'visible' })
    .then(() =>
      panel.page().waitForFunction(() => {
        const el = document.querySelector('[aria-label="Asset folder"]');
        return el && !el.disabled && el.options.length > 0;
      }),
    );
try {
  const context = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  const page = await context.newPage();
  listen(page);
  await page.goto(base);
  await page.waitForFunction(() => window.nect);
  let panel = await openPanel(page);
  const select = () => panel.getByLabel('Asset folder', { exact: true });
  async function addFolder(path) {
    const chooser = page.waitForEvent('filechooser');
    await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
    await (await chooser).setFiles(path);
    await ready(panel);
  }
  await addFolder(first);
  const firstId = await select().inputValue();
  await panel.getByRole('button', { name: 'alpha.svg', exact: true }).waitFor();
  await panel.getByRole('button', { name: 'alpha.svg', exact: true }).click();
  await panel.getByRole('button', { name: 'Pin asset', exact: true }).click({ timeout: 3000 });
  assert.equal(await panel.locator('[data-quick-asset]').count(), 1);
  await panel.getByLabel('Quick slot', { exact: true }).selectOption('1', { timeout: 3000 });
  await panel.getByRole('button', { name: 'Assign slot', exact: true }).click();
  await panel.getByRole('button', { name: 'Place slot 1: alpha.svg', exact: true }).waitFor();
  await panel.getByRole('button', { name: 'Pin folder', exact: true }).focus();
  await page.keyboard.press('Enter');
  assert.equal(await panel.locator('[data-quick-open]').count(), 1);
  await addFolder(second);
  const secondId = await select().inputValue();
  assert.notEqual(firstId, secondId);
  await panel.getByRole('button', { name: 'beta.svg', exact: true }).waitFor();
  await panel.getByRole('button', { name: 'beta.svg', exact: true }).click();
  await panel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  assert.equal(await panel.locator('[data-quick-asset]').count(), 2);
  await panel.getByLabel('Quick slot', { exact: true }).selectOption('2');
  await panel.getByRole('button', { name: 'Assign slot', exact: true }).click();
  await panel.getByLabel('Quick slot', { exact: true }).selectOption('1');
  await panel.getByRole('button', { name: 'Replace slot', exact: true }).click();
  await panel.getByRole('button', { name: 'Place slot 1: beta.svg', exact: true }).waitFor();
  await panel.getByLabel('Quick slot', { exact: true }).selectOption('2');
  await panel.getByRole('button', { name: 'Move slot', exact: true }).click();
  await select().selectOption(firstId);
  await ready(panel);
  await panel.getByRole('button', { name: 'alpha.svg', exact: true }).click();
  await panel.getByLabel('Quick slot', { exact: true }).selectOption('1');
  await panel.getByRole('button', { name: 'Assign slot', exact: true }).click();
  const beforeSlot = await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await panel.getByRole('button', { name: 'Place slot 1: alpha.svg', exact: true }).focus();
  await page.keyboard.press('Enter');
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers
          .flatMap((layer) => layer.nodes)
          .find((node) => node.type === 'SVG').svgAsset.rootId,
    ),
    firstId,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    beforeSlot,
  );
  await panel
    .getByRole('button', { name: 'Place slot 1: alpha.svg', exact: true })
    .evaluate((button) => {
      button.click();
      button.click();
    });
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers
          .flatMap((layer) => layer.nodes)
          .filter((node) => node.type === 'SVG').length,
    ),
    1,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    beforeSlot,
  );
  await select().selectOption(secondId);
  await ready(panel);
  await panel.getByRole('button', { name: 'beta.svg', exact: true }).click();
  await panel.getByRole('button', { name: 'Unpin asset', exact: true }).click();
  assert.equal(await panel.locator('[data-quick-asset]').count(), 1);
  await panel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  await panel.getByLabel('Search assets').fill('no matching file');
  await panel.locator('[data-quick-asset]').nth(0).focus();
  await page.keyboard.press('Enter');
  await ready(panel);
  assert.equal(await select().inputValue(), firstId);
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  assert.equal(
    await page.evaluate(() => document.activeElement?.hasAttribute('data-quick-asset')),
    true,
  );
  assert.equal(await panel.getByRole('button', { name: 'Unpin asset', exact: true }).count(), 1);
  await select().selectOption(secondId);
  await ready(panel);
  await panel.getByRole('button', { name: 'Pin folder', exact: true }).click();
  assert.equal(await panel.locator('[data-quick-open]').count(), 2);
  await panel.getByRole('button', { name: 'Unpin folder', exact: true }).click();
  assert.equal(await panel.locator('[data-quick-open]').count(), 1);
  await panel.getByRole('button', { name: 'Pin folder', exact: true }).click();
  assert.equal(await panel.locator('[data-quick-open]').count(), 2);
  const labels = await panel.locator('[data-quick-open]').allTextContents();
  assert.notEqual(labels[0], labels[1]);
  assert.equal(
    await page.evaluate(() => localStorage.getItem('nect.asset-folder-favorites.v1')),
    null,
  );
  const before = await page.evaluate(() => ({
    document: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  await panel.locator('[data-quick-open]').nth(0).focus();
  await page.keyboard.press('Enter');
  await ready(panel);
  assert.equal(await select().inputValue(), firstId);
  const pendingRefresh = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Refresh', exact: true }).click();
  const refreshChooser = await pendingRefresh;
  assert.equal(await panel.locator('[data-quick-open]').nth(1).isDisabled(), true);
  assert.equal(await panel.locator('[data-quick-asset]').nth(1).isDisabled(), true);
  assert.equal(
    await panel.getByRole('button', { name: 'Unpin folder', exact: true }).isDisabled(),
    true,
  );
  assert.equal(await select().isDisabled(), true);
  await panel
    .locator('[data-quick-open]')
    .nth(1)
    .evaluate((button) => button.dispatchEvent(new MouseEvent('click', { bubbles: true })));
  await panel
    .locator('[data-quick-asset]')
    .nth(1)
    .evaluate((button) => button.click());
  assert.equal(await select().inputValue(), firstId, 'pending picker cannot switch folders');
  await select().evaluate((element, id) => {
    element.value = id;
    element.dispatchEvent(new Event('change', { bubbles: true }));
  }, secondId);
  await panel
    .getByRole('button', { name: 'Unpin folder', exact: true })
    .evaluate((button) => button.dispatchEvent(new MouseEvent('click', { bubbles: true })));
  assert.equal(await select().inputValue(), firstId, 'stale change cannot retarget the picker');
  assert.equal(await panel.locator('[data-quick-open]').count(), 2);
  await refreshChooser.setFiles(first);
  await ready(panel);
  assert.equal(await select().inputValue(), firstId);
  assert.equal(await panel.locator('[data-quick-open]').nth(1).isDisabled(), false);
  const cancelRefresh = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Refresh', exact: true }).click();
  await cancelRefresh;
  await panel.locator('[data-asset-folder-input]').dispatchEvent('cancel');
  assert.equal(await select().isDisabled(), false, 'cancel restores folder controls');
  assert.equal(await panel.locator('[data-quick-open]').nth(1).isDisabled(), false);
  assert.deepEqual(
    await page.evaluate(() => ({
      document: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    })),
    before,
  );
  await panel.getByRole('button', { name: 'alpha.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  await panel.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  const svg = await page.evaluate(
    () =>
      window.nect.document.layers
        .flatMap((layer) => layer.nodes)
        .find((node) => node.type === 'SVG').svgAsset,
  );
  assert.equal(svg.rootId, firstId);
  assert.equal(svg.relativePath, 'alpha.svg');
  const afterPlacement = await page.evaluate(() => ({
    document: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  await panel.getByRole('button', { name: 'Close assets' }).click();
  await panel.waitFor({ state: 'detached' });
  panel = await openPanel(page);
  await ready(panel);
  assert.equal(await panel.locator('[data-quick-open]').count(), 2);
  assert.equal(await panel.locator('[data-quick-asset]').count(), 2);
  await panel.getByRole('button', { name: 'Place slot 1: alpha.svg', exact: true }).waitFor();
  assert.equal(
    await panel.getByRole('button', { name: 'Slot 2 empty', exact: true }).isDisabled(),
    true,
  );
  await panel.locator('[data-quick-open]').nth(1).click();
  await ready(panel);
  assert.equal(await select().inputValue(), secondId);
  assert.deepEqual(
    await page.evaluate(() => ({
      document: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    })),
    afterPlacement,
  );
  const search = panel.getByLabel('Search assets');
  await search.fill('入力中');
  await search.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await panel.locator('[data-quick-open]').nth(0).click();
  await panel.locator('[data-quick-asset]').nth(0).click();
  await panel
    .getByRole('button', { name: 'Place slot 1: alpha.svg', exact: true })
    .evaluate((button) => button.click());
  await panel
    .locator('[data-quick-open]')
    .nth(0)
    .evaluate((element) => element.click());
  assert.equal(await select().inputValue(), secondId);
  assert.equal(await search.inputValue(), '入力中');
  assert.equal(await search.evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(
    await page.evaluate(() => ({
      document: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    })),
    afterPlacement,
  );
  await search.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  await search.fill('');
  await page.setViewportSize({ width: 420, height: 900 });
  const dimensions = await panel.evaluate((element) => {
    const bar = element.querySelector('[aria-label="Quick access"]');
    const slotBar = element.querySelector('[aria-label="Quick slots"]');
    const rect = element.getBoundingClientRect();
    const slotRect = slotBar.getBoundingClientRect();
    return {
      panelWidth: rect.width,
      panelClient: element.clientWidth,
      panelScroll: element.scrollWidth,
      barClient: bar.clientWidth,
      barScroll: bar.scrollWidth,
      barWidth: bar.getBoundingClientRect().width,
      slotsFit: [...slotBar.querySelectorAll('button')]
        .filter((button) => !button.hidden)
        .every((button) => {
          const box = button.getBoundingClientRect();
          return box.left >= slotRect.left - 1 && box.right <= slotRect.right + 1;
        }),
    };
  });
  assert.ok(dimensions.panelWidth <= 420);
  assert.ok(dimensions.panelScroll <= dimensions.panelClient + 1, JSON.stringify(dimensions));
  assert.ok(dimensions.barWidth <= dimensions.panelClient);
  assert.equal(dimensions.slotsFit, true);
  await page.screenshot({ path: out + '/browser-420.png' });
  await page.setViewportSize({ width: 1280, height: 900 });
  await page.screenshot({ path: out + '/browser-1280.png' });
  await panel.getByRole('button', { name: 'Remove', exact: true }).click();
  await ready(panel);
  assert.equal(await panel.locator(`[data-quick-open="${secondId}"]`).count(), 0);
  assert.match(await panel.locator('[data-quick-remove]').textContent(), /Unavailable folder/);
  await addFolder(second);
  assert.notEqual(await select().inputValue(), secondId);
  assert.equal(await panel.locator(`[data-quick-open="${secondId}"]`).count(), 0);
  assert.equal(await panel.locator('[data-quick-remove]').count(), 1);
  await panel.locator('[data-quick-remove]').click();
  assert.equal(await panel.locator('[data-quick-open]').count(), 1);
  assert.deepEqual(
    await page.evaluate(() => ({
      document: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    })),
    afterPlacement,
  );
  await page.reload();
  await page.waitForFunction(() => window.nect);
  panel = page.locator('[data-asset-library]');
  if (!(await panel.isVisible().catch(() => false))) panel = await openPanel(page);
  assert.equal(await panel.locator('[data-quick-open]').count(), 0);
  assert.equal(await panel.locator('[data-quick-asset]').count(), 0);
  assert.equal(
    await panel.getByRole('group', { name: 'Quick slots', hidden: true }).isVisible(),
    false,
  );
  await addFolder(first);
  assert.notEqual(await select().inputValue(), firstId);
  assert.equal(await panel.locator('[data-quick-open]').count(), 0);
  results.browser = {
    firstId,
    secondId,
    directSelect: true,
    svgPlacement: true,
    remount: true,
    reloadClears: true,
    removedRootDoesNotReconnect: true,
    syntheticIme: true,
    dimensions,
  };
  await context.close();

  const nativeContext = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  const native = await nativeContext.newPage();
  listen(native);
  await native.addInitScript(() => {
    window.isTauri = true;
    window.calls = [];
    const originalSet = Storage.prototype.setItem;
    Storage.prototype.setItem = function (key, value) {
      if (window.failFavoriteSave && key === 'nect.asset-folder-favorites.v1')
        throw new DOMException('denied', 'QuotaExceededError');
      return originalSet.call(this, key, value);
    };
    const firstRoots = [
      { id: 'root-a', name: 'assets', path: 'C:\\A\\assets' },
      { id: 'root-b', name: 'assets', path: 'C:\\B\\assets' },
    ];
    const roots = sessionStorage.getItem('replace-a')
      ? [{ id: 'root-new', name: 'assets', path: 'C:\\A\\assets' }, firstRoots[1]]
      : firstRoots;
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => 1,
      unregisterCallback: () => {},
      invoke: async (command, args) => {
        window.calls.push({ command, args });
        if (command === 'list_asset_folders') return roots;
        if (command === 'refresh_asset_folder') {
          if (window.delayNext) {
            window.delayNext = false;
            return new Promise((resolve) => {
              window.finishDelayed = resolve;
            });
          }
          return {
            root: roots.find((root) => root.id === args.rootId),
            generation: 'test',
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
  await native.goto(base);
  await native.waitForFunction(() => window.nect);
  let nPanel = await openPanel(native);
  await ready(nPanel);
  await native.evaluate(() => {
    window.failFavoriteSave = true;
  });
  await nPanel.getByRole('button', { name: 'Pin folder', exact: true }).click();
  assert.equal(await nPanel.locator('[data-quick-open]').count(), 0);
  assert.match(
    await nPanel.locator('[data-asset-status]').textContent(),
    /Quick access could not be saved/,
  );
  await native.evaluate(() => {
    window.failFavoriteSave = false;
  });
  await nPanel.getByRole('button', { name: 'Pin folder', exact: true }).click();
  assert.deepEqual(
    await native.evaluate(() => JSON.parse(localStorage.getItem('nect.asset-folder-favorites.v1'))),
    ['root-a'],
  );
  await nPanel.getByLabel('Asset folder', { exact: true }).selectOption('root-b');
  await ready(nPanel);
  await nPanel.getByRole('button', { name: 'Pin folder', exact: true }).click();
  assert.deepEqual(
    await native.evaluate(() => JSON.parse(localStorage.getItem('nect.asset-folder-favorites.v1'))),
    ['root-a', 'root-b'],
  );
  await nPanel.locator('[data-quick-open="root-a"]').click();
  await ready(nPanel);
  assert.equal(await nPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-a');
  await native.evaluate(() => {
    window.finishDelayed = null;
    window.delayNext = true;
  });
  await nPanel.locator('[data-quick-open="root-a"]').click();
  await native.waitForFunction(() => typeof window.finishDelayed === 'function');
  await nPanel.getByLabel('Asset folder', { exact: true }).evaluate((select) => {
    select.value = 'root-b';
    select.dispatchEvent(new Event('change', { bubbles: true }));
  });
  assert.equal(await nPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-a');
  await native.evaluate(() =>
    window.finishDelayed({
      root: { id: 'root-a', name: 'assets' },
      generation: 'test',
      items: [],
      warnings: [],
    }),
  );
  await ready(nPanel);
  assert.equal(await nPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-a');
  await native.evaluate(() => {
    window.finishDelayed = null;
    window.delayNext = true;
  });
  await nPanel.locator('[data-quick-open="root-b"]').click();
  await native.waitForFunction(() => typeof window.finishDelayed === 'function');
  await nPanel.getByRole('button', { name: 'Close assets' }).click();
  nPanel = await openPanel(native);
  await ready(nPanel);
  const nativeBefore = await nPanel.locator('[data-asset-status]').textContent();
  await native.evaluate(() =>
    window.finishDelayed({
      root: { id: 'root-b', name: 'assets' },
      generation: 'late',
      items: [],
      warnings: [],
    }),
  );
  await native.waitForTimeout(50);
  assert.equal(await nPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-a');
  assert.equal(await nPanel.locator('[data-asset-status]').textContent(), nativeBefore);
  await native.evaluate(() => sessionStorage.setItem('replace-a', '1'));
  await native.reload();
  await native.waitForFunction(() => window.nect);
  nPanel = native.locator('[data-asset-library]');
  if (!(await nPanel.isVisible().catch(() => false))) nPanel = await openPanel(native);
  await ready(nPanel);
  assert.equal(await nPanel.locator('[data-quick-open="root-a"]').count(), 0);
  assert.match(
    await nPanel.locator('[data-quick-remove="root-a"]').textContent(),
    /Unavailable folder root-a/,
  );
  assert.equal(await nPanel.locator('[data-quick-open="root-new"]').count(), 0);
  await native.setViewportSize({ width: 420, height: 900 });
  const barBox = await nPanel.getByRole('group', { name: 'Quick access' }).boundingBox();
  const missingBox = await nPanel.locator('[data-quick-remove="root-a"]').boundingBox();
  assert.ok(missingBox.x + missingBox.width <= barBox.x + barBox.width + 1);
  await native.screenshot({ path: out + '/native-missing-420.png' });
  await nPanel.locator('[data-quick-remove="root-a"]').click();
  assert.deepEqual(
    await native.evaluate(() => JSON.parse(localStorage.getItem('nect.asset-folder-favorites.v1'))),
    ['root-b'],
  );
  await nPanel.locator('[data-quick-open="root-b"]').click();
  await ready(nPanel);
  assert.equal(await nPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-b');
  results.nativeStub = {
    storageFailureRollback: true,
    reload: true,
    sameNameSamePathNotRetargeted: true,
    staleCloseResponse: true,
  };
  assert.deepEqual(errors, []);
  await nativeContext.close();

  const itemContext = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  const itemPage = await itemContext.newPage();
  listen(itemPage);
  await itemPage.addInitScript(() => {
    window.isTauri = true;
    const originalSet = Storage.prototype.setItem;
    Storage.prototype.setItem = function (key, value) {
      if (window.failItemFavoriteSave && key === 'nect.asset-item-favorites.v1')
        throw new DOMException('denied', 'QuotaExceededError');
      return originalSet.call(this, key, value);
    };
    const roots = sessionStorage.getItem('replace-a')
      ? [
          { id: 'root-new', name: 'assets', path: 'C:\\A\\assets' },
          { id: 'root-b', name: 'assets', path: 'C:\\B\\assets' },
        ]
      : [
          { id: 'root-a', name: 'assets', path: 'C:\\A\\assets' },
          { id: 'root-b', name: 'assets', path: 'C:\\B\\assets' },
        ];
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => 1,
      unregisterCallback: () => {},
      invoke: async (command, args) => {
        if (command === 'list_asset_folders') return roots;
        if (command === 'refresh_asset_folder') {
          if (args.rootId === 'root-b' && window.failRootB) throw Error('Folder unavailable');
          const response = {
            root: roots.find((root) => root.id === args.rootId),
            generation: 'test',
            items:
              args.rootId === 'root-b' && sessionStorage.getItem('missing-b')
                ? []
                : [{ relativePath: 'same.svg', bytes: 75 }],
            warnings: [],
          };
          if (window.deferItemRefresh) {
            window.deferItemRefresh = false;
            return new Promise((resolve) => {
              window.finishItemRefresh = () => resolve(response);
            });
          }
          return response;
        }
        if (command === 'open_asset_source') {
          const color =
            args.rootId === 'root-b'
              ? sessionStorage.getItem('changed-b')
                ? '#20bf20'
                : '#203fbf'
              : '#bf2020';
          const source =
            args.rootId === 'root-b' && window.badAssetB
              ? 'not an SVG'
              : `<svg width="20" height="20"><rect width="20" height="20" fill="${color}"/></svg>`;
          return {
            bytes: [...new TextEncoder().encode(source)],
            sourceLocation: `${args.rootId}/same.svg`,
          };
        }
        if (command.startsWith('list_')) return [];
        if (command === 'enable_close_guard' || command === 'release_project') return null;
        if (command === 'choose_unsaved') return 'discard';
        throw Error('Unexpected IPC ' + command);
      },
    };
  });
  await itemPage.goto(base);
  await itemPage.waitForFunction(() => window.nect);
  let itemPanel = await openPanel(itemPage);
  await ready(itemPanel);
  await itemPanel.getByRole('button', { name: 'same.svg', exact: true }).click();
  await itemPage.evaluate(() => {
    window.failItemFavoriteSave = true;
  });
  await itemPanel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  assert.equal(await itemPanel.locator('[data-quick-asset]').count(), 0);
  assert.match(await itemPanel.locator('[data-asset-status]').textContent(), /could not be saved/);
  await itemPage.evaluate(() => {
    window.failItemFavoriteSave = false;
  });
  await itemPanel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  await itemPanel.getByLabel('Asset folder', { exact: true }).selectOption('root-b');
  await ready(itemPanel);
  await itemPanel.getByRole('button', { name: 'same.svg', exact: true }).click();
  await itemPanel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  assert.deepEqual(
    await itemPage.evaluate(() => JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1'))),
    [
      { rootId: 'root-a', relativePath: 'same.svg' },
      { rootId: 'root-b', relativePath: 'same.svg' },
    ],
  );
  const itemLabels = await itemPanel.locator('[data-quick-asset]').allTextContents();
  assert.match(itemLabels[0], /^1 · same\.svg/);
  assert.match(itemLabels[1], /^2 · same\.svg/);
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPanel.locator('[data-quick-asset]').first().focus();
  await itemPage.keyboard.press('Enter');
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPanel.locator('[data-asset-place-layer]:enabled').waitFor();
  await itemPage.waitForFunction(
    () => document.activeElement?.hasAttribute('data-quick-asset'),
    null,
    { timeout: 3000 },
  );
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPage.keyboard.press('Enter');
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPage.evaluate(() =>
    document.body.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true })),
  );
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPanel.locator('[data-asset-place-layer]:enabled').waitFor();
  assert.equal(await itemPage.evaluate(() => document.activeElement === document.body), true);
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPanel.locator('[data-quick-asset]').first().click();
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPanel.getByRole('button', { name: 'Close assets' }).click();
  itemPanel = await openPanel(itemPage);
  await ready(itemPanel);
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPage.waitForTimeout(50);
  assert.equal(await itemPanel.locator('[data-asset-place-layer]').isVisible(), false);
  assert.equal(await itemPanel.locator('[data-quick-asset]').count(), 2);
  await itemPanel.locator('[data-quick-asset]').first().click();
  await ready(itemPanel);
  assert.equal(await itemPanel.getByLabel('Asset folder', { exact: true }).inputValue(), 'root-a');
  await itemPanel.locator('[data-asset-place-layer]:enabled').click();
  await itemPage.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await itemPage.evaluate(
      () =>
        window.nect.document.layers
          .flatMap((layer) => layer.nodes)
          .find((node) => node.type === 'SVG').svgAsset.rootId,
    ),
    'root-a',
  );
  await itemPage.evaluate(() => {
    window.badAssetB = true;
  });
  await itemPanel.locator('[data-quick-asset]').last().click();
  const unpinBadAsset = itemPanel.getByRole('button', { name: 'Unpin asset', exact: true });
  await unpinBadAsset.waitFor();
  assert.equal(await unpinBadAsset.isDisabled(), false);
  await unpinBadAsset.click();
  await itemPage.evaluate(() => {
    window.badAssetB = false;
  });
  await itemPanel.getByRole('button', { name: 'Refresh', exact: true }).click();
  await ready(itemPanel);
  await itemPanel.getByRole('button', { name: 'same.svg', exact: true }).click();
  await itemPanel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  await itemPage.evaluate(() => {
    window.failRootB = true;
  });
  await itemPanel.locator('[data-quick-asset]').last().click();
  await itemPanel.locator('[data-quick-remove-asset]').waitFor({ timeout: 3000 });
  await itemPanel.locator('[data-quick-remove-asset]').click();
  await itemPage.evaluate(() => {
    window.failRootB = false;
  });
  await itemPanel.getByRole('button', { name: 'Refresh', exact: true }).click();
  await ready(itemPanel);
  await itemPanel.getByRole('button', { name: 'same.svg', exact: true }).click();
  await itemPanel.getByRole('button', { name: 'Pin asset', exact: true }).click();
  await itemPanel.getByText('Asset details', { exact: true }).click();
  await itemPanel.getByLabel('Asset author', { exact: true }).fill('Author');
  await itemPanel.getByLabel('Asset source', { exact: true }).fill('Catalog');
  await itemPanel.getByLabel('Asset license', { exact: true }).fill('CC0');
  await itemPanel.getByLabel('Quick slot', { exact: true }).selectOption('2');
  const beforeFailedSlotWrite = await itemPage.evaluate(() =>
    localStorage.getItem('nect.asset-item-favorites.v1'),
  );
  await itemPage.evaluate(() => {
    window.failItemFavoriteSave = true;
  });
  await itemPanel.getByRole('button', { name: 'Assign slot', exact: true }).click();
  assert.equal(
    await itemPage.evaluate(() => localStorage.getItem('nect.asset-item-favorites.v1')),
    beforeFailedSlotWrite,
  );
  assert.equal(
    await itemPanel.getByRole('button', { name: 'Slot 2 empty', exact: true }).isDisabled(),
    true,
  );
  await itemPage.evaluate(() => {
    window.failItemFavoriteSave = false;
  });
  await itemPanel.getByRole('button', { name: 'Assign slot', exact: true }).click();
  await itemPage.evaluate(() => sessionStorage.setItem('replace-a', '1'));
  await itemPage.reload();
  await itemPage.waitForFunction(() => window.nect);
  itemPanel = itemPage.locator('[data-asset-library]');
  if (!(await itemPanel.isVisible().catch(() => false))) itemPanel = await openPanel(itemPage);
  await ready(itemPanel);
  assert.equal(await itemPanel.locator('[data-quick-remove-asset]').count(), 1);
  assert.equal(await itemPanel.locator('[data-quick-asset]').count(), 1);
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).waitFor();
  await itemPanel.locator('[data-quick-remove-asset]').click();
  const savedItems = await itemPage.evaluate(() =>
    JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1')),
  );
  assert.equal(savedItems.length, 1);
  assert.equal(savedItems[0].rootId, 'root-b');
  assert.equal(savedItems[0].relativePath, 'same.svg');
  assert.equal(savedItems[0].slot.number, 2);
  assert.match(savedItems[0].slot.sha256, /^[a-f0-9]{64}$/);
  assert.deepEqual(savedItems[0].slot.metadata, {
    author: 'Author',
    source: 'Catalog',
    license: 'CC0',
  });
  await itemPanel.locator('[data-quick-asset]').click();
  await itemPanel.getByLabel('Quick slot', { exact: true }).selectOption('3');
  await itemPanel.getByRole('button', { name: 'Move slot', exact: true }).click();
  assert.deepEqual(
    await itemPage.evaluate(
      () => JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1'))[0].slot.metadata,
    ),
    { author: 'Author', source: 'Catalog', license: 'CC0' },
  );
  await itemPanel.getByLabel('Quick slot', { exact: true }).selectOption('2');
  await itemPanel.getByRole('button', { name: 'Move slot', exact: true }).click();
  await itemPage.evaluate(() => sessionStorage.setItem('changed-b', '1'));
  const beforeChangedSource = await itemPage.evaluate(() =>
    JSON.stringify(window.nect.projectSnapshot()),
  );
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() =>
    document
      .querySelector('[data-asset-status]')
      ?.textContent.includes('Asset changed; update its slot'),
  );
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    beforeChangedSource,
  );
  await itemPanel.locator('[data-quick-asset]').click();
  await itemPanel.getByText('Asset details', { exact: true }).click();
  assert.equal(await itemPanel.getByLabel('Asset author', { exact: true }).inputValue(), 'Author');
  assert.equal(await itemPanel.getByLabel('Asset source', { exact: true }).inputValue(), 'Catalog');
  assert.equal(await itemPanel.getByLabel('Asset license', { exact: true }).inputValue(), 'CC0');
  await itemPanel.getByLabel('Quick slot', { exact: true }).selectOption('3');
  await itemPanel.getByRole('button', { name: 'Move slot', exact: true }).click();
  const movedSlot = await itemPage.evaluate(
    () => JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1'))[0].slot,
  );
  assert.equal(movedSlot.number, 3);
  assert.equal(movedSlot.sha256, savedItems[0].slot.sha256);
  await itemPanel.getByRole('button', { name: 'Update slot', exact: true }).click();
  const updatedSlot = await itemPage.evaluate(
    () => JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1'))[0].slot,
  );
  assert.notEqual(updatedSlot.sha256, savedItems[0].slot.sha256);
  await itemPanel.getByRole('button', { name: 'Place slot 3: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await itemPage.evaluate(
      () =>
        window.nect.document.layers
          .flatMap((layer) => layer.nodes)
          .find((node) => node.type === 'SVG').svgAsset.author,
    ),
    'Author',
  );
  await itemPage.locator('[data-action="undo"]').click();
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    beforeChangedSource,
  );
  await itemPage.evaluate(() => sessionStorage.removeItem('changed-b'));
  await itemPanel.locator('[data-quick-asset]').click();
  await itemPanel.getByRole('button', { name: 'Update slot', exact: true }).click();
  await itemPanel.getByLabel('Quick slot', { exact: true }).selectOption('2');
  await itemPanel.getByRole('button', { name: 'Move slot', exact: true }).click();
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPage.evaluate(() => {
    const id = window.nect.activeCompositionId;
    const current = window.nect.projectSnapshot().compositions.find((item) => item.id === id);
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'rename-composition',
      payload: {
        compositionId: id,
        name: 'Changed while loading',
        expectedName: current.name,
        expectedProjectRevision: window.nect.projectRevision,
      },
    });
  });
  const afterRename = await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPage.waitForTimeout(100);
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterRename,
  );
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPanel.evaluate((panel) => {
    panel.hidden = true;
    panel.inert = true;
    panel.hidden = false;
    panel.inert = false;
  });
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPage.waitForTimeout(100);
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterRename,
  );
  await itemPage.evaluate(() => {
    window.deferItemRefresh = true;
    window.finishItemRefresh = null;
  });
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() => typeof window.finishItemRefresh === 'function');
  await itemPage.evaluate(() => {
    const dialog = document.createElement('dialog');
    dialog.open = true;
    document.body.append(dialog);
    dialog.remove();
  });
  await itemPage.evaluate(() => window.finishItemRefresh());
  await itemPage.waitForTimeout(100);
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterRename,
  );
  await itemPage.evaluate(() => {
    window.failRootB = true;
  });
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() =>
    document.querySelector('[data-asset-status]')?.textContent.includes('Folder unavailable'),
  );
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterRename,
  );
  await itemPage.evaluate(() => {
    window.failRootB = false;
  });
  await itemPanel.getByRole('button', { name: 'Place slot 2: same.svg', exact: true }).click();
  await itemPage.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await itemPage.evaluate(
      () =>
        window.nect.document.layers
          .flatMap((layer) => layer.nodes)
          .find((node) => node.type === 'SVG').svgAsset.rootId,
    ),
    'root-b',
  );
  await itemPage.locator('[data-action="undo"]').click();
  assert.equal(
    await itemPage.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterRename,
  );
  await itemPage.evaluate(() => sessionStorage.setItem('missing-b', '1'));
  await itemPanel.locator('[data-quick-asset]').click();
  await itemPanel.locator('[data-quick-remove-asset]').waitFor();
  await itemPanel
    .getByRole('button', { name: 'Clear slot 2', exact: true })
    .click({ timeout: 3000 });
  const missingPin = await itemPage.evaluate(
    () => JSON.parse(localStorage.getItem('nect.asset-item-favorites.v1'))[0],
  );
  assert.equal(missingPin.rootId, 'root-b');
  assert.equal(missingPin.relativePath, 'same.svg');
  assert.equal(missingPin.slot, undefined);
  await itemPage.evaluate(() => sessionStorage.removeItem('missing-b'));
  await itemPanel.getByRole('button', { name: 'Refresh', exact: true }).click();
  await itemPanel.locator('[data-quick-asset]').waitFor();
  assert.deepEqual(errors, []);
  await itemContext.close();
  results.itemFavorites = {
    nativeStubPersistence: true,
    exactRootAndPath: true,
    missingAndRecovery: true,
  };
  results.quickSlots = {
    assignMoveReplace: true,
    oneUndoPlacement: true,
    changedSourceRefusedUntilUpdated: true,
    staleSessionAndHiddenPanelRefused: true,
    nativeStubPersistence: true,
  };
  console.log(JSON.stringify(results, null, 2));
} finally {
  await browser.close();
}
