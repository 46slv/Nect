import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1500, height: 1000 } });
page.setDefaultTimeout(5000);
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
const drawing = () => page.locator('.artboard .shape').evaluate((el) => el.outerHTML);
const editor = page.getByRole('textbox', { name: 'Edit canvas text', exact: true });
const graphInput = page.locator('.graph textarea[data-text]');
async function setup(vertical = false, transformed = false) {
  await page.evaluate(
    async ({ vertical, transformed }) => {
      const { document, node, connect } = await import('/src/core.js');
      const doc = document(),
        l = doc.layers[0],
        text = node('Text', 50, 30),
        fill = node('Fill', 360, 30),
        out = node('Output', 680, 30),
        transform = node('Transform', 240, 400);
      text.text = vertical ? 'AB\nCD' : 'Nect\nText';
      text.params.size.value = 72;
      text.params.position.value = vertical ? [400, 100] : [200, 220];
      if (vertical) text.writingMode = 'vertical-rl';
      l.nodes = transformed ? [text, transform, fill, out] : [text, fill, out];
      l.output = out.id;
      if (transformed) {
        transform.params.rotation.value = 15;
        transform.params.scale.value = 1.2;
        connect(l, text.id, transform.id, 'path');
        connect(l, transform.id, fill.id, 'path');
      } else connect(l, text.id, fill.id, 'path');
      connect(l, fill.id, out.id, 'path');
      window.nect.load(JSON.stringify(doc));
    },
    { vertical, transformed },
  );
  await page.locator('.artboard .shape').waitFor();
}
async function open() {
  const box = await page
    .locator('.artboard .shape')
    .evaluate((el) => el.getBoundingClientRect().toJSON());
  // Use a real painted glyph point, including rotated/vertical output.
  const point = await page.evaluate(() => {
    const shape = document.querySelector('.artboard .shape');
    const path = shape.matches('path') ? shape : shape.querySelector('path'),
      box = path.getBBox(),
      ctm = path.getScreenCTM();
    for (let y = box.y + 2; y < box.y + box.height; y += 3)
      for (let x = box.x + 2; x < box.x + box.width; x += 3) {
        if (path.isPointInFill(new DOMPoint(x, y))) {
          const p = new DOMPoint(x, y).matrixTransform(ctm);
          return [p.x, p.y];
        }
      }
  });
  assert.ok(box.width > 0 && point);
  await page.mouse.dblclick(...point);
  await editor.waitFor();
  assert.equal(await page.locator('dialog[open]').count(), 0);
  assert.equal(await editor.evaluate((el) => getComputedStyle(el).opacity), '0');
  assert.equal(await page.locator('.canvas-text-caret').count(), 1);
}
async function frames() {
  await page.evaluate(
    () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
  );
}
async function undo() {
  await page.locator('[data-action="undo"]').click();
}
async function redo() {
  await page.locator('[data-action="redo"]').click();
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await setup();
  const before = await snapshot(),
    beforeDrawing = await drawing();
  await open();
  assert.equal(await snapshot(), before);
  assert.equal(await drawing(), beforeDrawing);
  await editor.press('Control+End');
  await page.keyboard.type('!');
  await frames();
  assert.equal(await editor.inputValue(), 'Nect\nText!');
  assert.equal(await graphInput.inputValue(), 'Nect\nText!');
  assert.equal(await snapshot(), before);
  const caretEnd = await page.evaluate(() => {
    const c = document.querySelector('.canvas-text-caret').getBBox(),
      path = document.querySelector('.artboard .shape'),
      shape = path.getBBox();
    let right = -Infinity;
    for (let y = shape.y + shape.height / 2 + 2; y <= shape.y + shape.height; y++)
      for (let x = shape.x; x <= shape.x + shape.width; x += 0.5)
        if (path.isPointInFill(new DOMPoint(x, y))) right = Math.max(right, x);
    const input = document.querySelector('.canvas-text-editor');
    return {
      x: c.x,
      right,
      caretIndex: document.querySelector('.canvas-text-caret').dataset.caretIndex,
      start: input.selectionStart,
      end: input.selectionEnd,
      value: input.value,
      geometry: document.querySelector('.canvas-text-selection').outerHTML.slice(0, 250),
      keyHandler: typeof input.onkeyup,
    };
  });
  assert.ok(
    caretEnd.x >= caretEnd.right && caretEnd.x - caretEnd.right < 16,
    JSON.stringify(caretEnd),
  );
  await editor.press('Backspace');
  await editor.press('Shift+Home');
  await frames();
  assert.equal(await page.locator('.canvas-text-selection polygon').count(), 4);
  await page.keyboard.type('Node');
  await editor.press('Enter');
  await page.keyboard.type('A');
  await frames();
  assert.equal(await editor.inputValue(), 'Nect\nNode\nA');
  const composingDrawing = await drawing();
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
  assert.equal(await drawing(), composingDrawing);
  await editor.dispatchEvent('compositionend');
  await frames();
  assert.notEqual(await drawing(), composingDrawing);
  await page.screenshot({ path: 'test-results/in-place-editing.png' });
  await editor.press('Escape');
  assert.equal(await snapshot(), before);
  assert.equal(await drawing(), beforeDrawing);
  await open();
  const textBox = await editor.boundingBox();
  await page.mouse.move(textBox.x + 5, textBox.y + 22);
  await page.mouse.down();
  await page.mouse.move(textBox.x + textBox.width - 8, textBox.y + 22, { steps: 8 });
  await page.mouse.up();
  await frames();
  assert.ok(await editor.evaluate((el) => el.selectionEnd > el.selectionStart));
  assert.ok((await page.locator('.canvas-text-selection polygon').count()) > 0);
  await editor.press('Control+A');
  await editor.press('Backspace');
  await frames();
  assert.equal(await editor.inputValue(), '');
  assert.equal(await page.locator('.canvas-text-caret').count(), 1);
  await page.keyboard.type('Fresh');
  assert.equal(await graphInput.inputValue(), 'Fresh');
  await editor.press('Escape');
  assert.equal(await snapshot(), before);
  await open();
  await editor.press('Control+A');
  await page.keyboard.type('Direct edit');
  await editor.press('Control+Enter');
  await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
  const edited = await snapshot();
  await undo();
  assert.equal(await snapshot(), before);
  await redo();
  assert.equal(await snapshot(), edited);
  for (const [vertical, transformed] of [
    [false, true],
    [true, false],
  ]) {
    await setup(vertical, transformed);
    await page.locator('[data-action="zoom-in"]').click();
    const stage = await page.locator('.canvas-stage').boundingBox();
    await page.keyboard.down('Space');
    await page.mouse.move(stage.x + stage.width - 80, stage.y + 100);
    await page.mouse.down();
    await page.mouse.move(stage.x + stage.width - 50, stage.y + 120);
    await page.mouse.up();
    await page.keyboard.up('Space');
    const prior = await snapshot();
    await open();
    await editor.press('Control+End');
    await page.keyboard.type('Z');
    await frames();
    assert.equal(await editor.evaluate((el) => el === document.activeElement), true);
    const caret = await page
      .locator('.canvas-text-caret')
      .evaluate((el) => el.getBoundingClientRect().toJSON());
    const input = await editor.boundingBox();
    assert.ok(
      caret.x >= input.x - 4 &&
        caret.x <= input.x + input.width + 4 &&
        caret.y >= input.y - 4 &&
        caret.y <= input.y + input.height + 4,
    );
    if (vertical) assert.ok(caret.width > caret.height);
    await page.screenshot({
      path: `test-results/in-place-${vertical ? 'vertical' : 'transformed'}.png`,
    });
    await editor.press('Control+Enter');
    await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
    const next = await snapshot();
    await undo();
    assert.equal(await snapshot(), prior);
    await redo();
    assert.equal(await snapshot(), next);
  }
  // Imported Windows line endings must survive opening/closing without an edit.
  await setup();
  await page.evaluate(() => {
    const layer = window.nect.document.layers[0];
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-text-content',
      payload: { layerId: layer.id, nodeId: layer.nodes[0].id, text: 'A\r\nB' },
    });
  });
  const crlf = await snapshot();
  await open();
  await editor.press('Control+Enter');
  await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
  assert.equal(await snapshot(), crlf, 'No-op direct editing preserves authored CRLF');
  await open();
  await editor.press('Control+End');
  await page.keyboard.type('C');
  await frames();
  assert.equal(await graphInput.inputValue(), 'A\nBC');
  assert.notEqual(await drawing(), beforeDrawing);
  await editor.press('Control+Enter');
  await page.waitForFunction(() => !document.querySelector('.canvas-text-editor'));
  await undo();
  assert.equal(await snapshot(), crlf);
  assert.deepEqual(errors, []);
  console.log(
    'PASS: direct caret/selection, typing/deletion/newlines, composition boundary/cancel/single Undo, node sync, transformed/vertical output after zoom and pan; no floating window.',
  );
} catch (error) {
  console.error(
    JSON.stringify({
      errors,
      focus: await page.evaluate(() => document.activeElement?.outerHTML.slice(0, 300)),
    }),
  );
  await page.screenshot({ path: 'test-results/in-place-failure.png' });
  throw error;
} finally {
  await browser.close();
}
