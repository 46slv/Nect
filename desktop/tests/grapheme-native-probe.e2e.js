// Standalone read-only browser diagnostic. Does not open the user's app/profile.
// Run: node tests/grapheme-native-probe.e2e.js
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { graphemeSamples, scalarSamples, expectedBoundaries } from './fixtures/grapheme-editing.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
});
const rows = [];
try {
  const page = await browser.newPage();
  await page.setContent('<textarea aria-label="isolated native text probe"></textarea>');
  const input = page.locator('textarea');
  for (const sample of [...graphemeSamples, ...scalarSamples]) {
    const value = `A${sample.text}B`;
    const browserBoundaries = await page.evaluate(
      (value) =>
        [...new Intl.Segmenter(undefined, { granularity: 'grapheme' }).segment(value)]
          .map((part) => part.index)
          .concat(value.length),
      value,
    );
    assert.deepEqual(
      browserBoundaries,
      expectedBoundaries(sample),
      `${sample.name}: browser segmentation`,
    );
    for (const [key, start] of [
      ['ArrowLeft', 1 + sample.text.length],
      ['ArrowRight', 1],
      ['Backspace', 1 + sample.text.length],
      ['Delete', 1],
    ]) {
      await input.fill(value);
      await input.evaluate((el, start) => {
        el.focus();
        el.setSelectionRange(start, start);
      }, start);
      await input.press(key);
      const actual = await input.evaluate((el) => ({
        value: el.value,
        start: el.selectionStart,
        end: el.selectionEnd,
      }));
      const composite = sample.boundaries.length === 2;
      const expected = key === 'ArrowLeft' ? 1 : 1 + sample.text.length;
      const safe = key.startsWith('Arrow')
        ? browserBoundaries.includes(actual.start) &&
          browserBoundaries.includes(actual.end) &&
          (!composite || actual.start === expected)
        : !composite || actual.value === 'AB';
      rows.push({
        name: sample.name,
        text: sample.text,
        key,
        boundaries: browserBoundaries,
        actual,
        safe,
      });
      // A fully native edit must remain in the browser's Undo/Redo history.
      if (!key.startsWith('Arrow')) {
        await input.press('Control+z');
        assert.equal(await input.inputValue(), value, `${sample.name}: native Undo ${key}`);
        await input.press('Control+y');
        assert.equal(await input.inputValue(), actual.value, `${sample.name}: native Redo ${key}`);
      }
    }
  }
  const output = {
    browserVersion: browser.version(),
    nodeVersion: process.version,
    rows,
    gaps: rows.filter((row) => !row.safe),
  };
  await mkdir('test-results/grapheme-editing', { recursive: true });
  await writeFile(
    'test-results/grapheme-editing/native-probe.json',
    JSON.stringify(output, null, 2),
  );
  console.log(
    JSON.stringify({
      browserVersion: output.browserVersion,
      checks: rows.length,
      gaps: output.gaps.map(({ name, key }) => ({ name, key })),
    }),
  );
} finally {
  await browser.close();
}
