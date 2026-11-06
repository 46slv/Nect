import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

// Real UI and storage adapter, with controllable native IPC and no native window.
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
async function fixture() {
  const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.addInitScript(() => {
    window.isTauri = true;
    window.closeCalls = 0;
    window.chooseCalls = 0;
    window.chooseResult = 'discard';
    window.holdRecovery = false;
    window.holdFinish = false;
    let nextId = 0;
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => ++nextId,
      unregisterCallback: () => {},
      invoke: async (command, args = {}) => {
        if (command === 'enable_close_guard') {
          window.closeChannel = args.channel;
          return null;
        }
        if (command === 'finish_close') {
          window.closeCalls++;
          if (window.holdFinish)
            await new Promise((resolve, reject) => {
              window.releaseFinish = resolve;
              window.rejectFinish = reject;
            });
          return null;
        }
        if (command === 'choose_unsaved') {
          window.chooseCalls++;
          return window.chooseResult;
        }
        if (command === 'begin_recovery') return `fixture-${++nextId}`;
        if (command === 'write_recovery') {
          if (window.holdRecovery)
            await new Promise((resolve) => {
              window.releaseRecovery = resolve;
            });
          if (window.failRecovery) throw Error('fixture recovery failed');
          return Date.now();
        }
        if (command === 'save_project') {
          if (window.holdSave)
            await new Promise((resolve) => {
              window.releaseSave = resolve;
            });
          return ['fixture-save', 'fixture.nect', args.operation];
        }
        if (command === 'open_project')
          return new Promise((resolve) => {
            window.releaseOpen = resolve;
          });
        if (command === 'list_system_fonts') return [['fixture', 'Fixture font', null]];
        if (command === 'read_system_font') {
          await new Promise((resolve) => {
            window.releaseFont = resolve;
          });
          const bytes = await (await fetch('/fonts/DejaVuSans.ttf')).arrayBuffer();
          window.fontReadCompleted = true;
          return Array.from(new Uint8Array(bytes));
        }
        if (['list_recovery', 'list_os_outline_fonts', 'list_desktop_font_faces'].includes(command))
          return [];
        if (command === 'release_project') {
          window.releaseCalls = (window.releaseCalls || 0) + 1;
          return null;
        }
        if (command === 'acknowledge_recovery') return null;
        throw Error(`Unexpected fixture IPC: ${command}`);
      },
    };
  });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5234');
  await page.waitForFunction(() => window.nect && window.closeChannel);
  await page.waitForFunction(
    () => !['pending', 'saving'].includes(window.nect.recoveryState?.phase),
  );
  return { page, errors };
}
async function holdDirtyRecovery(page) {
  await page.evaluate(() => {
    window.holdRecovery = true;
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
  });
  await page.waitForFunction(() => typeof window.releaseRecovery === 'function');
}
async function closeRequest(page) {
  await page.evaluate(() => {
    window.closeDone = false;
    window.closeChannel.onmessage().finally(() => {
      window.closeDone = true;
    });
  });
}
async function releaseRecovery(page) {
  await page.evaluate(() => {
    window.holdRecovery = false;
    window.releaseRecovery();
  });
}
async function run(name, action) {
  const { page, errors } = await fixture();
  try {
    await action(page);
    assert.deepEqual(errors, [], name);
    console.log(`PASS ${name}`);
  } finally {
    await page.close();
  }
}

try {
  await run('later commit invalidates the close approval', async (page) => {
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    await page.evaluate(() =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'height', value: 701 },
      }),
    );
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    assert.equal(await page.evaluate(() => window.nect.document.height), 701);
  });

  await run('later Session text draft remains editable', async (page) => {
    await page.evaluate(async () => {
      const core = await import('/src/core.js');
      const model = await import('/src/composition-project-model.js');
      const doc = core.document(),
        text = core.node('Text');
      text.text = 'A';
      doc.layers[0].nodes.push(text);
      window.textLayer = doc.layers[0].id;
      window.textNode = text.id;
      window.nect.load(JSON.stringify(model.migrateLegacyDocumentToCompositionProject(doc)));
    });
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    await page.evaluate(() => {
      const send = (type, payload) =>
        window.nect.dispatch({ ...window.nect.commandState, type, payload });
      const draft = send('begin-text-draft', {
        layerId: window.textLayer,
        nodeId: window.textNode,
      });
      window.textDraft = send('stage-text-draft', {
        draftId: draft.draftId,
        expectedDraftRevision: draft.draftRevision,
        action: 'edit',
        previousRaw: 'A',
        nextRaw: 'AB',
      });
    });
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    await page.evaluate(() =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'commit-text-draft',
        payload: {
          draftId: window.textDraft.draftId,
          expectedDraftRevision: window.textDraft.draftRevision,
        },
      }),
    );
    assert.equal(
      await page.evaluate(
        () => window.nect.document.layers[0].nodes.find((n) => n.id === window.textNode).text,
      ),
      'AB',
    );
  });

  await run('a later expression dialog draft remains editable', async (page) => {
    await page.evaluate(async () => {
      const core = await import('/src/core.js');
      const model = await import('/src/composition-project-model.js');
      const doc = core.document(),
        value = core.node('Value');
      doc.layers[0].nodes.push(value);
      window.valueNode = value.id;
      window.nect.load(JSON.stringify(model.migrateLegacyDocumentToCompositionProject(doc)));
    });
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    const valueId = await page.evaluate(() => window.valueNode);
    await page.locator(`.node[data-id="${valueId}"] .node-head strong`).click();
    await ensureWorkspacePanel(page, 'inspector');
    await page
      .locator(`.inspector-body .field[data-node="${valueId}"][data-key="value"] [data-fx]`)
      .click();
    await page.locator('dialog[open] input[aria-label="Parameter expression"]').fill('value + 1');
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    assert.equal(
      await page.locator('dialog[open] input[aria-label="Parameter expression"]').inputValue(),
      'value + 1',
    );
    await page.locator('dialog[open] [data-confirm]').click();
    assert.equal(
      await page.evaluate(
        () =>
          window.nect.document.layers[0].nodes.find((n) => n.id === window.valueNode).params.value
            .expression,
      ),
      'value + 1',
    );
  });

  await run('replacement owner invalidates the close approval', async (page) => {
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    await page.evaluate(() => window.nect.load(JSON.stringify(window.nect.projectSnapshot())));
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    assert.equal(await page.evaluate(() => window.nect.document.width), 961);
  });

  await run('a new Save during recovery invalidates the approval', async (page) => {
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    await page.evaluate(() => document.querySelector('[data-action="save"]').click());
    await page.waitForFunction(() => window.nect.fileState.binding?.token === 'fixture-save');
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  });

  await run('Save approval retains the save and rejects a later edit', async (page) => {
    await holdDirtyRecovery(page);
    await page.evaluate(() => {
      window.chooseResult = 'save';
    });
    await closeRequest(page);
    await page.waitForFunction(() => window.nect.fileState.binding?.token === 'fixture-save');
    await page.evaluate(() =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'height', value: 701 },
      }),
    );
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
    assert.equal(await page.evaluate(() => window.nect.fileState.binding.token), 'fixture-save');
    assert.equal(await page.evaluate(() => window.nect.document.height), 701);
  });

  await run('an in-flight Save prevents close', async (page) => {
    await page.evaluate(() => {
      window.holdSave = true;
      document.querySelector('[data-action="save"]').click();
    });
    await page.waitForFunction(() => typeof window.releaseSave === 'function');
    await closeRequest(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    await page.evaluate(() => window.releaseSave());
    await page.waitForFunction(() => !window.nect.fileState.saving);
    assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  });

  await run('Discard permits the approved snapshot and coalesces duplicate close', async (page) => {
    await holdDirtyRecovery(page);
    await closeRequest(page);
    await page.waitForFunction(() => window.chooseCalls === 1);
    await page.evaluate(() => window.closeChannel.onmessage());
    assert.equal(await page.evaluate(() => window.chooseCalls), 1);
    await page.evaluate(() => {
      window.failRecovery = true;
    });
    await releaseRecovery(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 1);
  });

  await run('Cancel does not wait for recovery or close', async (page) => {
    await holdDirtyRecovery(page);
    await page.evaluate(() => {
      window.chooseResult = 'cancel';
    });
    await closeRequest(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 0);
    await releaseRecovery(page);
  });

  await run('finish IPC holds edits, restores focus and releases on rejection', async (page) => {
    await page.evaluate(() => {
      window.holdFinish = true;
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'width', value: 961 },
      });
      window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
      document.querySelector('[data-action="save"]').focus();
      window.priorFocus = document.activeElement;
      window.priorProject = JSON.stringify(window.nect.projectSnapshot());
      window.priorState = window.nect.commandState;
    });
    await closeRequest(page);
    await page.waitForFunction(() => window.closeCalls === 1);
    const blocked = await page.evaluate(() => {
      let dispatch = false,
        load = false;
      try {
        window.nect.dispatch({ ...window.nect.commandState, type: 'redo' });
      } catch {
        dispatch = true;
      }
      try {
        window.nect.load(window.priorProject);
      } catch {
        load = true;
      }
      window.closeChannel.onmessage();
      return { dispatch, load, inert: document.body.inert };
    });
    assert.deepEqual(blocked, { dispatch: true, load: true, inert: true });
    await page.mouse.click(100, 100);
    assert.equal(await page.evaluate(() => window.closeCalls), 1);
    await page.evaluate(() => window.rejectFinish(Error('fixture close failed')));
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => document.body.inert), false);
    assert.equal(await page.evaluate(() => document.activeElement === window.priorFocus), true);
    assert.equal(
      await page.evaluate(
        () => JSON.stringify(window.nect.projectSnapshot()) === window.priorProject,
      ),
      true,
    );
    assert.deepEqual(
      await page.evaluate(() => window.nect.commandState),
      await page.evaluate(() => window.priorState),
    );
    await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
    assert.equal(await page.evaluate(() => window.nect.document.width), 961);
    await page.evaluate(() => {
      window.holdFinish = false;
    });
    await closeRequest(page);
    await page.waitForFunction(() => window.closeDone);
    assert.equal(await page.evaluate(() => window.closeCalls), 2);
  });

  await run(
    'an earlier asynchronous Open cannot replace the project during finish IPC',
    async (page) => {
      await page.evaluate(() => {
        window.openContents = JSON.stringify(window.nect.projectSnapshot());
        document.querySelector('[data-action="open"]').click();
      });
      await page.waitForFunction(() => typeof window.releaseOpen === 'function');
      await page.evaluate(() => {
        window.holdFinish = true;
        window.projectBeforeFinish = JSON.stringify(window.nect.projectSnapshot());
      });
      await closeRequest(page);
      await page.waitForFunction(() => window.closeCalls === 1);
      await page.evaluate(() =>
        window.releaseOpen(['open-token', 'other.nect', window.openContents]),
      );
      await page.waitForFunction(() => window.releaseCalls === 1);
      assert.equal(
        await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
        await page.evaluate(() => window.projectBeforeFinish),
      );
      await page.evaluate(() => window.releaseFinish());
      await page.waitForFunction(() => window.closeDone);
    },
  );

  await run('a pending installed-font read cannot commit during finish IPC', async (page) => {
    await page.evaluate(async () => {
      const core = await import('/src/core.js');
      const model = await import('/src/composition-project-model.js');
      const doc = core.document(),
        text = core.node('Text');
      text.text = 'A';
      doc.layers[0].nodes.push(text);
      window.textNode = text.id;
      window.nect.load(JSON.stringify(model.migrateLegacyDocumentToCompositionProject(doc)));
    });
    const textId = await page.evaluate(() => window.textNode);
    await page.locator(`.node[data-id="${textId}"] .node-head strong`).click();
    await ensureWorkspacePanel(page, 'inspector');
    await page.locator('.inspector-body [data-change-font]').click();
    await page.locator('.search-panel input[type="search"]').fill('Fixture font');
    await page.locator('.search-panel .picker-option').click();
    await page.locator('.search-panel .picker-apply').click();
    await page.waitForFunction(() => typeof window.releaseFont === 'function');
    await page.evaluate(() => {
      window.holdFinish = true;
      window.projectBeforeFinish = JSON.stringify(window.nect.projectSnapshot());
    });
    await closeRequest(page);
    await page.waitForFunction(() => window.closeCalls === 1);
    await page.evaluate(() => window.releaseFont());
    await page.waitForFunction(() => window.fontReadCompleted);
    await page.waitForFunction(() =>
      /font.*(reject|error|not applied)|finish or cancel/i.test(document.body.textContent),
    );
    assert.equal(
      await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
      await page.evaluate(() => window.projectBeforeFinish),
    );
    await page.evaluate(() => window.releaseFinish());
    await page.waitForFunction(() => window.closeDone);
  });
} finally {
  await browser.close();
}
