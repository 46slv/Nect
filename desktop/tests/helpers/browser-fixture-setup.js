import { document, serialize } from '../../src/core.js';

// Startup is intentionally blank. Legacy artwork/graph scenarios must opt into
// the existing demo document through the same explicit public load used by files.
// Do not use this in startup acceptance or replace scenario-specific fixtures.
export async function loadDemoFixture(page) {
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((json) => window.nect.load(json), serialize(document()));
}
