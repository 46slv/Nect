import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { activateCommand, revealCommandMenu } from './command-menu-actions.js';
import { revealBoundary, activateBoundaryCommand, revealNodePresets, revealNodeGroups } from './helpers/boundary-controls.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const out = resolve('test-results/workspace-panels');
await mkdir(out + '/parts', { recursive: true });
await writeFile(
  out + '/parts/blue.svg',
  '<svg width="40" height="30"><rect width="40" height="30" fill="#2468ab"/></svg>',
);
for (let i = 0; i < 30; i++)
  await writeFile(
    `${out}/parts/blue-${String(i).padStart(2, '0')}.svg`,
    '<svg width="10" height="10"><rect width="10" height="10"/></svg>',
  );
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const state = () =>
  page.evaluate(() => {
    const rect = (selector) => {
      const r = document.querySelector(selector).getBoundingClientRect();
      return [r.x, r.y, r.width, r.height];
    };
    const svg = document.querySelector('.artboard');
    return {
      stage: rect('.canvas-stage'),
      graph: rect('.graph-panel'),
      svg: rect('.artboard'),
      zoom: document.querySelector('[data-zoom="canvas"]').textContent,
      graphScroll: [
        document.querySelector('.graph-scroll').scrollLeft,
        document.querySelector('.graph-scroll').scrollTop,
      ],
      document: JSON.stringify(window.nect.document),
      dirty: window.nect.fileState.dirty,
      undo: document.querySelector('[data-action="undo"]').disabled,
      redo: document.querySelector('[data-action="redo"]').disabled,
    };
  });
const same = (a, b, label) => assert.deepEqual(a, b, label);
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => document.fonts.ready);
  await page.screenshot({ path: out + '/01-closed.png' });
  const initial = await state();
  const initialFit = await page.evaluate(() => {
    const stage = document.querySelector('.canvas-stage');
    const svg = document.querySelector('.artboard');
    const doc = window.nect.document;
    const expected = Math.max(
      0.05,
      Math.min(2, (stage.clientWidth - 70) / doc.width, (stage.clientHeight - 70) / doc.height),
    );
    return { actual: parseFloat(svg.style.width) / doc.width, expected };
  });
  assert.ok(
    Math.abs(initialFit.actual - initialFit.expected) < 0.001,
    'initial fit reflects settled UI font metrics',
  );
  const graphToggle = page.getByRole('button', { name: 'Toggle node graph controls', exact: true });
  const viewToggle = page.getByRole('button', {
    name: 'Toggle artboard view controls',
    exact: true,
  });
  assert.equal(await graphToggle.getAttribute('aria-expanded'), 'false');
  assert.equal(await viewToggle.getAttribute('aria-expanded'), 'false');
  assert.equal(await page.locator('#add-node').isVisible(), true);
  assert.equal(await page.locator('[data-action="graph-fit"]').isVisible(), false);
  assert.equal(await page.locator('[data-zoom="canvas"]').isVisible(), true);
  assert.equal(await page.locator('[data-action="fit"]').isVisible(), false);
  await page.screenshot({ path: out + '/boundary-closed.png', animations: 'disabled' });
  for (let i = 0; i < 2; i++) {
    await graphToggle.press('Enter');
    assert.equal(await page.locator('[data-action="graph-fit"]').isVisible(), true);
    same(await state(), initial, 'graph boundary open preserves canvas and graph');
    await graphToggle.press('Escape');
    assert.equal(await graphToggle.getAttribute('aria-expanded'), 'false');
    assert.equal(await graphToggle.evaluate((el) => el === document.activeElement), true);
    same(await state(), initial, 'graph boundary close preserves canvas and graph');
  }
  await graphToggle.press('Enter');
  await page.screenshot({ path: out + '/boundary-graph-open.png', animations: 'disabled' });
  await viewToggle.press('Enter');
  assert.equal(await graphToggle.getAttribute('aria-expanded'), 'false');
  assert.equal(await viewToggle.getAttribute('aria-expanded'), 'true');
  assert.equal(await page.locator('[data-action="fit"]').isVisible(), true);
  same(await state(), initial, 'switching boundary ends does not move either camera');
  await page.screenshot({ path: out + '/boundary-view-open.png', animations: 'disabled' });
  await viewToggle.press('Escape');
  assert.equal(await viewToggle.evaluate((el) => el === document.activeElement), true);
  same(await state(), initial, 'closing view controls is view-only');
  // A circle's visible face plus focus ring must remain inside the rounded surface;
  // its transparent 30px rectangular hit target is checked separately.
  const contourFits = (locator) =>
    locator.evaluate((bar) => {
      const regions = [
        bar.querySelector('[data-rail-fill]'),
        bar.querySelector(':scope > svg > rect'),
      ];
      return [...bar.querySelectorAll('button')].every((button) => {
        const rect = button.getBoundingClientRect();
        const cx = rect.left + rect.width / 2,
          cy = rect.top + rect.height / 2;
        return Array.from({ length: 32 }, (_, index) => (index * Math.PI) / 16).every((angle) => {
          const point = new DOMPoint(cx + 16 * Math.cos(angle), cy + 16 * Math.sin(angle));
          return regions.some((region) =>
            region.isPointInFill(point.matrixTransform(region.getScreenCTM().inverse())),
          );
        });
      });
    });
  for (const [action, icon] of [
    ['asset-library', 'assets'],
    ['undo', 'undo'],
    ['redo', 'redo'],
  ]) {
    const button = page.locator(`[data-action="${action}"]`);
    assert.equal(await button.locator(`svg[data-icon="${icon}"]`).count(), 1);
    assert.ok((await button.textContent()).trim(), `${action} retains its visible text label`);
  }
  const rightBar = page.locator('.workspace-panel-bar-right');
  const inspectorTrigger = page.getByRole('button', { name: 'Focus Inspector', exact: true });
  assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'false');
  await page.locator('.canvas-stage').focus();
  await page.mouse.move(0, 0);
  await page.waitForTimeout(550);
  assert.equal(
    await contourFits(rightBar),
    true,
    'right idle contour contains faces and focus rings',
  );
  await page.screenshot({ path: out + '/peek-right-idle.png', animations: 'disabled' });
  const rightBounds = await rightBar.boundingBox();
  await page.mouse.move(rightBounds.x - 10, rightBounds.y + rightBounds.height / 2);
  await page.waitForFunction(
    () => document.querySelector('.workspace-panel-bar-right').dataset.near === 'true',
  );
  await page.screenshot({ path: out + '/peek-right-near.png', animations: 'disabled' });
  await page.mouse.move(0, 0);
  await page.waitForTimeout(550);
  await inspectorTrigger.click();
  assert.ok(
    (await rightBar.boundingBox()).x < rightBounds.x,
    'right handle moves inward only on explicit open',
  );
  await page.screenshot({ path: out + '/peek-right-open.png', animations: 'disabled' });
  await inspectorTrigger.click();
  same(await state(), initial, 'testing right idle keeps document and canvas geometry');
  const railIcons = await page.evaluate(() =>
    [...document.querySelectorAll('.tools button, .workspace-panel-bar button')].map((button) => ({
      label: button.getAttribute('aria-label'),
      title: button.title,
      pressed: button.getAttribute('aria-pressed'),
      decorative: button.querySelector('svg')?.getAttribute('aria-hidden'),
    })),
  );
  assert.equal(railIcons.length, 13);
  assert.ok(railIcons.every((icon) => icon.label && icon.title && icon.decorative === 'true'));
  assert.ok(railIcons.every((icon) => ['true', 'false'].includes(icon.pressed)));
  const faces = await page.locator('.workspace-panel-bar button').evaluateAll((buttons) =>
    buttons.map((button) => {
      const style = getComputedStyle(button, '::before');
      return {
        width: parseFloat(style.width),
        height: parseFloat(style.height),
        radius: style.borderRadius,
      };
    }),
  );
  assert.ok(
    faces.every((face) => face.width === 26 && face.height === 26 && face.radius === '50%'),
  );
  // The painted handle grows near the pointer without moving its stable hit
  // boxes or changing panel state, then relaxes after a short leave delay.
  const leftBar = page.locator('.workspace-panel-bar-left');
  const leftBounds = await leftBar.boundingBox();
  const hitBoxes = () =>
    leftBar.locator('button').evaluateAll((buttons) =>
      buttons.map((button) => {
        const rect = button.getBoundingClientRect();
        return [rect.x, rect.y, rect.width, rect.height];
      }),
    );
  const contourWidth = () =>
    leftBar.locator('[data-rail-fill]').evaluate((path) => path.getBoundingClientRect().width);
  await page.waitForTimeout(550);
  const idleWidth = await contourWidth();
  const stableTargets = await hitBoxes();
  const idleFits = await contourFits(leftBar);
  assert.equal(idleFits, true, 'idle contour contains faces and focus rings');
  await page.screenshot({ path: out + '/peek-idle.png' });
  await page.mouse.move(leftBounds.x + leftBounds.width + 10, leftBounds.y + leftBounds.height / 2);
  await page.waitForFunction(
    () => document.querySelector('.workspace-panel-bar-left').dataset.near === 'true',
  );
  await page.waitForTimeout(160);
  const nearWidth = await contourWidth();
  assert.ok(nearWidth > idleWidth + 4, 'contour expands on proximity');
  same(await hitBoxes(), stableTargets, 'peek never changes button hit geometry');
  assert.equal(
    await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
    'false',
  );
  await page.screenshot({ path: out + '/peek-near.png' });
  await page.mouse.move(Math.round(page.viewportSize().width / 2), 1);
  await page.waitForFunction(
    () => document.querySelector('.workspace-panel-bar-left').dataset.near === 'false',
  );
  await page.waitForTimeout(100);
  assert.ok(
    (await contourWidth()) > idleWidth + 4,
    'leave hysteresis retains expanded contour briefly',
  );
  await page.waitForTimeout(500);
  assert.ok(Math.abs((await contourWidth()) - idleWidth) < 1, 'contour eventually returns to idle');
  same(await state(), initial, 'peek is view-only');
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await page.mouse.move(leftBounds.x + leftBounds.width + 10, leftBounds.y + leftBounds.height / 2);
  await page.waitForFunction(
    () => document.querySelector('.workspace-panel-bar-left').dataset.near === 'true',
  );
  const reducedPeek = await leftBar.locator('[data-rail-fill]').evaluate((path) => ({
    duration: getComputedStyle(path).transitionDuration,
    width: path.getBoundingClientRect().width,
  }));
  assert.equal(reducedPeek.duration, '0s');
  assert.ok(reducedPeek.width > idleWidth + 4, 'reduced motion changes geometry immediately');
  await page.mouse.move(Math.round(page.viewportSize().width / 2), 1);
  await page.waitForFunction(
    () => document.querySelector('.workspace-panel-bar-left').dataset.near === 'false',
  );
  await page.waitForTimeout(100);
  assert.ok((await contourWidth()) > idleWidth + 4, 'reduced motion retains leave hysteresis');
  await page.waitForTimeout(260);
  assert.ok(Math.abs((await contourWidth()) - idleWidth) < 1);
  await page.emulateMedia({ reducedMotion: 'no-preference' });

  // Capture transition starts before clicking so a loaded CI runner cannot
  // turn a completed 200 ms transition into a false negative.
  await page.evaluate(() => {
    window.__nectMotionEvents = [];
    document.addEventListener('transitionrun', (event) => {
      const element = event.target;
      const kind = element.matches?.('[data-drawer-side="left"]')
        ? 'drawer'
        : element.matches?.('[data-asset-library]')
          ? 'portal'
          : null;
      if (kind)
        window.__nectMotionEvents.push({
          kind,
          property: event.propertyName,
          pseudo: event.pseudoElement,
          time: performance.now(),
        });
    });
  });
  const supportsDiscrete = await page.evaluate(() =>
    CSS.supports('transition-behavior: allow-discrete'),
  );
  // First-ever Assets mount starts at its docked location before revealing.
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  assert.ok(
    (await leftBar.boundingBox()).x > leftBounds.x,
    'left handle moves inward only on explicit open',
  );
  if (supportsDiscrete)
    await page.waitForFunction(
      () =>
        ['drawer', 'portal'].every((kind) =>
          window.__nectMotionEvents.some(
            (event) => event.kind === kind && event.property === 'opacity',
          ),
        ) &&
        window.__nectMotionEvents.some(
          (event) =>
            event.kind === 'drawer' &&
            event.property === 'transform' &&
            event.pseudo === '::before',
        ),
    );
  const firstMotionEvents = await page.evaluate(() => [...window.__nectMotionEvents]);
  await page.screenshot({ path: out + '/motion-first-open.png' });
  await page.screenshot({ path: out + '/peek-open.png', animations: 'disabled' });
  const selectedFace = await page
    .locator('[data-workspace-panel="assets"]')
    .evaluate((button) => getComputedStyle(button, '::before').backgroundColor);
  const idleFace = await page
    .locator('[data-workspace-panel="colors"]')
    .evaluate((button) => getComputedStyle(button, '::before').backgroundColor);
  assert.notEqual(selectedFace, idleFace, 'selected face remains visually distinct');
  await page.emulateMedia({ forcedColors: 'active' });
  await page.locator('[data-workspace-panel="assets"]').focus();
  await page.keyboard.press('Tab');
  const forcedStates = await page.evaluate(() => {
    const selected = document.querySelector('[data-workspace-panel="assets"]');
    const focused = document.querySelector('[data-workspace-panel="colors"]');
    return {
      selectedBorder: getComputedStyle(selected, '::before').borderWidth,
      focusedOutline: getComputedStyle(focused, '::before').outlineWidth,
      focusOnOther: document.activeElement === focused && focused.matches(':focus-visible'),
    };
  });
  assert.deepEqual(forcedStates, {
    selectedBorder: '3px',
    focusedOutline: '2px',
    focusOnOther: true,
  });
  await page.emulateMedia({ forcedColors: 'none' });
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.waitForTimeout(250);

  // A rapid reversal changes semantics synchronously; visual exit must never
  // retain an actionable or stale panel. The artboard/graph are not animated.
  const rapid = await page.evaluate(() => {
    const assets = document.querySelector('[data-workspace-panel="assets"]');
    const colors = document.querySelector('[data-workspace-panel="colors"]');
    const host = document.querySelector('[data-drawer-side="left"]');
    assets.click();
    assets.click();
    const closed = {
      hidden: host.hidden,
      inert: host.inert,
      expanded: assets.getAttribute('aria-expanded'),
      focused: document.activeElement === assets,
    };
    assets.click();
    colors.click();
    assets.click();
    return {
      closed,
      current: host.dataset.activePanel,
      count: document.querySelectorAll('[data-asset-library]').length,
      hidden: host.hidden,
      inert: host.inert,
      expanded: assets.getAttribute('aria-expanded'),
    };
  });
  assert.deepEqual(rapid, {
    closed: { hidden: true, inert: true, expanded: 'false', focused: true },
    current: 'assets',
    count: 1,
    hidden: false,
    inert: false,
    expanded: 'true',
  });
  const portalBeforeSwitch = await page.locator('[data-asset-library]').elementHandle();
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).focus();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  assert.equal(
    await page.locator('[data-asset-library]').evaluate((el) => el.hidden && el.inert),
    true,
  );
  assert.equal(
    await page.locator('[data-drawer-side="left"]').evaluate((el) => el.hidden && el.inert),
    true,
  );
  assert.equal(
    await page
      .getByRole('button', { name: 'Focus Layers', exact: true })
      .getAttribute('aria-expanded'),
    'true',
  );
  assert.equal(
    await page.evaluate(() => document.activeElement?.closest('[inert]') === null),
    true,
  );
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  assert.equal(
    await page.evaluate(
      (node) => node === document.querySelector('[data-asset-library]'),
      portalBeforeSwitch,
    ),
    true,
  );
  assert.equal(
    await page.locator('[data-drawer-side="right"]').evaluate((el) => el.hidden && el.inert),
    true,
  );
  same(await state(), initial, 'exclusive cross-side switching keeps both cameras and history');
  // Give exit a settled frame before testing entrance: rapid reversals may be
  // legitimately coalesced before a browser has painted the first state.
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.waitForTimeout(250);
  await page.evaluate(() => {
    window.__nectMotionEvents = [];
  });
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  if (supportsDiscrete)
    await page.waitForFunction(() =>
      window.__nectMotionEvents.some(
        (event) => event.kind === 'drawer' && event.property === 'opacity',
      ),
    );
  const motion = await page.evaluate(() => {
    const host = document.querySelector('[data-drawer-side="left"]');
    const slot = host.querySelector('[data-workspace-asset-slot]');
    const portal = document.querySelector('[data-asset-library]');
    const a = slot.getBoundingClientRect(),
      b = portal.getBoundingClientRect();
    return {
      duration: getComputedStyle(host).transitionDuration,
      hostTransform: getComputedStyle(host).transform,
      portalDelta: Math.abs(a.left - b.left),
    };
  });
  const reopenedMotionEvents = await page.evaluate(() => [...window.__nectMotionEvents]);
  if (supportsDiscrete) {
    assert.match(motion.duration, /0\.2s/);
    assert.equal(motion.hostTransform, 'none', 'live controls do not move under input');
    assert.ok(motion.portalDelta < 3, `portal follows its slot: ${motion.portalDelta}px`);
  }
  same(await state(), initial, 'rapid motion cannot change document or workspace geometry');
  await page.screenshot({ path: out + '/motion-opening.png' });
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.emulateMedia({ reducedMotion: 'reduce' });
  const reduced = await page.evaluate(() => {
    const button = document.querySelector('[data-workspace-panel="assets"]');
    button.click();
    const host = document.querySelector('[data-drawer-side="left"]');
    const value = getComputedStyle(host).transitionDuration;
    button.click();
    return { duration: value, hidden: host.hidden, inert: host.inert };
  });
  assert.equal(reduced.duration, '0s');
  assert.equal(reduced.hidden, true);
  assert.equal(reduced.inert, true);
  await page.emulateMedia({ reducedMotion: 'no-preference' });
  same(await state(), initial, 'reduced-motion toggle remains view-only');
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.waitForTimeout(240);
  await page.locator('[data-tool="direct"]').click();
  const afterRerender = await page.locator('[data-drawer-side="left"]').evaluate((host) => ({
    enabled: host.dataset.motionEnabled,
    running: host.getAnimations().some((animation) => animation.playState === 'running'),
  }));
  assert.equal(
    afterRerender.enabled,
    'false',
    'ordinary editor rerender does not replay drawer motion',
  );
  assert.equal(afterRerender.running, false);
  await page.locator('[data-tool="select"]').click();
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  same(await state(), initial, 'ordinary rerender preserves viewport and document');
  const svgIdentity = await page.locator('.artboard').elementHandle();
  assert.equal(await contourFits(leftBar), true, 'left face and focus ring remain in the contour');
  assert.equal(
    await contourFits(rightBar),
    true,
    'right face and focus ring remain in the contour',
  );
  const targetGeometry = await page.locator('.workspace-panel-bar').evaluateAll((bars) =>
    bars.map((bar) =>
      [...bar.querySelectorAll('button')].map((button) => {
        const r = button.getBoundingClientRect();
        return {
          left: r.left,
          right: r.right,
          top: r.top,
          bottom: r.bottom,
          width: r.width,
          height: r.height,
        };
      }),
    ),
  );
  const hitSafety = await page.evaluate(() => {
    const tools = document.querySelector('.tools').getBoundingClientRect();
    return [...document.querySelectorAll('.workspace-panel-bar button')].map((button) => {
      const r = button.getBoundingClientRect();
      const top = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2);
      return {
        clearOfTools:
          r.right <= tools.left ||
          r.left >= tools.right ||
          r.bottom <= tools.top ||
          r.top >= tools.bottom,
        receivesCenter: button === top || button.contains(top),
      };
    });
  });
  assert.ok(
    hitSafety.every((target) => target.clearOfTools && target.receivesCenter),
    'drawer targets do not cover the creation rail and receive their center',
  );
  for (const targets of targetGeometry) {
    assert.ok(targets.every((target) => target.width >= 30 && target.height >= 30));
    for (let i = 1; i < targets.length; i++)
      assert.ok(targets[i - 1].bottom <= targets[i].top, 'adjacent hit targets do not overlap');
  }
  const stage = page.locator('.canvas-stage'),
    graph = page.locator('.graph-panel');
  const initialGraph = await graph.boundingBox();
  assert.ok(initialGraph.x <= initial.stage[0], 'Graph extends under the upper creation rail');
  for (const name of ['Focus Layers', 'Focus Inspector', 'Toggle Alignment panel']) {
    await page.getByRole('button', { name, exact: true }).click();
    same(await state(), initial, `${name} cannot resize or recenter either surface`);
  }
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  const panel = page.locator('[data-asset-library]');
  await panel.waitFor({ state: 'visible' });
  const identity = await panel.elementHandle();
  assert.equal(
    await page.locator('[data-drawer-side=right]').evaluate((el) => el.hidden && el.inert),
    true,
  );
  same(await state(), initial, 'opening the opposite side is view-only');
  const chooser = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
  await (await chooser).setFiles(out + '/parts');
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('blue');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.screenshot({ path: out + '/02-assets-and-alignment.png' });
  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  same(await state(), initial, 'switching left panel leaves both surfaces stable');
  assert.equal(
    await page.evaluate((node) => node === document.querySelector('.artboard'), svgIdentity),
    true,
    'view-only toggles do not replace or refit the canvas',
  );
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  assert.equal(
    await page.evaluate(
      (node) => node === document.querySelector('[data-asset-library]'),
      identity,
    ),
    true,
  );
  assert.equal(
    await page.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(),
    'blue',
  );
  same(await state(), initial, 'restoring Assets retains viewport and document');
  const beforeViewer = await state();
  await page.getByRole('button', { name: 'Viewers 1 / 2', exact: true }).click();
  assert.equal(await panel.isVisible(), false, 'Assets portal is not interactive over Viewers');
  assert.equal(await panel.evaluate((el) => el.inert), true);
  assert.equal(await page.locator('.workspace-panel-bar-left').isVisible(), false);
  assert.deepEqual((await state()).graph, beforeViewer.graph);
  await page.locator('.graph .node .node-head strong').first().click();
  await ensureWorkspacePanel(page, 'inspector');
  const assign = page.locator('.viewer-assignment [data-assign-viewer="0"]');
  await assign.click();
  assert.notEqual(
    await page.locator('.node-viewer [data-viewer-status]').first().innerText(),
    'Unassigned',
  );
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.document)),
    beforeViewer.document,
  );
  // The global Assets command has a visible effect even while its saved
  // left-panel state is suspended in Viewer mode.
  await activateCommand(page, 'asset-library');
  assert.equal(
    await page.getByRole('button', { name: 'Artwork', exact: true }).getAttribute('aria-pressed'),
    'true',
  );
  assert.equal(await panel.isVisible(), true);
  assert.equal(
    await page.evaluate(
      (node) => node === document.querySelector('[data-asset-library]'),
      identity,
    ),
    true,
  );
  assert.equal(
    await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(),
    'blue',
  );
  same(await state(), beforeViewer, 'viewer round trip keeps viewport and document');
  await ensureWorkspacePanel(page, 'align');
  assert.equal(
    await panel.evaluate((el) => el.hidden && el.inert),
    true,
    'opening opposite side hides the portal',
  );
  const boundaries = await page.evaluate(() => {
    const rect = (s) => {
      const r = document.querySelector(s).getBoundingClientRect();
      return { top: r.top, bottom: r.bottom, left: r.left, right: r.right };
    };
    return {
      left: rect('[data-drawer-side="left"]'),
      right: rect('[data-drawer-side="right"]'),
      stage: rect('.canvas-stage'),
      graph: rect('.graph-panel'),
    };
  });
  for (const side of ['right']) {
    assert.ok(
      boundaries[side].top >= boundaries.stage.top - 1 &&
        boundaries[side].bottom <= boundaries.stage.bottom + 1,
      `${side} drawer stays in stage`,
    );
    assert.ok(
      boundaries[side].bottom <= boundaries.graph.top + 1,
      `${side} drawer never covers graph`,
    );
  }
  for (const id of ['assets', 'colors']) {
    await ensureWorkspacePanel(page, id);
    const left = await page.locator('[data-drawer-side="left"]').boundingBox();
    const canvasBounds = await page.locator('.canvas-stage').boundingBox();
    const graphBounds = await page.locator('.graph-panel').boundingBox();
    assert.ok(left && canvasBounds && graphBounds && left.y >= canvasBounds.y - 1 && left.y + left.height <= canvasBounds.y + canvasBounds.height + 1, `${id} left drawer remains within Artboard`);
    assert.ok(left.y + left.height <= graphBounds.y + 1, `${id} left drawer never covers Graph`);
    assert.equal(await page.locator('[data-drawer-side="right"]').evaluate((el) => el.hidden && el.inert), true);
    same(await state(), initial, `${id} containment check preserves cameras and history`);
  }
  await ensureWorkspacePanel(page, 'assets');
  await page.evaluate(() => {
    const dialog = document.createElement('dialog');
    dialog.id = 'workspace-modal-probe';
    dialog.innerHTML = '<button>Modal owner</button>';
    document.body.append(dialog);
    dialog.showModal();
  });
  await page.keyboard.press('Escape');
  assert.equal(await panel.count(), 1, 'native modal owns Escape');
  assert.equal(
    await page.locator('#workspace-modal-probe').evaluate((dialog) => dialog.open),
    false,
  );
  await page.evaluate(() => document.querySelector('#workspace-modal-probe').remove());
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).focus();
  await page.keyboard.press('Escape');
  await panel.waitFor({ state: 'detached' });
  const assetsTrigger = page.getByRole('button', { name: 'Toggle Assets panel', exact: true });
  assert.equal(await assetsTrigger.getAttribute('aria-expanded'), 'false');
  assert.equal(await assetsTrigger.evaluate((button) => button === document.activeElement), true);
  same(await state(), initial, 'closing is view-only');
  // Manual zoom and pan must remain stable too, not only fit mode.
  await activateBoundaryCommand(page, 'zoom-in');
  await stage.hover();
  await page.mouse.wheel(40, 25);
  const manual = await state();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  same(await state(), manual, 'manual zoom and pan survive open');
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  same(await state(), manual, 'manual zoom and pan survive close');
  for (const width of [1600, 1366, 1000, 900]) {
    await page.setViewportSize({ width, height: 960 });
    const before = await state();
    const toolbar = await page.evaluate(() => {
      const groups = [...document.querySelectorAll('.canvas-toolbar-group')];
      const visible = [
        ...document.querySelectorAll(
          '#app > header button, #app > header select, #app > header summary, .canvas-toolbar button',
        ),
      ].filter((element) => element.getClientRects().length);
      return {
        names: groups.map((group) => group.getAttribute('aria-label')),
        clipped: visible
          .filter((element) => {
            const rect = element.getBoundingClientRect();
            return rect.left < -1 || rect.right > innerWidth + 1;
          })
          .map((element) => element.getAttribute('aria-label') || element.textContent.trim()),
      };
    });
    assert.deepEqual(toolbar.names, ['Zoom', 'Editing aids', 'Grid and guides']);
    assert.deepEqual(toolbar.clipped, [], `visible toolbar controls fit at ${width}`);
    const end = await page.locator('.header-end').boundingBox();
    const title = await page.locator('[data-document-title]').boundingBox();
    const help = await page.locator('[data-action="help"]').boundingBox();
    const build = await page.locator('[data-build-info] > summary').boundingBox();
    assert.ok(
      end && title && help && build && end.x >= 0 && end.x + end.width <= width,
      'document/help/build group fits the viewport',
    );
    assert.ok(
      Math.abs(title.y + title.height / 2 - (help.y + help.height / 2)) < 2 &&
        Math.abs(help.y + help.height / 2 - (build.y + build.height / 2)) < 2,
      'document/help/build wrap together',
    );

    await revealCommandMenu(page, 'Export');
    const outputSummary = page.locator('[data-output-settings] > summary');
    assert.equal(await outputSummary.isVisible(), true);
    await revealCommandMenu(page, 'Output settings');
    for (const name of [
      'SVG output sizing',
      'SVG export units',
      'SVG geometry precision',
      'PNG size mode',
    ])
      assert.equal(await page.getByRole('combobox', { name, exact: true }).isVisible(), true);
    const depth = await page.evaluate(() => {
      const channels = (value) => value.match(/[\d.]+/g).map(Number);
      const lum = (rgb) =>
        rgb
          .slice(0, 3)
          .map((v) => {
            const x = v / 255;
            return x <= 0.04045 ? x / 12.92 : ((x + 0.055) / 1.055) ** 2.4;
          })
          .reduce((sum, v, i) => sum + v * [0.2126, 0.7152, 0.0722][i], 0);
      const pane = channels(
        getComputedStyle(document.querySelector('.workspace-drawer'), '::before').backgroundColor,
      );
      const workspace = channels(
        getComputedStyle(document.querySelector('.canvas-wrap')).backgroundColor,
      );
      const popup = channels(
        getComputedStyle(document.querySelector('.output-settings-items')).backgroundColor,
      );
      return {
        workspace: lum(workspace),
        paneOverBlack: lum(pane.slice(0, 3).map((v) => v * (pane[3] ?? 1))),
        popup: lum(popup),
        iconOpacity: getComputedStyle(
          document.querySelector('.workspace-panel-bar .workspace-icon'),
        ).opacity,
      };
    });
    assert.ok(
      depth.workspace < depth.paneOverBlack && depth.paneOverBlack < depth.popup,
      'surface depth remains ordered even over a black underlay',
    );
    assert.equal(depth.iconOpacity, '1');
    const sizing = page.getByRole('combobox', { name: 'SVG output sizing', exact: true });
    await sizing.selectOption('responsive');
    assert.equal(
      await page
        .locator('[data-output-settings]')
        .evaluate((el) => el.open && el.closest('.command-menu').open),
      true,
      'nested output settings survive a render',
    );
    await sizing.selectOption('fixed');
    assert.equal(
      await page
        .locator('[data-output-settings]')
        .evaluate((el) => el.open && el.closest('.command-menu').open),
      true,
    );
    const outputBox = await page.locator('.output-settings-items').boundingBox();
    assert.ok(outputBox.x >= 0 && outputBox.x + outputBox.width <= width);
    if (width === 900 || width === 1366 || width === 1600)
      await page.screenshot({
        path: out + `/output-settings-${width}.png`,
        animations: 'disabled',
      });
    await outputSummary.focus();
    await page.keyboard.press('Escape');
    assert.equal(
      await page
        .locator('[data-output-settings]')
        .evaluate((el) => el.closest('.command-menu').open),
      false,
    );
    assert.equal(
      await page
        .locator('header .command-menu')
        .filter({ has: page.locator('summary', { hasText: /^Export$/ }) })
        .locator(':scope > summary')
        .evaluate((el) => el === document.activeElement),
      true,
    );
    if (width === 900 || width === 1366 || width === 1600) {
      assert.equal(await page.locator('[data-action="undo"]').isDisabled(), initial.undo);
      await page.screenshot({ path: out + `/toolbar-${width}.png`, animations: 'disabled' });
      await page.locator('[data-workspace-panel="colors"]').hover();
      await page.screenshot({ path: out + `/toolbar-${width}-hover.png`, animations: 'disabled' });
      await page.locator('[data-workspace-panel="colors"]').focus();
      await page.keyboard.press('Tab');
      assert.equal(await page.evaluate(() => !!document.querySelector(':focus-visible')), true);
      await page.screenshot({ path: out + `/toolbar-${width}-focus.png`, animations: 'disabled' });
    }
    await page.getByRole('button', { name: 'Focus Inspector', exact: true }).click();
    await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
    assert.equal(
      await page.locator('[data-drawer-side=right]').evaluate((el) => el.hidden && el.inert),
      true,
    );
    same(await state(), before, `exclusive side switch preserves geometry at ${width}`);
    if (width === 900 || width === 1366 || width === 1600)
      await page.screenshot({
        path: out + `/toolbar-${width}-selected.png`,
        animations: 'disabled',
      });
    assert.equal(
      await page.evaluate(() => document.documentElement.scrollWidth > innerWidth),
      false,
    );
    await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  }
  // An editing control owns Escape even with a Grid popover still open.
  await ensureWorkspacePanel(page, 'inspector');
  await activateBoundaryCommand(page, 'grid-panel');
  const nameDraft = page.locator('.inspector-body [data-layer-name]');
  const oldName = await nameDraft.inputValue();
  await nameDraft.fill('未確定の名前');
  await nameDraft.press('Escape');
  assert.equal(await nameDraft.inputValue(), '未確定の名前');
  assert.equal(
    await nameDraft.evaluate((el) => el.isConnected && el === document.activeElement),
    true,
  );
  assert.equal(
    await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'),
    'true',
  );
  await nameDraft.fill(oldName);
  const gridPanel = page.locator('.guides-panel[aria-label="Grid"]');
  await gridPanel.evaluate((el) => {
    el.tabIndex = -1;
    el.focus();
  });
  // Synthetic key events exercise the explicit guards; they do not simulate a
  // real OS input-method composition session.
  for (const init of [{ isComposing: true }, { keyCode: 229 }]) {
    const prevented = await gridPanel.evaluate((el, init) => {
      const event = new KeyboardEvent('keydown', {
        key: 'Escape',
        bubbles: true,
        cancelable: true,
        ...init,
      });
      el.dispatchEvent(event);
      return event.defaultPrevented;
    }, init);
    assert.equal(prevented, false);
    assert.equal(
      await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'),
      'true',
    );
  }
  await page.keyboard.press('Escape');
  assert.equal(
    await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'),
    'false',
  );

  // Placement rerenders the editor, but the live library search/scroll and its
  // native chooser/async owner stay attached; insertion is one Undo.
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await panel.waitFor({ state: 'visible' });
  const restoredIdentity = await panel.elementHandle();
  await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('blue');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  const grid = panel.locator('.asset-library-grid');
  await grid.evaluate((el) => {
    el.scrollTop = 45;
  });
  const scrollBefore = await grid.evaluate((el) => el.scrollTop);
  assert.ok(scrollBefore > 0, 'fixture exercises a real scrolled asset list');
  const priorPlacement = await page.evaluate(() => JSON.stringify(window.nect.document));
  await panel.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await page.evaluate(
      (node) => node === document.querySelector('[data-asset-library]'),
      restoredIdentity,
    ),
    true,
  );
  assert.equal(
    await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(),
    'blue',
  );
  assert.equal(await grid.evaluate((el) => el.scrollTop), scrollBefore);
  await activateCommand(page, 'undo');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), priorPlacement);
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();

  // A valid native numeric draft follows the existing blur/commit policy even
  // when the next click switches drawers; one Undo restores the authored value.
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const oldWidth = await page.evaluate(() => window.nect.document.width);
  await page.locator('[data-dimension="width"]').fill(String(oldWidth + 11));
  await page.getByRole('button', { name: 'Focus Inspector', exact: true }).click();
  await page.waitForFunction((value) => window.nect.document.width === value, oldWidth + 11);
  assert.equal(await page.locator('.inspector').isVisible(), true);
  await activateCommand(page, 'undo');
  assert.equal(await page.evaluate(() => window.nect.document.width), oldWidth);

  // The upper creation rail remains reachable when the graph is made tall.
  const splitter = page.getByRole('separator', { name: 'Resize node graph' });
  await splitter.focus();
  for (let i = 0; i < 7; i++) await splitter.press('ArrowUp');
  const lastTool = page.locator('.tools button').last();
  await lastTool.scrollIntoViewIfNeeded();
  const rail = await page.locator('.tools').boundingBox(),
    last = await lastTool.boundingBox();
  assert.ok(last.y >= rail.y && last.y + last.height <= rail.y + rail.height + 1);
  await page.setViewportSize({ width: 1000, height: 600 });
  if ((await inspectorTrigger.getAttribute('aria-expanded')) === 'true')
    await inspectorTrigger.click();
  await splitter.focus();
  await page.mouse.move(0, 0);
  await page.waitForTimeout(550);
  assert.equal(
    await contourFits(rightBar),
    true,
    'short-split right idle contour contains faces and focus rings',
  );
  for (const button of await page.locator('.workspace-panel-bar button').all()) {
    const b = await button.boundingBox(),
      bar = await button.locator('..').boundingBox();
    assert.ok(
      b &&
        b.width >= 30 &&
        b.height >= 30 &&
        b.y >= bar.y &&
        b.y + b.height <= bar.y + bar.height + 1,
      'short split retains each usable edge target',
    );
  }
  assert.equal(await contourFits(leftBar), true, 'short-split left face and focus ring fit');
  assert.equal(await contourFits(rightBar), true, 'short-split right face and focus ring fit');

  // Floating Presets and Groups follow the boundary after a short split and
  // resize, stay inside the viewport and retain native dismissal/focus.
  await page.setViewportSize({ width: 1366, height: 960 });
  await splitter.focus();
  for (let i = 0; i < 20; i++) await splitter.press('ArrowDown');
  await revealNodePresets(page);
  await page.setViewportSize({ width: 900, height: 600 });
  const popupInside = (selector) => page.waitForFunction((s) => {
    const el = document.querySelector(s);
    if (!el) return false;
    const r = el.getBoundingClientRect();
    return r.width > 0 && r.height > 0 && r.left >= 0 && r.top >= 0 && r.right <= innerWidth && r.bottom <= innerHeight;
  }, selector);
  await popupInside('.boundary-popover[open] > .node-preset-controls');
  const popupState = await state();
  const presetSummary = page.locator('.boundary-popover > summary').filter({ hasText: /^Presets$/ });
  await presetSummary.focus();
  await presetSummary.press('Escape');
  assert.equal(await presetSummary.evaluate((el) => el === document.activeElement), true);
  same(await state(), popupState, 'closing Presets changes no camera or history');
  await revealNodeGroups(page);
  await popupInside('.boundary-popover[open] > .boundary-group-content');
  await page.locator('[data-node-group-name]').focus();
  assert.equal(await page.locator('[data-node-group-name]').evaluate((el) => el === document.activeElement), true);
  const groupsSummary = page.locator('[data-node-groups] > summary');
  await groupsSummary.focus();
  await groupsSummary.press('Escape');
  assert.equal(await groupsSummary.evaluate((el) => el === document.activeElement), true);
  same(await state(), popupState, 'closing Groups changes no camera or history');
  await page.screenshot({ path: out + '/boundary-short-split.png', animations: 'disabled' });
  // The visible Artboard controls address the active Composition, not its selected layer.
  await activateCommand(page, 'add-composition');
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const projectBefore = await page.evaluate(() => window.nect.projectSnapshot());
  const activeId = await page.evaluate(() => window.nect.activeCompositionId);
  const target = projectBefore.compositions.find((composition) => composition.id === activeId);
  assert.ok((await page.locator('.artboard-settings').innerText()).includes(target.name));
  const width = page.locator('[data-dimension="width"]');
  await width.fill(String(target.width + 13));
  await width.press('Tab');
  const projectAfter = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(
    projectAfter.compositions.find((composition) => composition.id === activeId).width,
    target.width + 13,
  );
  assert.deepEqual(
    projectAfter.compositions.filter((composition) => composition.id !== activeId),
    projectBefore.compositions.filter((composition) => composition.id !== activeId),
  );
  assert.deepEqual(errors, []);
  await writeFile(
    out + '/result.json',
    JSON.stringify(
      {
        ok: true,
        errors,
        motion: {
          supportsDiscrete,
          duration: motion.duration,
          firstMotionEvents,
          reopenedMotionEvents,
          portalDelta: motion.portalDelta,
        },
        peek: { idleWidth, nearWidth, leaveDelayMs: 300 },
        evidenceLimit:
          'Transition events, computed timing and stills; no video or native WebView audition.',
      },
      null,
      2,
    ),
  );
  console.log(
    'workspace overlay PASS: stable fit/manual geometry, panel state, boundaries and asset identity',
  );
} catch (error) {
  await page.screenshot({ path: out + '/failure.png' }).catch(() => {});
  await writeFile(
    out + '/result.json',
    JSON.stringify({ ok: false, errors, failure: String(error) }, null, 2),
  ).catch(() => {});
  throw error;
} finally {
  await browser.close();
}
