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
layer.name =
  'UI review composition · A long layer name must truncate without clipping graph controls';
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
      paletteState: [
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
      panels: [
        ...document.querySelectorAll(
          '.graph-toolbar, .graph-view-more, .graph-view-more[open] .command-menu-items, .graph-scroll, .inspector, .shared-colors-panel',
        ),
      ].map(measure),
      hierarchy: [...document.querySelectorAll('.inspector-body > h3, .source-navigation')].map(
        measure,
      ),
      actions: [
        ...document.querySelectorAll(
          '.graph .node.selected .node-actions button, [data-wheel-select]',
        ),
      ].map(measure),
    };
  });
  const file = `ui-review-${name}.png`;
  await page.screenshot({ path: resolve(out, file), fullPage: false });
  captures.push({ file, action, state, ...geometry });
};
// Check the wheel/settings column and independently scrollable palette column.
const checkPaletteTopLayout = async () => {
  assert.equal(await page.locator('.harmony-coverage-status').isVisible(), true);
  const layout = await page.locator('.shared-colors-panel').evaluate((panel) => {
    const rect = (el) => {
      const { left, top, right, bottom } = el.getBoundingClientRect();
      return { left, top, right, bottom };
    };
    const bounds = rect(panel);
    const coverage = panel.querySelector('.harmony-coverage-status');
    return {
      port: {
        left: bounds.left + panel.clientLeft,
        top: bounds.top + panel.clientTop,
        right: bounds.left + panel.clientLeft + panel.clientWidth,
        bottom: bounds.top + panel.clientTop + panel.clientHeight,
      },
      bar: rect(panel.querySelector('.chroma-draft-bar')),
      wheel: rect(panel.querySelector('.harmony-wheel')),
      swatches: rect(panel.querySelector('.harmony-palette-swatches')),
      chips: [...panel.querySelectorAll('[data-wheel-select]')].map(rect),
      neutral: rect(panel.querySelector('.harmony-neutral-track')),
      coverage: {
        bounds: rect(coverage),
        text: coverage.textContent,
        title: coverage.title,
        clientWidth: coverage.clientWidth,
        scrollWidth: coverage.scrollWidth,
      },
      parameters: rect(panel.querySelector('.chroma-settings-scroll')),
      buttons: panel.querySelectorAll(
        '[data-chroma-preview],[data-chroma-apply],[data-chroma-cancel]',
      ).length,
    };
  });
  for (const [label, bounds] of [
    ['status/actions', layout.bar],
    ['whole wheel', layout.wheel],
    ['palette colors', layout.swatches],
    ['neutral track', layout.neutral],
  ])
    assert.ok(
      bounds.left >= layout.port.left &&
        bounds.right <= layout.port.right &&
        bounds.top >= layout.port.top &&
        bounds.bottom <= layout.port.bottom,
      `${label} is contained at panel top: ${JSON.stringify(layout)}`,
    );
  assert.ok(layout.wheel.top >= layout.bar.bottom, 'Status row does not cover the wheel');
  assert.ok(layout.swatches.left >= layout.wheel.right, 'Palette occupies the column to the right of the wheel');
  assert.ok(layout.swatches.top >= layout.bar.bottom, 'Status row does not cover palette');
  assert.ok(layout.neutral.top >= layout.wheel.bottom, 'Neutral track follows wheel in the left column');
  assert.equal(layout.chips.length, 5);
  for (const chip of layout.chips) {
    assert.ok(chip.right - chip.left >= 38 && chip.bottom - chip.top >= 44);
    assert.ok(chip.left >= layout.swatches.left && chip.right <= layout.swatches.right);
  }
  const swatches = page.locator('.harmony-palette-swatches');
  for (const chip of await swatches.locator('[data-wheel-select]').all()) {
    await chip.scrollIntoViewIfNeeded();
    assert.equal(await chip.evaluate((el) => {
      const r = el.getBoundingClientRect();
      const top = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2);
      return top === el || el.contains(top);
    }), true, 'each scrollable palette chip is reachable');
  }
  await swatches.evaluate((el) => { el.scrollTop = 0; });
  assert.equal(
    layout.coverage.title,
    layout.coverage.text,
    'full status is also available on hover',
  );
  assert.equal(layout.coverage.scrollWidth, layout.coverage.clientWidth, 'status wraps, not clips');
  assert.ok(layout.parameters.top >= layout.coverage.bounds.bottom, 'status never covers settings');
  assert.ok(
    layout.parameters.top >= layout.neutral.bottom,
    'Parameters scroll below the wheel and neutral track',
  );
  assert.equal(layout.buttons, 0, 'Live controls have no Preview/Apply gate');
};
const checkGraphToolbar = async (width) => {
  const compact = width < 820;
  await page.waitForFunction(
    (expected) =>
      document.querySelector('.graph-toolbar').classList.contains('graph-toolbar-compact') ===
      expected,
    compact,
  );
  const menu = page.locator('.graph-view-more');
  const summary = menu.locator('summary');
  const actions = ['graph-fit-selection', 'graph-reset', 'graph-overview'];
  const checkVisibleControls = async (secondary = false) => {
    const layout = await page.locator('.graph-toolbar').evaluate((toolbar, includeSecondary) => {
      const bounds = toolbar.getBoundingClientRect();
      const controls = [...toolbar.querySelectorAll('select,button,output,summary')].filter(
        (el) =>
          el.getClientRects().length &&
          (includeSecondary || !el.closest('.graph-view-more .command-menu-items')),
      );
      return {
        documentWidth: document.documentElement.scrollWidth,
        viewportWidth: innerWidth,
        controls: controls.map((el) => {
          const r = el.getBoundingClientRect();
          const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
          return {
            action: el.dataset.action || el.id || el.tagName,
            contained: r.left >= bounds.left && r.right <= bounds.right,
            centerHit: top === el || el.contains(top),
          };
        }),
      };
    }, secondary);
    assert.ok(layout.documentWidth <= layout.viewportWidth, 'toolbar does not widen the document');
    for (const control of layout.controls) {
      assert.equal(control.contained, true, `${width}: ${control.action} inside graph width`);
      assert.equal(control.centerHit, true, `${width}: ${control.action} center is unobscured`);
    }
  };
  for (const action of actions)
    assert.equal(await page.locator(`[data-action="${action}"]`).count(), 1);
  await checkVisibleControls();
  await capture(
    `graph-toolbar-${width}`,
    'Inspect graph view controls at current panel width',
    compact ? 'Secondary view commands in closed More disclosure' : 'All view commands direct',
  );
  if (!compact) {
    assert.equal(await menu.count(), 0);
    return;
  }
  const before = await encoded();
  const commandBefore = await page.evaluate(() => JSON.stringify(window.nect.commandState));
  const name = await page.locator('.graph-layer-name').evaluate((el) => ({
    clipped: el.scrollWidth > el.clientWidth,
    ellipsis: getComputedStyle(el).textOverflow,
  }));
  assert.equal(name.clipped, true, 'long layer name yields its space to controls');
  assert.equal(name.ellipsis, 'ellipsis');
  for (let i = 0; i < 2; i++) {
    await summary.focus();
    await summary.press(i ? 'Space' : 'Enter');
    assert.notEqual(await menu.getAttribute('open'), null);
    await page.keyboard.press('Escape');
    assert.equal(await menu.getAttribute('open'), null);
    assert.equal(await summary.evaluate((el) => document.activeElement === el), true);
  }
  await summary.press('Enter');
  await checkVisibleControls(true);
  await capture(
    `graph-toolbar-more-${width}`,
    'Open More with native keyboard activation',
    'Original Fit selection / Reset view / Overview buttons reachable',
  );
  await page.keyboard.press('Tab');
  assert.equal(
    await page
      .locator('[data-action="graph-fit-selection"]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  await page.keyboard.press('Tab');
  assert.equal(
    await page
      .locator('[data-action="graph-reset"]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  await page.keyboard.press('Tab');
  assert.equal(
    await page
      .locator('[data-action="graph-overview"]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  await page.keyboard.press('Tab');
  assert.equal(await menu.getAttribute('open'), null, 'Tab leaves and dismisses, not a focus trap');
  for (const action of actions) {
    await summary.press('Enter');
    await page.locator(`[data-action="${action}"]`).press('Enter');
    assert.equal(await menu.getAttribute('open'), null);
    assert.equal(await summary.evaluate((el) => document.activeElement === el), true);
  }
  // Return Overview to its original closed state, still through the same button.
  await summary.press('Enter');
  await page.locator('[data-action="graph-overview"]').press('Enter');
  await summary.press('Enter');
  await page.locator('[data-action="graph-reset"]').click();
  assert.equal(await menu.getAttribute('open'), null);
  assert.equal(
    await page.locator('.graph-scroll').evaluate((el) => document.activeElement === el),
    true,
    'pointer navigation keeps the existing viewport focus',
  );
  await summary.press('Enter');
  await page.locator('[data-zoom="graph"]').click();
  assert.equal(await menu.getAttribute('open'), null, 'outside click dismisses');
  await summary.focus();
  await summary.press('Enter');
  await page.setViewportSize({ width: 1600, height: 1000 });
  await page.waitForFunction(() => !document.querySelector('.graph-view-more'));
  assert.equal(
    await page
      .locator('[data-action="graph-fit-selection"]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  await page.setViewportSize({ width, height: 720 });
  await page.waitForSelector('.graph-view-more');
  assert.equal(await summary.evaluate((el) => document.activeElement === el), true);
  assert.equal(await menu.getAttribute('open'), null, 'resize does not reopen stale More');
  await checkVisibleControls();
  assert.equal(
    await encoded(),
    before,
    'view commands and responsive layout do not author project changes',
  );
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.commandState)), commandBefore);
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
    await checkGraphToolbar(viewport.width);
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
  // The graph now spans the workspace: 900px is expanded, while 800px
  // exercises the same compact More keyboard and responsive contracts.
  await page.setViewportSize({ width: 800, height: 720 });
  await checkGraphToolbar(800);
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
  assert.equal(await page.locator('[data-color-advanced]').evaluate((el) => el.open), false);
  assert.equal(await page.locator('[data-chroma-lock]').first().isVisible(), false);
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'saved',
  );
  for (const viewport of [
    { width: 1600, height: 1000 },
    { width: 900, height: 720 },
  ]) {
    await page.setViewportSize(viewport);
    await page.locator('.chroma-settings-scroll').evaluate((el) => {
      el.scrollTop = 0;
    });
    await checkPaletteTopLayout();
    await capture(
      `palette-saved-${viewport.width}`,
      'Open actual Shared Colors harmony panel',
      'Saved default five-role palette; wheel and palette colors in separate pinned rows',
    );
    const rotation = page.locator('[data-wheel-setting="hueRotation"]');
    const before = await page.evaluate(() => window.nect.commandState);
    await rotation.fill('75');
    await page.waitForFunction(
      () => window.nect.colorPreviewSnapshot()?.colorDesign.wheel.hueRotation === 75,
    );
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'editing',
    );
    assert.equal(await encoded(), saved, 'Typing projects live without authoring each keystroke');
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), before);
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 1);
    await checkPaletteTopLayout();
    await capture(
      `palette-live-editing-${viewport.width}`,
      'Type hue rotation 75 without Enter or Apply',
      'Live detached color edit; committed project and history unchanged',
    );
    await page.locator('[data-chroma-spread]').scrollIntoViewIfNeeded();
    assert.ok(await page.locator('.chroma-settings-scroll').evaluate((el) => el.scrollTop > 0));
    await checkPaletteTopLayout();
    await capture(
      `palette-live-parameters-${viewport.width}`,
      'Scroll real parameter region to Chroma Spread while typing remains active',
      'Same live edit; wheel, swatches and compact status remain pinned above settings',
    );
    assert.equal(await encoded(), saved, 'scrolling and capturing do not commit the live edit');
    assert.equal(await rotation.evaluate((el) => document.activeElement === el), true);
    await rotation.focus();
    await page.keyboard.press('Escape');
    await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
    assert.equal(await page.locator('.shared-colors-panel').count(), 1);
    assert.equal(
      await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
      'saved',
    );
    assert.equal(await encoded(), saved, 'Escape rolls back the exact live edit');
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), before);
    assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
  }
  const beforeCommit = await page.evaluate(() => window.nect.commandState);
  await page.locator('[data-wheel-setting="hueRotation"]').fill('75');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  await page.locator('[data-wheel-setting="hueRotation"]').press('Tab');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'saved',
  );
  assert.notEqual(await encoded(), saved, 'Blur commits the live palette once');
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeCommit.expectedRevision + 1,
  );
  await capture(
    'palette-live-committed-900',
    'Leave a valid live field with Tab; no Apply button',
    'Authored palette; one completed edit',
  );
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), saved, 'One Undo restores the complete live edit');
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
          graphToolbarKeyboardAndResizeReachability: true,
          explicitJoystickHideExposesDeleteCenter: true,
          liveInputDoesNotAuthorEachKeystroke: true,
          escapeRollsBackWithoutClosingPanel: true,
          liveBlurCommitSingleUndo: true,
          wheelAndPalettePinnedWhileParametersScroll: true,
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
