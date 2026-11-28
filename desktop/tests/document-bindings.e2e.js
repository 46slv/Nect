import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

// Reuse authored IDs and visible geometry from a saved example, but give this
// test its own explicit size. Feed it through Open, not the in-page load hook:
// this test needs the real storage adapter to adopt the first file binding.
const seedSize = [713.25, 421.75];
const seed = JSON.parse(
  await fs.readFile(new URL('../examples/object-resize.json', import.meta.url), 'utf8'),
);
const seedContents = JSON.stringify({ ...seed, width: seedSize[0], height: seedSize[1] });
const dimensions = (project) => project.compositions.map(({ width, height }) => [width, height]);
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
// Real UI and storage adapter with deterministic IPC; no native dialog or
// buffer ownership coverage. Recovery uses separate mock bookkeeping.
await page.addInitScript((contents) => {
  window.isTauri = true;
  const host = (window.bindingHost = {
    bindings: new Map(),
    nextToken: 0,
    openContents: contents,
    deferOpen: false,
    finishOpen: null,
    saveMode: 'normal',
    writes: [],
    released: [],
    unexpected: [],
    closeGuardRegistered: false,
  });
  let recoveryId = 0;
  const allocate = (contents) => {
    const token = String(++host.nextToken);
    host.bindings.set(token, contents);
    return [token, `${token}.json`, contents];
  };
  window.__TAURI_INTERNALS__ = {
    transformCallback: () => 1,
    unregisterCallback: () => {},
    invoke: async (command, args) => {
      switch (command) {
        case 'list_system_fonts':
        case 'list_os_outline_fonts':
        case 'list_desktop_font_faces':
        case 'list_recovery':
          return [];
        case 'enable_close_guard':
          host.closeGuardRegistered = true;
          return;
        case 'begin_recovery':
          return `recovery-${++recoveryId}`;
        case 'write_recovery':
          return '2026-11-28T00:00:00.000Z';
        case 'choose_unsaved':
          return 'discard';
        case 'release_project':
          host.bindings.delete(args.token);
          host.released.push(args.token);
          return;
        case 'open_project': {
          const contents = host.openContents;
          if (!host.deferOpen) return allocate(contents);
          host.deferOpen = false;
          return new Promise((resolve) => {
            host.finishOpen = () => {
              host.finishOpen = null;
              const opened = allocate(contents);
              resolve(opened);
              return opened[0];
            };
          });
        }
        case 'save_project': {
          host.writes.push({ ...args, mode: host.saveMode });
          if (host.saveMode === 'cancel') return null;
          if (host.saveMode === 'fail') throw Error('write failed');
          let token = args.token;
          if (token) {
            if (!host.bindings.has(token)) throw Error('Unknown file binding');
            host.bindings.set(token, args.contents);
          } else [token] = allocate(args.contents);
          return [token, `${token}.json`, args.operation + Number(host.saveMode === 'mismatch')];
        }
        default:
          host.unexpected.push(command);
          throw Error(`Unexpected IPC ${command}`);
      }
    },
  };
}, seedContents);
const click = (action) => activateBoundaryCommand(page, action);
const state = () =>
  page.evaluate(() => ({
    document: window.nect.snapshot(),
    project: window.nect.projectSnapshot(),
    command: window.nect.commandState,
    file: window.nect.fileState,
  }));
const bindings = () => page.evaluate(() => [...window.bindingHost.bindings]);
const writes = () => page.evaluate(() => window.bindingHost.writes);
const released = () => page.evaluate(() => window.bindingHost.released);
const waitForBinding = (token) =>
  page.waitForFunction((token) => {
    const { bindings } = window.bindingHost;
    const file = window.nect.fileState;
    return (
      file.binding?.token === token &&
      !file.dirty &&
      !file.saving &&
      bindings.size === 1 &&
      bindings.has(token)
    );
  }, token);
const waitForRelease = (token) =>
  page.waitForFunction((token) => window.bindingHost.released.includes(token), token);
const deferOpen = async () => {
  await page.evaluate((contents) => {
    window.bindingHost.openContents = contents;
    window.bindingHost.deferOpen = true;
  }, seedContents);
  await click('open');
  await page.waitForFunction(() => !!window.bindingHost.finishOpen);
};
const finishOpen = () => page.evaluate(() => window.bindingHost.finishOpen());
const save = async (action, mode = 'normal') => {
  const before = await page.evaluate((mode) => {
    window.bindingHost.saveMode = mode;
    return window.bindingHost.writes.length;
  }, mode);
  await click(action);
  await page.waitForFunction(
    (before) => window.bindingHost.writes.length === before + 1 && !window.nect.fileState.saving,
    before,
  );
  return page.evaluate(() => window.bindingHost.writes.at(-1));
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForFunction(
    () => window.nect && window.bindingHost.closeGuardRegistered && !document.body.inert,
  );
  await click('open');
  await waitForBinding('1');
  await page.waitForSelector('.shape');
  const original = await state();
  assert.deepEqual(dimensions(original.project), [seedSize]);
  assert.deepEqual(await bindings(), [['1', seedContents]]);
  for (let i = 2; i <= 8; i++) {
    await click('open');
    await waitForBinding(String(i));
    assert.deepEqual(await bindings(), [[String(i), seedContents]]);
    const reopened = await state();
    assert.deepEqual(reopened.document, original.document);
    assert.deepEqual(reopened.project, original.project);
  }
  const active = '8';
  const opened = await state();
  assert.deepEqual(await released(), ['1', '2', '3', '4', '5', '6', '7']);

  await page.evaluate(() => {
    window.bindingHost.openContents = '{invalid JSON';
  });
  await click('open');
  await waitForRelease('9');
  assert.deepEqual(await bindings(), [[active, seedContents]]);
  assert.deepEqual(await state(), opened);
  assert.deepEqual(await writes(), []);

  await deferOpen();
  await page.locator('[data-tool="Rectangle"]').click();
  const edited = await state();
  assert.notDeepEqual(edited.project, opened.project);
  assert.equal(edited.file.dirty, true);
  await waitForRelease(await finishOpen());
  await page.waitForFunction(() =>
    document.querySelector('.message')?.textContent.includes('changed while reading'),
  );
  assert.deepEqual(await bindings(), [[active, seedContents]]);
  assert.deepEqual(await state(), edited);
  assert.deepEqual(await writes(), []);
  console.log('PASS repeated, rejected and stale Open: preserved editor state and active bytes');

  const priorReleases = await released();
  for (const mode of ['cancel', 'fail', 'mismatch']) {
    const attempt = await save('save-as', mode);
    assert.equal(attempt.token, null);
    assert.deepEqual(JSON.parse(attempt.contents), edited.project);
    assert.deepEqual(await bindings(), [[active, seedContents]]);
    assert.deepEqual(await state(), edited);
    assert.deepEqual(
      await released(),
      mode === 'mismatch' ? [...priorReleases, '11'] : priorReleases,
    );
    console.log(`PASS Save As ${mode}: preserved active binding, bytes and dirty edit`);
  }
  const committed = await save('save-as');
  const savedToken = '12';
  await waitForBinding(savedToken);
  assert.equal(committed.token, null);
  assert.deepEqual(JSON.parse(committed.contents), edited.project);
  assert.deepEqual(dimensions(JSON.parse(committed.contents)), [seedSize]);
  assert.deepEqual(await bindings(), [[savedToken, committed.contents]]);
  assert.deepEqual(await released(), [...priorReleases, '11', active]);
  console.log('PASS Save As success: retired old binding and saved the edited project');

  await page.locator('[data-tool="Circle"]').click();
  const secondEdit = await state();
  assert.equal(secondEdit.file.dirty, true);
  const ordinary = await save('save');
  await waitForBinding(savedToken);
  assert.equal(ordinary.token, savedToken);
  assert.notEqual(ordinary.contents, committed.contents);
  assert.deepEqual(JSON.parse(ordinary.contents), secondEdit.project);
  assert.deepEqual(dimensions(JSON.parse(ordinary.contents)), [seedSize]);
  assert.deepEqual(await bindings(), [[savedToken, ordinary.contents]]);
  console.log('PASS ordinary Save: reused the binding and updated its bytes');

  const saved = await state();
  const writesBeforeNew = await writes();
  await deferOpen();
  const newSize = [627.5, 395.25];
  await click('new');
  const dialog = page.locator('.new-project-dialog[open]');
  await dialog.locator('[name="width"]').fill(String(newSize[0]));
  await dialog.locator('[name="height"]').fill(String(newSize[1]));
  await dialog.locator('button[type="submit"]').click();
  await page.waitForFunction(([width, height]) => {
    const doc = window.nect.document;
    return doc?.width === width && doc.height === height && window.nect.fileState.binding === null;
  }, newSize);
  await waitForRelease(savedToken);
  const created = await state();
  assert.notEqual(created.command.sessionId, saved.command.sessionId);
  assert.deepEqual(dimensions(created.project), [newSize]);
  assert.deepEqual(
    created.document.layers.map((layer) => layer.nodes.map((node) => node.type)),
    [['Output']],
  );
  assert.equal(created.file.dirty, true);
  await waitForRelease(await finishOpen());
  assert.deepEqual(await bindings(), []);
  assert.deepEqual(await state(), created);
  assert.deepEqual(
    await writes(),
    writesBeforeNew,
    'New and the stale Open must not write a project file',
  );
  assert.deepEqual(await page.evaluate(() => window.bindingHost.unexpected), []);
  assert.deepEqual(errors, []);
  console.log(
    'PASS explicit New before late Open: preserved new project, released both bindings and issued no file write',
  );
  console.log(
    'Boundary: real renderer and storage adapter / mocked Tauri IPC, not native dialogs or storage',
  );
} catch (error) {
  console.error(
    await page.evaluate(() => ({
      tokens: [...window.bindingHost.bindings.keys()],
      released: window.bindingHost.released,
      unexpected: window.bindingHost.unexpected,
      message: document.querySelector('.message')?.textContent,
      fileState: window.nect?.fileState,
    })),
    errors,
  );
  throw error;
} finally {
  await browser.close();
}
