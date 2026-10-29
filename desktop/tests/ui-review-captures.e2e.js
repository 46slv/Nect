// Actual UI captures plus a bounded regression for the explicit joystick occlusion escape.
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
// The fixture authors the existing AE-style 2D mode directly (schema 37).
fixture.schemaVersion = 37;
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
    // Read-only hit observation. A known joystick overlap is evidence, not a
    // fixture failure; no Delete click or synthetic pointer event is performed.
    const deleteButton = document.querySelector('.graph .node.selected [data-delete]');
    const deleteCenterHitTest = (() => {
      if (!deleteButton) return null;
      const bounds = rect(deleteButton);
      const point = { x: bounds.x + bounds.width / 2, y: bounds.y + bounds.height / 2 };
      const top = document.elementFromPoint(point.x, point.y);
      const joystick = document.querySelector('.graph-pan-joystick');
      return {
        point,
        deleteBounds: bounds,
        joystickBounds: joystick ? rect(joystick) : null,
        topElement: top
          ? { tag: top.tagName, id: top.id, className: top.getAttribute('class') }
          : null,
        hitsDelete: !!top && (top === deleteButton || deleteButton.contains(top)),
        hitsJoystick: !!top?.closest('.graph-pan-joystick'),
        note: 'Center elementFromPoint observation only; not a click, full-target, keyboard or actionability assessment.',
      };
    })();
    const active = document.activeElement;
    return {
      viewport: { width: innerWidth, height: innerHeight },
      selectedGraphNodes: [...document.querySelectorAll('.graph .node.selected')].map(
        (el) => el.dataset.id,
      ),
      deleteCenterHitTest,
      paletteDraft: [
        ...document.querySelectorAll(
          '.chroma-draft-bar, [data-chroma-state], .shared-colors-heading',
        ),
      ].map(measure),
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
// Preserve the whole wheel and a single compact primary row at panel top.
// This asserts box containment/order only, not full interaction actionability.
const checkPaletteTopLayout = async () => {
  const layout = await page.locator('.shared-colors-panel').evaluate((panel) => {
    const rect = (el) => {
      const { left, top, right, bottom } = el.getBoundingClientRect();
      return { left, top, right, bottom };
    };
    const bounds = rect(panel);
    return {
      port: {
        left: bounds.left + panel.clientLeft,
        top: bounds.top + panel.clientTop,
        right: bounds.left + panel.clientLeft + panel.clientWidth,
        bottom: bounds.top + panel.clientTop + panel.clientHeight,
      },
      bar: rect(panel.querySelector('.chroma-draft-bar')),
      wheel: rect(panel.querySelector('.harmony-wheel')),
      buttons: [...panel.querySelectorAll('.chroma-draft-actions button')].map(rect),
    };
  });
  for (const [label, bounds] of [
    ['status/actions', layout.bar],
    ['whole wheel', layout.wheel],
  ])
    assert.ok(
      bounds.left >= layout.port.left &&
        bounds.right <= layout.port.right &&
        bounds.top >= layout.port.top &&
        bounds.bottom <= layout.port.bottom,
      `${label} is contained at panel top: ${JSON.stringify(layout)}`,
    );
  assert.ok(layout.wheel.top >= layout.bar.bottom, 'Sticky bar does not cover the wheel at top');
  assert.ok(
    layout.buttons.every((b) => b.top === layout.buttons[0].top),
    'Primary actions fit one row',
  );
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
    // Use the existing menu, not a synthetic toggle or a destructive Delete click.
    await page.locator('.graph-scroll').focus();
    await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem', { name: 'Hide pan joystick', exact: true }).click();
    assert.equal(await page.locator('.graph-pan-joystick').isVisible(), false);
    assert.equal(
      await page.locator('.graph-scroll').evaluate((el) => document.activeElement === el),
      true,
    );
    const deleteCenter = await card()
      .locator('[data-delete]')
      .evaluate((el) => {
        const r = el.getBoundingClientRect();
        const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
        return top === el || el.contains(top);
      });
    assert.equal(deleteCenter, true, `${viewport.width}: explicit hide exposes Delete center`);
    await capture(
      `node-actions-${viewport.width}-joystick-hidden`,
      'Choose Hide pan joystick from graph context menu; no Delete click',
      'Same node/actions; joystick explicitly hidden, graph focus restored',
    );
    // Opening the node menu renders the graph; the user's hide must survive it.
    await card().locator('.node-head strong').click({ button: 'right' });
    assert.equal(await page.locator('.graph-pan-joystick').isVisible(), false);
    await page.getByRole('menuitem', { name: 'Show pan joystick', exact: true }).click();
    assert.equal(await page.locator('.graph-pan-joystick').isVisible(), true);
    assert.equal(await encoded(), original, 'Hide/show does not author the project');
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
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'saved',
  );
  for (const viewport of [
    { width: 1600, height: 1000 },
    { width: 900, height: 720 },
  ]) {
    await page.setViewportSize(viewport);
    await page.locator('.shared-colors-panel').evaluate((el) => {
      el.scrollTop = 0;
    });
    await checkPaletteTopLayout();
    await capture(
      `palette-saved-${viewport.width}`,
      'Open actual Shared Colors harmony panel at top',
      'Saved default five-role palette, no preview',
    );
    const rotation = page.locator('[data-wheel-setting="hueRotation"]');
    await rotation.fill('75');
    await rotation.press('Tab');
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'draft',
    );
    await page.locator('[data-chroma-preview]').focus();
    await page.locator('[data-chroma-preview]').press('Enter');
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'preview',
    );
    assert.equal(await page.locator('[data-chroma-apply]').count(), 1);
    assert.equal(await page.locator('[data-chroma-cancel]').count(), 1);
    assert.equal(
      await page.locator('[data-chroma-preview]').evaluate((el) => document.activeElement === el),
      true,
    );
    await page.keyboard.press('Tab');
    assert.equal(
      await page.locator('[data-chroma-apply]').evaluate((el) => document.activeElement === el),
      true,
    );
    await page.keyboard.press('Tab');
    assert.equal(
      await page.locator('[data-chroma-cancel]').evaluate((el) => document.activeElement === el),
      true,
    );
    assert.equal(await encoded(), saved, 'Preview does not author project changes');
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 1);
    await page.locator('.shared-colors-panel').evaluate((el) => {
      el.scrollTop = 0;
    });
    await checkPaletteTopLayout();
    await capture(
      `palette-preview-top-${viewport.width}`,
      'Preview hue rotation 75; scroll panel to top',
      'Saved palette plus non-authored proposed draft',
    );
    await page.locator('.chroma-preview > div').first().scrollIntoViewIfNeeded();
    await capture(
      `palette-preview-actions-${viewport.width}`,
      'Scroll real panel to proposed-color comparison without applying',
      'Same draft, sticky action geometry and scroll metrics',
    );
    await page.locator('[data-chroma-cancel]').focus();
    await page.locator('[data-chroma-cancel]').press('Enter');
    assert.equal(
      await page.locator('[data-chroma-preview]').evaluate((el) => document.activeElement === el),
      true,
    );
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'draft',
    );
    assert.equal(await encoded(), saved, 'Cancel preserves saved palette');
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
    await page.locator('[data-chroma-reload]').click();
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'saved',
    );
  }
  await page.locator('[data-wheel-setting="hueRotation"]').fill('75');
  await page.locator('[data-wheel-setting="hueRotation"]').press('Tab');
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').focus();
  await page.locator('[data-chroma-apply]').press('Enter');
  assert.equal(
    await page.locator('[data-chroma-preview]').evaluate((el) => document.activeElement === el),
    true,
  );
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'saved',
  );
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
          explicitJoystickHideExposesDeleteCenter: true,
          previewDoesNotAuthor: true,
          cancelPreservesSaved: true,
          applyUndoRestoresSaved: true,
          errors,
        },
        limits: [
          'Browser CI evidence, not native Windows/WebView2 acceptance.',
          'Bounds and scroll metrics are observations, not UX approval or proposal assertions.',
          'Viewport captures preserve actual clipping; no full-page expansion.',
          'Shown-joystick Delete hit tests remain observations; explicit user hide must expose the center. No Delete click is performed.',
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
