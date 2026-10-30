// Run against `npm run dev`: BASE_URL=http://127.0.0.1:5173 node tests/asset-drag.e2e.js
// Fixtures use the browser's real directory picker; placement uses native HTML drag events.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
const out = resolve('test-results/asset-drag');
const folder = resolve(out, 'fixtures');
await mkdir(folder, { recursive: true });
await writeFile(
  resolve(folder, 'drag-mark.svg'),
  '<svg width="40" height="20" viewBox="0 0 40 20"><rect width="40" height="20" fill="#2468ab"/></svg>',
);
await writeFile(
  resolve(folder, 'drag-pixel.png'),
  Buffer.from(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
    'base64',
  ),
);

let browser;
const results = { passed: false, routes: [], guards: [], errors: [] };
try {
  browser = await chromium.launch({
    executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
    headless: true,
    args: ['--no-sandbox', '--disable-dev-shm-usage'],
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  page.setDefaultTimeout(20000);
  page.on('pageerror', (error) => results.errors.push(error.message));
  page.on('dialog', (dialog) => dialog.accept());
  // Observation only: no synthetic drag/drop dispatch and no replacement application callbacks.
  await page.addInitScript(() => {
    window.assetDragEvents = [];
    for (const type of ['dragstart', 'drop', 'dragend'])
      document.addEventListener(
        type,
        (event) => {
          const card = event.target.closest?.('[data-asset-library] [data-path]');
          if (type !== 'drop' && !card) return;
          const graph = document.querySelector('.graph');
          const svg = document.querySelector('.artboard');
          const r = graph?.getBoundingClientRect();
          const scale = graph && new DOMMatrix(getComputedStyle(graph).transform).a;
          const canvas =
            svg &&
            new DOMPoint(event.clientX, event.clientY).matrixTransform(
              svg.getScreenCTM().inverse(),
            );
          window.assetDragEvents.push({
            type,
            trusted: event.isTrusted,
            path: card?.dataset.path,
            route: event.target.closest?.('.graph-scroll')
              ? 'graph'
              : event.target.closest?.('.canvas-stage')
                ? 'layer'
                : null,
            types: [...(event.dataTransfer?.types || [])],
            effect: event.dataTransfer?.dropEffect,
            graph: r && [(event.clientX - r.x) / scale, (event.clientY - r.y) / scale],
            canvas: canvas && [canvas.x, canvas.y],
          });
        },
        type === 'drop',
      );
  });
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);

  const panel = page.locator('[data-asset-library]');
  const status = panel.locator('[data-asset-status]');
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const model = async () => JSON.parse(await saved());
  const events = () => page.evaluate(() => window.assetDragEvents);
  const state = () =>
    page.evaluate(() => ({
      document: JSON.stringify(window.nect.document),
      project: JSON.stringify(window.nect.projectSnapshot()),
      command: window.nect.commandState,
      undoDisabled: document.querySelector('[data-action="undo"]').disabled,
      redoDisabled: document.querySelector('[data-action="redo"]').disabled,
    }));
  const closeTo = (actual, expected, label, tolerance = 0.05) => {
    assert.equal(actual.length, expected.length, label);
    actual.forEach((value, i) =>
      assert.ok(
        Math.abs(value - expected[i]) <= tolerance,
        `${label}[${i}]: ${value} != ${expected[i]}`,
      ),
    );
  };
  const ensurePanel = async () => {
    if (!(await panel.count()))
      await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
    await panel.waitFor({ state: 'visible' });
  };
  const reset = async () => {
    const doc = blankDocument();
    doc.name = 'Asset drag fixture';
    doc.layers[0].name = 'Drag destination';
    doc.layers.push(blankLayer('Other destination'));
    for (const layer of doc.layers) {
      layer.nodes[0].position = [560, 35];
      layer.nodes[0].collapsed = true;
    }
    await page.evaluate((text) => window.nect.load(text), serialize(doc));
    await ensurePanel();
    await page.locator('[data-action="graph-reset"]').click();
    await page.locator('[data-action="fit"]').click();
    assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
    return model();
  };
  await reset();
  const chooser = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
  await (await chooser).setFiles(folder);
  await page.waitForFunction(() =>
    ['drag-mark.svg', 'drag-pixel.png'].every((path) => {
      const card = [...document.querySelectorAll('[data-asset-library] [data-path]')].find(
        (el) => el.dataset.path === path,
      );
      return (
        card?.draggable &&
        card.querySelector('img')?.complete &&
        card.querySelector('img')?.naturalWidth
      );
    }),
  );

  const card = (name) => panel.getByRole('button', { name, exact: true });
  const targetPoint = async (route) => {
    const locator = page.locator(route === 'graph' ? '.graph-scroll' : '.canvas-stage');
    const box = await locator.boundingBox();
    assert.ok(box && box.width > 100 && box.height > 60, `${route} target is visible`);
    const point = [box.x + box.width * 0.38, box.y + box.height * 0.62];
    assert.equal(
      await page.evaluate(
        ({ point, selector }) => !!document.elementFromPoint(...point)?.closest(selector),
        { point, selector: route === 'graph' ? '.graph-scroll' : '.canvas-stage' },
      ),
      true,
      `${route} target is not covered by the asset panel`,
    );
    return point;
  };
  const startDrag = async (name) => {
    await ensurePanel();
    const from = card(name);
    await page.waitForFunction((name) => {
      const card = [...document.querySelectorAll('[data-asset-library] [data-path]')].find(
        (el) => el.dataset.path === name,
      );
      return card?.draggable && !card.closest('[inert]');
    }, name);
    await from.scrollIntoViewIfNeeded();
    assert.equal(await from.getAttribute('draggable'), 'true');
    const box = await from.boundingBox();
    const before = (await events()).length;
    const point = [box.x + box.width / 2, box.y + box.height / 2];
    await page.mouse.move(...point);
    await page.mouse.down();
    await page.mouse.move(point[0] + 14, point[1], { steps: 5 });
    await page.waitForFunction(
      (before) => window.assetDragEvents.slice(before).some((e) => e.type === 'dragstart'),
      before,
    );
    const start = (await events()).slice(before).find((e) => e.type === 'dragstart');
    assert.equal(start.trusted, true, 'browser must initiate the actual HTML drag');
    assert.equal(start.path, name);
    assert.ok(start.types.includes('application/x-nect-library-ticket'));
    await page.waitForFunction(() => document.querySelector('[data-asset-library]')?.style.opacity === '0');
    return before;
  };
  const finishDrag = async (point) => {
    await page.mouse.move(...point, { steps: 12 });
    // Chromium requires a second move over the destination before native drop.
    await page.mouse.move(...point);
    await page.mouse.up();
    await page.waitForFunction(() => document.querySelector('[data-asset-library]')?.style.opacity !== '0');
  };
  const dropAsset = async (name, route) => {
    const point = await targetPoint(route);
    const before = await startDrag(name);
    await finishDrag(point);
    await page.waitForFunction(
      (before) => window.assetDragEvents.slice(before).some((e) => e.type === 'drop'),
      before,
    );
    const dropped = (await events()).slice(before).find((e) => e.type === 'drop');
    assert.equal(dropped.trusted, true);
    assert.equal(dropped.route, route);
    return dropped;
  };
  const added = () =>
    page.waitForFunction(
      () => document.querySelector('[data-asset-status]')?.textContent === 'Asset added',
    );
  const undoRedo = async (before, after) => {
    await activateCommand(page, 'undo');
    assert.equal(await saved(), JSON.stringify(before), 'one Undo restores every authored change');
    assert.equal(
      await page.locator('[data-action="undo"]').isDisabled(),
      true,
      'exactly one history entry',
    );
    await activateCommand(page, 'redo');
    assert.equal(
      await saved(),
      JSON.stringify(after),
      'one Redo restores source, asset and wiring',
    );
  };

  // Cancel a native drag after retreat: the same live panel and search return,
  // with no document or history effect.
  await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('drag');
  const identity = await panel.elementHandle();
  const beforeCancel = await state();
  await startDrag('drag-mark.svg');
  await page.keyboard.press('Escape');
  await page.mouse.up();
  await page.waitForFunction(() => document.querySelector('[data-asset-library]')?.style.opacity !== '0');
  assert.equal(await page.evaluate((node) => node === document.querySelector('[data-asset-library]'), identity), true);
  assert.equal(await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(), 'drag');
  assert.deepEqual(await state(), beforeCancel, 'canceled drag leaves document and history unchanged');

  for (const name of ['drag-mark.svg', 'drag-pixel.png'])
    for (const route of ['graph', 'layer']) {
      const before = await reset();
      const isSVG = name.endsWith('.svg');
      await card(name).click();
      await panel.locator('[data-asset-place-layer]:enabled').waitFor();
      await panel.getByLabel('Asset license', { exact: true }).fill('Self-authored drag fixture');
      if (isSVG) await panel.getByLabel('Join before Out', { exact: true }).check();
      if (route === 'graph') {
        await page.locator('[data-action="graph-zoom-out"]').click();
        await page.locator('.graph-scroll').evaluate((el) => el.scrollTo(110, 70));
      }
      const originalExport = await page.evaluate(() => window.nect.export());
      const dropped = await dropAsset(name, route);
      await added();
      const after = await model();
      const type = isSVG ? 'SVG' : 'Image';
      const target = after.layers.find((layer) => layer.nodes.some((node) => node.type === type));
      const node = target.nodes.find((node) => node.type === type);
      const size = isSVG ? [40, 20] : [1, 1];
      const center = route === 'graph' ? [before.width / 2, before.height / 2] : dropped.canvas;
      closeTo(
        node.params.position.value,
        center.map((v, i) => v - size[i] / 2),
        `${name} artwork coordinates`,
      );
      const provenance = isSVG ? node.svgAsset : after.imageAssets[node.asset].provenance;
      assert.equal(provenance.relativePath, name);
      assert.equal(provenance.license, 'Self-authored drag fixture');
      if (!isSVG) {
        assert.equal(after.imageAssets[node.asset].mime, 'image/png');
        assert.equal(after.imageAssets[node.asset].width, 1);
        assert.equal(after.imageAssets[node.asset].height, 1);
      }
      const visible = after.layers.filter((layer) => layer.visible);
      if (route === 'graph') {
        assert.equal(target.id, before.layers[0].id);
        assert.equal(visible.length, before.layers.filter((layer) => layer.visible).length);
        assert.equal(target.output, before.layers[0].output);
        assert.deepEqual(
          target.nodes.filter((item) => item.id !== node.id),
          before.layers[0].nodes,
          'drop does not connect or change any existing node, even with Join before Out checked',
        );
        closeTo(node.position, dropped.graph, `${name} graph coordinates`);
        assert.equal(
          await page.evaluate(() => window.nect.export()),
          originalExport,
          'disconnected drop does not change artwork',
        );
      } else {
        assert.equal(visible.length, before.layers.filter((layer) => layer.visible).length + 1);
        assert.ok(!before.layers.some((layer) => layer.id === target.id));
        assert.deepEqual(
          after.layers.filter((layer) => before.layers.some((old) => old.id === layer.id)),
          before.layers,
        );
        const output = target.nodes.find((item) => item.id === target.output);
        assert.equal(output.type, isSVG ? 'Output' : 'Image Output');
        assert.ok(output.params.path.source, 'new layer has a connected output');
        const shape = page.locator(`.artboard .shape[data-layer="${target.id}"]`);
        await shape.waitFor();
        const bounds = await shape.evaluate((el) => {
          const box = el.getBoundingClientRect();
          const inverse = document.querySelector('.artboard').getScreenCTM().inverse();
          const a = new DOMPoint(box.left, box.top).matrixTransform(inverse);
          const b = new DOMPoint(box.right, box.bottom).matrixTransform(inverse);
          return [a.x, a.y, b.x - a.x, b.y - a.y];
        });
        closeTo(
          bounds,
          [...node.params.position.value, ...size],
          `${name} rendered artwork bounds`,
          0.1,
        );
      }
      const displayed = page.locator(`.graph .node[data-id="${node.id}"]`);
      await displayed.waitFor();
      closeTo(
        await displayed.evaluate((el) => [parseFloat(el.style.left), parseFloat(el.style.top)]),
        node.position,
        'rendered graph card matches authored coordinates',
      );
      await page.screenshot({ path: resolve(out, `${isSVG ? 'svg' : 'png'}-${route}.png`) });
      await undoRedo(before, after);
      results.routes.push({ name, route, nodeId: node.id, oneUndoRedo: true });
    }

  // Start with a real redo branch. Canceled/unsupported drops must preserve it.
  await reset();
  await dropAsset('drag-mark.svg', 'graph');
  await added();
  await activateCommand(page, 'undo');
  assert.equal(await page.locator('[data-action="redo"]').isDisabled(), false);
  for (const reason of ['unsupported-target', 'Escape']) {
    const before = await state();
    const eventIndex = await startDrag('drag-mark.svg');
    if (reason === 'Escape') {
      await page.mouse.move(...(await targetPoint('graph')), { steps: 8 });
      await page.keyboard.press('Escape');
      await page.mouse.up();
    } else {
      const box = await page.locator('.layers .panel-head').boundingBox();
      await finishDrag([box.x + 35, box.y + box.height / 2]);
    }
    await page.waitForFunction(
      (before) => window.assetDragEvents.slice(before).some((e) => e.type === 'dragend'),
      eventIndex,
    );
    assert.deepEqual(
      await state(),
      before,
      `${reason} changes neither model, revision nor undo/redo`,
    );
    assert.ok(!(await events()).slice(eventIndex).some((e) => e.type === 'drop' && e.route));
    results.guards.push(reason);
  }

  // Delay the real File.arrayBuffer result, not insert/capture callbacks or asset bytes.
  const gateRead = () =>
    page.evaluate(() => {
      const original = File.prototype.arrayBuffer;
      window.assetDragReadGate = { original, started: false };
      File.prototype.arrayBuffer = function () {
        const bytes = original.call(this);
        if (this.name !== 'drag-mark.svg') return bytes;
        return new Promise((resolve, reject) => {
          window.assetDragReadGate.started = true;
          window.assetDragReadGate.release = () => bytes.then(resolve, reject);
        });
      };
    });
  const releaseRead = () =>
    page.evaluate(async () => {
      const gate = window.assetDragReadGate;
      File.prototype.arrayBuffer = gate.original;
      await gate.release();
      delete window.assetDragReadGate;
    });
  for (const reason of ['stale-document', 'stale-selection', 'closed-panel']) {
    const fixture = await reset();
    await gateRead();
    try {
      await dropAsset('drag-mark.svg', 'graph');
      await page.waitForFunction(() => window.assetDragReadGate.started);
      assert.equal(await status.textContent(), 'Reading asset…');
      if (reason === 'stale-document') {
        const replacement = blankDocument();
        replacement.name = 'Newer document must survive';
        await page.evaluate((text) => window.nect.load(text), serialize(replacement));
      } else if (reason === 'stale-selection') {
        await page.locator(`[data-select-layer="${fixture.layers[1].id}"]`).click();
        assert.equal(await page.locator('.graph-layer-name').textContent(), 'Other destination');
        assert.equal(
          await saved(),
          JSON.stringify(fixture),
          'layer selection is not an authored edit',
        );
      } else await panel.getByRole('button', { name: 'Close assets', exact: true }).click();
      const protectedState = await state();
      await releaseRead();
      if (reason === 'closed-panel') {
        await panel.waitFor({ state: 'detached' });
        // A closed panel has no status node to signal its async completion.
        await page.waitForTimeout(200);
      } else {
        await page.waitForFunction(() =>
          document.querySelector('[data-asset-status]')?.textContent.includes('Document changed'),
        );
        assert.equal(await status.getAttribute('data-error'), 'true');
      }
      assert.deepEqual(await state(), protectedState, `${reason} rejects the entire pending drop`);
      results.guards.push(reason);
    } finally {
      await page.evaluate(async () => {
        if (!window.assetDragReadGate) return;
        const gate = window.assetDragReadGate;
        File.prototype.arrayBuffer = gate.original;
        await gate.release?.();
        delete window.assetDragReadGate;
      });
    }
  }
  // A rejected/canceled ticket must not poison a later real placement.
  const retryBefore = await reset();
  await dropAsset('drag-pixel.png', 'graph');
  await added();
  const retryAfter = await model();
  assert.equal(retryAfter.layers[0].nodes.filter((node) => node.type === 'Image').length, 1);
  await undoRedo(retryBefore, retryAfter);
  results.guards.push('fresh-drop-after-rejection');
  assert.deepEqual(results.errors, []);
  results.passed = true;
  console.log(
    'PASS asset drag: native SVG/PNG graph + artboard drops, coordinates, disconnected graph, new layers, one Undo/Redo, cancellation and stale guards',
  );
} catch (error) {
  results.failure = error.stack || error.message;
  throw error;
} finally {
  await writeFile(resolve(out, 'results.json'), JSON.stringify(results, null, 2));
  await browser?.close();
}
