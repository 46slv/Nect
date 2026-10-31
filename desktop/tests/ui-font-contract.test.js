import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';

test('interface typography uses one replaceable role separate from artwork fonts', async () => {
  const css = await readFile(new URL('../src/style.css', import.meta.url), 'utf8');
  assert.match(css, /--ui-font-family:\s*'IBM Plex Sans JP', 'Segoe UI', system-ui, sans-serif/);
  assert.match(css, /font-family:\s*var\(--ui-font-family\)/);
  assert.match(css, /--ui-font-weight-base:\s*300/);
  assert.match(css, /--ui-font-weight-medium:\s*400/);
  assert.match(css, /--ui-font-weight-strong:\s*400/);
  assert.deepEqual(
    [...css.matchAll(/(?<![-\w])font-family:\s*([^;]+);/g)].map((match) => match[1].trim()),
    ["'IBM Plex Sans JP'", "'IBM Plex Sans JP'", 'var(--ui-font-family)', 'inherit'],
  );
});

test('bundled offline IBM UI faces and license match the verified official bytes', async () => {
  const files = {
    'IBMPlexSansJP-Light.woff2': '4d40f9921e3467d01acf4d356b19754b1a80c65e24841dc79644134233f6002d',
    'IBMPlexSansJP-Regular.woff2':
      'ec27e9b75cd90a6f7abe96ede49d00b151eb5b1c2c886bfe9b4fc845f40be1ff',
    'OFL.txt': '7e6b2818edbd8f6a01ae80641cc8f16a51080d08fb4e532be3a0b6f74adb07da',
  };
  for (const [name, expected] of Object.entries(files)) {
    const bytes = await readFile(
      new URL(`../public/fonts/ibm-plex-sans-jp/${name}`, import.meta.url),
    );
    assert.equal(createHash('sha256').update(bytes).digest('hex'), expected);
  }
});
