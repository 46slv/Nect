import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, serialize } from '../src/core.js';

test('Angular Gradient Fill is searchable, creates by Enter and keeps settings through history/reload', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  t.after(() => win.happyDOM.abort());
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.angularTest = { open: openNodePicker };\nconst deadline = Date.now()+5000; while(!bundledFont) { if(Date.now()>deadline) throw Error("Font timeout"); await new Promise(r=>setTimeout(r,10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s);
  win.nect.load(serialize(blankDocument()));
  const before = JSON.stringify(win.nect.document);
  win.angularTest.open();
  await new Promise(queueMicrotask);
  const search = $('.search-panel input');
  search.value = '角度グラデーション';
  search.dispatchEvent(new win.Event('input'));
  assert.ok($('.picker-option[data-id="Angular Gradient Fill"]'));
  search.dispatchEvent(
    new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: 'Enter' }),
  );
  assert.equal($('.search-panel'), null);
  const created = win.nect.document.layers[0].nodes.find((n) => n.type === 'Angular Gradient Fill');
  assert.ok(created);
  assert.ok($('.field[data-key="position1"] [data-value]'));
  assert.match(document.body.textContent, /720 vector sectors/);
  const saved = JSON.stringify(win.nect.document);
  $('[data-action="undo"]').click();
  assert.equal(JSON.stringify(win.nect.document), before);
  $('[data-action="redo"]').click();
  assert.equal(JSON.stringify(win.nect.document), saved);
  win.nect.load(saved);
  assert.deepEqual(
    win.nect.document.layers[0].nodes.find((n) => n.type === 'Angular Gradient Fill').params,
    created.params,
  );
});
