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
});
