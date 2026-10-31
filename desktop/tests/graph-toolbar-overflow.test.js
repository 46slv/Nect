import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { ensureGraphToolbarOverflow } from '../src/graph-toolbar-overflow.js';

test('graph overflow reparents original commands, dismisses safely and follows panel width', async (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const { document } = win;
  const app = document.createElement('div');
  document.body.append(app);
  const markup = `<div class="graph-toolbar"><strong>Node graph</strong><span class="graph-layer-name">A very long composition title</span><select id="add-node"></select><button data-action="find-node">Find node</button><button data-action="graph-fit">Fit all</button><button data-action="graph-fit-selection" disabled>Fit selection</button><button data-action="graph-reset">Reset view</button><button data-action="graph-overview" aria-controls="graph-overview" aria-expanded="false">Overview</button></div><div id="graph-overview"></div><button id="outside">Outside</button>`;
  let width = 1200;
  let observed,
    disconnected = 0,
    resize;
  win.ResizeObserver = class {
    constructor(callback) {
      resize = callback;
    }
    observe(target) {
      observed = target;
    }
    disconnect() {
      disconnected++;
    }
  };
  const render = () => {
    app.innerHTML = markup;
    app.querySelector('.graph-toolbar').getBoundingClientRect = () => ({ width });
    ensureGraphToolbarOverflow(app, document);
  };
  render();
  const toolbar = app.querySelector('.graph-toolbar');
  assert.equal(observed, toolbar);
  const original = [...toolbar.querySelectorAll('button')];
  const reset = toolbar.querySelector('[data-action="graph-reset"]');
  let actions = 0;
  app.addEventListener('click', (event) => {
    if (event.target === reset) actions++;
  });
  assert.equal(toolbar.querySelector('details'), null, 'wide controls remain direct');
  width = 591;
  resize();
  const menu = toolbar.querySelector('details');
  const summary = menu.querySelector('summary');
  assert.equal(menu.open, false);
  assert.equal(toolbar.querySelectorAll('details button').length, 3);
  for (const button of original)
    assert.equal(app.querySelectorAll(`[data-action="${button.dataset.action}"]`).length, 1);
  assert.equal(menu.querySelector('[data-action="graph-fit-selection"]').disabled, true);
  assert.equal(toolbar.querySelector('[data-action="graph-fit"]').parentElement, toolbar);
  assert.equal(toolbar.querySelector('#add-node').parentElement, toolbar);
  assert.equal(toolbar.querySelector('[data-action="find-node"]').parentElement, toolbar);
  const overview = menu.querySelector('[data-action="graph-overview"]');
  assert.equal(
    document.getElementById(overview.getAttribute('aria-controls')).id,
    'graph-overview',
  );
  for (let i = 0; i < 3; i++) {
    menu.open = true;
    reset.focus();
    const event = new win.KeyboardEvent('keydown', {
      key: 'Escape',
      bubbles: true,
      cancelable: true,
    });
    reset.dispatchEvent(event);
    assert.equal(event.defaultPrevented, true);
    assert.equal(menu.open, false);
    assert.equal(document.activeElement, summary);
  }
  menu.open = true;
  reset.focus();
  reset.click();
  await Promise.resolve();
  assert.equal(actions, 1, 'one original delegated action, not a replacement action');
  assert.equal(menu.open, false);
  assert.equal(document.activeElement, summary);
  menu.open = true;
  overview.focus();
  document.querySelector('#outside').focus();
  assert.equal(
    menu.open,
    false,
    'Tab/focus leaving the disclosure closes it without trapping focus',
  );
  assert.equal(document.activeElement.id, 'outside');
  menu.open = true;
  reset.focus();
  width = 1200;
  resize();
  assert.equal(menu.isConnected, false);
  assert.equal(reset.parentElement, toolbar);
  assert.equal(document.activeElement, reset, 'resize preserves focused command');
  width = 591;
  resize();
  assert.equal(document.activeElement, summary, 'resize never strands focus in closed content');
  width = 1200;
  resize();
  assert.equal(document.activeElement, reset, 'hidden More restores an enabled direct control');
  for (let i = 0; i < 3; i++) {
    width = 591;
    resize();
    assert.equal(toolbar.querySelectorAll('details button').length, 3);
    width = 1200;
    resize();
    assert.equal(toolbar.querySelector('details'), null);
    for (const button of original)
      assert.equal(toolbar.querySelector(`[data-action="${button.dataset.action}"]`), button);
  }
  assert.equal(disconnected, 0, 'panel resizes reuse one observer, not one per toggle');
  width = 591;
  render();
  assert.equal(disconnected, 1);
  const replacement = app.querySelector('details');
  ensureGraphToolbarOverflow(app, document);
  assert.equal(app.querySelectorAll('details').length, 1, 'repeated mount is idempotent');
  assert.equal(app.querySelector('details'), replacement);
  assert.equal(
    replacement.open,
    false,
    'structural render does not revive a stale open disclosure',
  );
});
