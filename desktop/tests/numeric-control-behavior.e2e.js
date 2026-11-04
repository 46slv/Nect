import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
// These geometry-only fixtures do not need font binaries.
await page.route(/\.(ttf|otf|ttc|otc|woff2?)(\?|$)/i, (route) => route.abort());
const errors = [],
  results = [];
page.on('pageerror', (error) => errors.push(error.message));
const output = 'test-results/numeric-control-behavior';
await fs.mkdir(output, { recursive: true });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await ensureWorkspacePanel(page, 'inspector');
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const state = () => page.evaluate(() => window.nect.commandState);
  const load = async (points = false) => {
    const f = compoundFixture([8, 5, 7, 4]);
    f.circle = node('Circle', 500, 40);
    f.fill.position[0] = 900;
    f.output.position[0] = 1200;
    f.layer.nodes.push(f.circle);
    f.ids = [f.path.contours[0].points[1], f.path.contours[3].points[2]];
    await page.evaluate((text) => window.nect.load(text), serialize(f.doc));
    const id = points ? f.path.id : f.circle.id;
    await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
    if (points) {
      for (const [index, point] of f.ids.entries())
        await page
          .locator(`.canvas-stage .point-hit[data-point="${point}"]:not([data-handle])`)
          .click({ modifiers: index ? ['Shift'] : [] });
      await page.locator('.inspector-body [data-point-delta-panel]').waitFor();
    }
    return f;
  };
  const radius = (f) =>
    page.locator(
      `.inspector-body .field[data-node="${f.circle.id}"][data-key="radius"] input[data-value]`,
    );
  const radiusValue = (f) =>
    page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value,
      f.circle.id,
    );
  const delta = (axis = 0) => page.locator(`.inspector-body [data-point-delta-axis="${axis}"]`);
  const apply = () => page.locator('.inspector-body [data-apply-point-delta]').click();
  const undo = () => page.locator('[data-action="undo"]').click();
  const redo = () => page.locator('[data-action="redo"]').click();
  const start = async (input) => {
    await input.scrollIntoViewIfNeeded();
    const b = await input.boundingBox(),
      x = b.x + b.width * 0.4,
      y = b.y + b.height / 2;
    assert.equal(
      await input.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
      'real mouse hits the number',
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    return { x, y, handle: await input.elementHandle() };
  };
  const move = (p, dx) => page.mouse.move(p.x + dx, p.y, { steps: 5 });

  const f = await load();
  await radius(f).click();
  assert.equal(await radius(f).evaluate((el) => el === document.activeElement), true);
  await radius(f).fill('130.25');
  await radius(f).press('Tab');
  assert.equal(await radiusValue(f), 130.25);
  await radius(f).press('ArrowUp');
  const arrowValue = Number(await radius(f).inputValue());
  await radius(f).press('Tab');
  assert.ok(arrowValue > 130.25);
  assert.equal(await radiusValue(f), arrowValue);
  await undo();
  assert.equal(await radiusValue(f), 130.25);
  const liveBefore = await saved(),
    liveState = await state(),
    live = await start(radius(f));
  await move(live, 24);
  assert.equal(
    await live.handle.evaluate((el) => el.isConnected && el === document.activeElement),
    true,
  );
  assert.equal(await radius(f).inputValue(), '150.25');
  assert.equal(await saved(), liveBefore);
  await page.screenshot({ path: `${output}/live-preview.png` });
  await page.mouse.up();
  assert.equal(await radiusValue(f), 150.25);
  assert.equal((await state()).expectedRevision, liveState.expectedRevision + 1);
  const liveAfter = await saved();
  await undo();
  assert.equal(await saved(), liveBefore);
  const cancelState = await state(),
    cancel = await start(radius(f));
  await move(cancel, 24);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.equal(await saved(), liveBefore);
  assert.deepEqual(await state(), cancelState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await redo();
  assert.equal(await saved(), liveAfter);
  const pendingState = await state();
  await radius(f).fill('160.75');
  const pendingTyped = await start(radius(f));
  await move(pendingTyped, 24);
  assert.equal(await radiusValue(f), 160.75, 'pending typing commits before the scrub snapshot');
  await page.mouse.up();
  assert.equal(await radiusValue(f), 180.75);
  assert.equal((await state()).expectedRevision, pendingState.expectedRevision + 2);
  await undo();
  assert.equal(await radiusValue(f), 160.75);
  await undo();
  assert.equal(await saved(), liveAfter);
  results.push({
    case: 'Circle Radius',
    typingAndNativeArrow: true,
    detachedPreview: true,
    connectedFocusedTarget: true,
    oneUndoRedo: true,
    pendingTypedSeparateUndo: true,
    cancelPreservesRedo: true,
  });

  const p = await load(true),
    before = await saved(),
    original = await state();
  await delta().fill('12.5000');
  await delta(1).fill('-7.2500');
  await delta().press('ArrowUp');
  assert.ok(Number(await delta().inputValue()) > 12.5);
  assert.equal(await saved(), before);
  assert.deepEqual(await state(), original);
  await delta().fill('12.5000');
  const staged = await start(delta());
  await move(staged, 24);
  assert.equal(
    await staged.handle.evaluate((el) => el.isConnected && el === document.activeElement),
    true,
  );
  assert.equal(await delta().inputValue(), '32.5');
  assert.equal(await page.locator('.graph [data-point-delta-axis="0"]').inputValue(), '32.5');
  await page.keyboard.press('Enter');
  assert.equal(await saved(), before);
  assert.deepEqual(await state(), original);
  await page.mouse.up();
  assert.equal(await saved(), before);
  await page.screenshot({ path: `${output}/staged-preview.png` });
  await apply();
  assert.equal((await state()).expectedRevision, original.expectedRevision + 1);
  const after = await saved(),
    changed = JSON.parse(after).layers[0].nodes.find((n) => n.id === p.path.id);
  for (const point of p.path.points) {
    const expected = structuredClone(point);
    if (p.ids.includes(point.id))
      expected.position.value = point.position.value.map(
        (value, axis) => value + [32.5, -7.25][axis],
      );
    assert.deepEqual(
      changed.points.find((entry) => entry.id === point.id),
      expected,
    );
  }
  await undo();
  assert.equal(await saved(), before);
  await delta().fill('12.5000');
  const stagedCancelState = await state(),
    stagedCancel = await start(delta());
  await move(stagedCancel, 24);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.equal(await delta().inputValue(), '12.5000');
  assert.equal(await saved(), before);
  assert.deepEqual(await state(), stagedCancelState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await redo();
  assert.equal(await saved(), after);
  results.push({
    case: 'Point Delta',
    typingAndNativeArrow: true,
    stagedUntilApply: true,
    peerDisplay: true,
    connectedFocusedTarget: true,
    enterCannotApplyDuringScrub: true,
    oneUndoRedo: true,
    otherPointsPreserved: true,
    cancelPreservesExactDraftAndRedo: true,
  });

  const noop = await load(true);
  await delta().fill('12.5000');
  const noopBefore = await saved(),
    noopState = await state();
  for (const returnToOrigin of [false, true]) {
    const gesture = await start(delta());
    await move(gesture, returnToOrigin ? 24 : 3);
    if (returnToOrigin) await move(gesture, 0);
    await page.mouse.up();
    assert.equal(await delta().inputValue(), '12.5000');
    assert.equal(await saved(), noopBefore);
    assert.deepEqual(await state(), noopState);
  }
  await delta().fill('8');
  const selected = await start(delta());
  await move(selected, 24);
  await page.mouse.up();
  await page
    .locator(
      `.canvas-stage .point-hit[data-point="${noop.path.contours[1].points[0]}"]:not([data-handle])`,
    )
    .click();
  assert.equal(await delta().inputValue(), '0');
  await apply();
  assert.equal(await saved(), noopBefore);
  await delta().fill('8');
  const stale = await start(delta());
  await move(stale, 24);
  await page.mouse.up();
  await page.evaluate(
    ({ layerId, nodeId, pointId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'translate-points',
        payload: { layerId, nodeId, pointIds: [pointId], delta: [1, 0] },
      }),
    { layerId: noop.layer.id, nodeId: noop.path.id, pointId: noop.path.contours[2].points[0] },
  );
  const staleDoc = await saved(),
    staleState = await state();
  await apply();
  assert.equal(await saved(), staleDoc);
  assert.deepEqual(await state(), staleState);
  results.push({
    case: 'Point Delta interruption',
    noOpAndReturnToOrigin: true,
    selectionDiscardsDraft: true,
    staleRevisionRefused: true,
  });
  assert.deepEqual(errors, []);
  await fs.writeFile(
    `${output}/results.json`,
    JSON.stringify({ browser: await browser.version(), results, pageErrors: errors }, null, 2),
  );
  console.log(
    'P1 numeric behavior Chrome PASS: live scalar and staged Point Delta, native typing/arrows, guards, focus, one Undo and Redo preservation',
  );
} finally {
  await browser.close();
}

// Exercise the separate staged Remap adapter with its own isolated browser fixture.
await import('./remap-batch-scrub.e2e.js');
