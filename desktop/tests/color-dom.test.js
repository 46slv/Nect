import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { openDOMPanel } from './helpers/dom-workspace-controls.js';
// Load geometry before a synthetic browser global exists: geometry remains headless.
import '../src/core.js';
test('synthetic Color controls: source display, fallback restoration, alpha validation and locks', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
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
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      let b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      "from './artwork-gap-distribution.js'",
      `from '${new URL('../src/artwork-gap-distribution.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-distribution.js'",
      `from '${new URL('../src/artwork-distribution.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-alignment.js'",
      `from '${new URL('../src/artwork-alignment.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-multi-move.js'",
      `from '${new URL('../src/artwork-multi-move.js', import.meta.url)}'`,
    )
    .replace("from './images.js'", `from '${new URL('../src/images.js', import.meta.url)}'`)
    .replace("from './raster.js'", `from '${new URL('../src/raster.js', import.meta.url)}'`)
    .replace("from './focus.js'", `from '${new URL('../src/focus.js', import.meta.url)}'`)
    .replace("from './layout.js'", `from '${new URL('../src/layout.js', import.meta.url)}'`)
    .replace("from './core.js'", `from '${new URL('../src/core.js', import.meta.url)}'`)
    .replace(
      "from './artwork-creation.js'",
      `from '${new URL('../src/artwork-creation.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-move-target.js'",
      `from '${new URL('../src/artwork-move-target.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-deletion.js'",
      `from '${new URL('../src/artwork-deletion.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-ordering.js'",
      `from '${new URL('../src/artwork-ordering.js', import.meta.url)}'`,
    )
    .replace("from './svg-import.js'", `from '${new URL('../src/svg-import.js', import.meta.url)}'`)
    .replace("from './storage.js'", `from '${new URL('../src/storage.js', import.meta.url)}'`)
    .replace("from './fonts.js'", `from '${new URL('../src/fonts.js', import.meta.url)}'`);
  source = resolveMainRelativeImports(source);
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((r) => setTimeout(r, 100));
  assert.equal(document.querySelectorAll('.node').length, 1);
  assert.equal(document.querySelector('.shape'), null);
  // Color connections need a painted path; startup deliberately supplies none.
  win.nect.load(
    await fs.readFile(new URL('../examples/boolean-remap.json', import.meta.url), 'utf8'),
  );
  const tick = () => new Promise((r) => setTimeout(r, 10));
  const change = async (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
  };
  await change(document.querySelector('#add-node'), 'Color');
  const colorId = win.nect.document.layers[0].nodes.at(-1).id;
  const outputId = win.nect.document.layers[0].nodes.find((n) => n.type === 'Fill').id;
  const field = (id, key) =>
    document.querySelector(`.graph .field[data-node="${id}"][data-key="${key}"]`);
  // Color node fields stay compact; HSL belongs to the swatch popup.
  assert.equal(field(colorId, 'value').querySelector('[data-color="hsl-h"]'), null);
  field(colorId, 'value').querySelector('[data-hsl-picker-open]').click();
  assert.ok(document.querySelector('[data-hsl-plane]'));
  assert.equal(document.querySelectorAll('[data-hsl-number]').length, 3);
  await change(document.querySelector('[data-hsl-hex]'), '#ff0000');
  await change(document.querySelector('[data-hsl-number="h"]'), '30');
  document.querySelector('[data-hsl-close]').click();
  const beforeAlpha = JSON.stringify(win.nect.document);
  await change(field(colorId, 'value').querySelector('[data-color="alpha"]'), '0.5');
  const beforeCancel = JSON.stringify(win.nect.document);
  const revisionBeforeCancel = win.nect.commandState.expectedRevision;
  field(colorId, 'value').querySelector('[data-hsl-picker-open]').click();
  document.activeElement.blur();
  assert.equal(document.activeElement, document.body);
  const plane = document.querySelector('[data-hsl-plane]');
  plane.getBoundingClientRect = () => ({ left: 0, top: 0, width: 180, height: 132 });
  plane.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      bubbles: true,
      button: 0,
      pointerId: 9,
      isPrimary: true,
      clientX: 90,
      clientY: 66,
    }),
  );
  document.body.dispatchEvent(
    new win.KeyboardEvent('keydown', {
      key: 'Escape',
      bubbles: true,
      cancelable: true,
    }),
  );
  document.dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true, pointerId: 9 }));
  await tick();
  assert.equal(document.querySelector('.hsl-color-popover'), null);
  assert.equal(JSON.stringify(win.nect.document), beforeCancel);
  assert.equal(win.nect.commandState.expectedRevision, revisionBeforeCancel);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(JSON.stringify(win.nect.document), beforeAlpha, 'Escape added no Undo entry');
  document.querySelector('[data-action="redo"]').click();
  await tick();
  assert.equal(JSON.stringify(win.nect.document), beforeCancel);

  document.querySelector(`.node[data-id="${colorId}"] .node-head strong`).click();
  openDOMPanel(document, 'inspector');
  const inspectorSwatch = () =>
    document.querySelector(
      `.inspector-body .field[data-node="${colorId}"][data-key="value"] [data-hsl-picker-open]`,
    );
  const origin = inspectorSwatch();
  origin.click();
  await change(document.querySelector('[data-hsl-hex]'), '#00ff00');
  assert.equal(origin.isConnected, false, 'commit rerender replaced both field copies');
  document.querySelector('[data-hsl-close]').click();
  assert.equal(document.activeElement, inspectorSwatch(), 'close returns to Inspector, not Graph');
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(JSON.stringify(win.nect.document), beforeCancel, 'popup commit was one Undo');

  field(colorId, 'value').querySelector('[data-hsl-picker-open]').click();
  const layerId = win.nect.document.layers[0].id;
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-parameter',
    payload: { layerId, nodeId: colorId, key: 'value', value: { hex: '#ff8000', alpha: 0.6 } },
  });
  const newerDocument = JSON.stringify(win.nect.document);
  const newerRevision = win.nect.commandState.expectedRevision;
  await change(document.querySelector('[data-hsl-hex]'), '#123456');
  assert.equal(document.querySelector('.hsl-color-popover'), null, 'stale popup closes');
  assert.equal(
    JSON.stringify(win.nect.document),
    newerDocument,
    'stale popup preserves newer edit',
  );
  assert.equal(win.nect.commandState.expectedRevision, newerRevision);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(JSON.stringify(win.nect.document), beforeCancel, 'stale popup added no Undo entry');

  field(outputId, 'color').querySelector('[data-hsl-picker-open]').click();
  await change(document.querySelector('[data-hsl-hex]'), '#2040ff');
  document.querySelector('[data-hsl-close]').click();
  await change(field(outputId, 'color').querySelector('[data-color="alpha"]'), '0.25');
  assert.deepEqual(win.nect.evaluate().path.fill, { hex: '#2040ff', alpha: 0.25 });
  document
    .querySelector(`[data-source="${colorId}"]`)
    .dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true }));
  field(outputId, 'color').dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true }));
  await tick();
  assert.deepEqual(win.nect.evaluate().path.fill, { hex: '#ff8000', alpha: 0.5 });
  const connectedResult = () => field(outputId, 'color').querySelector('.parameter-result');
  assert.match(connectedResult().textContent, /Connected result #ff8000\s+alpha 0\.5/);
  const baseGroup = field(outputId, 'color').querySelector('.base-component[role="group"]');
  assert.ok(baseGroup, 'connected Color base is an accessible group');
  assert.equal(baseGroup.querySelector('label label'), null, 'HSL labels are not nested');
  assert.equal(baseGroup.querySelectorAll('[data-color-base]').length === 2, true);
  assert.equal(
    field(outputId, 'color')
      .querySelector('[data-color-base][data-hsl-picker-open]')
      .getAttribute('style'),
    'background:#2040ff',
  );
  assert.equal(
    field(outputId, 'color').querySelector('[data-color-base][data-color="alpha"]').disabled,
    false,
  );
  // A connected input still edits its authored Base without changing the result.
  await change(field(outputId, 'color').querySelector('[data-color="alpha"]'), '0.75');
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === outputId).params.color.value.alpha,
    0.75,
  );
  assert.deepEqual(win.nect.evaluate().path.fill, { hex: '#ff8000', alpha: 0.5 });
  assert.match(connectedResult().textContent, /Connected result #ff8000\s+alpha 0\.5/);
  document.querySelector('[data-action="undo"]').click();
  assert.equal(field(outputId, 'color').querySelector('[data-color="alpha"]').value, '0.25');
  assert.deepEqual(win.nect.evaluate().path.fill, { hex: '#ff8000', alpha: 0.5 });
  field(outputId, 'color')
    .querySelector('.input')
    .dispatchEvent(new win.MouseEvent('contextmenu', { bubbles: true }));
  await tick();
  assert.deepEqual(win.nect.evaluate().path.fill, { hex: '#2040ff', alpha: 0.25 });
  document.querySelector('[data-action="undo"]').click();
  assert.equal(win.nect.evaluate().path.fill.alpha, 0.5);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(win.nect.evaluate().path.fill.alpha, 0.25);
  assert.equal(document.querySelector('[data-fill]'), null);
  assert.equal(document.querySelector('[data-color-layer]'), null);
  await change(field(colorId, 'value').querySelector('[data-color="alpha"]'), '0');
  assert.equal(win.nect.document.layers[0].nodes.at(-1).params.value.value.alpha, 0);
  const saved = JSON.stringify(win.nect.document);
  await change(field(colorId, 'value').querySelector('[data-color="alpha"]'), '2');
  assert.equal(JSON.stringify(win.nect.document), saved);
  field(colorId, 'value').querySelector('[data-lock]').click();
  assert.equal(field(colorId, 'value').querySelector('[data-hsl-picker-open]').disabled, true);
  assert.equal(field(colorId, 'value').querySelector('[data-color="alpha"]').disabled, true);
  win.close();
});
