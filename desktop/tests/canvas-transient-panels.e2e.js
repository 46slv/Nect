import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
try {
  for (const [panel, composing] of ['align', 'layout', 'grid', 'guides'].flatMap((panel) =>
    [false, true].map((composing) => [panel, composing]),
  )) {
    const page = await browser.newPage({ viewport: { width: 1500, height: 1000 } });
    await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
    await page.waitForFunction(() => window.nect);
    const editor = page.getByRole('textbox', { name: 'Edit canvas text', exact: true });
    await page.evaluate(async () => {
      const { document, node, connect } = await import('/src/core.js');
      const doc = document(),
        layer = doc.layers[0],
        text = node('Text'),
        fill = node('Fill'),
        output = node('Output');
      text.text = 'Nect\nText';
      text.params.position.value = [200, 220];
      text.params.size.value = 72;
      layer.nodes = [text, fill, output];
      layer.output = output.id;
      connect(layer, text.id, fill.id, 'path');
      connect(layer, fill.id, output.id, 'path');
      window.nect.load(JSON.stringify(doc));
    });
    await page.waitForFunction(() => window.nect.evaluate().error === null);
    const open = async () => {
      const point = await page.locator('.artboard .shape').evaluate((shape) => {
        const path = shape.matches('path') ? shape : shape.querySelector('path'),
          box = path.getBBox();
        for (let y = box.y + 2; y < box.y + box.height; y += 3)
          for (let x = box.x + 2; x < box.x + box.width; x += 3)
            if (path.isPointInFill(new DOMPoint(x, y))) {
              const p = new DOMPoint(x, y).matrixTransform(path.getScreenCTM());
              return [p.x, p.y];
            }
      });
      assert.ok(point);
      await page.mouse.dblclick(...point);
      await editor.waitFor();
    };
    const frames = () =>
      page.evaluate(
        () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
      );
    const panelSelector = panel === 'align' ? '.alignment-panel' : '.guides-panel';
    const panelButton = `.canvas-toolbar [data-action="${panel}-panel"]`;
    const expanded = panel === 'align' ? 'aria-pressed' : 'aria-expanded';
    if ((await page.locator(panelButton).getAttribute(expanded)) !== 'true')
      await page.locator(panelButton).click();
    if (panel === 'layout') await page.getByLabel('Preview layout', { exact: true }).check();
    // Reopening a canceled direct edit retains its view-only panel state.
    await open();
    await editor.press('Control+End');
    await page.keyboard.type('discard');
    await editor.press('Escape');
    await open();
    assert.equal(await page.locator(panelSelector).count(), 1, `${panel} is open during editing`);
    if (panel === 'layout') assert.ok((await page.locator('.layout-preview').count()) > 0);
    await editor.press('Control+End');
    await page.keyboard.type('!');
    if (composing) {
      await editor.dispatchEvent('compositionstart');
      await editor.evaluate((el) => {
        el.value += 'B';
        el.dispatchEvent(
          new InputEvent('input', {
            bubbles: true,
            isComposing: true,
            inputType: 'insertCompositionText',
          }),
        );
      });
    }
    const before = await page.evaluate(() => ({
      document: window.nect.document,
      revision: window.nect.commandState.expectedRevision,
    }));
    await editor.evaluate((el) => {
      window.selectionEditor = el;
      window.editorBlurCount = 0;
      el.addEventListener('focusout', () => window.editorBlurCount++);
      document.addEventListener('click', (event) => {
        if (event.target === el)
          window.previewCountAfterDismissal = document.querySelectorAll('.layout-preview').length;
      });
    });
    const box = await editor.boundingBox();
    await page.mouse.move(box.x + 5, box.y + 22);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width - 8, box.y + 22, { steps: 8 });
    await page.mouse.up();
    assert.equal(
      await page.evaluate(() => window.previewCountAfterDismissal),
      0,
      'click dismissal synchronously clears layout preview',
    );
    await frames();
    assert.equal(await editor.count(), 1, 'panel dismissal must not detach the active editor');
    assert.equal(
      await editor.evaluate((el) => el === window.selectionEditor && el === document.activeElement),
      true,
    );
    assert.equal(await editor.evaluate((el) => el.selectionEnd > el.selectionStart), true);
    assert.equal(await page.evaluate(() => window.editorBlurCount), 0);
    assert.equal(
      await page.locator(panelSelector).count(),
      0,
      `outside click still closes ${panel}`,
    );
    assert.equal(await page.locator(panelButton).getAttribute(expanded), 'false');
    assert.equal(
      await page.locator('.layout-preview').count(),
      0,
      'dismissal clears layout preview immediately',
    );
    assert.deepEqual(await page.evaluate(() => window.nect.document), before.document);
    if (composing) {
      await editor.dispatchEvent('compositionend');
      await frames();
      assert.equal(
        await editor.evaluate(
          (el) => el === window.selectionEditor && el === document.activeElement,
        ),
        true,
      );
      assert.equal(await editor.inputValue(), 'Nect\nText!B');
    }
    await editor.press('Control+Enter');
    await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
    const committed = await page.evaluate(() => ({
      document: window.nect.document,
      revision: window.nect.commandState.expectedRevision,
    }));
    assert.equal(
      committed.document.layers[0].nodes[0].text,
      composing ? 'Nect\nText!B' : 'Nect\nText!',
    );
    assert.equal(committed.revision, before.revision + 1, 'explicit completion commits once');
    await page.locator('[data-action="undo"]').click();
    assert.deepEqual(await page.evaluate(() => window.nect.document), before.document);
    await open();
    await editor.press('Control+End');
    await page.keyboard.type(' blur');
    const priorBlur = await page.evaluate(() => window.nect.commandState.expectedRevision);
    await page.locator('.graph-scroll').focus();
    await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
    assert.equal(
      await page.evaluate(() => window.nect.document.layers[0].nodes[0].text),
      'Nect\nText blur',
    );
    assert.equal(
      await page.evaluate(() => window.nect.commandState.expectedRevision),
      priorBlur + 1,
      'actual blur still commits once',
    );
    const afterBlur = await page.evaluate(() => window.nect.document);
    await page.locator(panelButton).click();
    assert.equal(await page.locator(panelButton).getAttribute(expanded), 'true');
    assert.equal(await page.locator(panelSelector).count(), 1, `${panel} reopens`);
    if (panel === 'layout') {
      await page.getByLabel('Preview layout', { exact: true }).check();
      assert.ok((await page.locator('.layout-preview').count()) > 0);
    }
    await page.locator(panelButton).click();
    assert.equal(await page.locator(panelButton).getAttribute(expanded), 'false');
    assert.equal(await page.locator(panelSelector).count(), 0);
    assert.equal(await page.locator('.layout-preview').count(), 0);
    assert.deepEqual(await page.evaluate(() => window.nect.document), afterBlur);
    await page.close();
  }
  console.log(
    'PASS transient panel dismissal preserves native canvas selection, focus, draft/composition and history; explicit completion and real blur each commit once',
  );
} finally {
  await browser.close();
}
