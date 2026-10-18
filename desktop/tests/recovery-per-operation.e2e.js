// Production bundle/CSP. Slow/failing disk is an explicit IPC fixture.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize } from '../src/core.js';
const d = document(),
  l = d.layers[0],
  circle = l.nodes[0],
  text = node('Text', 1200, 30),
  fill = node('Fill', 380, 30),
  out = l.nodes.find((n) => n.id === l.output);
text.text = 'A';
circle.position = [30, 30];
out.position = [730, 30];
fill.params.path.source = circle.id;
out.params.path.source = fill.id;
l.nodes = [circle, fill, out, text];
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
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
const errors = [],
  rows = [];
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  page.setDefaultTimeout(10000);
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript(() => {
    window.isTauri = true;
    window.recoveryWrites = [];
    window.recoveryDurable = {};
    window.diskMode = 'normal';
    window.diskActive = 0;
    window.diskMaxActive = 0;
    let next = 0;
    window.__TAURI_INTERNALS__ = {
      transformCallback: () => 1,
      unregisterCallback: () => {},
      invoke: async (command, args) => {
        if (
          [
            'list_system_fonts',
            'list_os_outline_fonts',
            'list_desktop_font_faces',
            'list_recovery',
          ].includes(command)
        )
          return [];
        if (command === 'enable_close_guard' || command === 'release_project') return null;
        if (command === 'begin_recovery') return `recovery-${++next}`;
        if (command === 'write_recovery') {
          const copy = structuredClone(args),
            mode = window.diskMode;
          window.recoveryWrites.push(copy);
          window.diskActive++;
          window.diskMaxActive = Math.max(window.diskMaxActive, window.diskActive);
          try {
            if (mode === 'hold')
              await new Promise((r) => {
                window.releaseDisk = r;
              });
            if (mode === 'fail') throw Error('Fixture disk write failure');
            window.recoveryDurable[copy.id] = copy;
            return Date.now();
          } finally {
            window.diskActive--;
          }
        }
        throw Error(`Unexpected recovery fixture IPC: ${command}`);
      },
    };
  });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), serialize(d));
  const saved = () =>
    page.waitForFunction(
      () =>
        window.nect.recoveryState.phase === 'saved' &&
        window.nect.recoveryState.savedRevision === window.nect.commandState.expectedRevision,
    );
  const exact = async () => {
    await saved();
    const r = await page.evaluate(() => ({
      doc: window.nect.document,
      state: window.nect.recoveryState,
      disk: Object.values(window.recoveryDurable).at(-1),
    }));
    assert.deepEqual(JSON.parse(r.disk.contents), r.doc);
    assert.equal(r.disk.revision, r.state.savedRevision);
    return r;
  };
  await exact();
  const count = () => page.evaluate(() => window.recoveryWrites.length);
  const select = async (id) => {
    await page.locator('[data-action="graph-fit"]').click();
    await page.locator(`.graph .node[data-id="${id}"] .node-head .type-tag`).click();
  };
  const operation = async (label, action) => {
    const before = await count();
    await action();
    await exact();
    assert.ok((await count()) > before, label);
    rows.push(label);
  };
  await select(circle.id);
  await operation('Inspector parameter', async () => {
    const input = page.locator(
      `.inspector-body .field[data-node="${circle.id}"][data-key="radius"] [data-value][data-axis="0"]`,
    );
    await input.fill('75');
    await input.press('Enter');
  });
  await select(circle.id);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+c');
  await operation('Paste', () => page.keyboard.press('Control+v'));
  await page.locator('.graph-scroll').focus();
  await operation('Delete', () => page.keyboard.press('Delete'));
  await operation('Undo', () => page.keyboard.press('Control+z'));
  await operation('Redo', () => page.keyboard.press('Control+Shift+z'));
  await operation('Node addition', () => page.locator('#add-node').selectOption('Circle'));
  // Actual Canvas pointer preview remains private; release is the single save request.
  await select(circle.id);
  await page.locator('[data-tool="select"]').click();
  const start = await page
    .locator('.artboard .shape')
    .first()
    .evaluate((el) => {
      const p = el.matches('path,circle,rect,polygon')
          ? el
          : el.querySelector('path,circle,rect,polygon'),
        b = p.getBBox(),
        m = p.getScreenCTM();
      for (let y = b.y + 2; y < b.y + b.height; y += 4)
        for (let x = b.x + 2; x < b.x + b.width; x += 4)
          if (p.isPointInFill(new DOMPoint(x, y))) {
            const q = new DOMPoint(x, y).matrixTransform(m);
            if (document.elementFromPoint(q.x, q.y)?.closest('.shape')) return [q.x, q.y];
          }
      throw Error('No unobstructed painted drag point');
    });
  const beforeDrag = await count(),
    beforeRevision = await page.evaluate(() => window.nect.commandState.expectedRevision);
  await page.mouse.move(...start);
  await page.mouse.down();
  await page.mouse.move(start[0] + 20, start[1] + 10, { steps: 3 });
  const dragProbe = await page.evaluate(() => window.nect.gestureState);
  assert.equal(await count(), beforeDrag);
  assert.equal(
    await page.evaluate(() => window.nect.commandState.expectedRevision),
    beforeRevision,
  );
  await page.mouse.up();
  await exact();
  if ((await count()) === beforeDrag) {
    await mkdir('test-results/recovery-per-operation', { recursive: true });
    await writeFile(
      'test-results/recovery-per-operation/drag-probe.json',
      JSON.stringify(
        {
          start,
          dragProbe,
          after: await page.evaluate(() => ({
            gesture: window.nect.gestureState,
            revision: window.nect.commandState.expectedRevision,
          })),
        },
        null,
        2,
      ),
    );
  }
  assert.ok((await count()) > beforeDrag);
  rows.push('Canvas release, no preview writes');
  // Native input/IME drafts are not completed document states.
  await select(text.id);
  const input = page.locator(`.inspector-body textarea[data-text="${text.id}"]`),
    beforeIme = await count();
  await input.evaluate((el) => {
    el.focus();
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    el.value = 'AB';
    el.dispatchEvent(
      new InputEvent('input', {
        bubbles: true,
        inputType: 'insertCompositionText',
        isComposing: true,
      }),
    );
  });
  assert.equal(await count(), beforeIme);
  assert.equal(
    await page.evaluate(
      () => window.nect.document.layers[0].nodes.find((n) => n.type === 'Text').text,
    ),
    'A',
  );
  await input.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: 'B' })),
  );
  assert.equal(await count(), beforeIme);
  await operation('Text commit after IME draft', () => input.press('Control+Enter'));
  // Hold a real request open; later operations remain interactive and coalesce to latest.
  await page.evaluate(() => {
    window.diskMode = 'hold';
  });
  const edit = async (value) =>
    page.evaluate(
      ({ layerId, nodeId, value }) =>
        window.nect.dispatch({
          ...window.nect.commandState,
          type: 'set-parameter',
          payload: { layerId, nodeId, key: 'radius', value },
        }),
      { layerId: l.id, nodeId: circle.id, value },
    );
  await edit(80);
  await page.waitForFunction(() => typeof window.releaseDisk === 'function');
  const inFlight = await page.evaluate(() => structuredClone(window.recoveryWrites.at(-1)));
  await page.evaluate(
    ({ layerId, nodeId }) => {
      for (let value = 81; value <= 90; value++)
        window.nect.dispatch({
          ...window.nect.commandState,
          type: 'set-parameter',
          payload: { layerId, nodeId, key: 'radius', value },
        });
    },
    { layerId: l.id, nodeId: circle.id },
  );
  const queued = await page.evaluate(() => ({
    ...window.nect.recoveryState,
    revision: window.nect.commandState.expectedRevision,
  }));
  assert.ok(queued.savedRevision < queued.revision);
  assert.equal(await page.evaluate(() => window.recoveryWrites.at(-1).revision), inFlight.revision);
  await page.evaluate(() => {
    window.diskMode = 'normal';
    window.releaseDisk();
  });
  await exact();
  assert.equal(await page.evaluate(() => window.diskMaxActive), 1);
  rows.push('Slow disk burst drains latest automatically');
  const durableBefore = await page.evaluate(() => window.nect.recoveryState.savedRevision);
  await page.evaluate(() => {
    window.diskMode = 'fail';
  });
  await edit(91);
  await page.waitForFunction(() => window.nect.recoveryState.phase === 'failed');
  assert.equal(await page.evaluate(() => window.nect.recoveryState.savedRevision), durableBefore);
  assert.match(await page.locator('[data-recovery-status]').textContent(), /failed r/);
  await page.evaluate(() => {
    window.diskMode = 'normal';
  });
  await page.locator('[data-recovery-status]').click();
  await exact();
  rows.push('Write failure and retry');
  assert.deepEqual(errors, []);
  await mkdir('test-results/recovery-per-operation', { recursive: true });
  await page.screenshot({ path: 'test-results/recovery-per-operation/result.png' });
  await writeFile(
    'test-results/recovery-per-operation/receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        ipcWasStubbed: true,
        operations: rows,
        noPreviewOrImeWrites: true,
        allAcknowledgedSnapshotsExact: true,
        globalMaxConcurrentWrites: 1,
        slowDiskLatestDrained: true,
        failedRevisionPreservedPreviousAck: true,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify(rows));
} finally {
  if (browser) await browser.close();
  await new Promise((r) => server.close(r));
}
