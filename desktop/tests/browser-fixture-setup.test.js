import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { blankDocument, deserialize, evaluate } from '../src/core.js';
import { loadDemoFixture, preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
import { browserTestFiles } from '../scripts/browser-suite-runner.mjs';

// This checks the setup helper contract without launching a browser or server.
// Real pointer, visibility and download acceptance remains in the E2E scripts.
function fixture(t, load) {
  const previous = globalThis.window;
  globalThis.window = { nect: { document: blankDocument(), load } };
  t.after(() => {
    if (previous === undefined) delete globalThis.window;
    else globalThis.window = previous;
  });
  const calls = [];
  const page = {
    async waitForFunction(predicate) {
      calls.push('ready');
      assert.equal(predicate(), true);
    },
    async evaluate(callback, argument) {
      calls.push('load');
      return callback(argument);
    },
  };
  return { page, calls };
}

test('explicit demo setup waits for the public API and loads a real serialized fixture', async (t) => {
  let json;
  const { page, calls } = fixture(t, (text) => {
    json = text;
  });
  assert.equal(window.nect.document.layers[0].nodes.length, 1);
  await loadDemoFixture(page);
  assert.deepEqual(calls, ['ready', 'load']);
  const doc = deserialize(json),
    layer = doc.layers[0];
  assert.equal(layer.nodes.length, 6);
  assert.equal(layer.nodes[0].type, 'Circle');
  assert.equal(layer.nodes.at(-1).type, 'Output');
  assert.equal(evaluate(layer).error, null);
  assert.ok(evaluate(layer).path.contours.length > 0);
  // The helper only passes a fixture to load; it cannot mutate session/history
  // or turn the blank startup document into a demo behind the public API.
  assert.equal(window.nect.document.layers[0].nodes.length, 1);
});

test('demo setup awaits public loading and propagates import failures', async (t) => {
  let finish;
  const { page, calls } = fixture(
    t,
    () =>
      new Promise((resolve) => {
        finish = resolve;
      }),
  );
  let finished = false;
  const pending = loadDemoFixture(page).then(() => {
    finished = true;
  });
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(calls, ['ready', 'load']);
  assert.equal(finished, false);
  finish();
  await pending;
  assert.equal(finished, true);
  window.nect.load = () => {
    throw Error('Fixture import rejected');
  };
  await assert.rejects(loadDemoFixture(page), /Fixture import rejected/);
});

test('variable fixture preloading awaits the discovered optimized module without navigating or loading a document', async () => {
  const calls = [];
  let finishBody;
  const body = new Promise((resolve) => {
    finishBody = resolve;
  });
  const page = {
    url: () => 'about:blank',
    request: {
      get: async (url) => {
        calls.push(url);
        if (calls.length === 1)
          return {
            status: () => 200,
            text: async () =>
              'import { create } from "/@fs/C:/owned-cache/deps/fontkit.js?v=fresh";',
          };
        return { status: () => 200, body: () => body };
      },
    },
  };
  let finished = false;
  const pending = preloadVariableFontDependency(page, 'http://127.0.0.1:5173').then(() => {
    finished = true;
  });
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(calls, [
    'http://127.0.0.1:5173/src/variable-font/backend.js',
    'http://127.0.0.1:5173/@fs/C:/owned-cache/deps/fontkit.js?v=fresh',
  ]);
  assert.equal(finished, false, 'a discovered import alone does not establish readiness');
  finishBody(Buffer.from('export const create = () => {};'));
  await pending;
  assert.equal(finished, true);
});

test('variable fixture preloading propagates missing transforms and failed optimized requests', async () => {
  await assert.rejects(
    preloadVariableFontDependency({ url: () => 'http://127.0.0.1:5173' }, 'http://127.0.0.1:5173'),
    /before navigating to the fixture/,
  );
  await assert.rejects(
    preloadVariableFontDependency(
      { url: () => 'about:blank', request: { get: async () => ({ status: () => 503 }) } },
      'http://127.0.0.1:5173',
    ),
    /backend preload failed/,
  );
  await assert.rejects(
    preloadVariableFontDependency(
      {
        url: () => 'about:blank',
        request: {
          get: async () => ({
            status: () => 200,
            text: async () => '<html>not a transformed module</html>',
          }),
        },
      },
      'http://127.0.0.1:5173',
    ),
    /transformed fontkit import/,
  );
  let requests = 0;
  const page = {
    url: () => 'about:blank',
    request: {
      get: async () =>
        ++requests === 1
          ? {
              status: () => 200,
              text: async () => "import { create } from '/optimized/fontkit.js';",
            }
          : { status: () => 500 },
    },
  };
  await assert.rejects(
    preloadVariableFontDependency(page, 'http://127.0.0.1:5173'),
    /Optimized fontkit preload failed/,
  );
});

// Classification of the legacy cold-start family in the default browser chain.
// Demo cases retain the old baseline explicitly, including dirty Open/Discard.
const demoScenarios = [
  'artwork-workflow',
  'artwork-multi-move',
  'artwork-alignment',
  'artwork-distribution',
  'artwork-gap-distribution',
  'editor',
  'text',
  'system-font',
  'guides',
  'layout-guides',
  'linear-gradient',
  'radial-gradient',
  'closed-offset',
  'polygon-star',
];
// These scenarios construct/import their own artwork after public API readiness.
const fixtureOwningScenarios = [
  'node-output-alignment',
  'artwork-deletion',
  'artwork-duplication',
  'artwork-multi-duplication',
  'artwork-ordering',
  'text-trial-feedback',
  'canvas-in-place',
  'images',
  'color',
  'graph-interaction-feedback',
  'view-navigation',
  'paint',
  'text-path',
  'math',
  'wiring',
  'creation',
  'join-stroke',
  'transform-anchor',
  'position-reference',
  'numeric-scrub',
  'artboard-pan',
  'text-position-output',
  'character-position',
  'null-position',
  'vector-position',
  'transform-handles',
  'artwork-snap',
  'vector-reference',
  'character-transform',
  'rich-text',
  'text-wrap',
  'axis-drag',
  'character-numeric-scrub',
  'pair-kerning',
  'editor-names-bypass',
];

test('legacy browser startup cases explicitly declare demo versus independent fixture setup', async () => {
  assert.ok(demoScenarios.length > 0 && fixtureOwningScenarios.length > 0);
  assert.equal(new Set([...demoScenarios, ...fixtureOwningScenarios]).size, demoScenarios.length + fixtureOwningScenarios.length);
  for (const [names, setup] of [
    [demoScenarios, /await loadDemoFixture\(page\);/],
    [fixtureOwningScenarios, /await page\.waitForFunction\(\(\) => window\.nect\);/],
  ])
    for (const name of names) {
      const source = await fs.readFile(new URL(`./${name}.e2e.js`, import.meta.url), 'utf8');
      const navigation = source.indexOf('await page.goto(');
      assert.ok(navigation >= 0, `${name}: explicit navigation`);
      const afterNavigation = source.slice(navigation);
      const readiness = afterNavigation.search(setup);
      assert.ok(readiness >= 0, `${name}: explicit fixture/API readiness`);
      for (const action of ['await ensureWorkspacePanel(', 'await activateBoundaryCommand(']) {
        const index = afterNavigation.indexOf(action);
        assert.ok(index < 0 || readiness < index, `${name}: readiness precedes ${action}`);
      }
    }
});

test('an unavailable startup API cannot be bypassed by fixture injection', async () => {
  let loaded = false;
  await assert.rejects(
    loadDemoFixture({
      waitForFunction: async () => {
        throw Error('API is not ready');
      },
      evaluate: async () => {
        loaded = true;
      },
    }),
    /API is not ready/,
  );
  assert.equal(loaded, false);
});

test('the browser chain never waits for startup artwork before explicit fixture setup', async () => {
  const scripts = browserTestFiles;
  assert.ok(scripts.length > 0);
  for (const script of scripts) {
    const source = await fs.readFile(new URL(`../${script}`, import.meta.url), 'utf8');
    assert.doesNotMatch(
      source,
      /await page\.(?:goto\([^\n]+\)|reload\(\));\s*await page\.(?:waitForSelector\(['"][^'"]*\.shape|locator\(['"][^'"]*\.shape['"]\)\.waitFor\()/,
      `${script} must wait for readiness or explicitly load its artwork before any shape wait`,
    );
  }
  for (const name of ['blank-artwork', 'pen-artwork', 'production-csp']) {
    const source = await fs.readFile(new URL(`./${name}.e2e.js`, import.meta.url), 'utf8');
    assert.doesNotMatch(source, /loadDemoFixture/, `${name} retains its own startup acceptance`);
  }
});
