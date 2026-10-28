import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';

const out = resolve('test-results/workspace-panels');
await mkdir(out + '/parts', { recursive: true });
await writeFile(
  out + '/parts/blue.svg',
  '<svg width="40" height="30"><rect width="40" height="30" fill="#2468ab"/></svg>',
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
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const before = await snapshot();
  const separator = page.getByRole('separator', { name: 'Resize Layers' });
  await separator.focus();
  const initial = Number(await separator.getAttribute('aria-valuenow'));
  await separator.press('ArrowDown');
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), initial + 16);
  assert.equal(await snapshot(), before, 'workspace resize is not a document edit');
  const stored = await page.evaluate(() => localStorage.getItem('nect.workspace-panels.v1'));
  assert.equal(JSON.parse(stored).layersHeight, initial + 16);
  await page.screenshot({ path: out + '/01-closed.png' });

  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  const panel = page.locator('[data-asset-library]');
  await panel.waitFor();
  const libraryIdentity = await panel.elementHandle();
  const pending = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
  await (await pending).setFiles(out + '/parts');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('blue');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').click();
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')),
  );
  assert.equal(
    await page.evaluate(
      (node) => node === document.querySelector('[data-asset-library]'),
      libraryIdentity,
    ),
    true,
  );
  assert.equal(
    await page.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(),
    'blue',
  );
  const afterPlacement = await snapshot();
  for (const width of [1440, 1000, 900]) {
    await page.setViewportSize({ width, height: 960 });
    await page.waitForTimeout(80);
    const geometry = await page.evaluate(() => {
      const rect = (selector) => {
        const r = document.querySelector(selector).getBoundingClientRect();
        return { x: r.x, right: r.right, width: r.width, height: r.height };
      };
      return {
        asset: rect('[data-asset-library]'),
        canvas: rect('.canvas-stage'),
        layers: rect('.layers'),
        inspector: rect('.inspector'),
        bar: rect('.workspace-panel-bar'),
        overflow: document.documentElement.scrollWidth > innerWidth,
      };
    });
    assert.ok(
      geometry.asset.x >= geometry.canvas.right - 1,
      `library leaves artwork unobscured at ${width}`,
    );
    assert.ok(geometry.canvas.width >= 250);
    assert.ok(geometry.layers.height >= 96);
    assert.ok(geometry.inspector.height >= 120);
    assert.ok(geometry.asset.right <= geometry.bar.x + 1);
    assert.equal(geometry.overflow, false);
  }
  await page.screenshot({ path: out + '/02-open-narrow.png' });
  await page.setViewportSize({ width: 1440, height: 960 });
  await page.screenshot({ path: out + '/03-open.png' });
  assert.equal(await snapshot(), afterPlacement);

  await page.evaluate(() => {
    const dialog = document.createElement('dialog');
    dialog.id = 'workspace-modal-probe';
    dialog.innerHTML = '<button>Modal owner</button>';
    document.body.append(dialog);
    dialog.showModal();
  });
  await page.keyboard.press('Escape');
  assert.equal(await panel.count(), 1, 'native modal owns Escape, not the asset panel');
  await page.evaluate(() => document.querySelector('#workspace-modal-probe').remove());
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).focus();
  await page.keyboard.press('Escape');
  await panel.waitFor({ state: 'detached' });
  assert.equal(
    await page
      .getByRole('button', { name: 'Toggle Assets panel', exact: true })
      .getAttribute('aria-pressed'),
    'false',
  );
  assert.equal(await snapshot(), afterPlacement);

  await page.getByRole('button', { name: 'Toggle Alignment panel', exact: true }).click();
  await page.locator('.alignment-panel').waitFor();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  assert.equal(
    await page.locator('.alignment-panel').count(),
    1,
    'Layers and docked alignment coexist',
  );
  await page.getByRole('button', { name: 'Toggle Alignment panel', exact: true }).click();
  assert.equal(await page.locator('.alignment-panel').count(), 0);
  assert.equal(await snapshot(), afterPlacement);
  assert.deepEqual(errors, []);
  await writeFile(
    out + '/result.json',
    JSON.stringify(
      { ok: true, viewportWidths: [1440, 1000, 900], documentUnchangedByWorkspace: true, errors },
      null,
      2,
    ),
  );
  console.log(
    'workspace panels PASS: resize, persistence, stable asset DOM, visible targets, modal Escape and coexistence',
  );
} finally {
  await browser.close();
}
