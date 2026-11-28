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
import { agentWorkflowTools, createAgentWorkflow } from '../src/agent-workflow.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { CompositionSessionOwner } from '../src/composition-session-owner.js';
import { blankDocument, blankLayer, node } from '../src/core.js';

const sharedName = 'Same tab label';
const activeId = 'comp:wide';
const inactiveId = 'comp:tall';
const noTarget = { sessionId: 'absent', revision: 0, bindingRevision: 0 };
const activeName = `  表紙 · Cafe\u0301 🎨 ${'長'.repeat(140)}  `;
const inactiveName = '\t縦版 · 日本語 🖼\n ';
const rename = 'rename_composition';

function projectFixture() {
  function composition(id, width, height, center) {
    const layer = blankLayer('Rectangle proof');
    const [output] = layer.nodes;
    const rectangle = node('Rectangle');
    const fill = node('Fill');
    rectangle.params.center.value = center;
    rectangle.params.size.value = [180, 220];
    fill.params.path.source = rectangle.id;
    output.params.path.source = fill.id;
    layer.nodes = [rectangle, fill, output];
    delete layer.allowEmptyOutput;
    return { id, name: sharedName, width, height, layers: [layer] };
  }
  const project = migrateLegacyDocumentToCompositionProject(blankDocument());
  project.id = 'project-proof';
  project.name = 'Rename proof project';
  project.compositions = [
    composition(activeId, 377.5, 253.125, [160, 130]),
    composition(inactiveId, 303, 501, [120, 310]),
  ];
  return project;
}

function toolResult(reply, code) {
  assert.equal(reply.ok, !code, JSON.stringify(reply));
  if (code) assert.equal(reply.error.code, code, JSON.stringify(reply));
  return reply.result;
}

function direct(files) {
  const workflow = createAgentWorkflow({ files });
  return async (name, args, code) =>
    toolResult(await workflow.dispatch(`nect_${name}`, args), code);
}

function changedRename(result, previous) {
  assert.equal(result.changed, true);
  assert.deepEqual(result.target, { ...previous.target, revision: previous.target.revision + 1 });
  assert.equal(result.fileName, previous.fileName);
  assert.equal(result.dirty, true);
  assert.equal(result.savedRevision, previous.savedRevision);
  assert.equal(result.lifecycleRevision, previous.lifecycleRevision + 1);
  assert.deepEqual(result.history, {
    revision: result.target.revision,
    canUndo: true,
    canRedo: false,
  });
}

const stateOnly = ({ composition, compositions, ...state }) => state;

test('Inspect reuses the project snapshot until owner or revision changes', async (t) => {
  const snap = CompositionSessionOwner.prototype.projectSnapshot;
  const calls = t.mock.method(CompositionSessionOwner.prototype, 'projectSnapshot', function () {
    return snap.call(this);
  });
  const project = projectFixture();
  const other = projectFixture();
  other.compositions[0].name = 'Fresh owner';
  const request = direct({
    open: async (fileName) => ({
      contents: JSON.stringify(fileName === 'project.json' ? project : other),
      binding: { fileName, token: fileName },
    }),
  });
  const opened = await request('open', { target: null, fileName: 'project.json' });
  const afterOpen = calls.mock.callCount();
  for (let i = 0; i < 3; i++) assert.equal((await request('inspect')).composition.name, sharedName);
  assert.equal(calls.mock.callCount(), afterOpen, 'Inspect must not clone unchanged project');

  const edited = await request(rename, {
    target: opened.target,
    compositionId: activeId,
    name: activeName,
  });
  assert.equal((await request('inspect')).composition.name, activeName);
  const undone = await request('undo', { target: edited.target });
  assert.equal((await request('inspect')).composition.name, sharedName);
  const replaced = await request('open', { target: undone.target, fileName: 'other.json' });
  const freshInspect = await request('inspect');
  assert.deepEqual(freshInspect.target, replaced.target);
  assert.equal(freshInspect.composition.name, 'Fresh owner');
});

test('direct API validates rename, preserves binding through one inactive edit, and keeps deferred Redo', async () => {
  const initial = projectFixture();
  const binding = { fileName: 'project.json', token: 'bound' };
  let bytes = Buffer.from(JSON.stringify(initial));
  let writes = 0;
  const writing = Promise.withResolvers();
  const releases = [];
  const request = direct({
    open: async () => ({ binding, contents: bytes.toString('utf8') }),
    writeProject: async ({ contents, binding: live }) => {
      assert.equal(live, binding);
      writes++;
      await writing.promise;
      bytes = Buffer.from(contents);
      return binding;
    },
    release: (unused) => releases.push(unused),
  });
  const schema = agentWorkflowTools().find((tool) => tool.name === `nect_${rename}`).inputSchema;
  assert.deepEqual(schema.required, ['target', 'compositionId', 'name']);
  assert.equal(schema.additionalProperties, false);

  const empty = await request('inspect');
  await request(
    rename,
    { target: noTarget, compositionId: inactiveId, name: inactiveName },
    'NO_DOCUMENT',
  );
  assert.deepEqual(await request('inspect'), empty);
  await request('open', { target: null, fileName: binding.fileName });
  const before = await request('inspect');
  assert.equal(before.composition.id, activeId);
  assert.equal(before.composition.name, sharedName);

  const args = { target: before.target, compositionId: inactiveId, name: inactiveName };
  for (const replacement of [
    { name: undefined },
    { name: null },
    { name: 123 },
    { name: '' },
    { name: ' \u3000\t\n' },
    { compositionId: undefined },
    { compositionId: '' },
    { target: undefined },
    { target: { ...args.target, extra: true } },
    { expectedName: sharedName },
  ]) {
    await request(rename, { ...args, ...replacement }, 'INVALID_ARGUMENT');
    assert.deepEqual(await request('inspect'), before);
  }
  await request(
    rename,
    {
      ...args,
      get name() {
        assert.fail('Rename input getter must not run');
      },
    },
    'INVALID_ARGUMENT',
  );
  await request(rename, { ...args, compositionId: sharedName }, 'UNKNOWN_COMPOSITION');
  await request(
    rename,
    { ...args, target: { ...args.target, bindingRevision: 1 } },
    'STALE_TARGET',
  );
  // The private product-model limit counts UTF-16 code units, not code points.
  await request(rename, { ...args, name: '🎨'.repeat(50_001) }, 'EDIT_REJECTED');
  assert.deepEqual(await request('inspect'), before);

  const edited = await request(rename, args);
  changedRename(edited, before);
  const editedInfo = await request('inspect');
  assert.deepEqual(editedInfo.composition, before.composition);
  assert.deepEqual(await request(rename, { ...args, target: edited.target }), {
    ...edited,
    changed: false,
  });
  await request(rename, args, 'STALE_TARGET');
  assert.deepEqual(await request('inspect'), editedInfo);

  const undone = await request('undo', { target: edited.target });
  assert.equal(undone.dirty, false);
  assert.deepEqual(undone.history, { revision: 2, canUndo: false, canRedo: true });
  const undoInfo = await request('inspect');
  assert.deepEqual(await request(rename, { ...args, target: undone.target, name: sharedName }), {
    ...stateOnly(undoInfo),
    changed: false,
  });
  assert.deepEqual(await request('inspect'), undoInfo);
  await request('undo', { target: undone.target }, 'NOT_APPLICABLE');
  const redone = await request('redo', { target: undone.target });
  assert.deepEqual(redone.history, { revision: 3, canUndo: true, canRedo: false });
  assert.equal(writes, 0);
  assert.deepEqual(JSON.parse(bytes), initial);
  const save = request('save', { target: redone.target });
  try {
    await request(rename, { ...args, target: redone.target, name: 'During Save' }, 'BUSY');
  } finally {
    writing.resolve();
  }
  assert.deepEqual((await save).target, redone.target);
  assert.equal(writes, 1);
  assert.deepEqual(releases, []);
  const expected = structuredClone(initial);
  expected.compositions[1].name = inactiveName;
  assert.deepEqual(JSON.parse(bytes), expected);
});

test('direct API treats exact legacy blank labels as unchanged, not as blank replacements', async () => {
  const project = projectFixture();
  project.compositions[0].name = '';
  project.compositions[1].name = ' \u3000';
  const request = direct({
    open: async () => ({ contents: JSON.stringify(project), binding: { fileName: 'legacy.json' } }),
  });
  const opened = await request('open', { target: null, fileName: 'legacy.json' });
  for (const { id, name } of project.compositions) {
    const args = { target: opened.target, compositionId: id, name };
    assert.deepEqual(await request(rename, args), { ...opened, changed: false });
    await request(rename, { ...args, name: '\t' }, 'INVALID_ARGUMENT');
  }
  assert.deepEqual(stateOnly(await request('inspect')), opened);
});

const cli = fileURLToPath(new URL('../cli/agent-workflow.js', import.meta.url));

async function startMcp(root, cleanup) {
  const child = spawn(process.execPath, [cli, '--workspace', root], { windowsHide: true });
  child.stdin.on('error', () => {}); // EOF/exit reports failures with stderr below.
  const closed = once(child, 'close');
  let stderr = '';
  child.stderr.setEncoding('utf8').on('data', (chunk) => (stderr += chunk));
  cleanup.push(async () => {
    child.kill();
    await closed;
  });
  const lines = createInterface({ input: child.stdout })[Symbol.asyncIterator]();
  const send = (message) =>
    child.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', ...message })}\n`);
  let id = 0;
  async function rpc(method, params, frameTooLarge = false) {
    send({ id: ++id, method, params });
    const { done, value } = await lines.next();
    assert.equal(done, false, stderr);
    const reply = JSON.parse(value);
    assert.equal(reply.jsonrpc, '2.0');
    assert.equal(reply.id, frameTooLarge ? null : id);
    return reply;
  }
  const protocolVersion = '2025-06-18';
  const hello = await rpc('initialize', {
    protocolVersion,
    capabilities: {},
    clientInfo: { name: 'composition-rename-test', version: '1' },
  });
  assert.equal(hello.result.protocolVersion, protocolVersion);
  send({ method: 'notifications/initialized' });
  const call = (name, args) => ({ name: `nect_${name}`, arguments: args });
  return {
    async request(name, args, code) {
      const { result } = await rpc('tools/call', call(name, args));
      assert.equal(result.isError, !!code);
      assert.equal(result.content.length, 1);
      assert.equal(result.content[0].type, 'text');
      return toolResult(JSON.parse(result.content[0].text), code);
    },
    async oversizedRename(args) {
      const { error } = await rpc('tools/call', call(rename, args), true);
      assert.equal(error.code, -32700);
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
  'real stdio rename preserves graph and SVG, saves exact Unicode labels, and survives cold Open',
  { timeout: 30_000 },
  async (t) => {
    const parent = await fs.realpath(await fs.mkdtemp(path.join(tmpdir(), 'nect-rename-test-')));
    const root = path.join(parent, 'workspace');
    await fs.mkdir(root);
    const cleanup = [];
    t.after(async () => {
      for (const stop of cleanup) await stop();
      await fs.rm(parent, { recursive: true, force: true });
    });
    const file = (name) => path.join(root, name);
    const fileName = 'project.json';
    const initial = projectFixture();
    const originalBytes = Buffer.from(JSON.stringify(initial));
    await fs.writeFile(file(fileName), originalBytes);
    const mcp = await startMcp(root, cleanup);
    const { request } = mcp;
    const blank = await request('inspect');
    await request(
      rename,
      { target: noTarget, compositionId: activeId, name: activeName },
      'NO_DOCUMENT',
    );
    assert.deepEqual(await request('inspect'), blank);
    await request('open', { target: null, fileName });
    const before = await request('inspect');
    assert.equal(before.composition.id, activeId);
    assert.equal(before.composition.name, sharedName);
    const args = { target: before.target, compositionId: activeId, name: activeName };
    for (const [replacement, code] of [
      [{ name: ' \u3000\n' }, 'INVALID_ARGUMENT'],
      [{ name: '' }, 'INVALID_ARGUMENT'],
      [{ compositionId: 'does-not-exist' }, 'UNKNOWN_COMPOSITION'],
      [{ compositionId: sharedName }, 'UNKNOWN_COMPOSITION'],
      [{ target: { ...args.target, revision: 1 } }, 'STALE_TARGET'],
    ]) {
      await request(rename, { ...args, ...replacement }, code);
      assert.deepEqual(await request('inspect'), before);
    }
    await mcp.oversizedRename({ ...args, name: '界'.repeat(5500) });
    assert.deepEqual(await request('inspect'), before);

    const exportSvg = async (call, target, leaf) => {
      await call('export_svg', { target, fileName: leaf });
      return fs.readFile(file(leaf));
    };
    const svgBytes = await exportSvg(request, before.target, 'before.svg');
    const Parser = new Window().DOMParser;
    const svg = new Parser().parseFromString(svgBytes.toString('utf8'), 'image/svg+xml');
    assert.equal(svg.documentElement.getAttribute('viewBox'), '0 0 377.5 253.125');
    assert.deepEqual(
      [...svg.querySelectorAll('path')].map((p) => p.getAttribute('d')),
      ['M70,240v-220h180v220z'],
    );

    const active = await request(rename, args);
    changedRename(active, before);
    const activeComposition = { ...before.composition, name: activeName };
    assert.deepEqual((await request('inspect')).composition, activeComposition);
    assert.deepEqual(await exportSvg(request, active.target, 'active.svg'), svgBytes);
    assert.deepEqual(await fs.readFile(file(fileName)), originalBytes);
    const saved = await request('save', { target: active.target });
    const oneLabel = structuredClone(initial);
    oneLabel.compositions[0].name = activeName;
    const oneLabelBytes = await fs.readFile(file(fileName));
    assert.deepEqual(JSON.parse(oneLabelBytes), oneLabel);

    const inactive = await request(rename, {
      target: saved.target,
      compositionId: inactiveId,
      name: inactiveName,
    });
    changedRename(inactive, saved);
    assert.deepEqual((await request('inspect')).composition, activeComposition);
    assert.deepEqual(await exportSvg(request, inactive.target, 'inactive.svg'), svgBytes);
    assert.deepEqual(await fs.readFile(file(fileName)), oneLabelBytes);

    const undone = await request('undo', { target: inactive.target });
    assert.equal(undone.dirty, false);
    assert.deepEqual(undone.history, { revision: 3, canUndo: true, canRedo: true });
    const undoInfo = await request('inspect');
    assert.deepEqual(
      await request(rename, { target: undone.target, compositionId: inactiveId, name: sharedName }),
      { ...stateOnly(undoInfo), changed: false },
    );
    assert.deepEqual(await request('inspect'), undoInfo);
    const bothUndone = await request('undo', { target: undone.target });
    assert.deepEqual(bothUndone.history, { revision: 4, canUndo: false, canRedo: true });
    assert.deepEqual((await request('inspect')).composition, before.composition);
    const activeRedone = await request('redo', { target: bothUndone.target });
    assert.equal(activeRedone.dirty, false);
    assert.deepEqual((await request('inspect')).composition, activeComposition);
    const redone = await request('redo', { target: activeRedone.target });
    assert.deepEqual(redone.history, { revision: 6, canUndo: true, canRedo: false });
    const finalSave = await request('save', { target: redone.target });
    const bothLabels = structuredClone(oneLabel);
    bothLabels.compositions[1].name = inactiveName;
    const savedBytes = await fs.readFile(file(fileName));
    assert.notDeepEqual(savedBytes, oneLabelBytes);
    assert.deepEqual(JSON.parse(savedBytes), bothLabels);
    await mcp.stop();

    const cold = await startMcp(root, cleanup);
    assert.deepEqual(await cold.request('inspect'), blank);
    const reopened = await cold.request('open', { target: null, fileName });
    assert.notEqual(reopened.target.sessionId, finalSave.target.sessionId);
    assert.deepEqual(reopened.history, { revision: 0, canUndo: false, canRedo: false });
    assert.equal(reopened.dirty, false);
    assert.deepEqual((await cold.request('inspect')).composition, activeComposition);
    await cold.request(rename, { ...args, target: finalSave.target }, 'STALE_TARGET');
    assert.deepEqual(await exportSvg(cold.request, reopened.target, 'reopened.svg'), svgBytes);
    await cold.request('save_as', { target: reopened.target, fileName: 'reopened.json' });
    assert.deepEqual(await fs.readFile(file('reopened.json')), savedBytes);
    assert.deepEqual(await fs.readFile(file(fileName)), savedBytes);
    await cold.stop();
  },
);
