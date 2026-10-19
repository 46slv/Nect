import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  document as makeDoc,
  node,
  connect,
  serialize,
  evaluate,
  pathBounds,
} from '../src/core.js';
import { appendArtwork, appendBlankComposition } from '../src/artwork-creation.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

test('New and explicit blank compositions are safe empty artwork entry points', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  let pending = [];
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = (fn) => {
    pending.push(fn);
    return pending.length;
  };
  const flush = () => {
    const jobs = pending;
    pending = [];
    jobs.forEach((fn) => fn(0));
  };
  const matrix = {
    a: 2,
    b: 0,
    c: 0,
    d: 2,
    e: 80,
    f: 40,
    inverse: () => ({ a: 0.5, b: 0, c: 0, d: 0.5, e: -40, f: -20 }),
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: this.x * m.a + this.y * m.c + m.e, y: this.x * m.b + this.y * m.d + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  win.SVGSVGElement.prototype.getScreenCTM = () => matrix;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  const fileIO = {
    writes: [],
    opened: null,
    released: [],
    recovery: new Map(),
    recoverySequence: 0,
    recoveryWriteSequence: 0,
    choice: 'discard',
  };
  globalThis.__nectTestFileIO = fileIO;
  globalThis.Image = class {
    naturalWidth = 1;
    naturalHeight = 1;
    set src(value) {
      this._src = value;
    }
    async decode() {}
  };
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  const storageStub = `
    import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}';
    export { DocumentLifecycle };
    const io = globalThis.__nectTestFileIO;
    export let desktop = true;
    globalThis.__setNectTestDesktop = (value) => { desktop = !!value; };
    export const saveFile = async (contents, name, kind) => { io.exports ??= []; io.exports.push({ contents, name, kind }); return true; };
    export const openFile = async () => io.opened && structuredClone(io.opened);
    export const openSVGSource = async () => null;
    export const writeProject = async (request) => { io.writes.push(structuredClone(request)); return { token: 'memory-project-' + request.operation, path: 'memory/project.json' }; };
    export const releaseProject = async (binding) => { if (binding?.token) io.released.push(binding.token); };
    export const chooseUnsaved = async () => io.choice;
    export const registerClose = async () => {};
    export const listSystemFonts = async () => [];
    export const readSystemFont = async () => { throw Error('No system fonts in integration fixture'); };
    export const listOsOutlineFonts = async () => [];
    export const outlineOsText = async () => { throw Error('No OS text in integration fixture'); };
    export const listDesktopFontFaces = async () => [];
    export const listDesktopFontAxes = async () => [];
    export const shapeDesktopText = async () => { throw Error('No OS text shaping in integration fixture'); };
    export const beginRecovery = async () => { const id = 'memory-recovery-' + (++io.recoverySequence); io.recovery.set(id, null); return id; };
    export const writeRecovery = async (copy) => { const savedAt = new Date(Date.now() + (++io.recoveryWriteSequence)).toISOString(); io.recovery.set(copy.id, { ...structuredClone(copy), savedAt }); return savedAt; };
    export const listRecovery = async () => [...io.recovery.entries()].filter(([, copy]) => copy).map(([id, copy]) => ({ id, name: copy.name, savedAt: copy.savedAt }));
    export const readRecovery = async (id) => io.recovery.get(id)?.contents;
    export const acknowledgeRecovery = async (id) => { io.recovery.delete(id); };
  `;
  source = source
    .replace("import './style.css';", '')
    .replace(/from '(\.\/[^']+)'/g, (_, path) =>
      path === './storage.js'
        ? `from 'data:text/javascript;base64,${Buffer.from(storageStub).toString('base64')}'`
        : `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.artworkUI = () => ({activeLayer, selected, artworkTarget, selection: [...selection], past: history.past.length, future: history.future.length, drag: drag?.type, tool, draft: penDraft?.layer, dirty: documents.dirty}); window.markPenSaved = () => { documents.saved = serializeProject(session.projectSnapshot()); };';
  source +=
    '\nwindow.compositionTest = { flushRecovery: async () => { await Promise.resolve(); return recovery?.flush(); }, recoveryState: () => recovery?.state, pngSize: () => pngDimensions(history.doc, pngExportSettings()), dimensions: () => [history.doc.width, history.doc.height] };';
  // These suites synchronously create/evaluate Text after startup. Await the
  // font condition inside the test module, rather than racing filesystem I/O.
  source +=
    '\nconst fontTestDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() >= fontTestDeadline) throw Error("Bundled test font did not finish loading"); await new Promise((r) => setTimeout(r, 10)); }';
  source += '\nwindow.canvasPreview = () => displayDocument();';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  flush();
  const $ = (s) => document.querySelector(s),
    stage = () => $('.canvas-stage');
  const saved = () => serialize(win.nect.document),
    ui = () => win.artworkUI();
  const load = (d) => {
    win.nect.load(serialize(d));
    flush();
  };
  const click = (s) => {
    $(s).click();
    flush();
  };
  const xy = ([x, y]) => [x * 2 + 80, y * 2 + 40];
  const fire = (el, type, p = [0, 0], extra = {}) => {
    const screen = xy(p);
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 3,
        button: 0,
        isPrimary: true,
        clientX: screen[0],
        clientY: screen[1],
        ...extra,
      }),
    );
    flush();
  };
  const key = (el, name, extra = {}) => {
    const e = new win.KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: name,
      ...extra,
    });
    el.dispatchEvent(e);
    flush();
    return e.defaultPrevented;
  };
  const selectNode = (id) => {
    fire($(`.node[data-id="${id}"] .node-head`), 'pointerdown');
    fire(document, 'pointerup');
  };
  const shape = (id) => $(`.shape[data-layer="${id}"]`);
  let confirmed = true;
  globalThis.confirm = () => confirmed;
  const seed = () => makeDoc();
  const newBlank = async () => {
    fileIO.choice = confirmed ? 'discard' : 'cancel';
    click('[data-action="new"]');
    await Promise.resolve();
    document
      .querySelector('dialog button[value="' + (confirmed ? 'discard' : 'cancel') + '"]')
      ?.click();
    await new Promise((resolve) => setTimeout(resolve, 0));
    flush();
  };
  const penPoint = (p, release = true) => {
    fire($('.artboard'), 'pointerdown', p);
    if (release) fire(document, 'pointerup', p);
  };
  const endPen = () => click('[data-pen-cancel]');
  const newDoc = async () => {
    confirmed = true;
    load(seed());
    await newBlank();
    assert.equal(win.nect.document.layers[0].nodes.length, 1);
    return saved();
  };
  await t.test('launch is blank, has no demo UI, and preserves saved artwork when opened', () => {
    const launch = saved();
    assert.equal(win.nect.document.layers.length, 1);
    assert.equal(win.nect.document.layers[0].nodes.length, 1);
    assert.equal(win.nect.document.layers[0].nodes[0].type, 'Output');
    assert.equal(win.nect.evaluate().error, null);
    assert.equal($('.shape'), null);
    assert.equal($('.warning'), null);
    assert.equal(ui().selected, null);
    assert.deepEqual(ui().selection, []);
    assert.equal(ui().past, 0);
    assert.match($('.inspector-body').textContent, /Empty composition/);
    assert.equal($('[data-action="add-layer"]'), null);
    click('[data-action="help"]');
    assert.doesNotMatch($('dialog').textContent, /demo/i);
    $('dialog button').click();
    click('[data-tool="Circle"]');
    const artwork = saved();
    assert.equal(win.nect.document.layers.length, 2);
    assert.ok($('.shape'));
    click('[data-action="undo"]');
    assert.equal(saved(), launch);
    assert.equal($('.shape'), null);
    click('[data-action="redo"]');
    assert.equal(saved(), artwork);
    load(JSON.parse(artwork));
    assert.equal(saved(), artwork);
    const existing = seed();
    load(existing);
    assert.equal(saved(), serialize(existing));
    assert.equal(win.nect.document.layers[0].nodes.length, 6);
    assert.ok($('.shape'));
  });
  await t.test(
    'New creates an unsaved document with an independent empty Undo history',
    async () => {
      load(seed());
      win.markPenSaved();
      const original = saved(),
        prior = ui();
      assert.equal(win.nect.document.layers[0].nodes.length, 6);
      assert.equal(ui().dirty, false);
      confirmed = true;
      await newBlank();
      const blank = saved();
      assert.equal(win.nect.document.layers.length, 1);
      assert.equal($('.shape'), null);
      assert.equal($('.vertex'), null);
      assert.equal($('.node-stage-move'), null);
      assert.equal($('.artwork-selection'), null);
      assert.equal($('.warning'), null);
      assert.equal(ui().selected, null);
      assert.equal(ui().artworkTarget, null);
      assert.deepEqual(ui().selection, []);
      assert.equal(ui().past, 0);
      assert.equal(ui().dirty, true);
      assert.match($('.inspector-body').textContent, /Empty composition/);
      assert.equal(document.activeElement, stage());
      click('[data-action="undo"]');
      assert.equal(saved(), blank);
      assert.equal(ui().dirty, true);
      win.markPenSaved();
      const event = new win.Event('beforeunload', { cancelable: true });
      win.dispatchEvent(event);
      assert.equal(event.defaultPrevented, false);
    },
  );
  await t.test(
    'New preserves released and actively pressed Pen drafts and all history',
    async () => {
      for (const count of [0, 1, 3])
        for (const pressed of [false, true]) {
          load(seed());
          click('[data-tool="pen"]');
          for (let i = 0; i < count; i++) penPoint([100 + i * 80, 100 + i * 20]);
          if (pressed) {
            penPoint([500, 300], false);
            fire(document, 'pointermove', [530, 320]);
          }
          const before = saved(),
            prior = structuredClone(ui());
          confirmed = false;
          await newBlank();
          assert.equal(saved(), before);
          assert.deepEqual(ui(), prior);
          if (ui().draft) endPen();
          else click('[data-tool="select"]');
        }
    },
  );
  await t.test(
    'New requires explicit Pen completion and never imports the old history',
    async () => {
      load(seed());
      confirmed = true;
      click('[data-tool="pen"]');
      penPoint([100, 100]);
      penPoint([300, 200]);
      const before = saved();
      await newBlank();
      assert.equal(saved(), before);
      assert.ok(ui().draft);
      key(stage(), 'Enter');
      assert.equal(win.nect.document.layers.length, 2);
      await newBlank();
      assert.equal(win.nect.document.layers.length, 1);
      assert.equal(ui().past, 0);
      assert.equal(ui().draft, undefined);
      click('[data-action="undo"]');
      assert.equal($('.shape'), null);
    },
  );
  await t.test(
    'all toolbar shapes and Pen work independently from New and restore exact blank state',
    async () => {
      for (const tool of ['Circle', 'Rectangle', 'Polygon', 'Star', 'Text', 'pen']) {
        const before = await newDoc(),
          empty = structuredClone(win.nect.document.layers[0]);
        click(`[data-tool="${tool}"]`);
        if (tool === 'pen') {
          penPoint([250, 100]);
          penPoint([420, 160]);
          key(stage(), 'Enter');
        }
        if (tool === 'Text') {
          stage().getBoundingClientRect = () => ({
            left: 80,
            top: 40,
            right: 1080,
            bottom: 740,
            width: 1000,
            height: 700,
          });
          penPoint([250, 100]);
          const readyDeadline = Date.now() + 5000;
          while (win.nect.evaluate().error !== null) {
            assert.ok(Date.now() < readyDeadline, win.nect.evaluate().error);
            await new Promise((r) => setTimeout(r, 10));
            flush();
          }
        }
        const after = saved();
        assert.equal(win.nect.document.layers.length, 2);
        assert.deepEqual(win.nect.document.layers[0], empty);
        assert.ok($('.shape'));
        assert.equal(ui().artworkTarget, win.nect.document.layers.at(-1).id);
        click('[data-action="undo"]');
        assert.equal(saved(), before);
        assert.equal($('.shape'), null);
        assert.equal(ui().selected, null);
        assert.equal(ui().artworkTarget, null);
        click('[data-action="redo"]');
        assert.equal(saved(), after);
        load(JSON.parse(after));
        assert.equal(saved(), after);
      }
    },
  );
  await t.test('+Blank appends from locked hidden artwork and preserves selection history', () => {
    const d = seed();
    d.layers[0].locked = true;
    d.layers[0].visible = false;
    load(d);
    const before = saved(),
      prior = ui();
    click('[data-action="add-blank-layer"]');
    const after = saved(),
      blank = win.nect.document.layers.at(-1);
    assert.deepEqual(win.nect.document.layers[0], JSON.parse(before).layers[0]);
    assert.equal(blank.nodes.length, 1);
    assert.equal(blank.allowEmptyOutput, true);
    assert.equal(blank.locked, false);
    assert.equal(blank.visible, true);
    assert.equal($('.shape'), null);
    assert.equal(ui().selected, null);
    assert.equal(ui().artworkTarget, null);
    click(`[data-select-layer="${blank.id}"]`);
    assert.equal(ui().artworkTarget, null);
    assert.equal($('.artwork-selection'), null);
    click('[data-action="undo"]');
    assert.equal(saved(), before);
    assert.equal(ui().selected, prior.selected);
    click('[data-action="redo"]');
    assert.equal(saved(), after);
    assert.equal($('[data-action="add-layer"]'), null);
  });
  await t.test(
    'blank graph creation remains disconnected until explicit Use as output',
    async () => {
      await newDoc();
      const l = win.nect.document.layers[0],
        output = structuredClone(l.nodes[0]);
      const add = $('#add-node');
      add.value = 'Curve';
      add.dispatchEvent(new win.Event('change', { bubbles: true }));
      flush();
      const source = win.nect.document.layers[0].nodes.at(-1);
      assert.equal($('.shape'), null);
      assert.deepEqual(win.nect.document.layers[0].nodes[0], output);
      assert.equal(win.nect.evaluate().error, null);
      assert.equal(win.nect.document.layers.length, 1);
      click(`[data-preview="${source.id}"]`);
      assert.ok($('.shape'));
      assert.equal(win.nect.document.layers.length, 1);
      assert.equal(win.nect.evaluate().error, null);
      click('[data-action="undo"]');
      assert.equal($('.shape'), null);
    },
  );
  await t.test(
    '+Blank cancels live source/graph/wire gestures before switching layer scope',
    () => {
      for (const kind of ['source', 'node', 'wire', 'pan']) {
        load(seed());
        const before = saved(),
          source = win.nect.document.layers[0].nodes[0];
        if (kind === 'source') fire($('[data-node-stage-move]'), 'pointerdown', [100, 100]);
        if (kind === 'node')
          fire($(`.node[data-id="${source.id}"] .node-head`), 'pointerdown', [100, 100]);
        if (kind === 'wire') fire($(`[data-source="${source.id}"]`), 'pointerdown', [100, 100]);
        if (kind === 'pan') fire(stage(), 'pointerdown', [100, 100], { button: 1 });
        fire(document, 'pointermove', [140, 125]);
        if (kind === 'source') {
          assert.equal(saved(), before);
          assert.notDeepEqual(win.canvasPreview(), JSON.parse(before));
        }
        if (kind === 'node') {
          assert.equal(saved(), before, 'graph preview keeps the committed document');
          assert.notDeepEqual(
            win.nect.previewSnapshot(win.nect.gestureState.gestureId).document,
            JSON.parse(before),
          );
        }
        click('[data-action="add-blank-layer"]');
        assert.equal(ui().drag, undefined);
        assert.equal(ui().draft, undefined);
        assert.deepEqual(win.nect.document.layers[0], JSON.parse(before).layers[0]);
        assert.equal(ui().past, 1);
        const after = saved();
        fire(document, 'pointermove', [220, 180]);
        fire(document, 'pointerup');
        assert.equal(saved(), after);
        click('[data-action="undo"]');
        assert.equal(saved(), before);
      }
      const d = seed();
      while (d.layers.length < 100) appendBlankComposition(d);
      load(d);
      const before = saved();
      fire($('[data-node-stage-move]'), 'pointerdown', [100, 100]);
      fire(document, 'pointermove', [140, 125]);
      click('[data-action="add-blank-layer"]');
      assert.equal(saved(), before, 'capacity failure retains the pre-gesture committed document');
      assert.equal(ui().drag, undefined);
      assert.equal(ui().past, 0);
    },
  );
  await t.test('+Blank capacity failure is atomic in document, history and UI selection', () => {
    const d = seed();
    while (d.layers.length < 100) appendBlankComposition(d);
    load(d);
    const before = saved(),
      prior = structuredClone(ui());
    click('[data-action="add-blank-layer"]');
    assert.equal(saved(), before);
    assert.deepEqual(ui(), prior);
    assert.match($('.message').textContent, /100/);
  });
  await t.test(
    'Composition project save, open, recovery, legacy migration, and public load share one Session snapshot',
    async () => {
      const firstLegacy = makeDoc();
      firstLegacy.name = 'Composition lifecycle';
      firstLegacy.width = 1200;
      firstLegacy.height = 800;
      appendArtwork(firstLegacy, 'Rectangle', { at: [160, 180] });
      const fontBytes = await fs.readFile(
        new URL('../public/fonts/DejaVuSans.ttf', import.meta.url),
      );
      firstLegacy.fontAssets = {
        'font-shared': {
          name: 'DejaVu Sans',
          fileName: 'DejaVuSans.ttf',
          data: fontBytes.toString('base64'),
        },
      };
      firstLegacy.imageAssets = {
        'image-shared': {
          name: 'shared.png',
          mime: 'image/png',
          width: 1,
          height: 1,
          data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
        },
      };
      const project = migrateLegacyDocumentToCompositionProject(firstLegacy);
      const secondLegacy = makeDoc();
      secondLegacy.name = 'Composition lifecycle';
      secondLegacy.width = 420;
      secondLegacy.height = 760;
      const secondComposition =
        migrateLegacyDocumentToCompositionProject(secondLegacy).compositions[0];
      secondComposition.id = 'composition-tall';
      secondComposition.name = 'Tall format';
      project.compositions.push(secondComposition);
      const first = project.compositions[0],
        second = project.compositions[1],
        firstInactiveLayer = first.layers[1];
      load(project);
      assert.deepEqual(
        win.nect.compositions.map(({ width, height }) => [width, height]),
        [
          [1200, 800],
          [420, 760],
        ],
      );
      assert.deepEqual(Object.keys(win.nect.projectSnapshot().fontAssets), ['font-shared']);
      assert.deepEqual(Object.keys(win.nect.projectSnapshot().imageAssets), ['image-shared']);

      const stateChange = (layerId, value) =>
        win.nect.dispatch({
          ...win.nect.commandState,
          type: 'set-layer-state',
          payload: { layerId, key: 'visible', value },
        });
      const switchTo = async (compositionId) => {
        const select = $('[data-active-composition]');
        select.value = compositionId;
        select.dispatchEvent(new win.Event('change', { bubbles: true }));
        flush();
        await new Promise((resolve) => setTimeout(resolve, 0));
      };
      const tick = () => new Promise((resolve) => setTimeout(resolve, 0));
      const composition = (id) =>
        win.nect.projectSnapshot().compositions.find((entry) => entry.id === id);

      stateChange(firstInactiveLayer.id, false);
      const historyBeforeSwitch = ui().past;
      await switchTo(second.id);
      assert.equal(win.nect.activeCompositionId, second.id);
      assert.equal(ui().past, historyBeforeSwitch, 'switching tabs adds no authored history step');
      stateChange(second.layers[0].id, false);
      click('[data-action="undo"]');
      assert.equal(win.nect.activeCompositionId, second.id);
      assert.equal(
        composition(first.id).layers[1].visible,
        false,
        'Undo keeps the other Composition edit',
      );
      assert.equal(
        composition(second.id).layers[0].visible,
        true,
        'Undo targets the latest project-wide edit',
      );
      click('[data-action="redo"]');
      assert.equal(
        composition(second.id).layers[0].visible,
        false,
        'Redo restores the active Composition edit',
      );
      click('[data-action="undo"]');
      assert.equal(composition(second.id).layers[0].visible, true);
      assert.deepEqual(win.compositionTest.dimensions(), [420, 760]);
      assert.deepEqual(win.compositionTest.pngSize(), { width: 420, height: 760 });
      assert.match(win.nect.export(), /width="420" height="760"/);

      await switchTo(first.id);
      assert.deepEqual(win.compositionTest.dimensions(), [1200, 800]);
      assert.deepEqual(win.compositionTest.pngSize(), { width: 1200, height: 800 });
      assert.match(win.nect.export(), /width="1200" height="800"/);
      await switchTo(second.id);
      click('[data-action="save-as"]');
      await tick();
      assert.equal(fileIO.writes.length, 1);
      const savedProjectText = fileIO.writes[0].contents,
        savedProject = JSON.parse(savedProjectText);
      assert.equal(win.nect.fileState.dirty, false);
      assert.deepEqual(
        savedProject.compositions.map(({ width, height }) => [width, height]),
        [
          [1200, 800],
          [420, 760],
        ],
      );
      assert.equal(savedProject.compositions[0].layers[1].visible, false);
      assert.equal(savedProject.compositions[1].layers[0].visible, true);
      assert.deepEqual(Object.keys(savedProject.fontAssets), ['font-shared']);
      assert.deepEqual(Object.keys(savedProject.imageAssets), ['image-shared']);

      fileIO.opened = {
        contents: savedProjectText,
        binding: { token: 'memory-readback', path: 'memory/project.json' },
      };
      click('[data-action="open"]');
      await tick();
      assert.deepEqual(
        win.nect.projectSnapshot(),
        savedProject,
        'Open reads back the complete saved project',
      );
      assert.equal(win.nect.fileState.dirty, false);

      await switchTo(second.id);
      stateChange(second.layers[0].id, false);
      click('[data-action="save"]');
      await tick();
      assert.equal(fileIO.writes.length, 2, 'Save writes through the existing document binding');
      assert.equal(fileIO.writes[1].binding.token, 'memory-readback');
      await switchTo(first.id);
      stateChange(firstInactiveLayer.id, true);
      await win.compositionTest.flushRecovery();
      assert.equal(win.compositionTest.recoveryState().phase, 'saved');
      const recoverySnapshot = JSON.parse(
        [...fileIO.recovery.values()].filter(Boolean).at(-1).contents,
      );
      assert.equal(recoverySnapshot.compositions[0].layers[1].visible, true);
      assert.equal(recoverySnapshot.compositions[1].layers[0].visible, false);
      click('[data-recovery-open]');
      await tick();
      const recoveryDialog = $('dialog[data-recovery-dialog]');
      assert.ok(recoveryDialog, 'the production Recovery Copies action opens stored copies');
      recoveryDialog.querySelector('select').value = String(
        recoveryDialog.querySelector('select').options.length - 1,
      );
      recoveryDialog.querySelector('button[value="restore"]').click();
      await tick();
      await win.compositionTest.flushRecovery();
      assert.equal(composition(first.id).layers[1].visible, true);
      assert.equal(composition(second.id).layers[0].visible, false);
      assert.equal(win.nect.fileState.dirty, true, 'recovery opens as an unsaved project');

      const beforeRejectedLoad = JSON.stringify(win.nect.projectSnapshot());
      assert.throws(() => win.nect.load('{invalid JSON'), /JSON|Unexpected/);
      assert.equal(
        JSON.stringify(win.nect.projectSnapshot()),
        beforeRejectedLoad,
        'invalid public load is atomic',
      );
      win.nect.load(savedProjectText);
      assert.deepEqual(win.nect.projectSnapshot(), savedProject);

      const legacy = makeDoc();
      legacy.name = 'Legacy project';
      legacy.width = 777;
      legacy.height = 333;
      legacy.fontAssets = firstLegacy.fontAssets;
      legacy.imageAssets = firstLegacy.imageAssets;
      const legacyText = serialize(legacy),
        browserFile = { size: Buffer.byteLength(legacyText), text: async () => legacyText },
        browserInput = $('#open-file');
      Object.defineProperty(browserInput, 'files', { configurable: true, value: [browserFile] });
      browserInput.click = () =>
        browserInput.dispatchEvent(new win.Event('change', { bubbles: true }));
      globalThis.__setNectTestDesktop(false);
      click('[data-action="open"]');
      await tick();
      globalThis.__setNectTestDesktop(true);
      assert.equal(win.nect.compositions.length, 1);
      assert.deepEqual([win.nect.document.width, win.nect.document.height], [777, 333]);
      assert.deepEqual(Object.keys(win.nect.projectSnapshot().fontAssets), ['font-shared']);
      assert.deepEqual(Object.keys(win.nect.projectSnapshot().imageAssets), ['image-shared']);
      assert.equal(win.nect.fileState.dirty, false);
      assert.ok(firstLegacy.layers.length >= 2, 'the fixture includes a non-active Layer');
      assert.ok(fileIO.released.includes('memory-readback'), 'replaced file bindings are released');
    },
  );
  win.happyDOM.abort();
});
