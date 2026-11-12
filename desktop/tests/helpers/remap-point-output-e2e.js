import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { ensureWorkspacePanel } from './workspace-panels.js';
import { activateCommand } from '../command-menu-actions.js';

export async function verifyRemapPointOutput(page) {
  const fixture = await page.evaluate(async () => {
    const { blankDocument, node, point, connect } = await import('/src/core.js');
    const doc = blankDocument(),
      layer = doc.layers[0],
      curve = node('Curve', 40, 50),
      remap = node('Path Remap', 360, 50),
      fill = node('Fill', 680, 50),
      out = node('Output', 1000, 50),
      first = point(100, 100),
      second = point(300, 100);
    first.handleB.value = [0.1, 0];
    curve.points = [first, second, point(100, 300)];
    remap.edits = [{ pointId: first.id, handleB: [0.2, 0] }];
    layer.nodes = [curve, remap, fill, out];
    layer.output = out.id;
    connect(layer, curve.id, remap.id, 'path');
    connect(layer, remap.id, fill.id, 'path');
    connect(layer, fill.id, out.id, 'path');
    window.nect.load(JSON.stringify(doc));
    return {
      layer: layer.id,
      curve: curve.id,
      remap: remap.id,
      point: first.id,
      second: second.id,
    };
  });
  const panel = () => page.locator(`.inspector [data-point-panel="${fixture.remap}"]`),
    field = (key, component) =>
      panel().locator(
        `[data-point="${fixture.point}"][data-remap-output="${key}"][data-remap-output-component="${component}"]`,
      ),
    snapshot = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    output = () =>
      page.evaluate(async ({ remap, point }) => {
        const { evaluate } = await import('/src/core.js'),
          doc = window.nect.document,
          path = evaluate(doc.layers[0], {}, doc.imageAssets, doc).run(remap);
        return path.contours.flatMap((contour) => contour.points).find((item) => item.id === point);
      }, fixture),
    settle = () =>
      page.evaluate(
        () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
      );
  const selectRemap = async () => {
    await page.locator(`.graph .node[data-id="${fixture.remap}"] .node-head strong`).click();
    await ensureWorkspacePanel(page, 'inspector');
    if (!(await field('handleB', 'length').count()))
      await panel().getByRole('button', { name: 'Browse all points', exact: true }).click();
    await panel().locator(`[data-select-panel-point="${fixture.point}"]`).click();
  };
  await selectRemap();
  // Typing the same rounded display value is an explicit edit; merely focusing
  // or pressing Enter on the untouched display must not quantize the point.
  const beforeTinySource = await snapshot();
  await page.evaluate(({ layer, curve, point }) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer, nodeId: curve, key: `${point}.handleB`, value: [-0.1996, 0] },
    });
  }, fixture);
  await settle();
  assert.equal(await field('handleB', 'length').inputValue(), '0');
  const beforeTinyEdit = await snapshot();
  await field('handleB', 'length').focus();
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.equal(await snapshot(), beforeTinyEdit);
  await field('handleB', 'length').press('Control+A');
  await field('handleB', 'length').press('0');
  await field('handleB', 'length').press('/');
  await field('handleB', 'length').press('0');
  await field('handleB', 'length').press('Enter');
  assert.match(await page.locator('.status .message').innerText(), /Invalid calculation/);
  await field('handleB', 'length').blur();
  await settle();
  assert.equal(await snapshot(), beforeTinyEdit, 'invalid expression and blur do not quantize');
  await field('handleB', 'length').focus();
  await field('handleB', 'length').press('Control+A');
  await field('handleB', 'length').press('0');
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.deepEqual((await output()).handleB, [0, 0], 'explicit zero clears the rounded handle');
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeTinyEdit);
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeTinySource);
  await selectRemap();

  await page.evaluate(({ layer, curve, point }) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer, nodeId: curve, key: `${point}.position`, value: [100.0004, 100] },
    });
  }, fixture);
  await settle();
  assert.equal(await field('position', 'x').inputValue(), '100');
  const beforeTinyPosition = await snapshot();
  await field('position', 'x').focus();
  await field('position', 'x').press('Control+A');
  await field('position', 'x').pressSequentially('100');
  await page.evaluate(({ layer, curve }) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer, nodeId: curve, key: 'closed', value: false },
    });
  }, fixture);
  const intervening = await snapshot();
  await field('position', 'x').press('Enter');
  await settle();
  assert.equal(
    await snapshot(),
    intervening,
    'a same-display draft cannot target a newer revision',
  );
  assert.match(await page.locator('.status .message').innerText(), /Point changed/);
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeTinyPosition);
  await selectRemap();
  await field('position', 'x').focus();
  await field('position', 'x').press('Control+A');
  await field('position', 'x').pressSequentially('100');
  await field('position', 'x').blur();
  await settle();
  assert.equal(
    (await output()).position[0],
    100,
    'blur commits the explicitly typed rounded value',
  );
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeTinyPosition);
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeTinySource);
  await selectRemap();

  await page.setViewportSize({ width: 1000, height: 800 });
  const lastHandle = field('handleB', 'length'),
    rawDelta = panel().locator(
      `[data-remap="${fixture.point}"][data-component="handleB"][data-axis="1"]`,
    );
  for (const control of [lastHandle, rawDelta]) {
    await control.evaluate((el) => el.scrollIntoView({ block: 'center', inline: 'nearest' }));
    assert.equal(
      await control.evaluate((el) => {
        const rect = el.getBoundingClientRect();
        return document.elementFromPoint(rect.x + rect.width / 2, rect.y + rect.height / 2) === el;
      }),
      true,
      'the final output and existing delta controls remain reachable at 1000 × 800',
    );
  }
  await page.setViewportSize({ width: 1600, height: 1000 });
  assert.equal(Number(await field('handleB', 'length').inputValue()), 0.3);
  const before = await snapshot(),
    revision = await page.evaluate(() => window.nect.commandState.expectedRevision);
  await field('handleB', 'length').fill('0');
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.deepEqual((await output()).handleB, [0, 0]);
  assert.equal(await field('handleB', 'angle').isDisabled(), true);
  assert.equal(await page.evaluate(() => window.nect.commandState.expectedRevision), revision + 1);
  const zero = await snapshot();
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), before);
  assert.deepEqual((await output()).handleB, [0.30000000000000004, 0]);
  await page.locator('[data-action="redo"]').click();
  await settle();
  assert.equal(await snapshot(), zero);
  assert.equal(await field('handleB', 'angle').isDisabled(), true);

  await field('handleB', 'length').fill('20');
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.deepEqual((await output()).handleB, [20, 0]);
  const beforeEquivalent = await snapshot(),
    equivalentRevision = await page.evaluate(() => window.nect.commandState.expectedRevision);
  await field('handleB', 'length').fill('20.0');
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.equal(await snapshot(), beforeEquivalent);
  assert.equal(
    await page.evaluate(() => window.nect.commandState.expectedRevision),
    equivalentRevision,
  );
  await field('handleB', 'length').fill('0');
  await field('handleB', 'length').press('Enter');
  await settle();
  assert.deepEqual((await output()).handleB, [0, 0], 'typed delta can also reach exact zero');
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await output()).handleB, [20, 0]);
  await field('position', 'x').fill('123.25');
  await field('position', 'x').press('Enter');
  await settle();
  assert.equal((await output()).position[0], 123.25);
  assert.equal(
    await page
      .locator(
        `.graph [data-point="${fixture.point}"][data-remap-output="position"][data-remap-output-component="x"]`,
      )
      .inputValue(),
    await field('position', 'x').inputValue(),
  );
  const beforeSource = await snapshot();
  await page.evaluate(({ layer, curve, point }) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer, nodeId: curve, key: `${point}.handleB`, value: [2, 3] },
    });
  }, fixture);
  const followed = (await output()).handleB;
  assert.ok(Math.abs(followed[0] - 21.9) < 1e-12 && followed[1] === 3);
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal(await snapshot(), beforeSource);
  assert.deepEqual((await output()).handleB, [20, 0]);

  const input = field('position', 'y'),
    original = Number(await input.inputValue());
  await input.scrollIntoViewIfNeeded();
  const box = await input.boundingBox();
  await page.mouse.move(box.x + 12, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + 36, box.y + box.height / 2, { steps: 4 });
  await page.mouse.up();
  await settle();
  assert.equal((await output()).position[1], original + 20);
  await page.locator('[data-action="undo"]').click();
  await settle();
  assert.equal((await output()).position[1], original);

  // A dirty field retained across selection A → A+B → A keeps its original
  // selection epoch and cannot apply after the intervening selection.
  await field('position', 'x').fill('777');
  const beforeSelection = await snapshot();
  await page.evaluate(({ remap, second }) => {
    const graph = () => document.querySelector(`.graph [data-point-panel="${remap}"]`);
    const browse = graph().querySelector('[data-point-panel-mode]');
    if (browse?.textContent.includes('Browse')) browse.click();
    for (let i = 0; i < 2; i++) {
      const button = graph().querySelector(`[data-select-panel-point="${second}"]`);
      if (!button) throw Error('Second point is not available');
      button.dispatchEvent(new MouseEvent('click', { bubbles: true, shiftKey: true }));
    }
  }, fixture);
  await field('position', 'x').press('Enter');
  await settle();
  assert.equal(await snapshot(), beforeSelection);
  assert.match(await page.locator('.status .message').innerText(), /Point changed/);

  // A dirty focused field cannot retarget after a different authored revision.
  await field('position', 'x').fill('777');
  await page.evaluate(({ layer, curve }) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer, nodeId: curve, key: 'closed', value: false },
    });
  }, fixture);
  const newer = await snapshot();
  await field('position', 'x').press('Enter');
  await settle();
  assert.equal(await snapshot(), newer);
  assert.match(await page.locator('.status .message').innerText(), /Point changed/);

  const saved = await snapshot(),
    downloading = page.waitForEvent('download');
  await activateCommand(page, 'save');
  await mkdir('test-results', { recursive: true });
  await (await downloading).saveAs('test-results/remap-point-output-project.json');
  await page.reload();
  await page.waitForFunction(() => !!window.nect);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await page.locator('dialog[open] button[value="discard"]').click();
  await (await chooser).setFiles('test-results/remap-point-output-project.json');
  await page.waitForFunction(
    (serialized) => JSON.stringify(window.nect.projectSnapshot()) === serialized,
    saved,
  );
  await selectRemap();
  assert.equal((await output()).position[0], 123.25);
  assert.deepEqual((await output()).handleB, [20, 0]);
  await field('position', 'y').fill('140');
  await field('position', 'y').press('Enter');
  await settle();
  assert.equal((await output()).position[1], 140);
}
