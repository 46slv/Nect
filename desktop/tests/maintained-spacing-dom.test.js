import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  document as makeDoc,
  node,
  connect,
  setCharacterOffsets,
  setCharacterTransforms,
  setCharacterStyles,
} from '../src/core.js';

test('maintained ink gap inspector applies, preserves typography editability, scrubs and cancels atomically', async () => {
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
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((r) => setTimeout(r, 15));
  for (let i = 0; i < 300 && (!win.nect || win.nect.evaluate().error); i++) await tick();
  assert.equal(win.nect.evaluate().error, null);
  const fixture = makeDoc(),
    t = node('Text'),
    tr = node('Transform'),
    out = node('Output');
  t.text = 'AgW';
  t.params.size.value = 40;
  t.params.position.value = [100, 200];
  t.params.position.locked = true;
  tr.params.rotation.value = 0;
  tr.params.scale.value = 1;
  fixture.layers[0].nodes = [t, tr, out];
  fixture.layers[0].output = out.id;
  connect(fixture.layers[0], t.id, tr.id, 'path');
  connect(fixture.layers[0], tr.id, out.id, 'path');
  const read = () => win.nect.document.layers[0].nodes[0];
  const saved = () => JSON.stringify(win.nect.document);
  const undo = () => {
    const b = document.querySelector('[data-action="undo"]');
    b.focus();
    b.click();
  };
  const redo = () => {
    const b = document.querySelector('[data-action="redo"]');
    b.focus();
    b.click();
  };
  const empty = () => assert.equal(document.querySelector('[data-action="undo"]').disabled, true);
  const select = (ids = ['c1']) => {
    const field = document.querySelector('[data-character-selection]');
    for (const option of field.options) option.selected = ids.includes(option.value);
    field.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const mode = (ids = ['c1']) => {
    document.querySelector('[data-character-mode]').click();
    select(ids);
  };
  const load = async (setup = () => {}, ids = ['c1']) => {
    const doc = structuredClone(fixture);
    setup(doc.layers[0].nodes[0], doc);
    win.nect.load(JSON.stringify(doc));
    for (let i = 0; i < 300 && win.nect.evaluate().error; i++) await tick();
    if (!doc.layers[0].locked) mode(ids);
  };
  const field = (attribute, panel = '.character-floating') =>
    document.querySelector(`${panel} [data-${attribute}]`);
  const pointer = (el, type, x = 100, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX: x,
        clientY: 20,
        ...extra,
      }),
    );
  const start = (el) => {
    el.focus();
    pointer(el, 'pointerdown');
  };
  const scrub = (el, dx = 14, extra = {}) => {
    start(el);
    pointer(document, 'pointermove', 100 + dx, extra);
    pointer(document, 'pointerup', 100 + dx, extra);
  };
  const keyboard = (el, key) =>
    el.dispatchEvent(new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key }));
  const value = (key, id = 'c1') => {
    const result = win.nect.evaluate(),
      glyph = result.textLayout(t.id).characters.find((g) => g.id === id);
    if (key.startsWith('axis')) return glyph.offset[Number(key.at(-1))];
    return glyph[key];
  };

  const q = (selector) => document.querySelector(selector);
  const change = (selector, value) => {
    const input = q(selector);
    input.value = String(value);
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const gaps = (expected) => {
    const chars = win.nect.evaluate().textLayout(t.id).characters;
    for (let i = 1; i < chars.length; i++)
      assert.ok(Math.abs(chars[i].bounds[0] - chars[i - 1].bounds[2] - expected) < 1e-7);
  };
  const state = () => ({
    project: win.nect.projectSnapshot(),
    command: win.nect.commandState,
    file: win.nect.fileState,
    gesture: win.nect.gestureState,
    selectedNodes: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
    characterMode: q('[data-character-mode]').getAttribute('aria-pressed'),
    selectedCharacters: [...(q('[data-character-selection]')?.selectedOptions || [])].map(
      (o) => o.value,
    ),
    undoDisabled: q('[data-action="undo"]').disabled,
    redoDisabled: q('[data-action="redo"]').disabled,
  });
  const rejectInvalidGaps = () => {
    const before = state(),
      expected = String(read().characterArrangement?.gap ?? 10);
    const panel = '.inspector-body',
      selector = `${panel} [data-arrangement-gap]`;
    for (const draft of [
      { value: '' },
      { value: '   ', type: 'text' },
      { value: '-', type: 'text' },
      { value: 'NaN', type: 'text' },
      { value: 'Infinity', type: 'text' },
      { value: '1e309', type: 'text' },
      { value: '-1' },
      { value: '100000.1' },
      { value: '17', badInput: true },
    ]) {
      for (const action of ['change', 'apply']) {
        const input = q(selector);
        input.type = draft.type || 'number';
        input.value = draft.value;
        // HappyDOM cannot produce a browser's partial-number badInput state.
        if (draft.badInput)
          Object.defineProperty(input, 'validity', {
            configurable: true,
            value: { badInput: true },
          });
        input.focus();
        if (action === 'change') input.dispatchEvent(new win.Event('change', { bubbles: true }));
        else q(`${panel} [data-arrangement-apply]`).click();
        if (draft.badInput) delete input.validity;
        input.type = 'number';
        const label = `${panel} ${action} ${JSON.stringify(draft)}`;
        assert.deepEqual(state(), before, `${label} preserves document, history and selection`);
        assert.equal(q(selector), input, `${label} keeps the current control`);
        assert.equal(input.value, expected, `${label} restores the previous gap`);
        assert.match(q('.status .message').textContent, /0.*100000/, `${label} shows feedback`);
      }
    }
  };
  await load();
  assert.equal(q('[data-arrangement-apply]').disabled, false);
  assert.equal(q('[data-arrangement-gap]').value, '10');
  const before = saved();
  q('[data-arrangement-apply]').click();
  assert.equal(read().characterArrangement.gap, 10);
  assert.match(q('[data-arrangement-status]').textContent, /Active/);
  gaps(10);
  const applied = saved();
  undo();
  assert.equal(saved(), before);
  empty();
  redo();
  assert.equal(saved(), applied);
  mode();
  rejectInvalidGaps();
  assert.equal(q('[data-character-axis="0"]').disabled, true);
  assert.equal(q('[data-character-axis="1"]').disabled, false);
  assert.equal(q('[data-character-style="tracking"]').disabled, true);
  assert.equal(q('[data-character-style="size"]').disabled, false);
  assert.equal(q('[data-text-kerning]').disabled, true);
  assert.equal(q('.inspector-body .field[data-key="letterSpacing"] [data-value]').disabled, true);
  change('[data-character-style="size"]', 80);
  gaps(10);
  const sized = saved();
  undo();
  assert.equal(saved(), applied);
  redo();
  assert.equal(saved(), sized);
  mode();
  const chars = win.nect.evaluate().textLayout(t.id).characters;
  const g = chars[0];
  assert.equal(
    q(`[data-character-pivot="${g.id}"]`).getAttribute('d'),
    `M${g.pivot[0] + g.offset[0] + g.arrangementOffset[0] - 3} ${g.pivot[1] + g.offset[1] + g.arrangementOffset[1]}h6 M${g.pivot[0] + g.offset[0] + g.arrangementOffset[0]} ${g.pivot[1] + g.offset[1] + g.arrangementOffset[1] - 3}v6`,
  );
  change('[data-arrangement-gap]', 17);
  gaps(17);
  const updated = saved();
  undo();
  assert.equal(saved(), sized);
  rejectInvalidGaps();
  redo();
  assert.equal(saved(), updated);
  for (const gap of [0, 0.5, 100000]) {
    const previous = saved();
    change('[data-arrangement-gap]', gap);
    assert.equal(read().characterArrangement.gap, gap);
    gaps(gap);
    const committed = saved();
    undo();
    assert.equal(saved(), previous);
    redo();
    assert.equal(saved(), committed);
    undo();
    assert.equal(saved(), previous);
  }
  scrub(q('[data-arrangement-gap]'));
  gaps(27);
  undo();
  assert.equal(saved(), updated);
  const input = q('[data-arrangement-gap]');
  start(input);
  pointer(document, 'pointermove', 124);
  keyboard(input, 'Escape');
  pointer(document, 'pointerup', 124);
  assert.equal(saved(), updated);
  q('[data-arrangement-clear]').click();
  assert.equal(read().characterArrangement, undefined);
  if (!q('[data-character-axis="0"]')) mode();
  assert.equal(q('[data-character-axis="0"]').disabled, false);
  undo();
  assert.equal(saved(), updated);
  change('[data-text]', 'Ag W');
  await tick();
  assert.match(q('[data-arrangement-status]').textContent, /Suspended:/);
  assert.equal(q('[data-arrangement-apply]').disabled, true);
  assert.ok(read().characterArrangement);
  undo();
  gaps(17);
  if (!q('[data-character-axis="0"]')) mode();
  const stable = saved();
  const b = win.nect.evaluate().textLayout(t.id).characters[0].bounds,
    x = (b[0] + b[2]) / 2,
    y = (b[1] + b[3]) / 2;
  pointer(q('.canvas-stage'), 'pointerdown', x, { clientY: y });
  assert.equal(saved(), stable, 'pointerdown never disables maintained gaps');
  pointer(document, 'pointermove', x + 20, { clientY: y });
  pointer(document, 'pointerup', x + 20, { clientY: y });
  assert.equal(saved(), stable, 'unrestricted drag is rejected atomically');
  pointer(q('.canvas-stage'), 'pointerdown', x, { clientY: y });
  pointer(document, 'pointermove', x + 20, { clientY: y, shiftKey: true });
  pointer(document, 'pointerup', x + 20, { clientY: y, shiftKey: true });
  assert.equal(saved(), stable, 'axial Shift-drag is rejected atomically');
  pointer(q('.canvas-stage'), 'pointerdown', x, { clientY: y });
  pointer(document, 'pointermove', x, { clientY: y + 25, shiftKey: true, ctrlKey: true });
  pointer(document, 'pointerup', x, { clientY: y + 25, shiftKey: true, ctrlKey: true });
  assert.equal(read().characterPositions.offsets.c1[1], 25);
  gaps(17);
  undo();
  assert.equal(saved(), stable);
  const area = q('[data-text]');
  area.focus();
  area.setSelectionRange(3, 3);
  area.dispatchEvent(
    new win.InputEvent('beforeinput', { bubbles: true, inputType: 'insertText', data: 'W' }),
  );
  area.value = 'AgWW';
  area.dispatchEvent(
    new win.InputEvent('input', { bubbles: true, inputType: 'insertText', data: 'W' }),
  );
  area.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.match(q('[data-arrangement-status]').textContent, /Suspended:/);
  assert.equal(q('[data-arrangement-apply]').disabled, false);
  q('[data-arrangement-apply]').click();
  gaps(17);
  assert.equal(read().characterArrangement.ids.length, 4);
  await load();
  rejectInvalidGaps();
  const withoutArrangement = saved();
  change('.inspector-body [data-arrangement-gap]', '0');
  assert.equal(saved(), withoutArrangement, 'a valid draft does not enable maintained gaps');
  q('.inspector-body [data-arrangement-apply]').click();
  assert.equal(read().characterArrangement.gap, 0);
  gaps(0);
  undo();
  assert.equal(saved(), withoutArrangement);
  await load();
  change('.inspector-body [data-arrangement-gap]', 23);
  assert.equal(read().characterArrangement, undefined, 'typing a draft never enables mode');
  q('.inspector-body [data-arrangement-apply]').click();
  gaps(23);
  change('.inspector-body .field[data-key="size"] [data-value]', 80);
  gaps(23);
  await load();
  const pending = q('.inspector-body [data-text]');
  pending.focus();
  pending.setSelectionRange(3, 3);
  pending.dispatchEvent(
    new win.InputEvent('beforeinput', { bubbles: true, inputType: 'insertText', data: 'W' }),
  );
  pending.value = 'AgWW';
  pending.dispatchEvent(
    new win.InputEvent('input', { bubbles: true, inputType: 'insertText', data: 'W' }),
  );
  rejectInvalidGaps();
  assert.equal(pending.value, 'AgWW', 'invalid gaps preserve the pending Text draft');
  assert.equal(read().text, 'AgW', 'invalid gaps never flush the pending Text draft');
  q('.inspector-body [data-arrangement-apply]').click();
  assert.equal(read().text, 'AgWW');
  assert.equal(read().characterArrangement.ids.length, 4);
  gaps(10);
  for (const text of ['A', 'A W', 'AgW\n']) {
    await load((n) => {
      n.text = text;
    });
    assert.equal(q('[data-arrangement-apply]').disabled, true, text);
    assert.notEqual(q('[data-arrangement-status]').textContent, '');
  }
  await load((n) => {
    n.textWrap = { width: 500 };
  });
  assert.equal(q('[data-arrangement-apply]').disabled, true);
  await load((n, doc) => {
    doc.layers[0].locked = true;
  });
  assert.equal(q('[data-arrangement-apply]').disabled, true);
  assert.equal(q('[data-arrangement-gap]').disabled, true);
  await win.happyDOM.close();
});
