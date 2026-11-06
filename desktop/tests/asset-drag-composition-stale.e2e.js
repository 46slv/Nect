import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { blankDocument } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

// Real browser drag events with a disposable, delayed native-IPC fixture.
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
try {
  for (const route of ['layer', 'graph']) {
    const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
    const errors = [];
    page.on('pageerror', (error) => errors.push(error.message));
    await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (request) => request.abort());
    await page.addInitScript(() => {
      window.isTauri = true;
      window.assetReads = 0;
      window.dragEvents = [];
      const bytes = [
        ...new TextEncoder().encode(
          '<svg width="20" height="20"><rect width="20" height="20" fill="#3273c7"/></svg>',
        ),
      ];
      const delayed = new Promise((resolve) => {
        window.finishAssetRead = () => resolve({ bytes, sourceLocation: 'Synthetic/demo.svg' });
      });
      for (const type of ['dragstart', 'drop'])
        document.addEventListener(
          type,
          (event) => window.dragEvents.push({ type, trusted: event.isTrusted }),
          true,
        );
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command) => {
          if (command === 'list_asset_folders') return [{ id: 'synthetic', name: 'Synthetic' }];
          if (command === 'refresh_asset_folder')
            return {
              root: { id: 'synthetic', name: 'Synthetic' },
              generation: '1',
              items: [{ relativePath: 'demo.svg', bytes: 77 }],
              warnings: [],
            };
          if (command === 'open_asset_source') {
            window.assetReads++;
            return window.assetReads === 1
              ? { bytes, sourceLocation: 'Synthetic/demo.svg' }
              : delayed;
          }
          if (command.startsWith('list_')) return [];
          if (command === 'enable_close_guard' || command === 'release_project') return null;
          if (command === 'choose_unsaved') return 'discard';
          throw Error('Unexpected synthetic IPC ' + command);
        },
      };
    });
    await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
    await page.waitForFunction(() => window.nect);
    const project = migrateLegacyDocumentToCompositionProject(blankDocument());
    project.compositions[0].id = 'drag-source-composition';
    const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
    other.id = 'drag-other-composition';
    project.compositions.push(other);
    await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
    await page.evaluate(() => {
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'width', value: window.nect.document.width + 1 },
      });
      window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
    });
    const before = await page.evaluate(() => window.nect.projectSnapshot());
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
    await page.locator('[data-workspace-panel="assets"]').click();
    const card = page.locator('[data-asset-library] [data-path="demo.svg"]');
    await page.waitForFunction(() => {
      const item = document.querySelector('[data-asset-library] [data-path="demo.svg"]');
      return item?.draggable && item.querySelector('img')?.naturalWidth;
    });
    async function dragAsset() {
      await card.scrollIntoViewIfNeeded();
      const cardBox = await card.boundingBox();
      const target = page.locator(route === 'layer' ? '.canvas-stage' : '.graph-scroll');
      const box = await target.boundingBox();
      assert.ok(cardBox && box);
      const from = [cardBox.x + cardBox.width / 2, cardBox.y + cardBox.height / 2];
      const to = [box.x + box.width * 0.72, box.y + box.height * 0.55];
      const beforeEvents = await page.evaluate(() => window.dragEvents.length);
      const beforeReads = await page.evaluate(() => window.assetReads);
      await page.mouse.move(...from);
      await page.mouse.down();
      await page.mouse.move(from[0] + 14, from[1], { steps: 5 });
      await page.waitForFunction(
        (count) => window.dragEvents.slice(count).some((event) => event.type === 'dragstart'),
        beforeEvents,
      );
      await page.mouse.move(...to, { steps: 12 });
      await page.mouse.move(...to);
      await page.mouse.up();
      await page.waitForFunction((count) => window.assetReads === count + 1, beforeReads);
      const events = (await page.evaluate(() => window.dragEvents)).slice(beforeEvents);
      assert.ok(events.some((event) => event.type === 'dragstart' && event.trusted));
      assert.ok(events.some((event) => event.type === 'drop' && event.trusted));
    }
    await dragAsset();
    await page.locator('[role="tab"][data-active-composition="drag-other-composition"]').click();
    const afterSwitch = await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    }));
    assert.deepEqual(afterSwitch.project, before);
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
    await page.evaluate(() => window.finishAssetRead());
    await page.waitForFunction(
      () =>
        document.querySelector('[data-asset-status]')?.textContent ===
        'Document changed; choose the placement again',
    );
    assert.equal(await page.locator('[data-asset-status]').getAttribute('data-error'), 'true');
    assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), before);
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), afterSwitch.state);
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
    await dragAsset();
    await page.waitForFunction(
      () => document.querySelector('[data-asset-status]')?.textContent === 'Asset added',
    );
    const after = await page.evaluate(() => window.nect.projectSnapshot());
    assert.deepEqual(after.compositions[0], before.compositions[0]);
    assert.notDeepEqual(after.compositions[1], before.compositions[1]);
    await page.locator('[data-action="undo"]').click();
    assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), before);
    await page.locator('[data-action="redo"]').click();
    assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), after);
    assert.deepEqual(errors, []);
    await page.close();
  }
  console.log(
    'Assets: stale Composition drag rejects; fresh canvas/graph placement is one Undo/Redo',
  );
} finally {
  await browser.close();
}
