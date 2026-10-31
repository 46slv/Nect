import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { ensureCommandMenus } from '../src/command-menus.js';

test('command menus are idempotent and preserve an open Import menu', () => {
  const window = new Window();
  const app = window.document.createElement('div');
  app.innerHTML = `<header><div class="brand">Nect</div>
    <button data-action="import-svg">Import SVG</button>
    <button data-action="import-image">Import image</button>
    <button data-action="add-svg-part">Add SVG</button>
    <button data-action="new">New</button><button data-action="open">Open</button>
    <button data-action="save">Save</button><button data-action="save-as">Save As</button>
    <button data-action="export">Export</button><button data-action="export-png">PNG</button>
    <select id="png-scale"><option>1</option></select></header>`;
  window.document.body.append(app);

  ensureCommandMenus(app, window.document);
  const menu = [...app.querySelectorAll('.command-menu')].find(
    (entry) => entry.querySelector('summary')?.textContent === 'Project',
  );
  const importMenu = [...app.querySelectorAll('.command-menu')].find(
    (entry) => entry.querySelector('summary')?.textContent === 'Import',
  );
  const svg = importMenu.querySelector('[data-action="import-svg"]');
  importMenu.open = true;
  ensureCommandMenus(app, window.document);

  assert.equal(app.querySelectorAll('.command-menu').length, 3);
  assert.equal(importMenu.isConnected, true);
  assert.equal(importMenu.open, true);
  assert.equal(importMenu.querySelector('[data-action="import-svg"]'), svg);
  assert.equal(menu?.querySelector('summary')?.textContent, 'Project');
  assert.equal(
    importMenu.querySelector('[data-action="import-composition"]')?.textContent,
    'Project composition',
  );

  const menus = [...app.querySelectorAll('.command-menu')];
  const exportMenu = menus.find(
    (entry) => entry.querySelector('summary')?.textContent === 'Export',
  );
  for (const active of [menus[0], menus[1], exportMenu, menus[0]]) {
    active.open = true;
    active.dispatchEvent(new window.Event('toggle'));
    assert.equal(menus.filter((entry) => entry.open).length, 1);
    assert.equal(active.open, true);
  }
  menus.forEach((entry) => (entry.open = false));
  assert.equal(
    menus.some((entry) => entry.open),
    false,
  );
});

test('toolbar grouping keeps labeled output and history controls reachable once', () => {
  const window = new Window();
  const app = window.document.createElement('div');
  app.innerHTML = `<header><div class="brand">Nect</div>
    <button data-action="new">New</button><button data-action="open">Open</button>
    <button data-action="save">Save</button><button data-action="save-as">Save As</button>
    <button data-action="add-composition">Add composition</button><button data-action="asset-library">Assets</button>
    <button data-action="export">Export SVG</button><button data-action="export-png">Export PNG</button>
    <select id="svg-export-sizing" aria-label="SVG output sizing"></select>
    <select id="svg-export-unit" aria-label="SVG export units"></select>
    <select id="svg-export-precision" aria-label="SVG geometry precision"></select>
    <select id="png-export-mode" aria-label="PNG size mode"></select>
    <label>PPI<input id="png-ppi"></label><output data-png-dimensions></output>
    <details id="export-presets"><summary>Export presets</summary></details>
    <button data-action="undo">Undo</button><button data-action="redo">Redo</button></header>`;
  window.document.body.append(app);
  ensureCommandMenus(app, window.document);
  const output = app.querySelector('[data-output-settings]');
  assert.ok(output);
  for (const [id, label] of [
    ['svg-export-sizing', 'SVG sizing'],
    ['svg-export-unit', 'SVG units'],
    ['svg-export-precision', 'SVG precision'],
    ['png-export-mode', 'PNG size mode'],
  ]) {
    assert.equal(output.querySelectorAll(`#${id}`).length, 1);
    assert.equal(output.querySelector(`#${id}`).parentElement.textContent, label);
  }
  assert.ok(output.querySelector('#export-presets'));
  assert.ok(app.querySelector('[aria-label="History"] [data-action="undo"]'));
  assert.ok(
    app.querySelector('.command-menu [data-action="asset-library"]'),
  );
  window.happyDOM.abort();
});
