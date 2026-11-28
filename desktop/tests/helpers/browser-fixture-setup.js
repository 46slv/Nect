import assert from 'node:assert/strict';
import { blankDocument, document, serialize } from '../../src/core.js';
import { activateCommand } from '../command-menu-actions.js';

// Finish the worker's lazy Vite dependency optimization before navigation, when
// no authored fixture can be lost to its reload. Requesting modules changes no UI.
export async function preloadVariableFontDependency(page, baseURL) {
  assert.equal(page.url(), 'about:blank', 'Preload dependencies before navigating to the fixture');
  const backendURL = new URL('/src/variable-font/backend.js', baseURL).href;
  const backend = await page.request.get(backendURL);
  assert.equal(backend.status(), 200, 'Variable-font backend preload failed');
  const source = await backend.text();
  const dependency = /from\s+['"]([^'"]*fontkit[^'"]*)['"]/.exec(source);
  assert.ok(dependency, 'Vite must expose the transformed fontkit import');
  const optimized = await page.request.get(new URL(dependency[1], backendURL).href);
  assert.equal(optimized.status(), 200, 'Optimized fontkit preload failed');
  // The transform discovers the dependency; its optimized module must finish too.
  await optimized.body();
}

// Startup is intentionally blank. Legacy artwork/graph scenarios must opt into
// the existing demo document through the same explicit public load used by files.
// Do not use this in startup acceptance or replace scenario-specific fixtures.
export async function loadDemoFixture(page) {
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((json) => window.nect.load(json), serialize(document()));
}

// Opt in to the historical blank fixture without pointer or keyboard input.
export async function loadBlankFixture(page) {
  await page.waitForFunction(() => typeof window.nect?.load === 'function' && !document.body.inert);
  await page.evaluate((json) => window.nect.load(json), serialize(blankDocument()));
}

// Author an actual New document when the scenario exercises creation or file commands.
export async function createNewFixture(page) {
  await activateCommand(page, 'new');
  const dialog = page.locator('.new-project-dialog[open]');
  await dialog.locator('[name="width"]').fill('960');
  await dialog.locator('[name="height"]').fill('540');
  await dialog.locator('button[type="submit"]').click();
  await page.waitForFunction(
    () => window.nect?.document?.width === 960 && window.nect.document.height === 540,
  );
}
