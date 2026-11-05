import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize, layer, node, connect } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';

test('real main.js routes the experimental editor through closed Apply, not a layer edit', async (t) => {
  const keys = [
    'window',
    'document',
    'HTMLElement',
    'MutationObserver',
    'requestAnimationFrame',
    'DOMPoint',
    'fetch',
  ];
  const original = new Map(
    keys.map((key) => [key, Object.getOwnPropertyDescriptor(globalThis, key)]),
  );
  t.after(() => {
    for (const [key, descriptor] of original)
      if (descriptor) Object.defineProperty(globalThis, key, descriptor);
      else delete globalThis[key];
  });
  const main = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  async function boot(source, label) {
    const win = new Window({ url: 'http://localhost:5173' });
    win.document.body.innerHTML = '<div id="app"></div>';
    for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
      globalThis[key] = win[key];
    globalThis.requestAnimationFrame = () => 0;
    const frames = new Map();
    if (label === 'artwork-preview') {
      let nextFrame = 1;
      globalThis.requestAnimationFrame = win.requestAnimationFrame = (cb) => {
        const id = nextFrame++;
        frames.set(id, cb);
        return id;
      };
      win.cancelAnimationFrame = (id) => frames.delete(id);
    }
    globalThis.DOMPoint = class {
      constructor(x, y) {
        this.x = x;
        this.y = y;
      }
      matrixTransform(m) {
        return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
      }
    };
    win.SVGElement.prototype.getScreenCTM = () => ({
      inverse: () => ({ a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }),
    });
    globalThis.fetch = async () => {
      throw Error('This color-only harness does not read font bytes');
    };
    // Disable only unrelated startup font loading. App, panel, Session and
    // commandChange stay production code; no font bytes or user files are read.
    const start = source.indexOf("fetch('/fonts/DejaVuSans.ttf')");
    const end = source.indexOf('// Refresh actual disabled state', start);
    assert.ok(start >= 0 && end > start);
    source = source.slice(0, start) + source.slice(end);
    // Observe the actual main-injected callback while still calling the real coverage view.
    const coverageCallback = 'getCoverageSnapshot: () => coverageView.snapshot(),';
    assert.equal(source.split(coverageCallback).length - 1, 2);
    source = source.replace(
      coverageCallback,
      'getCoverageSnapshot: () => { window.coverageReadCount = (window.coverageReadCount || 0) + 1; return coverageView.snapshot(); },',
    );
    source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
    source +=
      '\nwindow.keyColorAppTest = () => ({ past: history.past.length, future: history.future.length, message, error, renderNow: () => render(), queueCanvas: () => updateCanvas(), initializeFixture: (payload, envelope) => commandChange("initialize-key-color-recipe", payload, undefined, false, envelope) });';
    source += `\n//# sourceURL=main-key-color-${label}.js`;
    await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    const query = (selector) => win.document.querySelector(selector);
    const fire = (el, type, options = {}) =>
      el.dispatchEvent(
        new (type === 'pointerdown'
          ? win.PointerEvent
          : type === 'keydown'
            ? win.KeyboardEvent
            : win.Event)(type, { bubbles: true, cancelable: true, ...options }),
      );
    const click = (selector) => {
      assert.ok(query(selector), `Missing ${selector}`);
      query(selector).click();
    };
    const input = (kind, value) => {
      const el = query(`[data-key-color-input="${kind}"]`);
      el.value = String(value);
      fire(el, 'input');
    };
    return {
      win,
      query,
      fire,
      click,
      input,
      ui: () => win.keyColorAppTest(),
      snapshot: () => win.nect.projectSnapshot(),
      flushFrames: () => {
        const pending = [...frames.values()];
        frames.clear();
        pending.forEach((cb) => cb());
      },
      load: (doc = retainedRecipeFixture()) => win.nect.load(serialize(doc)),
      open: () => {
        openDOMPanel(win.document, 'colors');
        click('[data-key-color-recipe-open]');
      },
    };
  }

  await t.test(
    'trial B artwork preview draws only explicit bound proposal and restores authored view',
    async () => {
      const s = await boot(main, 'artwork-preview');
      try {
        const fixture = retainedRecipeFixture();
        delete fixture.keyColorRecipe;
        const unbound = layer('Unbound fixture');
        const rectangle = node('Rectangle'),
          plainFill = node('Fill');
        const out = node('Output');
        plainFill.params.color.value = { hex: '#e01020', alpha: 1 };
        unbound.nodes = [rectangle, plainFill, out];
        unbound.output = out.id;
        connect(unbound, rectangle.id, plainFill.id, 'path');
        connect(unbound, plainFill.id, out.id, 'path');
        fixture.layers.push(unbound);
        s.load(fixture);
        s.open();
        s.query('[data-research-hex]').value = '#3273c7';
        s.fire(s.query('[data-research-hex]'), 'input');
        s.query('[data-research-alpha]').value = '1';
        s.fire(s.query('[data-research-alpha]'), 'input');
        s.query('[data-research-plan]').value = 'research-neutral-start';
        s.fire(s.query('[data-research-plan]'), 'change');
        s.click('[data-research-preview]');
        s.click('[data-research-create]');
        const project = s.snapshot();
        const id = project.keyColorRecipe.committedSnapshot.intent.slots[0].id;
        const fill = project.compositions[0].layers[0].nodes.find((n) => n.type === 'Fill');
        fill.params.color.colorSlotId = id;
        const boundLayerId = project.compositions[0].layers[0].id;
        s.win.nect.load(serializeCompositionProject(project));
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-document-settings',
          payload: { key: 'width', value: 321 },
        });
        s.win.nect.dispatch({ ...s.win.nect.commandState, type: 'undo', payload: {} });
        const before = s.snapshot(),
          state = { ...s.win.nect.commandState },
          past = s.ui().past,
          future = s.ui().future;
        const svg = () => s.query('.artboard').innerHTML;
        const fillOf = (layerId) =>
          s.query(`.artboard path[data-layer="${layerId}"]`)?.getAttribute('fill');
        const saved = svg();
        const boundSaved = fillOf(boundLayerId),
          unboundSaved = fillOf(unbound.id);
        assert.ok(boundSaved && unboundSaved);
        s.open();
        const delta = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`);
        delta.value = '0.2';
        s.fire(delta, 'input');
        s.click('[data-key-color-preview]');
        assert.equal(svg(), saved, 'preview is off by default');
        s.click('[data-artwork-show]');
        const projected = svg();
        assert.notEqual(projected, saved, 'bound SVG changes when explicitly shown');
        assert.notEqual(fillOf(boundLayerId), boundSaved);
        assert.equal(fillOf(unbound.id), unboundSaved, 'unbound literal is unchanged');
        assert.equal(s.query('[data-artwork-show]').getAttribute('aria-pressed'), 'true');
        s.ui().renderNow();
        assert.equal(svg(), projected, 'full render keeps the projection');
        s.ui().queueCanvas();
        s.flushFrames();
        assert.equal(svg(), projected, 'queued redraw keeps the projection');
        assert.deepEqual(s.snapshot(), before);
        assert.deepEqual(s.win.nect.commandState, state);
        assert.equal(s.ui().past, past);
        assert.equal(s.ui().future, future);
        s.click('[data-artwork-restore]');
        assert.equal(svg(), saved);
        assert.equal(s.query('[data-artwork-show]').getAttribute('aria-pressed'), 'false');
        s.click('[data-artwork-show]');
        assert.equal(svg(), projected);
        delta.value = '0.4'; // A silent mutation is detected on the next draw.
        s.ui().queueCanvas();
        s.flushFrames();
        assert.equal(svg(), saved);
        assert.equal(s.query('[data-artwork-show]').getAttribute('aria-pressed'), 'false');
        delta.value = '0.2';
        s.ui().queueCanvas();
        s.flushFrames();
        assert.equal(svg(), saved, 'a silently restored input does not re-enable projection');
        s.click('[data-artwork-show]');
        assert.equal(svg(), projected);
        delta.value = '';
        s.fire(delta, 'input');
        assert.equal(svg(), saved, 'invalidating input restores the authored SVG');
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().future, future);
        s.open();
        const cancelDelta = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`);
        cancelDelta.value = '0.2';
        s.fire(cancelDelta, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-artwork-show]');
        assert.equal(svg(), projected);
        s.click('[data-key-color-cancel]');
        assert.equal(svg(), saved, 'cancel restores the authored SVG while projection is visible');
        assert.equal(s.ui().future, future);
        s.click('[data-action="redo"]');
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        s.open();
        const changed = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`);
        changed.value = '0.2';
        s.fire(changed, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-artwork-show]');
        assert.notEqual(fillOf(boundLayerId), boundSaved);
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-color-slot',
          payload: { slotId: id, value: { hex: '#123abc', alpha: 1 } },
        });
        const external = s.snapshot();
        assert.equal(fillOf(boundLayerId).toLowerCase(), '#123abc');
        assert.equal(fillOf(unbound.id), unboundSaved);
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /stale/i);
        s.click('[data-artwork-show]');
        assert.equal(fillOf(boundLayerId).toLowerCase(), '#123abc');
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), external);

        s.open();
        const again = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`);
        again.value = '0.1';
        s.fire(again, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-artwork-show]');
        assert.notEqual(fillOf(boundLayerId).toLowerCase(), '#123abc');
        s.win.nect.load(serializeCompositionProject(external));
        assert.equal(s.query('dialog[data-key-color-recipe-panel]'), null);
        assert.equal(
          fillOf(boundLayerId).toLowerCase(),
          '#123abc',
          'replacement session draws its authored state',
        );
        s.open();
        const appliedDelta = s.query(
          `[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`,
        );
        appliedDelta.value = '0.2';
        s.fire(appliedDelta, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-artwork-show]');
        const toApply = fillOf(boundLayerId);
        assert.notEqual(toApply.toLowerCase(), '#123abc');
        s.click('[data-key-color-apply]');
        const applied = s.snapshot();
        assert.equal(fillOf(boundLayerId), toApply);
        assert.equal(s.ui().past, 1);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), external);
        assert.equal(fillOf(boundLayerId).toLowerCase(), '#123abc');
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), applied);
        assert.equal(fillOf(boundLayerId), toApply);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test('B explicit pair compares current and valid preview without authoring', async () => {
    const s = await boot(main, 'pair-advisory');
    try {
      s.open();
      const hex = s.query('[data-research-hex]'),
        alpha = s.query('[data-research-alpha]'),
        choice = s.query('[data-research-plan]');
      hex.value = '#3273c7';
      s.fire(hex, 'input');
      alpha.value = '1';
      s.fire(alpha, 'input');
      choice.value = 'research-neutral-start';
      s.fire(choice, 'change');
      s.click('[data-research-preview]');
      s.click('[data-research-create]');
      const created = s.snapshot(),
        ids = created.keyColorRecipe.committedSnapshot.intent.slots.map((slot) => slot.id);
      s.win.nect.dispatch({
        ...s.win.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'width', value: 1001 },
      });
      s.win.nect.dispatch({ ...s.win.nect.commandState, type: 'undo', payload: {} });
      const before = s.snapshot(),
        state = { ...s.win.nect.commandState },
        past = s.ui().past,
        future = s.ui().future;
      assert.equal(future, 1);
      s.open();
      const fg = s.query('[data-pair-foreground]'),
        bg = s.query('[data-pair-background]');
      const result = () => s.query('[data-pair-result]').textContent;
      s.click('[data-pair-run]');
      assert.match(result(), /Choose both colors/);
      fg.value = ids[0];
      s.fire(fg, 'change');
      bg.value = ids[0];
      s.fire(bg, 'change');
      s.click('[data-pair-run]');
      assert.match(result(), /Choose two different colors/);
      bg.value = ids[3];
      s.fire(bg, 'change');
      assert.match(result(), /Not evaluated/);
      s.click('[data-pair-run]');
      assert.match(result(), /Current shared colors: [\d.]+:1/);
      assert.match(result(), /Proposed colors: not evaluated/);
      const delta = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${ids[0]}"]`);
      delta.value = '0.2';
      s.fire(delta, 'input');
      assert.match(result(), /Not evaluated/);
      s.click('[data-key-color-preview]');
      s.click('[data-pair-run]');
      const both = result().match(/(?:Current shared colors|Proposed colors): ([\d.]+):1/g);
      assert.equal(both.length, 2);
      assert.notEqual(both[0].split(': ')[1], both[1].split(': ')[1]);
      fg.value = ids[1];
      s.fire(fg, 'change');
      assert.match(result(), /Not evaluated/);
      assert.equal(s.query('[data-key-color-apply]').disabled, false);
      s.click('[data-pair-run]');
      assert.match(result(), /Proposed colors: [\d.]+:1/);
      delta.value = '0.3'; // Silent mutation must not reuse the old Preview.
      s.click('[data-pair-run]');
      assert.match(result(), /Current shared colors:/);
      assert.match(result(), /Proposed colors: not evaluated/);
      assert.equal(s.query('[data-key-color-apply]').disabled, true);
      assert.match(s.query('[data-key-color-status]').textContent, /Draft only/i);
      delta.value = '0.2';
      s.fire(delta, 'input');
      s.click('[data-key-color-preview]');
      delta.value = ''; // Invalid silent mutation must also replace the old status.
      s.click('[data-pair-run]');
      assert.match(result(), /Current shared colors:/);
      assert.match(result(), /Proposed colors: not evaluated/);
      assert.equal(s.query('[data-key-color-preview]').disabled, true);
      assert.equal(s.query('[data-key-color-apply]').disabled, true);
      assert.match(s.query('[data-key-color-status]').textContent, /finite value/i);
      assert.deepEqual(s.snapshot(), before);
      assert.deepEqual(s.win.nect.commandState, state);
      assert.equal(s.ui().past, past);
      assert.equal(s.ui().future, future);
      s.click('[data-key-color-cancel]');
      assert.deepEqual(s.snapshot(), before);
      assert.equal(s.ui().future, future);
      s.click('[data-action="redo"]');
      s.click('[data-action="undo"]');
      assert.deepEqual(s.snapshot(), before);
      s.open();
      const fg2 = s.query('[data-pair-foreground]'),
        bg2 = s.query('[data-pair-background]');
      fg2.value = ids[0];
      s.fire(fg2, 'change');
      bg2.value = ids[3];
      s.fire(bg2, 'change');
      s.click('[data-pair-run]');
      assert.match(result(), /Current shared colors:/);
      s.win.nect.dispatch({
        ...s.win.nect.commandState,
        type: 'set-document-settings',
        payload: { key: 'width', value: 1002 },
      });
      assert.match(result(), /Not evaluated/);
      // Exercise the stale check's status update independently of the prior render.
      s.query('[data-key-color-status]').textContent = 'Old preview ready';
      s.click('[data-pair-run]');
      assert.match(result(), /stale/i);
      assert.match(s.query('[data-key-color-status]').textContent, /stale/i);
      assert.doesNotMatch(result(), /at least|below/);
    } finally {
      await s.win.happyDOM.abort();
    }
  });

  await t.test(
    'explicit B preview creates five fresh trial colors in one Undo and reopens without derived chroma controls',
    async () => {
      const s = await boot(main, 'trial-create');
      try {
        const existing = retainedRecipeFixture();
        delete existing.keyColorRecipe;
        s.load(existing);
        const before = s.snapshot();
        s.open();
        const hex = s.query('[data-research-hex]'),
          alpha = s.query('[data-research-alpha]'),
          choice = s.query('[data-research-plan]'),
          create = s.query('[data-research-create]');
        assert.equal(choice.value, '');
        assert.equal(create.disabled, true);
        hex.value = '#aBcD09';
        s.fire(hex, 'input');
        alpha.value = '0';
        s.fire(alpha, 'input');
        choice.value = 'research-triadic-offset';
        s.fire(choice, 'change');
        s.click('[data-research-preview]');
        assert.equal(create.disabled, true);
        assert.deepEqual(s.snapshot(), before);
        choice.value = 'research-neutral-start';
        s.fire(choice, 'change');
        s.click('[data-research-preview]');
        assert.equal(create.disabled, false);
        assert.match(
          s.query('[data-research-details]').textContent,
          /experimental-affine-neutral-text-v1/,
        );
        const ids = Array.from(
          s.win.document.querySelectorAll('[data-research-row]'),
          (r) => r.dataset.researchRow,
        );
        assert.equal(new Set(ids).size, 5);
        assert.ok(ids.every((id) => !before.colorSlots.some((slot) => slot.id === id)));
        assert.deepEqual(s.snapshot(), before);
        alpha.value = '0.5';
        s.fire(alpha, 'input');
        assert.equal(create.disabled, true);
        alpha.value = '0';
        s.fire(alpha, 'input');
        s.click('[data-research-preview]');
        assert.deepEqual(
          Array.from(
            s.win.document.querySelectorAll('[data-research-row]'),
            (r) => r.dataset.researchRow,
          ),
          ids,
        );
        const previewHex = Array.from(
          s.win.document.querySelectorAll('[data-research-row]'),
          (row) => row.querySelector('code').textContent,
        );
        create.click();
        assert.equal(s.ui().error, false);
        assert.equal(s.ui().past, 1);
        const after = s.snapshot(),
          receipt = after.keyColorRecipe.committedSnapshot;
        assert.deepEqual(after.colorSlots.slice(0, before.colorSlots.length), before.colorSlots);
        assert.deepEqual(
          after.colorSlots.slice(-5).map((slot) => slot.id),
          ids,
        );
        assert.deepEqual(
          after.colorSlots.slice(-5).map((slot) => slot.value.hex),
          previewHex,
        );
        assert.deepEqual(after.compositions, before.compositions);
        assert.deepEqual(after.colorDesign, before.colorDesign);
        assert.deepEqual(after.colorSlots.find((slot) => slot.id === ids[0]).value, {
          hex: '#aBcD09',
          alpha: 0,
        });
        assert.equal(receipt.solverVersion, 'experimental-affine-neutral-text-v1');
        assert.equal(receipt.intent.preset.id, 'research-neutral-start');
        assert.ok(receipt.resolved.slice(1).every((row) => row.C === 0 && row.value.alpha === 1));
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), after);
        s.win.nect.load(serializeCompositionProject(after));
        assert.deepEqual(s.snapshot(), after);
        s.open();
        assert.ok(s.win.coverageReadCount > 0);
        assert.match(s.query('[data-trial-usage]').textContent, /unavailable/i);
        assert.match(
          s.query('[data-trial-status="Saved recipe"] [data-model-l-order]').textContent,
          /Derived model L order/,
        );
        for (const id of ids.slice(1)) {
          assert.ok(s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`));
          const chroma = s.query(
            `[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${id}"]`,
          );
          if (ids.slice(1, 3).includes(id)) assert.ok(chroma);
          else assert.equal(chroma, null);
        }
        const chroma = s.query(
          `[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${ids[1]}"]`,
        );
        chroma.value = '0.25';
        s.fire(chroma, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        const colored = s.snapshot();
        assert.equal(colored.keyColorRecipe.committedSnapshot.intent.slots[0].deltaRatioCmax, 0.25);
        assert.ok(colored.keyColorRecipe.committedSnapshot.resolved[1].C > 0);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), after);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), colored);
        // Reopen the real project format; load starts a new history.
        s.win.nect.load(serializeCompositionProject(colored));
        assert.deepEqual(s.snapshot(), colored);
        assert.equal(
          s.snapshot().keyColorRecipe.committedSnapshot.intent.slots[0].deltaRatioCmax,
          0.25,
        );
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().future, 0);
        s.open();
        const pendingChroma = s.query(
          `[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${ids[1]}"]`,
        );
        const c1 = s.query('[data-key-color-input="hex"]');
        assert.equal(Number(pendingChroma.value), 0.25);
        pendingChroma.value = '0.35';
        s.fire(pendingChroma, 'input');
        c1.value = '#808080';
        s.fire(c1, 'input');
        assert.equal(pendingChroma.value, '0.35');
        assert.equal(s.query('[data-key-color-preview]').disabled, true);
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /gray.*no defined hue/i);
        assert.deepEqual(s.snapshot(), colored);
        pendingChroma.value = '-0.1';
        s.fire(pendingChroma, 'input');
        assert.equal(s.query('[data-key-color-preview]').disabled, true);
        assert.equal(pendingChroma.value, '-0.1');
        pendingChroma.value = '0.35';
        s.fire(pendingChroma, 'input');
        c1.value = '#808081';
        s.fire(c1, 'input');
        assert.equal(s.query('[data-key-color-preview]').disabled, false);
        assert.match(s.query('dialog').textContent, /Near-gray hue is not evaluated/);
        c1.value = '#aBcD09';
        s.fire(c1, 'input');
        s.click('[data-key-color-preview]');
        assert.equal(s.query('[data-key-color-apply]').disabled, false);
        c1.value = '#808080'; // Silent DOM change cannot bypass Apply validation.
        s.click('[data-key-color-apply]');
        assert.deepEqual(s.snapshot(), colored);
        assert.equal(pendingChroma.value, '0.35');
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        pendingChroma.value = '0';
        s.fire(pendingChroma, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        const gray = s.snapshot();
        assert.equal(gray.keyColorRecipe.committedSnapshot.intent.keyColor.value.hex, '#808080');
        assert.ok(
          gray.keyColorRecipe.committedSnapshot.resolved.slice(1).every((row) => row.C === 0),
        );
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), colored);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), gray);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), colored);
        s.open();
        const delta = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${ids[1]}"]`);
        delta.value = '0.25';
        s.fire(delta, 'input');
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        const edited = s.snapshot();
        assert.equal(edited.keyColorRecipe.committedSnapshot.intent.slots[0].deltaL, 0.25);
        s.open();
        const input = s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${ids[1]}"]`);
        const reset = s.query(`[data-key-color-reset-delta-l="${ids[1]}"]`);
        const stateBeforeReset = { ...s.win.nect.commandState },
          pastBeforeReset = s.ui().past;
        s.click('[data-key-color-preview]');
        assert.equal(s.query('[data-key-color-apply]').disabled, false);
        s.fire(input, 'compositionstart');
        reset.click();
        assert.equal(input.value, '0.25');
        assert.match(s.query('[data-key-color-status]').textContent, /Finish the active input/);
        s.fire(input, 'compositionend');
        const pointer = (target, type, id) =>
          target.dispatchEvent(
            new s.win.PointerEvent(type, {
              bubbles: true,
              cancelable: true,
              pointerId: id,
              button: 0,
              clientX: 100,
              isPrimary: true,
            }),
          );
        pointer(input, 'pointerdown', 17);
        reset.click();
        assert.equal(input.value, '0.25');
        assert.match(s.query('[data-key-color-status]').textContent, /Finish the active input/);
        pointer(s.win.document, 'pointercancel', 17);
        const alphaInput = s.query('[data-key-color-input="alpha"]');
        pointer(alphaInput, 'pointerdown', 18);
        reset.click();
        assert.equal(input.value, '0.25');
        assert.match(s.query('[data-key-color-status]').textContent, /Finish the active input/);
        pointer(s.win.document, 'pointercancel', 18);
        assert.deepEqual(s.snapshot(), edited);
        assert.deepEqual(s.win.nect.commandState, stateBeforeReset);
        assert.equal(s.ui().past, pastBeforeReset);
        s.click('[data-key-color-preview]');
        reset.click();
        assert.equal(input.value, '0');
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.deepEqual(s.snapshot(), edited);
        assert.deepEqual(s.win.nect.commandState, stateBeforeReset);
        assert.equal(s.ui().past, pastBeforeReset);
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        const resetResult = s.snapshot();
        assert.doesNotThrow(() => reset.click());
        assert.deepEqual(s.snapshot(), resetResult);
        assert.equal(resetResult.keyColorRecipe.committedSnapshot.intent.slots[0].deltaL, 0);
        assert.deepEqual(
          resetResult.colorSlots.filter((slot) => slot.id !== ids[1]),
          edited.colorSlots.filter((slot) => slot.id !== ids[1]),
        );
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), edited);
        const undoState = { ...s.win.nect.commandState },
          undoFuture = s.ui().future;
        assert.equal(undoFuture, 1);
        s.open();
        s.click(`[data-key-color-reset-delta-l="${ids[1]}"]`);
        assert.equal(
          s.query(`[data-key-color-input="deltaL"][data-key-color-slot="${ids[1]}"]`).value,
          '0',
        );
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), edited);
        assert.deepEqual(s.win.nect.commandState, undoState);
        assert.equal(s.ui().future, undoFuture);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), resetResult);
        s.open();
        s.click('[data-key-color-preview]');
        const staleReset = s.query(`[data-key-color-reset-delta-l="${ids[1]}"]`);
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-document-settings',
          payload: { key: 'width', value: 1002 },
        });
        const external = s.snapshot(),
          staleState = { ...s.win.nect.commandState };
        staleReset.click();
        assert.match(s.query('[data-key-color-status]').textContent, /stale/i);
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        const staleChroma = s.query(
          `[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${ids[1]}"]`,
        );
        staleChroma.value = '0.6';
        s.fire(staleChroma, 'input');
        assert.equal(s.query('[data-key-color-preview]').disabled, true);
        assert.deepEqual(s.snapshot(), external);
        assert.deepEqual(s.win.nect.commandState, staleState);
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), external);
        // Fixture preparation uses the real snapshot API; the gray edit below is through the UI.
        s.win.nect.load(serializeCompositionProject(after));
        let dormant = editKeyColorRecipe(after.keyColorRecipe.committedSnapshot, {
          type: 'slot-delta',
          expectedRevision: after.keyColorRecipe.committedSnapshot.revision,
          slotId: ids[2],
          deltaRatioCmax: 0.2,
        });
        dormant = editKeyColorRecipe(dormant, {
          type: 'preset',
          expectedRevision: dormant.revision,
          preset: {
            ...dormant.intent.preset,
            slots: dormant.intent.preset.slots.filter((slot) => slot.id !== ids[2]),
          },
        });
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'apply-key-color-recipe-snapshot',
          payload: { snapshot: dormant },
        });
        const dormantBefore = s.snapshot();
        s.open();
        assert.ok(s.query(`[data-key-color-dormant="${ids[2]}"]`));
        assert.equal(
          s.query(`[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${ids[2]}"]`),
          null,
        );
        s.input('hex', '#808080');
        assert.equal(s.query('[data-key-color-preview]').disabled, false);
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        const dormantAfter = s.snapshot().keyColorRecipe.committedSnapshot;
        assert.equal(
          dormantAfter.intent.slots.find((slot) => slot.id === ids[2]).deltaRatioCmax,
          0.2,
        );
        assert.deepEqual(
          dormantAfter.resolved.find((row) => row.id === ids[2]),
          dormant.resolved.find((row) => row.id === ids[2]),
        );
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), dormantBefore);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'no receipt research preview is explicit, detached, invalidated and stale-safe',
    async () => {
      const s = await boot(main, 'research-preview');
      try {
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-document-settings',
          payload: { key: 'width', value: 1001 },
        });
        s.win.nect.dispatch({ ...s.win.nect.commandState, type: 'undo', payload: {} });
        const before = s.snapshot(),
          state = { ...s.win.nect.commandState },
          past = s.ui().past,
          future = s.ui().future;
        assert.equal(future, 1);
        s.open();
        const hex = s.query('[data-research-hex]'),
          alpha = s.query('[data-research-alpha]'),
          choice = s.query('[data-research-plan]'),
          button = s.query('[data-research-preview]');
        assert.equal(hex.value, '');
        assert.equal(alpha.value, '');
        assert.equal(choice.value, '');
        assert.equal(button.disabled, true);
        assert.equal(s.query('[data-key-color-input="hsl-h"]').disabled, true);
        assert.equal(
          s.query('canvas[aria-label^="Saturation"]').getAttribute('aria-disabled'),
          'true',
        );
        assert.equal(s.query('[data-key-color-apply]'), null);
        hex.value = '#aAaAaA';
        s.fire(hex, 'input');
        assert.equal(s.query('[data-key-color-input="hsl-h"]').disabled, false);
        const hue = s.query('[data-key-color-input="hsl-h"]');
        hue.value = '240';
        s.fire(hue, 'input');
        assert.equal(hex.value, '#aAaAaA');
        alpha.value = '0';
        s.fire(alpha, 'input');
        choice.value = 'research-neutral-start';
        s.fire(choice, 'change');
        const composedSaturation = s.query('[data-key-color-input="hsl-s"]');
        s.fire(composedSaturation, 'compositionstart');
        composedSaturation.value = '100';
        s.fire(composedSaturation, 'input');
        assert.equal(button.disabled, true);
        assert.equal(hex.value, '#aAaAaA');
        s.fire(composedSaturation, 'compositionend');
        // No change event: the ended composition itself must synchronize C1.
        assert.equal(composedSaturation.value, '100');
        assert.equal(hex.value, '#5555ff');
        assert.equal(button.disabled, false);
        button.click();
        assert.equal(s.query('[data-research-role="Identity"] code').textContent, '#5555ff');
        s.fire(alpha, 'compositionstart');
        s.fire(composedSaturation, 'compositionstart');
        composedSaturation.value = '40';
        s.fire(composedSaturation, 'input');
        s.fire(composedSaturation, 'compositionend');
        assert.equal(hex.value, '#5555ff');
        assert.equal(button.disabled, true);
        s.fire(alpha, 'compositionend');
        assert.notEqual(hex.value, '#5555ff');
        assert.equal(button.disabled, false);
        hex.value = '#AbCd09';
        s.fire(hex, 'input');
        alpha.value = '0';
        s.fire(alpha, 'input');
        choice.value = 'research-neutral-start';
        s.fire(choice, 'change');
        assert.equal(button.disabled, false);
        assert.match(s.query('[data-research-details]').textContent, /research-neutral-start/);
        button.click();
        const rows = [...s.win.document.querySelectorAll('[data-research-row]')];
        assert.equal(rows.length, 5);
        assert.match(rows[0].textContent, /#AbCd09/);
        assert.match(rows[0].textContent, /α\s*0/);
        assert.deepEqual(s.snapshot(), before);
        assert.deepEqual(s.win.nect.commandState, state);
        assert.equal(s.ui().past, past);
        assert.equal(s.ui().future, future);
        for (const row of rows.slice(1)) {
          const match = row.querySelector('code').textContent.match(/^#(..)(..)(..)$/);
          assert.ok(match && match[1] === match[2] && match[2] === match[3]);
        }
        s.click('[data-action="new"]');
        assert.match(s.ui().message, /Finish or cancel the active edit/);
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().future, future);
        assert.ok(s.query('dialog'));
        const saturation = s.query('[data-key-color-input="hsl-s"]');
        saturation.value = '50';
        s.fire(saturation, 'input');
        assert.equal(s.query('[data-research-row]'), null);
        assert.notEqual(hex.value, '#AbCd09');
        hex.value = '#AbCd09';
        s.fire(hex, 'input');
        choice.value = 'research-triadic-offset';
        s.fire(choice, 'change');
        assert.equal(s.query('[data-research-row]'), null);
        assert.deepEqual(s.snapshot(), before);
        assert.deepEqual(s.win.nect.commandState, state);
        assert.equal(s.ui().past, past);
        assert.equal(s.ui().future, future);
        s.fire(hex, 'compositionstart');
        assert.equal(button.disabled, true);
        s.fire(hex, 'compositionend');
        assert.equal(button.disabled, false);
        hex.value = '#bad';
        s.fire(hex, 'input');
        assert.equal(button.disabled, true);
        assert.equal(s.query('[data-key-color-input="hsl-h"]').disabled, true);
        assert.equal(s.query('[data-key-color-hue]').hidden, true);
        hex.value = '#AbCd09';
        s.fire(hex, 'input');
        assert.equal(s.query('[data-key-color-input="hsl-h"]').disabled, false);
        button.click();
        assert.ok(s.query('[data-research-row]'));
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-document-settings',
          payload: { key: 'width', value: 1002 },
        });
        assert.equal(s.query('[data-research-row]'), null);
        assert.equal(button.disabled, true);
        assert.equal(s.query('[data-research-create]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /stale/i);
        const afterExternal = s.snapshot();
        s.fire(s.query('[data-research-create]'), 'click');
        assert.deepEqual(s.snapshot(), afterExternal);
        assert.match(s.ui().message, /stale/i);
        const staleHex = hex.value;
        s.fire(composedSaturation, 'compositionstart');
        composedSaturation.value = '30';
        s.fire(composedSaturation, 'input');
        s.fire(composedSaturation, 'compositionend');
        assert.equal(hex.value, staleHex);
        assert.equal(button.disabled, true);
        s.click('[data-key-color-cancel]');
        assert.equal(s.query('dialog'), null);
        s.open();
        assert.equal(s.query('[data-research-hex]').value, '');
        s.fire(s.query('[data-research-hex]'), 'keydown', { key: 'Escape' });
        assert.equal(s.query('dialog'), null);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'closed initialization through real main preserves atomic history and rejects stale or duplicate requests',
    async () => {
      const s = await boot(main, 'initialize-fixture');
      try {
        // Fixed developer-only input: not an approved plan, preset or user entry.
        const payload = {
          version: 1,
          modelVersion: 2,
          solverVersion: 'experimental-affine-v1',
          colorSemanticsVersion: 'linear-srgb-straight-v1',
          paletteId: 'fixture-only-palette',
          intent: {
            keyColor: {
              id: 'fresh-key',
              role: 'Identity',
              value: { hex: '#aBcD09', alpha: 0 },
              hueHint: 17,
            },
            preset: {
              id: 'explicit-fixture-only',
              version: 1,
              harmony: 'triadic',
              hueSteps: 12,
              slots: [
                {
                  id: 'fresh-derived',
                  role: 'Fixture background',
                  position: 1,
                  lightnessScale: 0,
                  lightnessOffset: 0.83,
                  chromaScale: 0,
                  chromaOffset: 0,
                  alpha: 1,
                },
              ],
            },
            slots: [
              { id: 'fresh-derived', role: 'Fixture background', deltaL: 0.03, deltaRatioCmax: 0 },
            ],
            settings: { areaAware: false },
          },
          allocations: [
            { slotId: 'fresh-derived', name: 'Exact background name' },
            { slotId: 'fresh-key', name: 'Exact C1 name' },
          ],
        };
        const existing = retainedRecipeFixture();
        delete existing.keyColorRecipe;
        s.load(existing);
        const before = s.snapshot();
        const captured = { ...s.win.nect.commandState };
        assert.notEqual(s.ui().initializeFixture(payload, captured), null);
        const after = s.snapshot();
        assert.equal(s.ui().error, false);
        assert.equal(s.ui().past, 1);
        assert.equal(after.colorSlots.length, before.colorSlots.length + 2);
        assert.deepEqual(
          after.colorSlots.slice(-2).map((slot) => slot.id),
          ['fresh-derived', 'fresh-key'],
        );
        assert.deepEqual(after.colorSlots.find((slot) => slot.id === 'fresh-key').value, {
          hex: '#aBcD09',
          alpha: 0,
        });
        assert.ok(after.keyColorRecipe.committedSnapshot);
        const duplicate = structuredClone(payload);
        duplicate.paletteId = 'second-fixture-palette';
        duplicate.intent.keyColor.id = 'second-key';
        duplicate.intent.slots[0].id = 'second-derived';
        duplicate.intent.preset.slots[0].id = 'second-derived';
        duplicate.allocations[0].slotId = 'second-derived';
        duplicate.allocations[1].slotId = 'second-key';
        const current = { ...s.win.nect.commandState };
        assert.equal(s.ui().initializeFixture(duplicate, current), null);
        assert.match(s.ui().message, /Retained key-color recipe exists/);
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 1);
        assert.equal(s.ui().future, 0);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().future, 1);
        assert.equal(s.ui().initializeFixture(payload, captured), null);
        assert.match(s.ui().message, /STALE_SESSION/);
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().future, 1);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 1);
        assert.equal(s.ui().future, 0);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'empty state and real Apply/Undo/Redo/native reopen preserve legacy data/IDs/refs',
    async () => {
      const s = await boot(main, 'closed-apply');
      try {
        const blank = s.snapshot();
        s.open();
        assert.ok(s.query('[data-key-color-empty]'));
        assert.deepEqual(s.snapshot(), blank);
        assert.equal(s.ui().past, 0);
        s.click('[data-key-color-cancel]');
        s.load();
        const before = s.snapshot();
        s.open();
        assert.equal(s.query('[data-key-color-reset-delta-l]'), null);
        s.input('hex', '#DeAd09');
        s.input('alpha', 0);
        s.input('deltaL', -0.125);
        s.input('deltaRatioCmax', 0.375);
        s.click('[data-key-color-preview]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, 0);
        s.click('[data-key-color-apply]');
        const after = s.snapshot();
        assert.equal(s.query('dialog'), null);
        assert.equal(s.ui().error, false);
        assert.match(s.ui().message, /Apply accepted/);
        assert.equal(s.ui().past, 1);
        assert.equal(s.ui().future, 0);
        assert.deepEqual(after.colorSlots.find((slot) => slot.id === 'key').value, {
          hex: '#DeAd09',
          alpha: 0,
        });
        assert.deepEqual(after.colorDesign, before.colorDesign);
        assert.deepEqual(after.compositions, before.compositions);
        assert.deepEqual(after.colorSlots.slice(2), before.colorSlots.slice(2));
        assert.deepEqual(after.keyColorRecipe.committedSnapshot.intent.slots[0], {
          id: 'derived',
          role: 'Background',
          deltaL: -0.125,
          deltaRatioCmax: 0.375,
        });
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), after);
        s.win.nect.load(serializeCompositionProject(after));
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 0);
        s.open();
        assert.equal(s.query('[data-key-color-input="hex"]').value, '#DeAd09');
        s.fire(s.query('[data-key-color-input]'), 'keydown', { key: 'Escape' });
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 0);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'launcher cancels an actual legacy live gesture without blur auto-commit',
    async () => {
      const s = await boot(main, 'legacy-isolation');
      try {
        s.load();
        openDOMPanel(s.win.document, 'colors');
        const mode = s.query('[data-wheel-chroma-mode]');
        mode.value = 'override';
        s.fire(mode, 'change');
        const before = s.snapshot(),
          past = s.ui().past;
        const authoredSvg = s.query('.artboard').innerHTML;
        const input = s.query('[data-wheel-chroma]');
        input.focus();
        input.value = '0.03';
        s.fire(input, 'input');
        await new Promise((resolve) => setTimeout(resolve, 40));
        assert.ok(s.win.nect.colorPreviewSnapshot(), 'legacy preview is genuinely pending');
        assert.notEqual(
          s.query('.artboard').innerHTML,
          authoredSvg,
          'legacy preview reaches the SVG',
        );
        assert.deepEqual(s.snapshot(), before);
        const launcher = s.query('[data-key-color-recipe-open]');
        assert.equal(
          s.fire(launcher, 'pointerdown', { button: 0, pointerId: 19, isPrimary: true }),
          false,
        );
        assert.equal(s.win.nect.colorPreviewSnapshot(), null);
        // An ensuing native change cannot re-author the canceled draft: launcher
        // pointerdown prevents that native blur, then click opens the separate modal.
        s.click('[data-key-color-recipe-open]');
        assert.ok(s.query('dialog'));
        assert.equal(
          s.query('.artboard').innerHTML,
          authoredSvg,
          'opening the dialog redraws the legacy authored SVG',
        );
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), before);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'real external edit and Undo invalidate, never refresh, the open draft envelope',
    async () => {
      const s = await boot(main, 'stale');
      try {
        s.load();
        s.open();
        s.click('[data-key-color-preview]');
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-color-slot',
          payload: { slotId: 'other', value: { hex: '#010203', alpha: 1 } },
        });
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /stale/);
        s.win.nect.dispatch({ ...s.win.nect.commandState, type: 'undo', payload: {} });
        assert.equal(s.query('[data-key-color-preview]').disabled, true);
        const before = s.snapshot(),
          past = s.ui().past,
          future = s.ui().future;
        s.fire(s.query('[data-key-color-apply]'), 'click');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        assert.equal(s.ui().future, future);
        s.click('[data-key-color-cancel]');
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'negative control: old main layerId injection yields null, never a success notice',
    async () => {
      const start = main.indexOf('function commandChange('),
        end = main.indexOf('\nfunction settleNumericControl(', start);
      const commandChange = main.slice(start, end),
        dispatchCall = /result = session\.dispatch\(\{[\s\S]*?\n    \}\);/g;
      assert.equal([...commandChange.matchAll(dispatchCall)].length, 1);
      const broken =
        main.slice(0, start) +
        commandChange.replace(
          dispatchCall,
          'result = session.dispatch({ ...envelope, type, payload: { layerId: current().id, ...payload } });',
        ) +
        main.slice(end);
      const s = await boot(broken, 'old-injection-negative');
      try {
        s.load();
        const before = s.snapshot();
        s.open();
        s.input('hex', '#DeAd09');
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().future, 0);
        assert.ok(s.query('dialog'));
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /not accepted/);
        assert.equal(s.ui().error, true);
        assert.doesNotMatch(s.ui().message, /Apply accepted/);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );
});
