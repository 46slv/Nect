import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createColorCoverageView } from '../src/color-coverage-view.js';

const flush = () => new Promise((resolve) => setImmediate(resolve));
const coverage = (a = 60, b = 30, other = 10) => ({
  slots: [
    { id: 'slot-b', percent: b },
    { id: 'slot-a', percent: a },
  ],
  other: { percent: other },
  sampleSize: { width: 256, height: 128 },
});

function setup(t) {
  const window = new Window();
  t.after(() => window.happyDOM.abort());
  const document = window.document;
  document.body.innerHTML = `<main><section id="panel"><input id="name" value="Original"><output data-color-coverage-id="slot-a">—</output><output data-color-coverage-id="slot-b">—</output><output data-color-coverage-id="missing">—</output><p data-color-coverage-status role="status">Initial</p></section></main>`;
  const app = document.querySelector('main');
  const requests = [];
  let documentId = 'document-a',
    revision = 1,
    open = true;
  const view = createColorCoverageView({
    app,
    getKey: () => JSON.stringify([documentId, revision]),
    isOpen: () => open,
    measure: (isCurrent) =>
      new Promise((resolve, reject) => requests.push({ isCurrent, resolve, reject })),
  });
  return {
    window,
    document,
    app,
    view,
    requests,
    output: (id) => app.querySelector(`[data-color-coverage-id="${id}"]`),
    get status() {
      return app.querySelector('[data-color-coverage-status]');
    },
    set revision(value) {
      revision = value;
    },
    set documentId(value) {
      documentId = value;
    },
    set open(value) {
      open = value;
    },
  };
}

test('same-key refreshes share one pending measurement and cache its stable-ID result', async (t) => {
  const s = setup(t);
  s.view.refresh();
  s.view.refresh();
  assert.equal(s.status.textContent, 'Measuring visible area…');
  await flush();
  assert.equal(s.requests.length, 1);
  assert.equal(s.requests[0].isCurrent(), true);
  for (let i = 0; i < 5; i++) s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 1);
  s.requests[0].resolve(coverage());
  await flush();
  assert.equal(s.output('slot-a').textContent, '60%');
  assert.equal(s.output('slot-b').textContent, '30%');
  assert.equal(s.output('missing').textContent, '—');
  assert.equal(s.status.textContent, 'Visible area · approx. 256 × 128 sample · Other 10%');
  for (let i = 0; i < 5; i++) s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 1, 'Settled same-key data must not decode again');
});

test('revision and document changes discard old completions and queue the latest key', async (t) => {
  for (const change of ['revision', 'document']) {
    await t.test(change, async (t) => {
      const s = setup(t);
      s.view.refresh();
      await flush();
      if (change === 'revision') s.revision = 2;
      else s.documentId = 'document-b';
      assert.equal(s.requests[0].isCurrent(), false);
      s.view.refresh();
      assert.equal(s.requests.length, 1, 'Do not start overlapping decodes');
      s.requests[0].resolve(coverage(99, 1, 0));
      await flush();
      assert.equal(s.requests.length, 2);
      assert.equal(s.output('slot-a').textContent, '—', 'Never display an old completion');
      assert.equal(s.status.textContent, 'Measuring visible area…');
      assert.equal(s.requests[1].isCurrent(), true);
      s.requests[1].resolve(coverage(20, 40, 40));
      await flush();
      assert.equal(s.output('slot-a').textContent, '20%');
      assert.equal(s.output('slot-b').textContent, '40%');
    });
  }
});

test('stale rejections cannot poison the next key and multiple changes coalesce', async (t) => {
  const s = setup(t);
  s.view.refresh();
  await flush();
  s.revision = 2;
  s.view.refresh();
  s.revision = 3;
  s.view.refresh();
  s.documentId = 'document-b';
  s.view.refresh();
  s.requests[0].reject(new Error('Old document failed'));
  await flush();
  assert.equal(s.requests.length, 2);
  assert.equal(s.status.textContent, 'Measuring visible area…');
  assert.equal(s.requests[1].isCurrent(), true);
  s.requests[1].resolve(coverage(75, 25, 0));
  await flush();
  assert.equal(s.output('slot-a').textContent, '75%');
});

test('key changes before the measurement microtask skip obsolete work', async (t) => {
  const s = setup(t);
  s.view.refresh();
  s.revision = 2;
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 1);
  assert.equal(s.requests[0].isCurrent(), true);
  s.requests[0].resolve(coverage());
  await flush();
  assert.equal(s.output('slot-a').textContent, '60%');
});

test('closed panels neither start work nor apply pending completions; reopening retries', async (t) => {
  const s = setup(t);
  s.open = false;
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 0);
  assert.equal(s.status.textContent, 'Initial');
  s.open = true;
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 1);
  s.open = false;
  assert.equal(s.requests[0].isCurrent(), false);
  s.status.textContent = 'Closed panel sentinel';
  s.requests[0].resolve(coverage(90, 10, 0));
  await flush();
  assert.equal(s.output('slot-a').textContent, '—');
  assert.equal(s.status.textContent, 'Closed panel sentinel');
  assert.equal(s.requests.length, 1);
  s.open = true;
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 2, 'A result discarded while closed must be measured again');
  s.requests[1].resolve(coverage(10, 20, 70));
  await flush();
  assert.equal(s.output('slot-a').textContent, '10%');
});

test('close/reopen preserves valid settled cache without another decode', async (t) => {
  const s = setup(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage());
  await flush();
  s.open = false;
  s.view.refresh();
  s.open = true;
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 1);
  assert.equal(s.output('slot-a').textContent, '60%');
});

test('error state clears old percentages, caches the failure, and recovers on a new key', async (t) => {
  const s = setup(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage());
  await flush();
  s.revision = 2;
  s.view.refresh();
  assert.equal(s.output('slot-a').textContent, '—');
  await flush();
  s.requests[1].reject(new Error('Decoder failed'));
  await flush();
  assert.equal(s.output('slot-a').textContent, '—');
  assert.equal(s.output('slot-b').textContent, '—');
  assert.equal(s.status.textContent, 'Coverage unavailable: Decoder failed');
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 2, 'Same-key failure must not enter a decode retry loop');
  s.revision = 3;
  s.view.refresh();
  await flush();
  s.requests[2].resolve(coverage(30, 70, 0));
  await flush();
  assert.equal(s.output('slot-a').textContent, '30%');
  assert.match(s.status.textContent, /^Visible area/);
});

test('missing results are visible errors rather than fabricated percentages', async (t) => {
  const s = setup(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(undefined);
  await flush();
  assert.equal(s.output('slot-a').textContent, '—');
  assert.equal(s.status.textContent, 'Coverage unavailable: No coverage result');
  assert.equal(s.requests.length, 1);
});

test('coverage and error messages are painted as text, never interpreted as HTML', async (t) => {
  const s = setup(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage('<img src=x onerror=alert(1)>', 30, '<script>bad()</script>'));
  await flush();
  assert.equal(s.output('slot-a').textContent, '<img src=x onerror=alert(1)>%');
  assert.match(s.status.textContent, /Other <script>bad\(\)<\/script>%/);
  assert.equal(s.app.querySelector('img, script'), null);
  s.revision = 2;
  s.view.refresh();
  await flush();
  s.requests[1].reject(new Error('<img src=x onerror=alert(2)>'));
  await flush();
  assert.equal(s.status.textContent, 'Coverage unavailable: <img src=x onerror=alert(2)>');
  assert.equal(s.app.querySelector('img, script'), null);
});

test('output patches preserve focused input identity, selection, draft, and IME composition', async (t) => {
  const s = setup(t);
  const input = s.document.querySelector('#name');
  const panel = s.document.querySelector('#panel');
  const output = s.output('slot-a');
  let composing = false,
    edits = 0;
  input.addEventListener('compositionstart', () => {
    composing = true;
  });
  input.addEventListener('compositionend', () => {
    composing = false;
  });
  input.addEventListener('input', () => edits++);
  input.focus();
  input.value = '編集中の色';
  input.setSelectionRange(1, 4);
  input.dispatchEvent(new s.window.CompositionEvent('compositionstart', { data: '編' }));
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage());
  await flush();
  assert.equal(s.document.querySelector('#panel'), panel);
  assert.equal(s.document.querySelector('#name'), input);
  assert.equal(s.output('slot-a'), output);
  assert.equal(s.document.activeElement, input);
  assert.equal(input.value, '編集中の色');
  assert.equal(input.selectionStart, 1);
  assert.equal(input.selectionEnd, 4);
  assert.equal(composing, true);
  assert.equal(edits, 0, 'Derived measurements must not dispatch authored input edits');
  input.dispatchEvent(new s.window.CompositionEvent('compositionend', { data: '編集中' }));
  assert.equal(composing, false, 'The original input retains its composition event listeners');
});
