import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import fs from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import { Window } from 'happy-dom';
import { createAgentWorkflow } from '../src/agent-workflow.js';
import {
  MAX_COMPOSITIONS,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { CompositionSessionOwner } from '../src/composition-session-owner.js';
import { blankDocument, blankLayer, node } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';

const a = 'comp:wide';
const b = 'comp:tall';
const duplicateName = ' 同じ · Cafe\u0301 🎨 ';
const renamed = `\t縦版 · ${'絵'.repeat(140)} 🖼 `;
const absent = { sessionId: 'absent', revision: 0, bindingRevision: 0 };
const select = 'switch_composition';
const source = 'project.json';

function projectFixture() {
  function composition(id, width, height, center, size) {
    const layer = blankLayer('Rectangle proof');
    const [output] = layer.nodes;
    const rectangle = node('Rectangle');
    const fill = node('Fill');
    rectangle.params.center.value = center;
    rectangle.params.size.value = size;
    fill.params.path.source = rectangle.id;
    output.params.path.source = fill.id;
    layer.nodes = [rectangle, fill, output];
    delete layer.allowEmptyOutput;
    return { id, name: duplicateName, width, height, layers: [layer] };
  }
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  project.compositions = [
    composition(a, 377.5, 253.125, [160, 130], [180, 220]),
    composition(b, 303, 501, [120, 310], [80, 120]),
  ];
  return project;
}

const catalog = ({ compositions }) =>
  compositions.map(({ id, name, width, height }) => ({ id, name, width, height }));
const stateOnly = ({ composition, compositions, ...state }) => state;

function toolResult(reply, code) {
  assert.equal(reply.ok, !code, JSON.stringify(reply));
  if (code) {
    assert.equal(reply.error.code, code, JSON.stringify(reply));
    return reply.error;
  }
  return reply.result;
}

function direct(files) {
  const workflow = createAgentWorkflow({ files });
  return async (name, args, code) =>
    toolResult(await workflow.dispatch(`nect_${name}`, args), code);
}

async function switchTo(request, before, compositionId) {
  const switched = await request(select, { target: before.target, compositionId });
  assert.notEqual(switched.target.sessionId, before.target.sessionId);
  assert.deepEqual(switched, {
    ...stateOnly(before),
    target: { ...before.target, sessionId: switched.target.sessionId },
    changed: true,
  });
  const after = await request('inspect');
  const { layers, ...active } = after.composition;
  assert.deepEqual(
    active,
    before.compositions.find(({ id }) => id === compositionId),
  );
  assert.deepEqual({ ...stateOnly(after), changed: true }, switched);
  return after;
}

async function assertSelectionRejections(request, before) {
  const args = { target: before.target, compositionId: b };
  for (const [input, code] of [
    [{ compositionId: b }, 'INVALID_ARGUMENT'],
    [{ target: before.target }, 'INVALID_ARGUMENT'],
    [{ ...args, target: null }, 'INVALID_ARGUMENT'],
    [{ ...args, target: { ...args.target, extra: true } }, 'INVALID_ARGUMENT'],
    [{ ...args, compositionId: '' }, 'INVALID_ARGUMENT'],
    [{ ...args, compositionId: 'x'.repeat(182) }, 'INVALID_ARGUMENT'],
    [{ ...args, compositionId: 123 }, 'INVALID_ARGUMENT'],
    [{ ...args, extra: true }, 'INVALID_ARGUMENT'],
    [{ ...args, compositionId: duplicateName }, 'UNKNOWN_COMPOSITION'],
    [{ ...args, compositionId: b.toUpperCase() }, 'UNKNOWN_COMPOSITION'],
    [{ ...args, target: absent }, 'STALE_TARGET'],
    [{ ...args, target: { ...args.target, revision: args.target.revision + 1 } }, 'STALE_TARGET'],
    [{ ...args, target: { ...args.target, bindingRevision: 1 } }, 'STALE_TARGET'],
  ]) {
    await request(select, input, code);
    assert.deepEqual(await request('inspect'), before);
  }
}

test('direct catalog is exact, independent, and cached across clean and dirty view switches', async (t) => {
  const snapshot = CompositionSessionOwner.prototype.projectSnapshot;
  const snapshots = t.mock.method(
    CompositionSessionOwner.prototype,
    'projectSnapshot',
    function () {
      return snapshot.call(this);
    },
  ).mock;
  const project = projectFixture();
  const another = projectFixture();
  another.compositions[0].name = '';
  another.compositions[1].name = ' \u3000';
  for (let i = 2; i < MAX_COMPOSITIONS; i++) {
    another.compositions.push({
      id: i === MAX_COMPOSITIONS - 1 ? 'x'.repeat(181) : `comp:extra-${i}`,
      name: `Artwork ${i}`,
      width: 100,
      height: 100,
      layers: [blankLayer()],
    });
  }
  const bytes = serializeCompositionProject(project);
  const binding = { fileName: source, token: 'source' };
  const io = [];
  const writing = Promise.withResolvers();
  const request = direct({
    async open(fileName) {
      io.push(['open', fileName]);
      return fileName === source
        ? { binding, contents: bytes }
        : { binding: { fileName, token: 'other' }, contents: serializeCompositionProject(another) };
    },
    async writeProject({ contents, binding: live }) {
      io.push(['write', live]);
      await writing.promise;
      const expected = structuredClone(project);
      expected.compositions[1].name = renamed;
      assert.deepEqual(JSON.parse(contents), expected);
      return live;
    },
    release(live) {
      io.push(['release', live]);
    },
  });
  const cold = await request('inspect');
  assert.equal(cold.target, null);
  assert.equal(cold.composition, null);
  assert.deepEqual(cold.compositions, []);
  await request(select, { target: absent, compositionId: a }, 'NO_DOCUMENT');
  assert.deepEqual(await request('inspect'), cold);
  assert.deepEqual(io, []);

  const opened = await request('open', { target: null, fileName: source });
  assert.equal(Object.hasOwn(opened, 'compositions'), false);
  const afterOpen = snapshots.callCount();
  const before = await request('inspect');
  assert.equal(before.dirty, false);
  assert.equal(before.composition.id, a);
  assert.deepEqual(before.compositions, catalog(project));
  const copy = await request('inspect');
  assert.notEqual(copy.compositions, before.compositions);
  assert.notEqual(copy.compositions[1], before.compositions[1]);
  Object.assign(copy.compositions[1], { id: 'wrong-id', name: 'wrong name', width: 1, height: 1 });
  copy.compositions.reverse().pop();
  assert.deepEqual(await request('inspect'), before);
  await assertSelectionRejections(request, before);

  const second = await switchTo(request, before, b);
  assert.notEqual(second.composition.layers[0].id, before.composition.layers[0].id);
  assert.deepEqual(await request(select, { target: second.target, compositionId: b }), {
    ...stateOnly(second),
    changed: false,
  });
  for (let i = 0; i < 3; i++) assert.deepEqual(await request('inspect'), second);
  const back = await switchTo(request, second, a);
  assert.deepEqual(back.composition, before.composition);
  assert.notEqual(back.target.sessionId, before.target.sessionId);
  await request(select, { target: before.target, compositionId: a }, 'STALE_TARGET');
  await request('create_rectangle', { target: second.target, x: 100, y: 100 }, 'STALE_TARGET');
  assert.deepEqual(await request('inspect'), back);
  assert.equal(
    snapshots.callCount(),
    afterOpen,
    'Inspect and view switches must reuse the project snapshot',
  );

  const edit = await request('rename_composition', {
    target: back.target,
    compositionId: b,
    name: renamed,
  });
  const edited = await request('inspect');
  assert.deepEqual(edited.compositions, [
    before.compositions[0],
    { ...before.compositions[1], name: renamed },
  ]);
  assert.equal(edited.dirty, true);
  const undone = await request('undo', { target: edit.target });
  const pending = await request('inspect');
  assert.equal(pending.dirty, false);
  assert.deepEqual(pending.compositions, before.compositions);
  assert.deepEqual(undone.history, { revision: 2, canUndo: false, canRedo: true });
  const afterUndo = snapshots.callCount();
  const pendingOnB = await switchTo(request, pending, b);
  assert.deepEqual(await request('inspect'), pendingOnB);
  assert.equal(snapshots.callCount(), afterUndo);
  const redo = await request('redo', { target: pendingOnB.target });
  const dirtyOnB = await request('inspect');
  assert.equal(dirtyOnB.composition.name, renamed);
  assert.deepEqual(dirtyOnB.compositions, edited.compositions);
  assert.equal(redo.dirty, true);
  const dirtyOnA = await switchTo(request, dirtyOnB, a);
  assert.deepEqual(
    io,
    [['open', source]],
    'switch, rejection, and history do not call the file port',
  );

  const saving = request('save', { target: dirtyOnA.target });
  try {
    await request(select, { target: dirtyOnA.target, compositionId: b }, 'BUSY');
  } finally {
    writing.resolve();
  }
  const saved = await saving;
  assert.equal(saved.dirty, false);
  assert.equal(Object.hasOwn(saved, 'compositions'), false);
  assert.deepEqual(io, [
    ['open', source],
    ['write', binding],
  ]);
  assert.equal((await request('inspect')).composition.id, a);

  await request('open', { target: saved.target, fileName: 'other.json' });
  const replaced = await request('inspect');
  assert.equal(replaced.composition.name, '');
  assert.equal(replaced.compositions.length, MAX_COMPOSITIONS);
  assert.deepEqual(replaced.compositions, catalog(another));
  await request(
    select,
    { target: replaced.target, compositionId: ' \u3000' },
    'UNKNOWN_COMPOSITION',
  );
  const blankTab = await switchTo(request, replaced, b);
  assert.equal(blankTab.composition.name, ' \u3000');
  await switchTo(request, blankTab, another.compositions.at(-1).id);
});

test('direct switch fails closed for an inactive Composition that needs variable-font preparation', async () => {
  const project = projectFixture();
  const bytes = await fs.readFile(
    new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
  );
  const { id, asset } = makeVariableFontAsset(Uint8Array.from(bytes).buffer, 'Roboto.ttf');
  project.fontAssets = { [id]: asset };
  const layer = project.compositions[1].layers[0];
  const output = layer.nodes.at(-1);
  const text = Object.assign(node('Text'), {
    text: 'AV',
    font: id,
    variation: { version: 1, fontId: id, axes: { wght: 550 } },
  });
  output.params.path.source = text.id;
  layer.nodes = [text, output];
  const request = direct({
    async open(fileName) {
      return {
        binding: { fileName, token: 'source' },
        contents: serializeCompositionProject(project),
      };
    },
  });
  await request('open', { target: null, fileName: source });
  const before = await request('inspect');
  assert.deepEqual(before.compositions, catalog(project));
  const error = await request(
    select,
    { target: before.target, compositionId: b },
    'PREPARATION_REQUIRED',
  );
  assert.match(error.message, /use the native Nect Desktop editor/i);
  assert.deepEqual(await request('inspect'), before);
  assert.equal((await request(select, { target: before.target, compositionId: a })).changed, false);
});

async function startMcp(root, cleanup) {
  const cli = fileURLToPath(new URL('../cli/agent-workflow.js', import.meta.url));
  const child = spawn(process.execPath, [cli, '--workspace', root], { windowsHide: true });
  child.stdin.on('error', () => {}); // Report early exit with stderr through rpc/stop.
  const closed = once(child, 'close');
  cleanup.push(async () => {
    if (child.exitCode === null && child.signalCode === null) child.kill();
    await closed;
  });
  const lines = createInterface({ input: child.stdout })[Symbol.asyncIterator]();
  let id = 0,
    stderr = '';
  child.stderr.setEncoding('utf8').on('data', (chunk) => (stderr += chunk));
  const send = (message) =>
    child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', ...message })}\n`);
  const rpc = async (method, params) => {
    send({ id: ++id, method, params });
    const line = await lines.next();
    assert.equal(line.done, false, stderr);
    const reply = JSON.parse(line.value);
    assert.equal(reply.id, id);
    assert.equal(reply.jsonrpc, '2.0');
    return reply.result;
  };
  const protocolVersion = '2025-06-18';
  const hello = await rpc('initialize', {
    protocolVersion,
    capabilities: {},
    clientInfo: { name: 'composition-navigation-test', version: '1' },
  });
  assert.equal(hello.protocolVersion, protocolVersion);
  send({ method: 'notifications/initialized' });
  return {
    rpc,
    async request(name, args, code) {
      const result = await rpc('tools/call', { name: `nect_${name}`, arguments: args });
      assert.equal(result.isError, !!code);
      assert.equal(result.content.length, 1);
      assert.equal(result.content[0].type, 'text');
      return toolResult(JSON.parse(result.content[0].text), code);
    },
    async stop() {
      child.stdin.end();
      assert.deepEqual(await closed, [0, null], stderr);
      assert.equal(stderr, '');
      assert.equal((await lines.next()).done, true);
    },
  };
}

test(
  'real stdio selection rotates targets, renders only the selected scope, and saves only authored changes',
  { timeout: 30_000 },
  async (t) => {
    const parent = await fs.realpath(
      await fs.mkdtemp(path.join(tmpdir(), 'nect-navigation-test-')),
    );
    const root = path.join(parent, 'workspace');
    await fs.mkdir(root);
    const cleanup = [];
    t.after(async () => {
      for (const stop of cleanup) await stop();
      await fs.rm(parent, { recursive: true, force: true });
    });
    const start = () => startMcp(root, cleanup);
    const file = (leaf) => path.join(root, leaf);
    const initial = projectFixture();
    const original = Buffer.from(serializeCompositionProject(initial));
    await fs.writeFile(file(source), original);
    const stat = await fs.stat(file(source), { bigint: true });
    async function untouched(...exports) {
      assert.deepEqual(await fs.readFile(file(source)), original);
      const current = await fs.stat(file(source), { bigint: true });
      for (const key of ['ino', 'mtimeNs', 'ctimeNs']) assert.equal(current[key], stat[key], key);
      assert.deepEqual((await fs.readdir(root)).sort(), [source, ...exports].sort());
    }
    const mcp = await start();
    const { request } = mcp;
    const { tools } = await mcp.rpc('tools/list');
    const schema = tools.find(({ name }) => name === `nect_${select}`).inputSchema;
    const renameSchema = tools.find(({ name }) => name === 'nect_rename_composition').inputSchema;
    assert.deepEqual(schema, {
      type: 'object',
      properties: {
        target: renameSchema.properties.target,
        compositionId: renameSchema.properties.compositionId,
      },
      required: ['target', 'compositionId'],
      additionalProperties: false,
    });
    const empty = await request('inspect');
    assert.deepEqual(empty.compositions, []);
    assert.equal(empty.composition, null);
    await request(select, { target: absent, compositionId: b }, 'NO_DOCUMENT');
    assert.deepEqual(await request('inspect'), empty);
    await untouched();

    await request('open', { target: null, fileName: source });
    const first = await request('inspect');
    assert.equal(first.composition.id, a);
    assert.equal(first.dirty, false);
    assert.deepEqual(first.compositions, catalog(initial));
    await assertSelectionRejections(request, first);
    const second = await switchTo(request, first, b);
    assert.deepEqual(await request(select, { target: second.target, compositionId: b }), {
      ...stateOnly(second),
      changed: false,
    });
    const back = await switchTo(request, second, a);
    assert.notEqual(back.target.sessionId, first.target.sessionId);
    for (const { target } of [first, second]) {
      await request(select, { target, compositionId: a }, 'STALE_TARGET');
      await request('export_svg', { target, fileName: 'stale.svg' }, 'STALE_TARGET');
    }
    assert.deepEqual(await request('inspect'), back);
    await untouched();

    const Parser = new Window().DOMParser; // In-memory XML parser; no browser or GUI.
    async function svg(call, { target }, fileName, dimensions, d) {
      await call('export_svg', { target, fileName });
      const data = await fs.readFile(file(fileName));
      const xml = new Parser().parseFromString(data.toString('utf8'), 'image/svg+xml');
      assert.equal(xml.documentElement.localName, 'svg');
      assert.equal(xml.documentElement.getAttribute('viewBox'), `0 0 ${dimensions}`);
      assert.deepEqual(
        [...xml.querySelectorAll('path')].map((p) => p.getAttribute('d')),
        [d],
      );
      return data;
    }
    const wide = await svg(request, back, 'wide.svg', '377.5 253.125', 'M70,240v-220h180v220z');
    const tall = await switchTo(request, back, b);
    await svg(request, tall, 'tall.svg', '303 501', 'M80,370v-120h80v120z');
    const move = await request('move_artwork', {
      target: tall.target,
      layerId: tall.composition.layers[0].id,
      dx: 10,
      dy: -5,
    });
    const undo = await request('undo', { target: move.target });
    assert.equal(undo.dirty, false);
    assert.deepEqual(undo.history, { revision: 2, canUndo: false, canRedo: true });
    const pending = await switchTo(request, await request('inspect'), a);
    const redo = await request('redo', { target: pending.target });
    assert.equal(redo.dirty, true);
    const ready = await switchTo(request, await request('inspect'), b);
    const moved = await svg(request, ready, 'moved.svg', '303 501', 'M90,365v-120h80v120z');
    await untouched('wide.svg', 'tall.svg', 'moved.svg');
    const saved = await request('save', { target: ready.target });
    assert.equal(saved.dirty, false);
    assert.equal((await request('inspect')).composition.id, b);
    const savedBytes = await fs.readFile(file(source));
    const expected = structuredClone(initial);
    expected.compositions[1].layers[0].nodes[0].params.center.value = [130, 305];
    assert.deepEqual(JSON.parse(savedBytes), expected);
    await mcp.stop();

    const cold = await start();
    assert.deepEqual(await cold.request('inspect'), empty);
    await cold.request('open', { target: null, fileName: source });
    const reopened = await cold.request('inspect');
    assert.equal(reopened.composition.id, a);
    assert.equal(reopened.dirty, false);
    assert.deepEqual(reopened.history, { revision: 0, canUndo: false, canRedo: false });
    assert.deepEqual(reopened.compositions, catalog(initial));
    await cold.request(select, { target: saved.target, compositionId: b }, 'STALE_TARGET');
    assert.deepEqual(
      await svg(cold.request, reopened, 'cold-wide.svg', '377.5 253.125', 'M70,240v-220h180v220z'),
      wide,
    );
    const coldB = await switchTo(cold.request, reopened, b);
    assert.deepEqual(
      await svg(cold.request, coldB, 'cold-tall.svg', '303 501', 'M90,365v-120h80v120z'),
      moved,
    );
    assert.deepEqual(await fs.readFile(file(source)), savedBytes);
    await cold.stop();
  },
);
