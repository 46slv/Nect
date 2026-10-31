import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(async () => {
    const { document, node, connect, setReference } = await import('/src/core.js');
    const doc = document(),
      layer = doc.layers[0],
      reference = node('Reference'),
      master = node('Text'),
      output = node('Output');
    master.name = 'Master';
    layer.nodes = [reference, master, output];
    layer.output = output.id;
    connect(layer, master.id, output.id, 'path');
    setReference(layer, reference.id, { nodeId: master.id, portId: 'position' });
    window.nect.load(JSON.stringify(doc));
  });
  // Start this disclosure check with transient panels dismissed.
  await page.keyboard.press('Escape');
  const status = page.locator('.inspector-body [data-reference-status]');
  assert.match(await status.textContent(), /Master · Position/);
  assert.equal(await status.isVisible(), true, 'live status must remain visible with Help closed');
  assert.equal(await status.getAttribute('role'), 'status');
  const help = page.locator('.inspector-body details.control-help');
  assert.equal(await help.getAttribute('open'), null);
  assert.match(await help.textContent(), /Read-only value reference/);
  assert.doesNotMatch(await help.innerText(), /Read-only value reference/);
  await help.locator('summary').click();
  assert.match(await help.innerText(), /Read-only value reference/);
  await help.locator('summary').click();
  assert.equal(await status.isVisible(), true);
  assert.match(await status.innerText(), /Master · Position/);
  console.log('PASS live Reference status remains visible while explanatory Help opens and closes');
} finally {
  await browser.close();
}
