import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, serialize } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';

test('Inspector Escape after document loads reaches its drawer owner and restores focus', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    },
  });
  const source = resolveMainRelativeImports(
    (await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8')).replace(
      "import './style.css';",
      '',
    ),
  );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (selector) => win.document.querySelector(selector);
  const escape = (repeat = false) =>
    win.document.activeElement.dispatchEvent(
      new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true, repeat }),
    );
  const snapshot = () => ({
    document: serialize(win.nect.document),
    svg: win.nect.export(),
    file: win.nect.fileState,
    command: win.nect.commandState,
    gesture: win.nect.gestureState,
    selected: [...win.document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
    undo: $('[data-action="undo"]').disabled,
    redo: $('[data-action="redo"]').disabled,
  });

  for (const name of ['First loaded document', 'Replacement document']) {
    const doc = makeDocument();
    doc.name = name;
    win.nect.load(serialize(doc));
    revealDOMBoundary(win.document, 'graph');
    revealDOMBoundary(win.document, 'canvas');
    openDOMPanel(win.document, 'inspector');
    const before = snapshot();
    const inspector = $('.inspector');
    assert.equal(win.document.activeElement, inspector);
    escape();
    const trigger = $('[data-workspace-panel="inspector"]');
    assert.equal(trigger.getAttribute('aria-expanded'), 'false', name);
    assert.equal(win.document.activeElement, trigger, 'dismissal restores the live rail trigger');
    assert.equal(inspector.isConnected, true, 'view-only dismissal does not rerender the editor');
    for (const repeat of [false, true]) {
      escape(repeat);
      assert.equal(win.document.activeElement, trigger);
      assert.equal(trigger.getAttribute('aria-expanded'), 'false');
    }
    assert.equal(
      $('[aria-label="Toggle node graph controls"]').getAttribute('aria-expanded'),
      'true',
    );
    assert.equal(
      $('[aria-label="Toggle artboard view controls"]').getAttribute('aria-expanded'),
      'true',
    );
    assert.deepEqual(snapshot(), before);

    openDOMPanel(win.document, 'inspector');
    const input = $('.inspector-body input[type="number"]');
    input.focus();
    escape();
    assert.equal($('[data-workspace-panel="inspector"]').getAttribute('aria-expanded'), 'true');
    assert.equal(win.document.activeElement, input, 'Inspector input retains Escape ownership');
    assert.deepEqual(snapshot(), before);
    $('[data-workspace-panel="inspector"]').click();
  }
});
