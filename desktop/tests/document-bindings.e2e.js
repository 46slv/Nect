import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
// Real UI and storage adapter with deterministic IPC. Native buffer ownership is
// checked separately by Rust tests; this does not claim native dialog acceptance.
await page.addInitScript(() => {
  window.isTauri = true;
  window.hostBindings = new Map();
  window.nextToken = 0;
  window.saveMode = 'normal';
  window.__TAURI_INTERNALS__ = {
    transformCallback: () => 1,
    unregisterCallback: () => {},
    invoke: async (command, args) => {
      if (command === 'list_system_fonts') return [];
      if (command === 'enable_close_guard') return;
      if (command === 'choose_unsaved') return 'discard';
      if (command === 'release_project') {
        window.hostBindings.delete(args.token);
        return;
      }
      const allocate = (contents) => {
        const token = String(++window.nextToken);
        window.hostBindings.set(token, contents);
        return [token, `${token}.json`, contents];
      };
      if (command === 'open_project') {
        if (window.deferOpen) {
          window.deferOpen = false;
          return new Promise((resolve) => {
            window.finishOpen = () => resolve(allocate(window.openContents));
          });
        }
        return allocate(window.openContents);
      }
      if (command === 'save_project') {
        if (window.saveMode === 'cancel') return null;
        if (window.saveMode === 'fail') throw 'write failed';
        const result = args.token
          ? [args.token, `${args.token}.json`, args.contents]
          : allocate(args.contents);
        return [result[0], result[1], args.operation + (window.saveMode === 'mismatch' ? 1 : 0)];
      }
      throw Error(`Unexpected IPC ${command}`);
    },
  };
});
const click = (action) => page.locator(`[data-action="${action}"]`).click();
const tokens = () => page.evaluate(() => [...window.hostBindings.keys()]);
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('.shape');
  const original = await page.evaluate(() => JSON.stringify(window.nect.snapshot()));
  await page.evaluate((contents) => {
    window.openContents = contents;
  }, original);
  for (let i = 0; i < 8; i++) {
    await click('open');
    await page.waitForFunction(
      () => window.hostBindings.size === 1 && !window.nect.fileState.dirty,
    );
    assert.deepEqual(await tokens(), [String(i + 1)]);
  }
  const active = (await tokens())[0];
  await page.evaluate(() => {
    window.openContents = '{invalid JSON';
  });
  await click('open');
  await page.waitForFunction(() => window.nextToken === 9 && window.hostBindings.size === 1);
  assert.deepEqual(await tokens(), [active]);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.snapshot())), original);
  await page.evaluate((contents) => {
    window.openContents = contents;
    window.deferOpen = true;
  }, original);
  await click('open');
  await page.waitForFunction(() => !!window.finishOpen);
  await page.locator('[data-tool="Rectangle"]').click();
  const edited = await page.evaluate(() => JSON.stringify(window.nect.snapshot()));
  await page.evaluate(() => {
    window.finishOpen();
    window.finishOpen = null;
  });
  await page.waitForFunction(() =>
    document.querySelector('.message').textContent.includes('changed while reading'),
  );
  assert.deepEqual(await tokens(), [active]);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.snapshot())), edited);
  for (const mode of ['cancel', 'fail', 'mismatch']) {
    await page.evaluate((mode) => {
      window.saveMode = mode;
    }, mode);
    await click('save-as');
    await page.waitForFunction(() => !window.nect.fileState.saving);
    assert.deepEqual(await tokens(), [active]);
    assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
  }
  await page.evaluate(() => {
    window.saveMode = 'normal';
  });
  await click('save-as');
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  const saved = await tokens();
  assert.equal(saved.length, 1);
  assert.notDeepEqual(saved, [active]);
  await click('save');
  assert.deepEqual(await tokens(), saved);
  await page.evaluate(() => {
    window.deferOpen = true;
  });
  await click('open');
  await page.waitForFunction(() => !!window.finishOpen);
  await click('new');
  await page.evaluate(() => {
    window.finishOpen();
  });
  await page.waitForFunction(() => window.hostBindings.size === 0);
  assert.deepEqual(errors, []);
  console.log(
    'PASS real UI/storage IPC: repeated/rejected/stale Open, Save As retirement, cancelled/failed/mismatched save and New preserve only the active binding. Native dialogs are separate.',
  );
} catch (error) {
  console.error(
    await page.evaluate(() => ({
      tokens: [...window.hostBindings.keys()],
      message: document.querySelector('.message')?.textContent,
      fileState: window.nect?.fileState,
    })),
    errors,
  );
  throw error;
} finally {
  await browser.close();
}
