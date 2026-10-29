// Evidence capture only: geometry is reported, never judged against a proposed redesign.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';

// Top-level PNG and workflow-*.json match existing CI artifact globs unchanged.
const out = resolve('test-results');
await mkdir(out, { recursive: true });
const fixture = blankDocument(),
  layer = fixture.layers[0];
fixture.width = 320;
fixture.height = 240;
layer.name = 'UI review composition';
const source = node('Curve', 40, 40),
  transform = node('Transform', 40, 340),
  repeat = node('Geometry Repeater', 340, 40),
  fill = node('Fill', 640, 40),
  output = node('Output', 940, 40);
source.name = 'Review source curve';
repeat.name = 'Review repeated geometry';
transform.name = 'Review source transform';
repeat.operation = 'ae-2d';
repeat.params.count.value = 3;
repeat.params.position.value = [20, 0];
for (const n of [source, transform, fill, output]) n.collapsed = true;
repeat.collapsed = false;
layer.nodes = [source, transform, repeat, fill, output];
layer.output = output.id;
connect(layer, source.id, transform.id, 'path');
connect(layer, transform.id, repeat.id, 'path');
connect(layer, repeat.id, fill.id, 'path');
connect(layer, fill.id, output.id, 'path');
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(30000);
const errors = [],
  captures = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const encoded = async () => JSON.stringify(await snapshot());
const card = () => page.locator(`.graph .node[data-id="${repeat.id}"]`);
const capture = async (name, action, state) => {
  await page.evaluate(async () => {
    await document.fonts.ready;
    await new Promise(requestAnimationFrame);
  });
  const geometry = await page.evaluate(() => {
    const rect = (el) => {
      const { x, y, width, height } = el.getBoundingClientRect();
      return { x, y, width, height };
    };
    const measure = (el) => ({
      tag: el.tagName,
      text: el.textContent.trim().slice(0, 180),
      bounds: rect(el),
      scrollTop: el.scrollTop,
      scrollLeft: el.scrollLeft,
      clientHeight: el.clientHeight,
      scrollHeight: el.scrollHeight,
      clientWidth: el.clientWidth,
      scrollWidth: el.scrollWidth,
      clippingAncestors: [
        ...(function* () {
          for (let p = el.parentElement; p; p = p.parentElement) {
            const s = getComputedStyle(p);
            if (/(auto|scroll|hidden|clip)/.test(`${s.overflowX} ${s.overflowY}`))
              yield {
                className: p.className,
                bounds: rect(p),
                scrollTop: p.scrollTop,
                clientHeight: p.clientHeight,
                scrollHeight: p.scrollHeight,
              };
          }
        })(),
      ],
    });
    const active = document.activeElement;
    return {
      viewport: { width: innerWidth, height: innerHeight },
      selectedGraphNodes: [...document.querySelectorAll('.graph .node.selected')].map(
        (el) => el.dataset.id,
      ),
      focus: active
        ? {
            tag: active.tagName,
            text: active.textContent.slice(0, 100),
            selectionStart: active.selectionStart ?? null,
            selectionEnd: active.selectionEnd ?? null,
            attributes: [...active.attributes].map((a) => [a.name, a.value]),
          }
        : null,
      panels: [...document.querySelectorAll('.graph-scroll, .inspector, .shared-colors-panel')].map(
        measure,
      ),
      hierarchy: [...document.querySelectorAll('.inspector-body > h3, .source-navigation')].map(
        measure,
      ),
      actions: [
        ...document.querySelectorAll(
          '.graph .node.selected .node-actions button, [data-chroma-preview], [data-chroma-apply], [data-chroma-cancel]',
        ),
      ].map(measure),
    };
  });
  const file = `ui-review-${name}.png`;
  await page.screenshot({ path: resolve(out, file), fullPage: false });
  captures.push({ file, action, state, ...geometry });
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((text) => window.nect.load(text), serialize(fixture));
  const original = await encoded();
  for (const viewport of [
    { width: 1600, height: 1000 },
    { width: 900, height: 720 },
  ]) {
    await page.setViewportSize(viewport);
    await card().locator('.node-head strong').click();
    await card().locator('.node-head strong').focus();
    assert.equal(await page.locator('.inspector-body > h3').innerText(), repeat.name);
    assert.equal(await card().locator('.node-actions button').count(), 3);
    await capture(
      `node-top-${viewport.width}`,
      'Select expanded Geometry Repeater in Node Graph; focus title',
      'Unmodified scene; node settings top',
    );
    await card().locator('[data-duplicate]').scrollIntoViewIfNeeded();
    await capture(
      `node-actions-${viewport.width}`,
      'Scroll real graph to Duplicate action without invoking it',
      'Same selected node; action row reached',
    );
    assert.equal(await encoded(), original);
  }
  await page.setViewportSize({ width: 1600, height: 1000 });
  // Source search deliberately supports Transform, not Geometry Repeater.
  // Select a production-supported route rather than inventing a candidate.
  await page.locator(`.graph .node[data-id="${transform.id}"] .node-head strong`).click();
  await page.locator('[data-source-nav="open"]').click();
  await capture(
    'source-candidates',
    'Open Edit source search from selected Transform',
    'Source candidates; selected node identity retained',
  );
  await page.locator(`[data-source-nav="enter"][data-source-id="${source.id}"]`).click();
  assert.equal(await page.locator('.inspector-body > h3').innerText(), source.name);
  await capture(
    'source-entered',
    'Enter original Curve through source candidate button',
    'Source editing heading receives existing keyboard focus',
  );
  await page.locator('[data-source-nav="back"]').click();
  assert.equal(await page.locator('.inspector-body > h3').innerText(), transform.name);
  await capture(
    'source-return',
    'Back restores selected Transform and graph view',
    'Existing source-heading focus retained',
  );
  assert.equal(await encoded(), original);

  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  await page.locator('[data-harmony-create]').click();
  const saved = await encoded();
  assert.equal((await snapshot()).colorDesign.palette.length, 5);
  for (const viewport of [
    { width: 1600, height: 1000 },
    { width: 900, height: 720 },
  ]) {
    await page.setViewportSize(viewport);
    await page.locator('.shared-colors-panel').evaluate((el) => {
      el.scrollTop = 0;
    });
    await capture(
      `palette-saved-${viewport.width}`,
      'Open actual Shared Colors harmony panel at top',
      'Saved default five-role palette, no preview',
    );
    const rotation = page.locator('[data-wheel-setting="hueRotation"]');
    await rotation.fill('75');
    await rotation.press('Tab');
    await page.locator('[data-chroma-preview]').click();
    assert.equal(await encoded(), saved, 'Preview does not author project changes');
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 1);
    await page.locator('.shared-colors-panel').evaluate((el) => {
      el.scrollTop = 0;
    });
    await capture(
      `palette-preview-top-${viewport.width}`,
      'Preview hue rotation 75; scroll panel to top',
      'Saved palette plus non-authored proposed draft',
    );
    await page.locator('[data-chroma-apply]').scrollIntoViewIfNeeded();
    await capture(
      `palette-preview-actions-${viewport.width}`,
      'Scroll real panel to Apply/Cancel without applying',
      'Same draft, action geometry and scroll metrics',
    );
    await page.locator('[data-chroma-cancel]').click();
    assert.equal(await encoded(), saved, 'Cancel preserves saved palette');
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
  }
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  assert.notEqual(await encoded(), saved, 'Apply authors the proposed palette');
  await capture(
    'palette-applied-900',
    'Apply the same proposed draft',
    'Authored palette, preview dismissed',
  );
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), saved);
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(out, 'workflow-ui-review-captures.json'),
    JSON.stringify(
      {
        ok: true,
        capturedAt: new Date().toISOString(),
        scene: {
          composition: layer.id,
          source: source.id,
          settingsNode: repeat.id,
          sourceNavigationNode: transform.id,
          nodeTypes: layer.nodes.map((n) => n.type),
        },
        integrity: {
          navigationDoesNotAuthor: true,
          previewDoesNotAuthor: true,
          cancelPreservesSaved: true,
          applyUndoRestoresSaved: true,
          errors,
        },
        limits: [
          'Browser CI evidence, not native Windows/WebView2 acceptance.',
          'Bounds and scroll metrics are observations, not UX approval or proposal assertions.',
          'Viewport captures preserve actual clipping; no full-page expansion.',
          'No product UI, workflow permissions, browser dependencies, or proposed layout changes.',
        ],
        captures,
      },
      null,
      2,
    ),
  );
  console.log('PASS UI review captures: current UI evidence, not subjective UX acceptance');
} catch (error) {
  await page.screenshot({ path: resolve(out, 'ui-review-failure.png'), fullPage: false });
  await writeFile(
    resolve(out, 'workflow-ui-review-failure.json'),
    JSON.stringify({ error: error.message, errors, captures }, null, 2),
  );
  throw error;
} finally {
  await browser.close();
}
