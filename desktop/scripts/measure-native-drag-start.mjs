import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from '../tests/fixtures/graph-performance.js';

assert.ok(process.env.NECT_PERF_CDP_URL && process.env.NECT_DRAG_OUTPUT);
const output = resolve(process.env.NECT_DRAG_OUTPUT);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(process.env.NECT_PERF_CDP_URL);
const context = browser.contexts()[0];
if (!context.pages().length) await context.waitForEvent('page', { timeout: 30000 });
await Promise.any(context.pages().map((p) => p.waitForURL(/tauri\.localhost/, { timeout: 30000 })));
const pages = context.pages().filter((p) => /tauri\.localhost/.test(p.url()));
assert.equal(pages.length, 1);
const [page] = pages,
  root = await browser.newBrowserCDPSession(),
  cdp = await context.newCDPSession(page);
const receipt = {
  at: new Date().toISOString(),
  input: 'synthetic CDP in native Tauri/WebView2',
  trials: [],
  errors: [],
};
page.on('pageerror', (e) => receipt.errors.push(e.message));
const save = () => fs.writeFile(resolve(output, 'summary.json'), JSON.stringify(receipt, null, 2));
const pause = (ms) => page.waitForTimeout(ms);
const cpu = async () => (await root.send('SystemInfo.getProcessInfo')).processInfo;
const metrics = async () =>
  Object.fromEntries(
    (await cdp.send('Performance.getMetrics')).metrics.map((m) => [m.name, m.value]),
  );
const snapshot = () =>
  page.evaluate(() => ({
    json: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    undo: document.querySelector('[data-action="undo"]').disabled,
  }));
const selection = () =>
  page.locator('.graph .node.selected').evaluateAll((els) => els.map((e) => e.dataset.id));
async function stage(trial, kind, operation) {
  const label = `drag-trial-${trial.index}-${kind}`;
  const c0 = await cpu(),
    m0 = await metrics();
  await page.evaluate((label) => performance.mark(label + '-start'), label);
  const t0 = performance.now();
  await operation();
  const cdpAwaitMs = performance.now() - t0;
  await page.evaluate((label) => performance.mark(label + '-end'), label);
  const m1 = await metrics(),
    c1 = await cpu(),
    old = new Map(c0.map((p) => [p.id, p.cpuTime]));
  assert.ok(
    c1.every((p) => old.has(p.id)),
    'stable dedicated CPU process set',
  );
  trial.stages.push({
    kind,
    label,
    cdpAwaitMs,
    cpuSeconds: c1.reduce((s, p) => s + p.cpuTime - old.get(p.id), 0),
    engineSeconds: Object.fromEntries(
      ['TaskDuration', 'ScriptDuration', 'LayoutDuration', 'RecalcStyleDuration'].map((k) => [
        k,
        m1[k] - m0[k],
      ]),
    ),
    rootWrites: await page.evaluate(() => window.__dragStartProbe.rootWrites),
  });
}
try {
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  if (await page.locator('[data-recovery-dialog]').count())
    await page
      .locator('[data-recovery-dialog]')
      .getByRole('button', { name: 'Later', exact: true })
      .click();
  // Initial bundled/system-font callbacks can rerender the root. This is outside
  // the measured gesture, and start stages below additionally reject root writes.
  await pause(2000);
  const f = graphPerformanceFixture(160);
  await fs.writeFile(resolve(output, 'scene-160.json'), serialize(f.doc));
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  if (await page.locator('.alignment-panel').count())
    await page.locator('.canvas-toolbar [data-action="align-panel"]').click();
  await page.locator('[data-action="graph-reset"]').click();
  const head = (id) => page.locator(`.graph .node[data-id="${id}"] .type-tag`);
  await head(f.dragNodeId).click();
  await pause(1000);
  receipt.viewport = await page.evaluate(() => ({
    width: innerWidth,
    height: innerHeight,
    dpr: devicePixelRatio,
    graphWidth: document.querySelector('.graph-scroll').clientWidth,
    graphHeight: document.querySelector('.graph-scroll').clientHeight,
  }));
  assert.deepEqual(receipt.viewport, {
    width: 1469,
    height: 827,
    dpr: 1.090000033378601,
    graphWidth: 1132,
    graphHeight: 201,
  });
  const before = await snapshot();
  await page.evaluate(() => {
    const p = (window.__dragStartProbe = { rootWrites: 0, longTasks: [] });
    p.observer = new MutationObserver((entries) => (p.rootWrites += entries.length));
    p.observer.observe(document.querySelector('#app'), { childList: true });
    p.longObserver = new PerformanceObserver((list) =>
      p.longTasks.push(
        ...list.getEntries().map((e) => ({ startTime: e.startTime, duration: e.duration })),
      ),
    );
    p.longObserver.observe({ entryTypes: ['longtask'] });
  });
  await cdp.send('Performance.enable');
  await cdp.send('Profiler.enable');
  await cdp.send('Profiler.setSamplingInterval', { interval: 1000 });
  await cdp.send('Profiler.start');
  await root.send('Tracing.start', {
    categories: 'devtools.timeline,v8,blink.user_timing',
    transferMode: 'ReturnAsStream',
  });
  for (const [index, id] of [f.dragNodeId, f.doc.layers[0].nodes[1].id].entries()) {
    const trial = {
      index,
      nodeId: id,
      previouslySelected: (await selection()).includes(id),
      stages: [],
    };
    await page.evaluate(() => (window.__dragStartProbe.rootWrites = 0));
    const box = await head(id).boundingBox(),
      x = box.x + box.width / 2,
      y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.keyboard.down('Control');
    await stage(trial, 'down', () => page.mouse.down());
    await stage(trial, 'first-move', () => page.mouse.move(x + 12, y + 8));
    await stage(trial, 'second-move', () => page.mouse.move(x + 24, y + 16));
    assert.equal(trial.stages.at(-1).rootWrites, 0, 'start must not overlap startup/root rerender');
    assert.equal((await snapshot()).json, before.json, 'draft positions remain unauthored');
    await stage(trial, 'escape', () => page.keyboard.press('Escape'));
    await page.mouse.up();
    await page.keyboard.up('Control');
    await pause(200);
    assert.deepEqual(await snapshot(), before, 'Cancel preserves exact JSON/SVG and Undo state');
    assert.equal(await page.locator('.wire').count(), f.connectionCount);
    receipt.trials.push(trial);
    await save();
  }
  const profile = (await cdp.send('Profiler.stop')).profile;
  await fs.writeFile(resolve(output, 'drag-stages.cpuprofile'), JSON.stringify(profile));
  const done = new Promise((r) => root.once('Tracing.tracingComplete', r));
  await root.send('Tracing.end');
  const { stream } = await done;
  let data = '';
  for (;;) {
    const chunk = await root.send('IO.read', { handle: stream });
    data += chunk.data;
    if (chunk.eof) break;
  }
  await root.send('IO.close', { handle: stream });
  await fs.writeFile(resolve(output, 'drag-stages.trace.json'), data);
  const events = JSON.parse(data).traceEvents;
  for (const trial of receipt.trials)
    for (const stage of trial.stages) {
      const start = events.find((e) => e.name === stage.label + '-start')?.ts,
        end = events.find((e) => e.name === stage.label + '-end')?.ts;
      assert.ok(start && end > start, 'stage trace marks');
      const dispatches = events.filter(
        (e) => e.name === 'EventDispatch' && e.ph === 'X' && e.ts >= start && e.ts < end,
      );
      stage.otherDispatchCount = dispatches.filter(
        (e) => !['pointerdown', 'pointermove', 'keydown', 'keyup'].includes(e.args?.data?.type),
      ).length;
      stage.dispatches = dispatches
        .filter((e) =>
          ['pointerdown', 'pointermove', 'keydown', 'keyup'].includes(e.args?.data?.type),
        )
        .map((e) => ({
          type: e.args?.data?.type,
          durationMs: e.dur / 1000,
          layoutStyleMs: events
            .filter(
              (x) =>
                x.ph === 'X' &&
                ['Layout', 'UpdateLayoutTree'].includes(x.name) &&
                x.pid === e.pid &&
                x.tid === e.tid &&
                x.ts >= e.ts &&
                x.ts < e.ts + e.dur,
            )
            .reduce((s, x) => s + x.dur / 1000, 0),
        }));
    }
  receipt.longTasks = await page.evaluate(() => window.__dragStartProbe.longTasks);
  receipt.cpuProcesses = await cpu();
  assert.deepEqual(receipt.errors, []);
  await page.screenshot({ path: resolve(output, 'scene.png') });
  await save();
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  // Also stop bounded diagnostics after an assertion/connection error. A failed
  // probe must not leave tracing running in the task-owned native process.
  await cdp.send('Profiler.stop').catch(() => {});
  await root.send('Tracing.end').catch(() => {});
  await page
    .evaluate(() => {
      window.__dragStartProbe?.observer.disconnect();
      window.__dragStartProbe?.longObserver.disconnect();
    })
    .catch(() => {});
  await browser.close();
}
