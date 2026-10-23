import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { ensureCommandMenus } from '../src/command-menus.js';
import { activateCommand, revealCommandMenu } from './command-menu-actions.js';

// This is a synthetic locator contract check, not browser visibility/input proof.
// The production menu builder supplies the hierarchy; closed-details action
// activation fails instead of silently simulating a hidden DOM button click.
function fixture() {
  const window = new Window();
  const app = window.document.createElement('div');
  const markup = `<header><span class="brand">Nect</span>${[
    'new',
    'open',
    'save',
    'save-as',
    'export',
    'export-png',
    'import-svg',
    'import-image',
    'add-svg-part',
    'undo',
  ]
    .map((action) => `<button data-action="${action}">${action}</button>`)
    .join('')}</header>`;
  window.document.body.append(app);
  const render = () => {
    app.innerHTML = markup;
    ensureCommandMenus(app, window.document);
  };
  render();
  const calls = [];
  class Locator {
    constructor(roots, selector, options = {}, selected = null) {
      Object.assign(this, { roots, selector, options, selected });
    }
    elements() {
      if (this.selected) return this.selected;
      return this.roots
        .flatMap((root) => [...root.querySelectorAll(this.selector)])
        .filter(
          (element) => !this.options.hasText || this.options.hasText.test(element.textContent),
        );
    }
    element() {
      const elements = this.elements();
      assert.equal(elements.length, 1, this.selector);
      return elements[0];
    }
    filter({ has }) {
      return new Locator(
        [],
        this.selector,
        this.options,
        this.elements().filter(
          (root) => new Locator([root], has.selector, has.options).elements().length,
        ),
      );
    }
    locator(selector, options) {
      return new Locator(this.elements(), selector, options);
    }
    async getAttribute(name) {
      return this.element().getAttribute(name);
    }
    activate(method) {
      const element = this.element();
      const menu = element.closest('details');
      assert.ok(element.matches('summary') || !menu || menu.open, 'action must be visible');
      assert.equal(element.disabled || false, false, 'disabled actions stay unavailable');
      calls.push(`${method}:${element.dataset.action || element.textContent}`);
      element.click();
      if (element.matches('summary')) menu.dispatchEvent(new window.Event('toggle'));
    }
    async click() {
      this.activate('click');
    }
    async press(key) {
      assert.equal(key, this.element().matches('summary') ? 'Space' : 'Enter');
      // Happy DOM does not synthesize keyboard default activation.
      this.activate(`press:${key}`);
    }
  }
  const page = { locator: (selector, options) => new Locator([app], selector, options) };
  const menu = (label) =>
    [...app.querySelectorAll('details')].find(
      (entry) => entry.querySelector('summary').textContent === label,
    );
  return { app, page, calls, menu, render };
}

test('browser commands reveal Project, Import and Export actions through their real summaries', async () => {
  for (const [label, actions] of [
    ['Project', ['new', 'open', 'save', 'save-as']],
    ['Export', ['export', 'export-png']],
    ['Import', ['import-svg', 'import-image', 'add-svg-part', 'import-composition']],
  ]) {
    for (const action of actions) {
      const { app, page, calls, menu } = fixture();
      const original = app.querySelector(`[data-action="${action}"]`);
      assert.equal(menu(label).open, false);
      await activateCommand(page, action);
      assert.deepEqual(calls, [`click:${label}`, `click:${action}`]);
      assert.equal(menu(label).open, true);
      assert.equal(menu(label).querySelector(`[data-action="${action}"]`), original);
    }
  }
});

test('repeated commands keep an open menu open and switching uses the other summary', async () => {
  const { page, calls, menu } = fixture();
  await activateCommand(page, 'save');
  await activateCommand(page, 'save');
  await activateCommand(page, 'export');
  assert.deepEqual(calls, [
    'click:Project',
    'click:save',
    'click:save',
    'click:Export',
    'click:export',
  ]);
  assert.equal(menu('Project').open, false);
  assert.equal(menu('Export').open, true);
});

test('keyboard New opens Project and invokes New without a pointer click', async () => {
  const { page, calls } = fixture();
  await activateCommand(page, 'new', { keyboard: true });
  await activateCommand(page, 'new', { keyboard: true });
  assert.deepEqual(calls, ['press:Space:Project', 'press:Enter:new', 'press:Enter:new']);
});

test('non-menu actions retain normal activation and rerenders get fresh locators', async () => {
  const { app, page, calls, render } = fixture();
  await activateCommand(page, 'undo');
  await activateCommand(page, 'save');
  const oldButton = app.querySelector('[data-action="save"]');
  render();
  assert.equal(oldButton.isConnected, false);
  await activateCommand(page, 'save');
  assert.deepEqual(calls, [
    'click:undo',
    'click:Project',
    'click:save',
    'click:Project',
    'click:save',
  ]);
});

test('disabled menu commands are not bypassed', async () => {
  const { app, page, calls } = fixture();
  app.querySelector('[data-action="save"]').disabled = true;
  await assert.rejects(activateCommand(page, 'save'), /disabled actions stay unavailable/);
  assert.deepEqual(calls, ['click:Project']);
});

test('revealing Export for PNG options does not activate any command or close it on reuse', async () => {
  const { page, calls, menu } = fixture();
  await revealCommandMenu(page, 'Export');
  await revealCommandMenu(page, 'Export');
  assert.deepEqual(calls, ['click:Export']);
  assert.equal(menu('Export').open, true);
  await activateCommand(page, 'export-png');
  assert.deepEqual(calls, ['click:Export', 'click:export-png']);
});
