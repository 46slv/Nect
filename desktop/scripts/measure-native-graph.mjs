import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { resolve } from 'node:path';
import { performance } from 'node:perf_hooks';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from '../tests/fixtures/graph-performance.js';

const output = resolve(
  process.env.NECT_PERF_OUTPUT || 'test-results/native-graph-performance/before',
);
const sizes = process.env.NECT_PERF_SIZES
  ? process.env.NECT_PERF_SIZES.split(',').map(Number)
  : [16, 160];
const kinds = process.env.NECT_PERF_ONLY_IDLE
  ? ['idle']
  : process.env.NECT_PERF_OPERATIONS?.split(',') || ['idle', 'pan', 'zoom', 'node-drag'];
const traceKind = process.env.NECT_PERF_TRACE_OPERATION || 'node-drag';
assert.ok(sizes.length && sizes.every((n) => [16, 160].includes(n)));
assert.ok(kinds.length && kinds.every((k) => ['idle', 'pan', 'zoom', 'node-drag'].includes(k)));
assert.ok(['zoom', 'node-drag'].includes(traceKind));
await fs.mkdir(output, { recursive: true });
const b = await chromium.connectOverCDP(process.env.NECT_PERF_CDP_URL || 'http://127.0.0.1:9454');
const summary = {
  at: new Date().toISOString(),
  input: 'synthetic CDP in native Tauri/WebView2',
  phases: [],
  errors: [],
};
const pause = (ms) => new Promise((r) => setTimeout(r, ms));
const pct = (values, p) => {
  const s = [...values].sort((a, b) => a - b);
  return s.length ? s[Math.min(s.length - 1, Math.ceil(s.length * p) - 1)] : null;
};
let page;
try {
  const context = b.contexts()[0];
  if (!context.pages().length) await context.waitForEvent('page', { timeout: 30000 });
  await Promise.any(
    context.pages().map((p) => p.waitForURL(/tauri\.localhost/, { timeout: 30000 })),
  );
  const pages = context.pages().filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  page.on('pageerror', (e) => summary.errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  try {
    await page.locator('[data-recovery-dialog]').waitFor({ state: 'visible', timeout: 500 });
    await page
      .locator('[data-recovery-dialog]')
      .getByRole('button', { name: 'Later', exact: true })
      .click();
  } catch (e) {
    if (e.name !== 'TimeoutError') throw e;
  }
  const root = await b.newBrowserCDPSession(),
    cdp = await context.newCDPSession(page);
  const cpu = async () => {
    try {
      return (await root.send('SystemInfo.getProcessInfo')).processInfo;
    } catch (e) {
      return { unavailable: e.message };
    }
  };
  summary.cpuProvider =
    'CDP SystemInfo actual cumulative process cpuTime seconds; browser/renderer/GPU/services in this dedicated WebView2 browser';
  summary.viewport = await page.evaluate(() => ({
    width: innerWidth,
    height: innerHeight,
    dpr: devicePixelRatio,
    visibility: document.visibilityState,
    hardwareConcurrency: navigator.hardwareConcurrency,
  }));
  await cdp.send('Performance.enable');
  const metrics = async () =>
    Object.fromEntries(
      (await cdp.send('Performance.getMetrics')).metrics.map((m) => [m.name, m.value]),
    );
  async function observeStart(trackFrames) {
    await page.evaluate((trackFrames) => {
      const p = (window.__graphMeasure = {
        frames: [],
        longTasks: [],
        appWrites: 0,
        wireWrites: 0,
        graphStyleWrites: 0,
        canvasWrites: 0,
        active: true,
      });
      let last = performance.now();
      function frame(now) {
        if (!p.active) return;
        p.frames.push(now - last);
        last = now;
        p.raf = requestAnimationFrame(frame);
      }
      if (trackFrames) p.raf = requestAnimationFrame(frame);
      p.observers = [];
      for (const [selector, key, options] of [
        ['#app', 'appWrites', { childList: true }],
        ['.wires', 'wireWrites', { childList: true }],
        ['.graph', 'graphStyleWrites', { attributes: true, attributeFilter: ['style'] }],
        ['.artboard', 'canvasWrites', { childList: true }],
      ]) {
        const el = document.querySelector(selector);
        const observer = new MutationObserver((records) => (p[key] += records.length));
        observer.observe(el, options);
        p.observers.push(observer);
      }
      try {
        p.longObserver = new PerformanceObserver((list) =>
          p.longTasks.push(...list.getEntries().map((e) => e.duration)),
        );
        p.longObserver.observe({ entryTypes: ['longtask'] });
      } catch {}
    }, trackFrames);
  }
  async function observeStop() {
    return page.evaluate(() => {
      const p = window.__graphMeasure;
      p.active = false;
      cancelAnimationFrame(p.raf);
      p.observers.forEach((o) => o.disconnect());
      p.longObserver?.disconnect();
      return {
        frames: p.frames,
        longTasks: p.longTasks,
        appWrites: p.appWrites,
        wireWrites: p.wireWrites,
        graphStyleWrites: p.graphStyleWrites,
        canvasWrites: p.canvasWrites,
      };
    });
  }
  async function prepare() {
    await page.locator('[data-action="graph-reset"]').click();
    await pause(150);
  }
  const latencies = [];
  async function moves(x, y, button) {
    await page.mouse.move(x, y);
    await page.mouse.down({ button });
    for (let i = 0; i < 40; i++) {
      const t = performance.now();
      const angle = (i / 39) * Math.PI * 2;
      await page.mouse.move(x + 55 * Math.sin(angle), y + 25 * (1 - Math.cos(angle)), { steps: 1 });
      latencies.push(performance.now() - t);
      await pause(16);
    }
  }
  async function operation(kind, f) {
    const rect = await page.locator('.graph-scroll').boundingBox();
    if (kind === 'idle') {
      await pause(1500);
      return;
    }
    if (kind === 'pan') {
      await page.locator('.graph-scroll').evaluate((el) => {
        el.scrollLeft = 180;
        el.scrollTop = 100;
      });
      await pause(80);
      await moves(rect.x + 12, rect.y + 12, 'middle');
      await page.mouse.up({ button: 'middle' });
      return;
    }
    if (kind === 'zoom') {
      for (let i = 0; i < 12; i++) {
        const t = performance.now();
        await page.locator(`[data-action="graph-zoom-${i % 2 ? 'out' : 'in'}"]`).click();
        latencies.push(performance.now() - t);
        await pause(40);
      }
      return;
    }
    const head = await page
      .locator(`.graph .node[data-id="${f.dragNodeId}"] .type-tag`)
      .boundingBox();
    assert.ok(head);
    await page.keyboard.down('Control');
    await moves(head.x + head.width / 2, head.y + head.height / 2, 'left');
    await page.keyboard.press('Escape');
    await page.mouse.up();
    await page.keyboard.up('Control');
  }
  async function phase(count, kind, f) {
    await prepare();
    latencies.length = 0;
    const graphViewport = await page.evaluate(() => {
      const el = document.querySelector('.graph-scroll');
      return { width: el.clientWidth, height: el.clientHeight };
    });
    assert.ok(graphViewport.width > 0 && graphViewport.height > 0, 'current live Graph viewport');
    const beforeDoc = await page.evaluate(() => JSON.stringify(window.nect.document));
    const c0 = await cpu(),
      m0 = await metrics();
    await observeStart(kind !== 'idle');
    const t0 = performance.now();
    await operation(kind, f);
    await pause(80);
    const wall = (performance.now() - t0) / 1000;
    const observation = await observeStop(),
      m1 = await metrics(),
      c1 = await cpu();
    if (kind === 'zoom') {
      assert.equal(observation.appWrites, 0, 'zoom must not overlap a startup/root rerender');
      assert.equal(
        observation.wireWrites,
        12,
        'observe the same live wire SVG for all zoom clicks',
      );
    }
    assert.equal(
      await page.evaluate(() => JSON.stringify(window.nect.document)),
      beforeDoc,
      'operation/cancel must preserve authored data',
    );
    let cpuResult = { unavailable: c0.unavailable || c1.unavailable };
    if (Array.isArray(c0) && Array.isArray(c1)) {
      const before = new Map(c0.map((p) => [p.id, p]));
      const processes = c1.map((p) => ({
        id: p.id,
        type: p.type,
        cpuSeconds: before.has(p.id) ? p.cpuTime - before.get(p.id).cpuTime : null,
      }));
      cpuResult = {
        processes,
        complete: processes.every((p) => p.cpuSeconds !== null),
        cpuSeconds: processes.reduce((s, p) => s + (p.cpuSeconds || 0), 0),
      };
      cpuResult.oneCorePercent = (100 * cpuResult.cpuSeconds) / wall;
    }
    const deltas = Object.fromEntries(
      [
        'TaskDuration',
        'ScriptDuration',
        'LayoutDuration',
        'RecalcStyleDuration',
        'LayoutCount',
        'RecalcStyleCount',
      ].map((k) => [k, m1[k] - m0[k]]),
    );
    const r = {
      nodes: count,
      connections: f.connectionCount,
      kind,
      graphViewport,
      wallSeconds: wall,
      expanded: true,
      images: 0,
      dithers: 0,
      groups: 0,
      cpu: cpuResult,
      raf: {
        measured: kind !== 'idle',
        count: observation.frames.length,
        p50: pct(observation.frames, 0.5),
        p95: pct(observation.frames, 0.95),
        max: Math.max(...observation.frames),
        over33ms: observation.frames.filter((x) => x > 33.3).length,
      },
      cdpAwait: { count: latencies.length, p50: pct(latencies, 0.5), p95: pct(latencies, 0.95) },
      dom: {
        appWrites: observation.appWrites,
        wireWrites: observation.wireWrites,
        graphStyleWrites: observation.graphStyleWrites,
        canvasWrites: observation.canvasWrites,
      },
      longTasks: observation.longTasks,
      engineMetrics: deltas,
    };
    summary.phases.push(r);
    console.log(JSON.stringify(r));
    await fs.writeFile(resolve(output, 'summary.json'), JSON.stringify(summary, null, 2));
  }
  for (const count of sizes) {
    const f = graphPerformanceFixture(count);
    await fs.writeFile(resolve(output, `scene-${count}.json`), serialize(f.doc));
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    if (await page.locator('.alignment-panel').count())
      await page.locator('.canvas-toolbar [data-action="align-panel"]').click();
    await pause(500);
    assert.equal(await page.locator('.graph .node').count(), count);
    for (const kind of kinds) await phase(count, kind, f);
  }
  // One short diagnostic trace/profile of the last scene, separate from timing phases.
  if (!process.env.NECT_PERF_ONLY_IDLE && !process.env.NECT_PERF_SKIP_TRACE) {
    const f = graphPerformanceFixture(sizes.at(-1)),
      traceName = traceKind === 'zoom' ? 'medium-zoom' : 'medium-drag';
    await prepare();
    await cdp.send('Profiler.enable');
    await cdp.send('Profiler.setSamplingInterval', { interval: 1000 });
    await cdp.send('Profiler.start');
    await root.send('Tracing.start', {
      categories: 'devtools.timeline,v8',
      transferMode: 'ReturnAsStream',
    });
    await operation(traceKind, f);
    const profile = await cdp.send('Profiler.stop');
    await fs.writeFile(resolve(output, `${traceName}.cpuprofile`), JSON.stringify(profile.profile));
    const completion = new Promise((r) => root.once('Tracing.tracingComplete', r));
    await root.send('Tracing.end');
    const { stream } = await completion;
    let trace = '';
    for (;;) {
      const chunk = await root.send('IO.read', { handle: stream });
      trace += chunk.data;
      if (chunk.eof) break;
    }
    await root.send('IO.close', { handle: stream });
    await fs.writeFile(resolve(output, `${traceName}.trace.json`), trace);
    const parsed = JSON.parse(trace),
      events = parsed.traceEvents.filter((e) => e.ph === 'X');
    summary.traceSummary = Object.fromEntries(
      ['Layout', 'UpdateLayoutTree', 'Paint', 'EventDispatch', 'FunctionCall'].map((name) => {
        const es = events.filter((e) => e.name === name);
        return [
          name,
          { count: es.length, totalMs: es.reduce((s, e) => s + (e.dur || 0), 0) / 1000 },
        ];
      }),
    );
    summary.profileFunctions = profile.profile.nodes
      .filter((n) => n.hitCount)
      .sort((a, b) => b.hitCount - a.hitCount)
      .slice(0, 18)
      .map((n) => ({
        name: n.callFrame.functionName,
        hits: n.hitCount,
        url: n.callFrame.url,
        line: n.callFrame.lineNumber,
        column: n.callFrame.columnNumber,
      }));
  }
  summary.finalFileState = await page.evaluate(() => window.nect.fileState);
  await page.screenshot({ path: resolve(output, 'scene.png') });
  assert.deepEqual(summary.errors, []);
  await fs.writeFile(resolve(output, 'summary.json'), JSON.stringify(summary, null, 2));
  console.log(JSON.stringify({ trace: summary.traceSummary, profile: summary.profileFunctions }));
} finally {
  await b.close();
}
