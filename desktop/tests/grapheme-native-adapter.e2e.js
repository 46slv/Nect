// Isolated integration experiment, NOT installed in the application. Native
// editing still owns actual text mutations, beforeinput/input and Undo/Redo.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { graphemeSamples, scalarSamples } from './fixtures/grapheme-editing.js';

const source = await readFile(new URL('../src/grapheme-editing.js', import.meta.url), 'utf8');
const server = createServer((req, res) => {
  if (req.url === '/grapheme-editing.js') {
    res.setHeader('Content-Type', 'text/javascript');
    res.end(source);
  } else {
    res.setHeader('Content-Type', 'text/html');
    res.end('<textarea aria-label="isolated grapheme adapter"></textarea>');
  }
});
await new Promise((done) => server.listen(0, '127.0.0.1', done));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage();
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.evaluate(async () => {
    const { graphemeDeletionRange, graphemeMovementCorrection } = await import(
      '/grapheme-editing.js'
    );
    const el = document.querySelector('textarea');
    let composing = false,
      before;
    const selection = () => ({
      start: el.selectionStart,
      end: el.selectionEnd,
      selectionDirection: el.selectionDirection,
    });
    const focus = (range) => (range.selectionDirection === 'backward' ? range.start : range.end);
    window.events = [];
    el.addEventListener('compositionstart', () => {
      composing = true;
    });
    el.addEventListener('compositionend', () => {
      composing = false;
    });
    el.addEventListener('beforeinput', (e) => {
      const range = graphemeDeletionRange(el.value, {
        ...selection(),
        inputType: e.inputType,
        composing: composing || e.isComposing,
      });
      if (
        range &&
        (el.selectionStart !== el.selectionEnd ||
          (range.start < el.selectionStart && el.selectionStart < range.end) ||
          [...el.value.slice(range.start, range.end)].length > 1)
      )
        el.setSelectionRange(range.start, range.end);
      window.events.push({
        type: 'beforeinput',
        inputType: e.inputType,
        trusted: e.isTrusted,
        prevented: e.defaultPrevented,
      });
    });
    el.addEventListener('input', (e) => {
      window.events.push({ type: 'input', inputType: e.inputType, trusted: e.isTrusted });
    });
    el.addEventListener('keydown', (e) => {
      before =
        !composing &&
        !e.isComposing &&
        !e.ctrlKey &&
        !e.altKey &&
        !e.metaKey &&
        ['ArrowLeft', 'ArrowRight'].includes(e.key)
          ? selection()
          : null;
    });
    el.addEventListener('keyup', (e) => {
      if (!before || composing || e.isComposing) return;
      const after = selection(),
        delta = focus(after) - focus(before);
      before = null;
      if (!delta) return;
      const correction = graphemeMovementCorrection(el.value, {
        ...after,
        logicalDirection: delta < 0 ? 'backward' : 'forward',
        extend: e.shiftKey,
      });
      if (correction)
        el.setSelectionRange(correction.start, correction.end, correction.selectionDirection);
    });
  });
  const input = page.locator('textarea');
  const rows = [];
  for (const sample of [...graphemeSamples, ...scalarSamples]) {
    const value = `A${sample.text}B`,
      end = 1 + sample.text.length;
    const deletions = [
      ['Backspace', end, 1 + sample.boundaries.at(-2), end],
      ['Delete', 1, 1, 1 + sample.boundaries[1]],
    ];
    if (sample.boundaries.length === 2 && sample.text.length > 1)
      deletions.push(['Backspace', 2, 1, end], ['Delete', 2, 1, end]);
    for (const [key, start, from, to] of deletions) {
      await input.fill(value);
      await input.evaluate((el, start) => {
        el.focus();
        el.setSelectionRange(start, start);
        window.events = [];
      }, start);
      await input.press(key);
      const expected = value.slice(0, from) + value.slice(to);
      assert.equal(await input.inputValue(), expected, `${sample.name}: ${key}`);
      const events = await page.evaluate(() => window.events);
      assert.deepEqual(
        events.map(({ type, inputType, trusted }) => ({ type, inputType, trusted })),
        [
          {
            type: 'beforeinput',
            inputType: key === 'Backspace' ? 'deleteContentBackward' : 'deleteContentForward',
            trusted: true,
          },
          {
            type: 'input',
            inputType: key === 'Backspace' ? 'deleteContentBackward' : 'deleteContentForward',
            trusted: true,
          },
        ],
      );
      assert.equal(events[0].prevented, false);
      await input.press('Control+z');
      assert.equal(await input.inputValue(), value, `${sample.name}: Undo ${key}`);
      await input.press('Control+y');
      assert.equal(await input.inputValue(), expected, `${sample.name}: Redo ${key}`);
      rows.push({ name: sample.name, key, initialCaret: start, passed: true });
    }
    for (const [key, start, expectedStart, expectedEnd] of [
      ['ArrowLeft', end, 1 + sample.boundaries.at(-2), 1 + sample.boundaries.at(-2)],
      ['ArrowRight', 1, 1 + sample.boundaries[1], 1 + sample.boundaries[1]],
      ['Shift+ArrowLeft', end, 1 + sample.boundaries.at(-2), end],
      ['Shift+ArrowRight', 1, 1, 1 + sample.boundaries[1]],
    ]) {
      await input.fill(value);
      await input.evaluate((el, start) => {
        el.focus();
        el.setSelectionRange(start, start);
      }, start);
      await input.press(key);
      assert.deepEqual(
        await input.evaluate((el) => [el.selectionStart, el.selectionEnd]),
        [expectedStart, expectedEnd],
        `${sample.name}: ${key}`,
      );
      assert.equal(await input.inputValue(), value);
      rows.push({ name: sample.name, key, passed: true });
    }
  }
  await input.fill('Ax\u0301B');
  await input.evaluate((el) => {
    el.setSelectionRange(2, 2);
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    el.dispatchEvent(
      new InputEvent('beforeinput', {
        inputType: 'deleteContentBackward',
        bubbles: true,
        cancelable: true,
      }),
    );
  });
  assert.deepEqual(
    await input.evaluate((el) => [el.selectionStart, el.selectionEnd, el.value]),
    [2, 2, 'Ax\u0301B'],
    'composition bypasses selection correction',
  );
  const result = {
    browserVersion: browser.version(),
    checks: rows.length,
    compositionBypass: true,
    rows,
  };
  await mkdir('test-results/grapheme-editing', { recursive: true });
  await writeFile(
    'test-results/grapheme-editing/native-adapter.json',
    JSON.stringify(result, null, 2),
  );
  console.log(
    JSON.stringify({
      browserVersion: result.browserVersion,
      checks: rows.length,
      compositionBypass: true,
    }),
  );
} finally {
  await browser?.close();
  await new Promise((done) => server.close(done));
}
