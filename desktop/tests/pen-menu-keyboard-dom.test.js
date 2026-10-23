import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, serialize } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { penMenuMutationObserver } from './helpers/pen-menu-mutation-observer.js';
import { isolatedDomScenarios } from './run-isolated-dom-scenarios.js';

test('Pen finish shortcuts leave native menu activation intact', async (rawT) => {
  const t = isolatedDomScenarios(rawT, import.meta.url);
  const win = new Window({
    url: 'http://localhost:5173',
    // Surface event/observer failures instead of hiding them in the virtual console.
    settings: { disableErrorCapturing: true },
  });
  rawT.after(() => win.happyDOM.close());
  win.MutationObserver = penMenuMutationObserver(win.MutationObserver);
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
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
      const data = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
  const fontRequest = "fetch('/fonts/DejaVuSans.ttf')";
  assert.equal(source.split(fontRequest).length, 2, 'capture the real font startup promise');
  source = source.replace(fontRequest, `window.penKeyboardFontReady = ${fontRequest}`);
  source += `
    window.penKeyboardState = () => structuredClone({
      draft: penDraft?.layer, drag: drag?.type, tool, activeLayer, selected,
      artworkTarget, selection: [...selection], past: history.past.length,
      future: history.future.length,
    });
    window.penKeyboardFontLoaded = () => !!bundledFont;
  `;
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await win.penKeyboardFontReady;
  const $ = (selector) => document.querySelector(selector);
  const state = () => win.penKeyboardState();
  const settleMenus = async (phase) => {
    // Drain the actual observer microtasks, then inspect their result. No timer
    // can recover a GC'd Happy DOM listener, and calling ensureCommandMenus here
    // would bypass the startup path this test needs to exercise.
    await win.happyDOM.waitUntilComplete();
    const summaries = [...document.querySelectorAll('.command-menu > summary')].map(
      (summary) => summary.textContent,
    );
    const diagnostics = JSON.stringify({
      phase,
      fontLoaded: win.penKeyboardFontLoaded(),
      headerMounted: !!$('header'),
      summaries,
      status: $('.status .message')?.textContent,
      console: win.happyDOM.virtualConsolePrinter.readAsString(),
    });
    assert.equal(win.penKeyboardFontLoaded(), true, diagnostics);
    assert.deepEqual(summaries, ['Project', 'Import', 'Export'], diagnostics);
  };
  await settleMenus('startup');
  const saved = () => serialize(win.nect.document);
  const pointer = (type, position = [100, 100], target = document) =>
    target.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        buttons: type === 'pointerup' ? 0 : 1,
        clientX: position[0],
        clientY: position[1],
      }),
    );
  const key = (target, name, extra = {}) => {
    const event = new win.KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: name,
      ...extra,
    });
    target.dispatchEvent(event);
    return event;
  };
  const point = (position, release = true) => {
    pointer('pointerdown', position, $('.artboard'));
    pointer('pointermove', [position[0] + 20, position[1] + 10]);
    if (release) pointer('pointerup', [position[0] + 20, position[1] + 10]);
  };
  const start = async (released, pressed = false) => {
    win.nect.load(serialize(blankDocument()));
    // Retain a redo entry too: menu activation must not consume either stack.
    $('[data-tool="Circle"]').click();
    $('[data-action="undo"]').click();
    $('[data-tool="pen"]').click();
    for (let i = 0; i < released; i++) point([100 + i * 80, 100 + i * 40]);
    if (pressed) point([350, 300], false);
    await settleMenus(`Pen draft: ${released} released, pointer ${pressed ? 'held' : 'released'}`);
    assert.ok(state().draft);
    assert.equal(state().drag, pressed ? 'pen' : undefined);
    assert.equal(state().past, 0);
    assert.equal(state().future, 1);
  };
  const projectSummary = () =>
    [...document.querySelectorAll('.command-menu > summary')].find(
      (summary) => summary.textContent === 'Project',
    );

  for (const pressed of [false, true]) {
    for (const released of pressed ? [0, 1, 2] : [1, 2]) {
      await t.test(
        `Project Enter preserves ${released} released points${pressed ? ' and held pointer' : ''}`,
        async () => {
          await start(released, pressed);
          const summary = projectSummary();
          summary.focus();
          const before = state(),
            documentBefore = saved(),
            projectBefore = win.nect.projectSnapshot();
          // Synthetic DOM does not implement native keyboard-to-click activation.
          // Prove the app leaves Enter's default action and draft ownership intact.
          const event = key(summary, 'Enter');
          assert.deepEqual(state(), before, 'menu Enter must neither finish nor cancel the draft');
          assert.equal(event.defaultPrevented, false, 'native summary activation must be allowed');
          assert.equal(document.activeElement, summary);
          assert.equal(saved(), documentBefore);
          assert.deepEqual(win.nect.projectSnapshot(), projectBefore);
          if (pressed) {
            pointer('pointerup', [370, 310]);
            assert.equal(state().drag, undefined);
            assert.equal(state().draft.nodes[0].points.length, released + 1);
            assert.equal(state().past, 0);
            assert.equal(state().future, 1);
            assert.equal(saved(), documentBefore);
          }
        },
      );
    }
  }

  await t.test(
    'all command summaries and their descendants retain Enter and Space defaults',
    async () => {
      await start(2);
      const before = state(),
        documentBefore = saved();
      for (const summary of document.querySelectorAll('.command-menu > summary')) {
        const label = document.createElement('span');
        label.textContent = summary.textContent;
        summary.replaceChildren(label);
        for (const target of [summary, label]) {
          summary.focus();
          assert.equal(key(target, 'Enter').defaultPrevented, false);
          assert.equal(key(target, ' ', { code: 'Space' }).defaultPrevented, false);
          assert.deepEqual(state(), before);
          assert.equal(saved(), documentBefore);
          assert.equal(document.activeElement, summary);
        }
      }
    },
  );

  await t.test(
    'buttons, links, role buttons and editing controls retain their existing defaults',
    async () => {
      await start(2);
      const before = state(),
        documentBefore = saved(),
        fixture = document.createElement('div');
      fixture.innerHTML =
        '<button><span>Button</span></button><a href="#"><span>Link</span></a>' +
        '<div role="button" tabindex="0"><span>Action</span></div>' +
        '<input><textarea></textarea><select><option>Value</option></select>' +
        '<div contenteditable="true">Edit</div>';
      $('#app').append(fixture);
      for (const target of fixture.querySelectorAll(
        'span,input,textarea,select,[contenteditable]',
      )) {
        assert.equal(key(target, 'Enter').defaultPrevented, false);
        assert.deepEqual(state(), before);
        assert.equal(saved(), documentBefore);
      }
    },
  );

  await t.test(
    'canvas Enter and Escape still finish only released points in one history unit',
    async () => {
      for (const name of ['Enter', 'Escape']) {
        for (const pressed of [false, true]) {
          await start(2, pressed);
          const before = saved(),
            points = state().draft.nodes[0].points.slice(0, 2);
          assert.equal(key($('.canvas-stage'), name).defaultPrevented, true);
          assert.equal(state().draft, undefined);
          assert.equal(state().drag, undefined);
          assert.equal(state().past, 1);
          assert.equal(state().future, 0);
          assert.equal(win.nect.document.layers.length, 2);
          assert.deepEqual(win.nect.document.layers.at(-1).nodes[0].points, points);
          const finished = saved();
          pointer('pointerup', [370, 310]);
          assert.equal(saved(), finished, 'late release must not add the rolled-back point');
          $('[data-action="undo"]').click();
          assert.equal(saved(), before);
          $('[data-action="redo"]').click();
          assert.equal(saved(), finished);
        }
      }
    },
  );

  await t.test('summary Escape keeps the existing finish-open behavior', async () => {
    await start(2, true);
    assert.equal(key(projectSummary(), 'Escape').defaultPrevented, true);
    assert.equal(state().draft, undefined);
    assert.equal(state().drag, undefined);
    assert.equal(state().past, 1);
    assert.equal(win.nect.document.layers.at(-1).nodes[0].points.length, 2);
  });
});
