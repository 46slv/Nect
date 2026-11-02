import assert from 'node:assert/strict';
import { document as makeDocument, node, connect } from '../../src/core.js';
import { ensureWorkspacePanel } from './workspace-panels.js';
import { activateCommand } from '../command-menu-actions.js';

// Real native controls, alongside the DOM handler regressions.
export async function verifyNodeBodySelection(page) {
  await page.setViewportSize({ width: 1920, height: 1200 });
  const evidence = { pan: [], stationary: [], syntheticComposition: false };
  const card = (id) => page.locator('.graph .node[data-id="' + id + '"]');
  const state = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      revision: window.nect.commandState.expectedRevision,
    }));
  const selected = () =>
    page.locator('.graph .node.selected').evaluateAll((nodes) => nodes.map((n) => n.dataset.id));
  const scroll = () =>
    page.locator('.graph-scroll').evaluate((el) => [el.scrollLeft, el.scrollTop]);
  async function load(text = false) {
    const doc = makeDocument(),
      layer = doc.layers[0],
      a = node('Circle', 100, 70),
      b = node(text ? 'Text' : 'Circle', 500, 70),
      fill = node('Fill', 900, 70),
      out = node('Output', 2300, 900);
    a.name = 'Body A';
    b.name = 'Body B';
    a.params.radius.value = 101;
    if (text) b.text = 'Initial text';
    else b.params.radius.value = 303;
    layer.nodes = [a, b, fill, out];
    layer.output = out.id;
    connect(layer, a.id, fill.id, 'path');
    connect(layer, fill.id, out.id, 'path');
    await page.evaluate((s) => window.nect.load(s), JSON.stringify(doc));
    await card(a.id).locator('.node-head strong').click();
    return { a, b, fill };
  }
  async function inspector(name) {
    await page.waitForFunction(
      (value) => document.querySelector('.inspector-body h3')?.textContent === value,
      name,
    );
  }
  async function nonControlPoint(target) {
    await target.scrollIntoViewIfNeeded();
    return target.evaluate((el) => {
      const r = el.getBoundingClientRect();
      for (let y = r.top + 2; y < Math.min(r.bottom - 1, r.top + 100); y += 3)
        for (let x = r.left + 2; x < r.right - 1; x += 3) {
          const hit = document.elementFromPoint(x, y);
          if (
            el.contains(hit) &&
            hit.closest('.node-body') &&
            !hit.closest(
              'button,input,textarea,select,label,summary,a,[contenteditable]:not([contenteditable="false"])',
            )
          )
            return { x, y };
        }
      throw Error('No non-control node body point');
    });
  }
  for (const surface of ['.node-body', '.field']) {
    for (const chord of ['Space', 'Control+Shift', 'Meta+Shift']) {
      const { a, b } = await load();
      await page.locator('.graph-scroll').focus();
      const point = await nonControlPoint(card(b.id).locator(surface).first()),
        before = await state(),
        start = await scroll(),
        keys = chord.split('+');
      await page.mouse.move(point.x, point.y);
      for (const key of keys) await page.keyboard.down(key);
      try {
        await page.mouse.down();
        await page.mouse.move(point.x - 80, point.y - 40, { steps: 5 });
        await page.mouse.up();
      } finally {
        await page.mouse.up();
        for (const key of keys.reverse()) await page.keyboard.up(key);
      }
      assert.deepEqual(await selected(), [a.id], chord + ' keeps selection');
      const end = await scroll();
      assert.deepEqual(
        end.map((n, i) => n - start[i]),
        [80, 40],
      );
      assert.deepEqual(await state(), before, 'view pan never authors/history');
      evidence.pan.push({ surface, chord, scrollDelta: [80, 40], selectionPreserved: true });
    }
  }

  await ensureWorkspacePanel(page, 'inspector');
  for (const kind of ['label', 'input']) {
    const { b } = await load(),
      field = card(b.id).locator('.field[data-key="radius"]'),
      target = field.locator(kind === 'label' ? 'label' : 'input[type="number"]').first(),
      original = await target.elementHandle(),
      before = await state();
    await inspector('Body A');
    await target.click();
    await inspector('Body B');
    assert.match(await page.locator('.hint > span:last-child').innerText(), /Body B/);
    assert.deepEqual(await selected(), [b.id]);
    assert.equal(await original.evaluate((el) => el.isConnected), true);
    if (kind === 'input') {
      assert.equal(await original.evaluate((el) => document.activeElement === el), true);
      assert.equal(await target.inputValue(), '303');
    }
    assert.deepEqual(await state(), before);
    evidence.stationary.push({ kind, inspector: 'Body B', targetRetained: true });
  }

  {
    const { b } = await load(),
      before = await state(),
      target = card(b.id).locator('.field[data-key="radius"] input[type="number"]').first(),
      original = await target.elementHandle();
    await page.locator('.inspector-body .field[data-key="radius"] input').first().focus();
    await target.click();
    await inspector('Body B');
    assert.equal(
      await original.evaluate((el) => el.isConnected && document.activeElement === el),
      true,
    );
    assert.deepEqual(await state(), before);
    evidence.inspectorFocusTransfer = true;
  }
  {
    const { a, b } = await load(),
      before = await state();
    await page.locator('.inspector-body .field[data-key="radius"] input').first().fill('151');
    await card(b.id).locator('.field[data-key="radius"] label').click();
    await inspector('Body B');
    assert.equal(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value,
        a.id,
      ),
      151,
    );
    assert.equal((await state()).revision, before.revision + 1);
    await activateCommand(page, 'undo');
    assert.equal((await state()).doc, before.doc);
    evidence.previousInspectorDraft = { nativeBlurCommitted: true, oneUndo: true };
  }

  // Selection must not replace a graph input that already owns a native draft.
  {
    const { b } = await load(),
      input = card(b.id).locator('.field[data-key="radius"] input[type="number"]').first(),
      before = await state();
    await input.fill('444');
    const original = await input.elementHandle();
    await input.click();
    await inspector('Body B');
    assert.equal(
      await original.evaluate((el) => el.isConnected && document.activeElement === el),
      true,
    );
    assert.equal(await input.inputValue(), '444');
    assert.deepEqual(await state(), before);
    await input.press('Tab');
    await page.waitForFunction(
      (id) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value === 444,
      b.id,
    );
    assert.equal((await state()).revision, before.revision + 1);
    await activateCommand(page, 'undo');
    assert.equal((await state()).doc, before.doc);
    evidence.nativeDraft = { retained: true, oneCommitUndo: true };
  }

  // Numeric capture survives the Inspector-only view update.
  {
    const { b } = await load(),
      input = card(b.id).locator('.field[data-key="radius"] input[type="number"]').first(),
      original = await input.elementHandle(),
      before = await state(),
      box = await input.boundingBox();
    await page.mouse.move(box.x + 8, box.y + box.height / 2);
    await page.mouse.down();
    await inspector('Body B');
    assert.equal(await original.evaluate((el) => el.isConnected), true);
    await page.mouse.move(box.x + 36, box.y + box.height / 2, { steps: 5 });
    assert.deepEqual(await state(), before, 'scrub preview is unauthored');
    await page.mouse.up();
    assert.equal((await state()).revision, before.revision + 1);
    assert.notEqual(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value,
        b.id,
      ),
      303,
    );
    await activateCommand(page, 'undo');
    assert.equal((await state()).doc, before.doc);
    evidence.scrub = { captureRetained: true, oneUndo: true };
  }

  {
    const { fill } = await load(),
      lock = card(fill.id).locator('.field[data-key="color"] [data-lock]'),
      original = await lock.elementHandle(),
      before = await state(),
      box = await lock.boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.down();
    await inspector(fill.name);
    assert.equal(await original.evaluate((el) => el.isConnected), true);
    await page.mouse.up();
    assert.equal(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.color.locked,
        fill.id,
      ),
      true,
    );
    await activateCommand(page, 'undo');
    assert.equal((await state()).doc, before.doc);
    evidence.button = { nativeClickRetained: true, oneUndo: true };
  }

  {
    const { fill } = await load(),
      input = card(fill.id).locator('.field[data-key="path"] .input'),
      original = await input.elementHandle(),
      before = await state(),
      from = await input.boundingBox(),
      to = await page.locator('.graph-scroll').boundingBox();
    await page.mouse.move(from.x + from.width / 2, from.y + from.height / 2);
    await page.mouse.down();
    await inspector(fill.name);
    assert.equal(await original.evaluate((el) => el.isConnected), true);
    await page.mouse.move(to.x + 20, to.y + 20, { steps: 5 });
    await page.mouse.up();
    await page.waitForFunction(
      (id) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).params.path.source ===
        undefined,
      fill.id,
    );
    assert.equal((await state()).revision, before.revision + 1);
    await activateCommand(page, 'undo');
    assert.equal((await state()).doc, before.doc);
    evidence.socket = { inputCaptureRetained: true, disconnectAndUndo: true };
  }

  {
    const { b } = await load(true),
      input = card(b.id).locator('[data-text="' + b.id + '"]'),
      original = await input.elementHandle(),
      before = await state();
    await input.focus();
    await input.dispatchEvent('compositionstart', { data: '' });
    await input.evaluate((el) => {
      el.value = '未確定 draft';
      el.dispatchEvent(
        new InputEvent('input', {
          bubbles: true,
          inputType: 'insertCompositionText',
          data: '未確定',
          isComposing: true,
        }),
      );
    });
    await input.click();
    await inspector('Body A');
    assert.equal(
      await original.evaluate((el) => el.isConnected && document.activeElement === el),
      true,
    );
    assert.equal(await input.inputValue(), '未確定 draft');
    assert.deepEqual(await state(), before);
    await input.dispatchEvent('compositionend', { data: '未確定' });
    await page.evaluate(() => new Promise(requestAnimationFrame));
    const afterComposition = await input.elementHandle();
    await input.click();
    await inspector('Body B');
    assert.equal(
      await afterComposition.evaluate((el) => el.isConnected && document.activeElement === el),
      true,
    );
    assert.equal(await input.inputValue(), '未確定 draft');
    assert.deepEqual(await state(), before);
    await page.evaluate(() => new Promise(requestAnimationFrame));
    assert.match(await page.locator('.hint > span:last-child').innerText(), /Body B/);
    evidence.syntheticComposition = true;
    evidence.physicalOSIME = 'NOT_RUN';
  }
  await page.screenshot({ path: 'test-results/node-body-selection.png', fullPage: true });
  {
    const { b } = await load(),
      before = await state();
    await card(b.id).locator('.field[data-key="radius"] label').click();
    await inspector('Body B');
    await page.locator('.inspector-body [data-assign-viewer="0"]').click();
    await page.waitForFunction(() =>
      document.querySelector('.node-viewer [data-viewer-label]')?.textContent.includes('Body B'),
    );
    assert.deepEqual(await state(), before);
    evidence.viewerAssignment = { selectedOwner: 'Body B', unauthored: true };
  }
  return evidence;
}
