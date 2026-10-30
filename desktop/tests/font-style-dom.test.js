import test from 'node:test';
import { spawnSync } from 'node:child_process';
import os from 'node:os';
import path from 'node:path';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { makeFontAsset, parseFont } from '../src/fonts.js';
import { Window } from 'happy-dom';
import {
  document as newDocument,
  node,
  serialize,
  createCharacterPositions,
  setCharacterStyles,
} from '../src/core.js';
const buffer = async (path) => {
  const b = await fs.readFile(new URL(path, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
test('static family/style picker uses actual faces with atomic whole/range history and stale-safe preview', async (testContext) => {
  try {
    await fs.access('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf');
    await fs.access('/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf');
  } catch {
    testContext.skip('Optional licensed local DejaVu styles absent');
    return;
  }
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of [
    'window',
    'document',
    'HTMLElement',
    'DOMPoint',
    'Event',
    'requestAnimationFrame',
  ])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({ a: 1, b: 0, inverse: () => ({}) });
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: () => buffer('../public/fonts/DejaVuSans.ttf'),
  });
  const realBytes = async (style) => {
    const b = await fs.readFile(`/usr/share/fonts/truetype/dejavu/DejaVuSans-${style}.ttf`);
    return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
  };
  const bold = await realBytes('Bold'),
    oblique = await realBytes('Oblique');
  const meta = (bytes) => parseFont(bytes).nectMetadata;
  globalThis.styleCatalog = [
    ['system-bold', 'Same face name', null, meta(bold)],
    ['system-oblique', 'Same face name', null, meta(oblique)],
    [
      'system-disabled',
      'Variable face',
      'Variable fonts unsupported',
      { family: 'DejaVu Sans', subfamily: 'Variable', weightClass: 700 },
    ],
    ['system-legacy', 'Legacy face', null],
    ['system-unknown', 'Bold name is not metadata', null, { family: 'Unknown test' }],
  ];
  let calls = [];
  globalThis.readRichFont = (id) => {
    calls.push(id);
    throw Error('Normal font selection must not read installed font bytes');
  };
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /import \{[^}]*\} from '.\/storage.js';/,
      `import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}'; const releaseProject = async () => {}, writeProject = async () => null, chooseUnsaved = async () => 'cancel', registerClose = async () => {}; const desktop=true, saveFile=async()=>true, openFile=async()=>null, listSystemFonts=async()=>globalThis.styleCatalog, readSystemFont=(id)=>globalThis.readRichFont(id); const beginRecovery=async()=> 'test-recovery', writeRecovery=async()=>Date.now(), listRecovery=async()=>[], readRecovery=async()=>'', acknowledgeRecovery=async()=>{};`,
    )
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const until = async (fn) => {
    const until = Date.now() + 6000;
    while (!fn()) {
      assert.ok(Date.now() < until, 'Timed out');
      await new Promise((r) => setTimeout(r, 10));
    }
  };
  const tick = () => new Promise((r) => setTimeout(r, 20));
  await until(() => !!win.nect);
  const doc = newDocument(),
    t = node('Text'),
    out = node('Output');
  const boldAsset = makeFontAsset(bold, 'bold.ttf'),
    obliqueAsset = makeFontAsset(oblique, 'oblique.ttf');
  boldAsset.asset.name = 'Same face name';
  obliqueAsset.asset.name = 'Same face name';
  doc.fontAssets = { [boldAsset.id]: boldAsset.asset, [obliqueAsset.id]: obliqueAsset.asset };
  t.text = 'ABA';
  t.characterPositions = createCharacterPositions(t.text);
  doc.schemaVersion = 3;
  t.params.size.value = 40;
  out.params.path.source = t.id;
  doc.layers[0].nodes = [t, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  await until(() => win.nect.evaluate().error === null);
  const get = () => win.nect.document.layers[0].nodes[0],
    saved = () => JSON.stringify(win.nect.document);
  const q = (selector) => document.querySelector(selector);
  const change = (selector, value) => {
    const control = q(selector);
    control.value = value;
    control.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const open = async (range = false) => {
    q(range ? '[data-character-style="font"]' : '.inspector-body [data-change-font]').dispatchEvent(
      new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }),
    );
    await tick();
    assert.ok(q('dialog[open]'));
    for (const group of [...document.querySelectorAll('.picker-family')]) {
      const current = [...document.querySelectorAll('.picker-family')].find((g) => g.dataset.fontFamily === group.dataset.fontFamily);
      if (current?.getAttribute('aria-expanded') === 'false') current.querySelector('.picker-family-heading').click();
    }
  };
  const row = (id, source = 'embedded') => [...document.querySelectorAll('.picker-option')]
    .find((entry) => entry.dataset.id === JSON.stringify([source, id])) || null;
  const ready = () => until(() => q('.picker-apply') && !q('.picker-apply').disabled);
  const cancel = () => q('.picker-cancel').click();
  const apply = async () => {
    await ready();
    q('.picker-apply').click();
    await until(() => !q('dialog[open]'));
  };
  const baseline = saved(),
    baselineSVG = win.nect.export();
  const characterIds = get().characterPositions.units.map((unit) => unit.id);
  assert.equal(calls.length, 0);
  await open();
  assert.equal(calls.length, 0, 'catalog grouping does not read installed bytes');
  assert.match(row('system-bold', 'installed').textContent, /Bold · weight 700/);
  assert.match(row('system-oblique', 'installed').textContent, /Oblique · weight 400/);
  assert.match(row('system-unknown', 'installed').textContent, /Unknown style · weight unknown/);
  assert.match(row('system-legacy', 'installed').textContent, /Unknown family/);
  assert.match(row('system-bold', 'installed').textContent, /face system-bold/);
  assert.equal(row('system-disabled', 'installed').getAttribute('aria-disabled'), 'true');
  assert.equal(row('system-bold', 'installed').getAttribute('aria-disabled'), 'true');
  assert.match(row('system-bold', 'installed').textContent, /Font file import is required; use Advanced/);
  row(boldAsset.id).querySelector('.picker-star').click();
  assert.equal(
    row(obliqueAsset.id).querySelector('.picker-star').getAttribute('aria-pressed'),
    'false',
  );
  change('[aria-label="Font family"]', 'family:DejaVu Sans');
  assert.equal(row('system-legacy', 'installed'), null);
  change('[aria-label="Available weight"]', '700');
  await ready();
  assert.equal(row('system-oblique', 'installed'), null);
  assert.match(row('system-disabled', 'installed').textContent, /Variable fonts unsupported/);
  row(boldAsset.id).click();
  await ready();
  assert.equal(saved(), baseline, 'preview does not commit');
  assert.equal(win.nect.export(), baselineSVG);
  await apply();
  const whole = saved(),
    wholeSVG = win.nect.export(),
    wholeId = get().font;
  assert.notEqual(wholeSVG, baselineSVG);
  assert.deepEqual(
    Buffer.from(win.nect.document.fontAssets[wholeId].data, 'base64'),
    Buffer.from(bold),
  );
  assert.deepEqual(
    get().characterPositions.units.map((unit) => unit.id),
    characterIds,
  );
  q('[data-action="undo"]').click();
  assert.equal(saved(), baseline);
  q('[data-action="redo"]').click();
  assert.equal(saved(), whole);
  win.nect.load(whole);
  assert.equal(win.nect.export(), wholeSVG);
  // Embedded faces expose the same parsed family/style metadata in browser mode too.
  await open();
  assert.match(row(wholeId).textContent, /Bold · weight 700/);
  cancel();
  const textarea = q('.inspector-body [data-text]');
  textarea.setSelectionRange(1, 2);
  textarea.dispatchEvent(new win.Event('select', { bubbles: true }));
  q('.inspector-body [data-style-text-range]').click();
  await open(true);
  row(obliqueAsset.id).click();
  await ready();
  await apply();
  const ranged = saved(),
    rangedSVG = win.nect.export();
  assert.equal(get().font, wholeId);
  const rangeId = get().characterPositions.styles.c2.font;
  assert.deepEqual(
    Buffer.from(win.nect.document.fontAssets[rangeId].data, 'base64'),
    Buffer.from(oblique),
  );
  assert.equal(get().characterPositions.styles.c1, undefined);
  assert.equal(get().characterPositions.styles.c3, undefined);
  assert.deepEqual(
    get().characterPositions.units.map((unit) => unit.id),
    characterIds,
  );
  assert.notEqual(rangedSVG, wholeSVG);
  q('[data-action="undo"]').click();
  assert.equal(saved(), whole);
  q('[data-action="redo"]').click();
  assert.equal(saved(), ranged);
  win.nect.load(ranged);
  assert.equal(win.nect.export(), rangedSVG);
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'nect-style-test-'));
  try {
    const input = path.join(temp, 'project.json'),
      output = path.join(temp, 'out.svg');
    await fs.writeFile(input, ranged);
    const result = spawnSync(process.execPath, ['cli/export.js', input, output], {
      encoding: 'utf8',
    });
    assert.equal(result.status, 0, result.stderr);
    assert.equal(await fs.readFile(output, 'utf8'), rangedSVG);
  } finally {
    await fs.rm(temp, { recursive: true, force: true });
  }
  // Normal browsing cannot start an installed byte read, even for same-name rows.
  // Rapid asynchronous native results are covered by unified-font-choice tests.
  await open();
  row('system-bold', 'installed').click();
  row('system-oblique', 'installed').click();
  await tick();
  assert.equal(calls.length, 0);
  assert.equal(saved(), ranged);
  cancel();
  assert.equal(saved(), ranged);
  assert.equal(win.nect.export(), rangedSVG);
  // Repeated real document-face previews remain cancelable without authored edits.
  await open();
  row(boldAsset.id).click();
  row(obliqueAsset.id).click();
  await ready();
  const canvas = q('.artboard').innerHTML;
  await tick();
  assert.equal(q('.artboard').innerHTML, canvas);
  assert.equal(saved(), ranged);
  cancel();
  assert.equal(saved(), ranged);
  assert.equal(win.nect.export(), rangedSVG);
  // Legacy discovery rows retain unknown metadata and an explicit import reason.
  await open();
  change('[aria-label="Font family"]', '__unknown__');
  assert.ok(row('system-legacy', 'installed'));
  assert.equal(row('system-legacy', 'installed').getAttribute('aria-disabled'), 'true');
  assert.equal(row('system-bold', 'installed'), null);
  cancel();
  win.nect.load(baseline);
  assert.equal(win.nect.export(), baselineSVG, 'old project output unchanged');
  win.happyDOM.abort();
});
