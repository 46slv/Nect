import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { matchesGlob } from 'node:path';

test('Windows artifact retains nested asset and workspace acceptance evidence, not fixtures', () => {
  const workflow = readFileSync(
    new URL('../../.github/workflows/verify-windows.yml', import.meta.url),
    'utf8',
  );
  const step = workflow.split('- name: Preserve bounded development evidence')[1];
  assert.ok(step, 'bounded evidence upload step exists');
  const block = step.match(/path: \|\r?\n([\s\S]*?)\n\s+if-no-files-found:/)?.[1];
  assert.ok(block, 'artifact path list exists');
  const patterns = block
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);
  const retained = (path) => patterns.some((pattern) => matchesGlob(path, pattern));
  for (const path of [
    'asset-library/browse.png',
    'asset-library/placed.png',
    'asset-library/results.json',
    'asset-catalog/results.json',
    'workspace-panels/01-closed.png',
    'workspace-panels/02-open-narrow.png',
    'workspace-panels/result.json',
    'asset-drag/svg-graph.png',
    'asset-drag/png-layer.png',
    'asset-drag/results.json',
  ])
    assert.ok(retained(`desktop/test-results/${path}`), `retain ${path}`);
  for (const path of [
    'asset-library/fixtures/photo.png',
    'asset-drag/fixtures/drag-pixel.png',
    'workspace-panels/parts/blue.svg',
  ])
    assert.equal(retained(`desktop/test-results/${path}`), false, `exclude input fixture ${path}`);
});
