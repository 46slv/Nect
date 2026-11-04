import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, point, serialize } from '../src/core.js';
const buffer = async (path) => {
  const b = await fs.readFile(new URL(path, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
test('Type on Path controls attach/edit/detach atomically with schema, locks, source identity and history', async () => {
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
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source = source.replace(
    'const binding = numericBehaviorBinding(numeric),',
    'const binding = numericBehaviorBinding(numeric), __record = (globalThis.__pathBinding = !!binding),',
  );
  source +=
    '\nwindow.pathTestSelection = (primary, ids, refresh = false) => { selected = primary; selection = new Set(ids); if (refresh) render(); };';
  source +=
    '\nwindow.pathTestHistory = () => ({revision: session.state.expectedRevision, past: history.past.length, future: history.future.length});';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const until = async (fn) => {
    const limit = Date.now() + 6000;
    while (!fn()) {
      assert.ok(Date.now() < limit, 'Timed out');
      await new Promise((r) => setTimeout(r, 10));
    }
  };
  const tick = () => new Promise((r) => setTimeout(r, 20));
  await until(() => !!win.nect);
  const doc = newDocument(),
    t = node('Text'),
    c = node('Curve'),
    c2 = node('Curve'),
    out = node('Output');
  t.text = 'TYPE';
  t.params.size.value = 40;
  t.params.position.value = [100, 250];
  c.points = [point(30, 100), point(630, 100)];
  c.params.closed.value = false;
  c2.points = [point(30, 100), point(630, 100)];
  c2.params.closed.value = false;
  out.params.path.source = t.id;
  doc.layers[0].nodes = [t, c, c2, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  openDOMPanel(document, 'inspector');
  await until(() => win.nect.evaluate().error === null);
  const saved = () => JSON.stringify(win.nect.document),
    text = () => win.nect.document.layers[0].nodes[0],
    control = (key) => document.querySelector(`.inspector-body [data-${key}]`),
    change = async (key, value) => {
      const el = control(key);
      if (typeof value === 'boolean') el.checked = value;
      else el.value = value;
      el.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
    },
    undo = () => document.querySelector('[data-action="undo"]').click(),
    redo = () => document.querySelector('[data-action="redo"]').click();
  const before = saved(),
    flatSVG = win.nect.export();
  await change('text-path', JSON.stringify([c.id, c.id]));
  assert.equal(win.nect.document.schemaVersion, Math.max(doc.schemaVersion, 7));
  assert.equal(text().pathAttachment.nodeId, c.id);
  assert.notEqual(win.nect.export(), flatSVG);
  const attached = saved();
  undo();
  assert.equal(saved(), before);
  redo();
  assert.equal(saved(), attached);
  await change('text-path-start', '.5');
  await change('text-alignment', 'center');
  const centered = saved(),
    currentSVG = win.nect.export();
  await change('text-path-mode', 'distance');
  assert.ok(Math.abs(text().pathAttachment.start - 300) < 1e-7);
  assert.equal(win.nect.export(), currentSVG);
  const pointer = (el, type, x = 100) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX: x,
        clientY: 20,
      }),
    );
  const pathBaseline = text().pathAttachment.start;
  const pathInput = control('text-path-start');
  pathInput.focus();
  pointer(pathInput, 'pointerdown');
  assert.equal(globalThis.__pathBinding, true, 'shared behavior resolves the path input');
  pointer(document, 'pointermove', 114);
  assert.equal(text().pathAttachment.start, pathBaseline, 'preview remains detached');
  pointer(document, 'pointerup', 114);
  assert.equal(text().pathAttachment.start, 310);
  undo();
  const afterFocusedUndo = control('text-path-start');
  assert.equal(afterFocusedUndo.value, String(pathBaseline));
  assert.equal(afterFocusedUndo.defaultValue, String(pathBaseline));
  afterFocusedUndo.focus();
  pointer(afterFocusedUndo, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointercancel', 114);
  assert.equal(text().pathAttachment.start, pathBaseline, 'cancel does not re-commit old value');
  redo();
  assert.equal(text().pathAttachment.start, 310);
  control('text-path-start').blur();
  undo();
  assert.equal(text().pathAttachment.start, pathBaseline);
  redo();
  assert.equal(text().pathAttachment.start, 310);
  undo();
  assert.equal(text().pathAttachment.start, pathBaseline);
  const beforeCancel = saved();
  const cancelInput = control('text-path-start');
  cancelInput.focus();
  pointer(cancelInput, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointercancel', 114);
  assert.equal(saved(), beforeCancel);
  redo();
  assert.equal(text().pathAttachment.start, 310, 'cancel preserves redo');
  undo();
  const typed = control('text-path-start');
  typed.value = '320';
  typed.focus();
  pointer(typed, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  assert.equal(text().pathAttachment.start, 330);
  control('text-path-start').blur();
  undo();
  assert.equal(text().pathAttachment.start, 320, 'scrub is separate from typing');
  undo();
  assert.equal(text().pathAttachment.start, pathBaseline, 'typing has its own undo');
  const readonlyBefore = saved();
  const readonlyInput = control('text-path-start');
  readonlyInput.focus();
  pointer(readonlyInput, 'pointerdown');
  pointer(document, 'pointermove', 114);
  readonlyInput.readOnly = true;
  pointer(document, 'pointerup', 114);
  assert.equal(saved(), readonlyBefore, 'readonly interruption does not commit');
  control('text-path-start').blur();
  assert.equal(saved(), readonlyBefore, 'late blur does not commit preview');
  await change('text-path-reversed', true);
  assert.ok(win.nect.evaluate().textLayout(t.id).placements[0].tangent[0] < 0);
  const reversed = saved();
  await change('text-path-start', '-999');
  assert.equal(saved(), reversed);
  assert.match(document.querySelector('.status .message').textContent, /OVERFLOW/);
  await change('text-path-start', '');
  assert.equal(saved(), reversed);
  await change('writing-mode', 'vertical-rl');
  assert.equal(saved(), reversed);
  assert.match(document.querySelector('.status .message').textContent, /horizontal/);
  await change('text', 'A\nB');
  assert.equal(saved(), reversed);
  assert.match(document.querySelector('.status .message').textContent, /one line/);
  const ta = control('text');
  ta.setSelectionRange(0, 1);
  ta.dispatchEvent(new win.Event('select', { bubbles: true }));
  document.querySelector('.inspector-body [data-style-text-range]').click();
  await change('character-axis', '9');
  assert.equal(text().characterPositions.offsets.c1[0], 9);
  assert.match(
    document.querySelector('.inspector-body').textContent,
    /Canvas character dragging is unavailable/,
  );
  const adjusted = saved();
  await change('text-path', '');
  assert.equal(text().pathAttachment, undefined);
  assert.deepEqual(text().params.position.value, [100, 250]);
  undo();
  assert.equal(saved(), adjusted);
  win.nect.load(adjusted);
  assert.equal(saved(), adjusted);
  document.querySelector(`.graph [data-delete="${c.id}"]`).click();
  assert.equal(saved(), adjusted);
  assert.match(document.querySelector('.status .message').textContent, /Missing source/);
  const locked = JSON.parse(adjusted);
  locked.layers[0].locked = true;
  win.nect.load(JSON.stringify(locked));
  assert.equal(control('text-path').disabled, true);
  assert.equal(control('text-path-start').disabled, true);
  assert.notEqual(centered, attached);
  // An empty source has no glyph span, allowing both normalized endpoints to
  // remain valid with the existing one-unit scrub step.
  const endpointDoc = JSON.parse(attached);
  const endpointText = endpointDoc.layers[0].nodes[0];
  endpointText.text = '';
  endpointText.alignment = 'start';
  endpointText.pathAttachment.startMode = 'normalized';
  endpointText.pathAttachment.start = 0;
  win.nect.load(JSON.stringify(endpointDoc));
  const normalizedStart = saved();
  let normalizedInput = control('text-path-start');
  normalizedInput.focus();
  globalThis.__pathBinding = false;
  pointer(normalizedInput, 'pointerdown');
  assert.equal(globalThis.__pathBinding, true, 'normalized resolver admitted input');
  pointer(document, 'pointermove', 105);
  pointer(document, 'pointerup', 105);
  assert.equal(
    text().pathAttachment.start,
    1,
    'normalized endpoint: ' + document.querySelector('.status .message').textContent,
  );
  assert.equal(text().pathAttachment.startMode, 'normalized');
  control('text-path-start').blur();
  undo();
  assert.equal(saved(), normalizedStart);
  redo();
  assert.equal(text().pathAttachment.start, 1);
  const normalizedEnd = saved();
  normalizedInput = control('text-path-start');
  normalizedInput.focus();
  pointer(normalizedInput, 'pointerdown');
  pointer(document, 'pointermove', 105);
  pointer(document, 'pointerup', 105);
  assert.equal(saved(), normalizedEnd, 'invalid normalized preview does not commit');
  // Returning to the origin cancels without consuming the redo entry.
  control('text-path-start').blur();
  undo();
  assert.equal(saved(), normalizedStart);
  const originInput = control('text-path-start');
  originInput.focus();
  pointer(originInput, 'pointerdown');
  pointer(document, 'pointermove', 105);
  pointer(document, 'pointermove', 100);
  pointer(document, 'pointerup', 100);
  assert.equal(saved(), normalizedStart);
  control('text-path-start').blur();
  redo();
  assert.equal(saved(), normalizedEnd, 'origin return keeps redo');
  undo();
  const staleModeInput = control('text-path-start');
  staleModeInput.focus();
  pointer(staleModeInput, 'pointerdown');
  await change('text-path-mode', 'distance');
  const afterModeChange = saved();
  assert.equal(text().pathAttachment.startMode, 'distance');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  staleModeInput.blur();
  assert.equal(saved(), afterModeChange, 'late pointer cannot apply after mode change');
  const staleSourceInput = control('text-path-start');
  staleSourceInput.focus();
  pointer(staleSourceInput, 'pointerdown');
  await change('text', 'A');
  assert.equal(text().text, 'A');
  const afterSourceChange = saved();
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  staleSourceInput.blur();
  assert.equal(saved(), afterSourceChange, 'late pointer cannot apply after source change');
  const oldPathInput = control('text-path-start');
  oldPathInput.focus();
  pointer(oldPathInput, 'pointerdown');
  await change('text-path', JSON.stringify([c2.id, c2.id]));
  assert.equal(text().pathAttachment.nodeId, c2.id);
  const afterPathChange = saved(),
    historyAfterPath = win.pathTestHistory();
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  oldPathInput.blur();
  assert.equal(saved(), afterPathChange, 'late pointer cannot restore the old path');
  assert.deepEqual(win.pathTestHistory(), historyAfterPath);
  await change('text-path-mode', 'distance');
  assert.equal(text().pathAttachment.startMode, 'distance');
  const nextInput = control('text-path-start');
  nextInput.focus();
  pointer(nextInput, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  assert.equal(text().pathAttachment.start, 10, 'current field remains editable');
  control('text-path-start').blur();
  undo();
  assert.equal(text().pathAttachment.start, 0, 'new path scrub is one undo');
  const baselineSelectionDoc = saved(),
    baselineSelectionHistory = win.pathTestHistory();
  const primaryInput = control('text-path-start');
  primaryInput.focus();
  pointer(primaryInput, 'pointerdown');
  pointer(document, 'pointermove', 114);
  win.pathTestSelection(c2.id, [c2.id]);
  pointer(document, 'pointerup', 114);
  primaryInput.blur();
  assert.equal(saved(), baselineSelectionDoc, 'different primary cancels the old preview');
  assert.deepEqual(win.pathTestHistory(), baselineSelectionHistory);
  win.pathTestSelection(t.id, [t.id], true);
  const multiInput = control('text-path-start');
  multiInput.focus();
  pointer(multiInput, 'pointerdown');
  pointer(document, 'pointermove', 114);
  win.pathTestSelection(t.id, [t.id, c2.id]);
  pointer(document, 'pointerup', 114);
  multiInput.blur();
  assert.equal(saved(), baselineSelectionDoc, 'changed selection set cancels the old preview');
  assert.deepEqual(win.pathTestHistory(), baselineSelectionHistory);
  win.pathTestSelection(t.id, [t.id], true);
  const recovered = control('text-path-start');
  recovered.focus();
  pointer(recovered, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointercancel', 114);
  assert.equal(saved(), baselineSelectionDoc, 'new current gesture remains cancellable');
  assert.deepEqual(win.pathTestHistory(), baselineSelectionHistory);
  await win.happyDOM.close();
});
