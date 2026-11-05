import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
const coreUrl = new URL('../src/core.js', import.meta.url).href;
const registryUrl = new URL('../src/builtin-node-registry.js', import.meta.url).href;
const core = await import(coreUrl),
  registryModule = await import(registryUrl);
const registry = registryModule.builtinNodeRegistry,
  resolved = registry.get('Dev Affine Fixture');
assert.ok(resolved);
assert.ok(Object.isFrozen(resolved));
assert.equal((await import(registryUrl)).builtinNodeRegistry, registry);
assert.equal(await import(coreUrl), core);
const doc = core.document(),
  layer = doc.layers[0],
  dev = core.node('Dev Affine Fixture'),
  circle = core.node('Circle');
layer.nodes.push(dev, circle);
core.connect(layer, dev.id, circle.id, 'radius');
assert.equal(dev.operation, 'subtract');
assert.deepEqual(
  Object.values(dev.params).map((q) => q.value),
  [2, 3, 4],
);
assert.deepEqual(core.outputPorts(dev), [{ id: undefined, label: '', type: 'Scalar' }]);
assert.equal(core.outputType(dev, 'default'), undefined);
const headless = core.evaluate(layer);
assert.equal(headless.run(dev.id), 10);
const firstOutputUtc = new Date().toISOString();
const before = core.serialize(doc);
assert.equal(core.serialize(core.deserialize(before)), before);
const win = new Window({ url: 'http://localhost:5173' });
try {
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
  globalThis.fetch = async () => ({ ok: false });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  assert.ok(source.includes(`from '${coreUrl}'`));
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await tick();
  assert.ok(win.nect);
  win.nect.load(before);
  revealDOMBoundary(document, 'graph');
  const head = document.querySelector(`.graph .node[data-id="${dev.id}"] .node-head .type-tag`);
  assert.ok(head);
  head.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      button: 0,
      pointerId: 7,
    }),
  );
  document.dispatchEvent(
    new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 7 }),
  );
  openDOMPanel(document, 'inspector');
  assert.equal(win.nect.evaluate().run(dev.id), 10);
  const input = document.querySelector(
    `.inspector-body .field[data-node="${dev.id}"][data-key="x"] [data-value]`,
  );
  assert.ok(input);
  input.value = '5';
  input.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(win.nect.evaluate().run(dev.id), 19);
  document.querySelector('[data-action="undo"]').click();
  assert.equal(win.nect.evaluate().run(dev.id), 10);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(win.nect.evaluate().run(dev.id), 19);
  const saved = core.serialize(win.nect.document);
  assert.equal(core.serialize(core.deserialize(saved)), saved);
  win.nect.load(saved);
  assert.equal(win.nect.evaluate().run(dev.id), 19);
  const catalog = await import(new URL('../src/node-catalog.js', import.meta.url).href);
  assert.equal(
    catalog.nodePickerItems(core.definitions).some((item) => item.id === dev.type),
    false,
  );
  const state = core.deserialize(saved),
    vm = state.layers[0].nodes.find((n) => n.id === dev.id),
    session = new core.CommandSession(state);
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: { layerId: state.layers[0].id, nodeId: vm.id, key: 'x', value: 7 },
  });
  session.dispatch({ ...session.state, type: 'undo' });
  const checkpoint = core.serialize(session.snapshot()),
    history = JSON.stringify(session.historyView),
    revision = session.state.expectedRevision;
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'connect',
        payload: { layerId: state.layers[0].id, nodeId: vm.id, key: 'x', sourceId: 'missing' },
      }),
    /Missing/,
  );
  assert.equal(core.serialize(session.snapshot()), checkpoint);
  assert.equal(JSON.stringify(session.historyView), history);
  assert.equal(session.state.expectedRevision, revision);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.throws(() => resolved.project('unknown', 1), /output/);
  const overflow = core.node('Dev Affine Fixture');
  overflow.params.x.value = 1e308;
  overflow.params.scale.value = 1e308;
  assert.throws(
    () => core.evaluate({ ...layer, nodes: [overflow], output: overflow.id }).run(overflow.id),
    /finite|Scalar/,
  );
  process.stdout.write(
    JSON.stringify({
      pid: process.pid,
      firstOutputUtc,
      coreUrl,
      registryUrl,
      manifestUrl: new URL('../src/builtin-node-manifest.js', import.meta.url).href,
      registrySame: true,
      headless: 10,
      uiAfterEdit: 19,
      undoRedo: true,
      reopen: true,
      menuHidden: true,
      atomicInvalid: true,
    }) + '\n',
  );
} finally {
  await win.happyDOM.close();
}
