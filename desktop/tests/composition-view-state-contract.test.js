import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../src/main.js', import.meta.url), 'utf8'),
  css = await readFile(new URL('../src/style.css', import.meta.url), 'utf8');

test('Composition activation saves and restores window state without a document command', () => {
  const start = source.indexOf('function switchActiveComposition(compositionId) {'),
    end = source.indexOf('function addComposition()', start),
    flow = source.slice(start, end);
  assert.ok(start >= 0 && end > start);
  assert.match(flow, /saveCompositionViewState\(\)/);
  assert.match(flow, /session\.switchComposition\(compositionId\)/);
  assert.match(flow, /compositionViewStates\.prune\(project\.compositions\)/);
  assert.match(flow, /restoreCompositionViewState\(\)/);
  assert.match(flow, /render\(false, null, graphScroll\)/);
  assert.doesNotMatch(flow, /session\.dispatch\(/);
  assert.doesNotMatch(flow, /render\(true\)/);
});

test('Open and recovery replace the project and discard all prior Composition view snapshots', () => {
  const start = source.indexOf('function replaceDocument('),
    end = source.indexOf('function compositionViewDefaults(', start),
    flow = source.slice(start, end);
  assert.ok(start >= 0 && end > start);
  assert.match(flow, /compositionViewStates\.clear\(\)/);
  assert.match(flow, /detailsState\.clear\(\)/);
  assert.match(flow, /contextMenuScroll\.clear\(\)/);
  assert.match(flow, /zoom = 1;[\s\S]*canvasFit = true;[\s\S]*pan = \[0, 0\];[\s\S]*graphZoom = 1/);
});

test('Add and project Import save the outgoing view and initialize the newly active Composition', () => {
  const addStart = source.indexOf('function addComposition()'),
    addEnd = source.indexOf('function download(', addStart),
    importStart = source.indexOf('async function openCompositionImportDialog()'),
    importEnd = source.indexOf('function adoptSVGPart(', importStart),
    addFlow = source.slice(addStart, addEnd),
    importFlow = source.slice(importStart, importEnd);
  assert.match(addFlow, /saveCompositionViewState\(\)/);
  assert.match(addFlow, /restoreCompositionViewState\(\)/);
  assert.match(addFlow, /compositionViewStates\.forget\(compositionId\)/);
  assert.match(importFlow, /saveCompositionViewState\(\)/);
  assert.match(importFlow, /restoreCompositionViewState\(\)/);
  assert.match(importFlow, /compositionViewStates\.forget\(compositionId\)/);
  assert.match(
    importFlow,
    /compositionViewStates\.prune\(session\.projectSnapshot\(\)\.compositions\)/,
  );
});

test('Composition tablist exposes accessible selection, keyboard activation, and horizontal scrolling', () => {
  assert.match(source, /setAttribute\('role', 'tablist'\)/);
  assert.match(source, /setAttribute\('role', 'tab'\)/);
  assert.match(source, /setAttribute\('aria-selected'/);
  assert.match(source, /setAttribute\('aria-controls', 'composition-panel'\)/);
  assert.match(source, /setAttribute\('role', 'tabpanel'\)/);
  assert.match(
    source,
    /case 'ArrowRight':[\s\S]*?case 'ArrowLeft':[\s\S]*?case 'Home':[\s\S]*?case 'End':/,
  );
  assert.match(source, /case 'Enter':[\s\S]*?case ' '/);
  assert.match(source, /function switchActiveComposition\(compositionId\)/);
  const tabStyles = css.match(/\.composition-tabs\s*\{([^}]*)\}/)?.[1],
    tabButtonStyles = css.match(/\.composition-tab\s*\{([^}]*)\}/)?.[1];
  assert.ok(tabStyles);
  assert.ok(tabButtonStyles);
  assert.match(tabStyles, /overflow-x:\s*auto/);
  assert.match(tabStyles, /max-width:\s*30vw/);
  assert.match(tabButtonStyles, /text-overflow:\s*ellipsis/);
  assert.match(tabButtonStyles, /white-space:\s*nowrap/);
  assert.match(css, /\.composition-tab\[aria-selected=['"]true['"]\]/);
});
