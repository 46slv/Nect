import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize, blankDocument } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { compoundFixture } from './fixtures/contour-navigation.js';
import { displayLengthToDocumentUnits } from '../src/artboard-units.js';
import { guideDefaults } from '../src/guides.js';

test('Document and Guide numeric behavior preserves canonical state and ownership', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
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
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.p2History = () => ({past:history.past.length,future:history.future.length,gesture:session.hasGesture});';
  source += '\nwindow.p2Switch = switchActiveComposition;';
  source +=
    '\nfor(let attempt=0; !bundledFont && attempt<500; attempt++) await new Promise(r=>setTimeout(r,10)); if(!bundledFont) throw Error(document.body.textContent.slice(-1000));';
  t.after(() => win.happyDOM.abort());
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s),
    saved = () => serialize(win.nect.document),
    state = () => win.p2History();
  const fire = (el, type, x = 100) =>
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
  const change = (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const unit = (value) => change($('[data-artboard-unit]'), value);
  const sameSaved = (before) => {
    const after = saved();
    let index = 0;
    while (index < before.length && before[index] === after[index]) index++;
    assert.equal(
      after === before,
      true,
      JSON.stringify({
        index,
        before: before.slice(index - 30, index + 90),
        after: after.slice(index - 30, index + 90),
      }),
    );
  };
  const width = () => $('[data-dimension="width"]');
  const begin = (el, dx = 24) => {
    el.focus();
    fire(el, 'pointerdown');
    const nativeDrag = new win.Event('dragstart', { bubbles: true, cancelable: true });
    el.dispatchEvent(nativeDrag);
    assert.equal(nativeDrag.defaultPrevented, true);
    fire(document, 'pointermove', 100 + dx);
  };
  const end = (dx = 24) => fire(document, 'pointerup', 100 + dx);
  const undo = () => {
    document.activeElement?.blur();
    $('[data-action="undo"]').click();
  };
  const load = async (withGuide = false) => {
    document.activeElement?.blur();
    const f = compoundFixture([8, 5, 7, 4]);
    f.doc.width = 123.45678901234567;
    if (withGuide)
      f.doc.editor = {
        ...guideDefaults(),
        guides: [{ id: 'p2-guide', axis: 'x', position: 100, locked: false }],
      };
    win.nect.load(serialize(f.doc));
    await new Promise((r) => setTimeout(r, 15));
    const layers = $('[data-workspace-panel="layers"]');
    if (layers.getAttribute('aria-expanded') !== 'true') layers.click();
    return f;
  };
  const activate = (action) => {
    const toggle = $('[aria-label="Toggle artboard view controls"]');
    if (toggle.getAttribute('aria-expanded') !== 'true') toggle.click();
    $(`[data-action="${action}"]`).click();
  };
  const guides = () => {
    if (!$('[data-add-guide="x"]')) activate('guides-panel');
  };
  await t.test('px/mm/cm scrub preserves exact baseline and one Undo', async () => {
    for (const name of ['px', 'mm', 'cm']) {
      await load();
      unit(name);
      const before = saved(),
        original = win.nect.document.width,
        h = state();
      const el = width();
      begin(el);
      assert.equal(el.isConnected, true);
      assert.equal(document.activeElement === el, true);
      sameSaved(before);
      end();
      assert.ok(
        Math.abs(win.nect.document.width - (original + displayLengthToDocumentUnits(20, name))) <
          1e-10,
      );
      assert.equal(state().past, h.past + 1);
      undo();
      sameSaved(before);
      assert.equal(state().future, 1);
      for (const mode of ['jitter', 'origin', 'cancel']) {
        const snapshot = state();
        const input = width();
        begin(input, mode === 'jitter' ? 3 : 24);
        if (mode === 'origin') fire(document, 'pointermove', 100);
        if (mode === 'cancel') fire(document, 'pointercancel', 124);
        end(mode === 'jitter' ? 3 : mode === 'origin' ? 0 : 24);
        assert.equal(saved(), before);
        assert.deepEqual(state(), snapshot);
      }
    }
  });
  await t.test(
    'pending typing is its own Undo and unit changes cancel late completion',
    async () => {
      await load();
      unit('mm');
      const before = saved();
      width().value = '100';
      begin(width());
      end();
      assert.equal(state().past, 2);
      undo();
      assert.equal(win.nect.document.width, displayLengthToDocumentUnits(100, 'mm'));
      undo();
      sameSaved(before);
      const h = state();
      begin(width());
      unit('cm');
      end();
      assert.equal(saved(), before);
      assert.deepEqual(state(), h);
    },
  );
  await t.test('guide position and grid fields retain their existing domains', async () => {
    for (const selector of [
      '[data-guide-position]',
      '[data-guide-setting="gridSize"]',
      '[data-guide-setting="gridAngle"]',
    ]) {
      await load(true);
      guides();
      if (!$(selector)) activate('grid-panel');
      const input = $(selector),
        original = Number(input.value),
        before = saved(),
        h = state();
      begin(input);
      assert.equal(saved(), before);
      end();
      assert.equal(state().past, h.past + 1);
      assert.equal(Number($(selector).value), original + 20);
      undo();
      sameSaved(before);
      const history = state();
      if (!$(selector))
        activate(selector.includes('guide-position') ? 'guides-panel' : 'grid-panel');
      begin($(selector));
      fire(document, 'pointercancel', 124);
      end();
      sameSaved(before);
      assert.deepEqual(state(), history);
    }
  });
  await t.test(
    'replaced document, guide and input target cannot receive a late commit',
    async () => {
      await load(true);
      guides();
      begin($('[data-guide-position]'));
      const f = compoundFixture([8, 5, 7, 4]);
      win.nect.load(serialize(f.doc));
      const before = saved();
      end();
      assert.equal(saved(), before);
      assert.equal(state().past, 0);
      const h = state();
      const input = width();
      begin(input);
      input.dataset.dimension = 'height';
      end();
      assert.equal(saved(), before);
      assert.deepEqual(state(), h);
    },
  );
  await t.test('height and untouched rounded display cannot create extra edits', async () => {
    await load();
    unit('mm');
    const before = saved(),
      h = state();
    change(width(), width().value);
    sameSaved(before);
    assert.deepEqual(state(), h);
    const height = $('[data-dimension="height"]'),
      original = win.nect.document.height;
    begin(height);
    end();
    assert.ok(
      Math.abs(win.nect.document.height - original - displayLengthToDocumentUnits(20, 'mm')) <
        1e-10,
    );
    undo();
    sameSaved(before);
  });
  await t.test(
    'pending Guide typing commits separately without replacing its scrub target',
    async () => {
      await load(true);
      guides();
      const before = saved(),
        input = $('[data-guide-position]');
      input.value = '50';
      begin(input);
      assert.equal(input.isConnected, true);
      end();
      assert.equal(state().past, 2);
      assert.equal(win.nect.document.editor.guides[0].position, 70);
      undo();
      assert.equal(win.nect.document.editor.guides[0].position, 50);
      undo();
      sameSaved(before);
    },
  );
  await t.test('locked Guide replacement is retained after late pointerup', async () => {
    const f = await load(true);
    guides();
    begin($('[data-guide-position]'));
    f.doc.editor.guides[0].locked = true;
    win.nect.load(serialize(f.doc));
    const before = saved(),
      h = state();
    end();
    sameSaved(before);
    assert.deepEqual(state(), h);
    guides();
    assert.equal($('[data-guide-position]').disabled, true);
  });
  await t.test(
    'Composition switch retains both documents across cancelled numeric input',
    async () => {
      const a = migrateLegacyDocumentToCompositionProject(blankDocument()),
        b = migrateLegacyDocumentToCompositionProject(blankDocument());
      a.compositions[0].id = 'p2-composition-a';
      b.compositions[0].id = 'p2-composition-b';
      const aid = a.compositions[0].id,
        bid = b.compositions[0].id;
      a.compositions.push(b.compositions[0]);
      document.activeElement?.blur();
      win.nect.load(JSON.stringify(a));
      const before = JSON.stringify(win.nect.projectSnapshot());
      begin(width());
      assert.throws(() => win.p2Switch(bid), /Finish or cancel/);
      assert.equal(win.nect.activeCompositionId, aid);
      fire(document, 'pointercancel', 124);
      assert.equal(win.p2Switch(bid), true);
      end();
      assert.equal(JSON.stringify(win.nect.projectSnapshot()), before);
      assert.equal(state().past, 0);
    },
  );
});
