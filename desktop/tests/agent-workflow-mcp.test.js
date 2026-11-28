import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { once } from 'node:events';
import fs from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import { crc32, deflateSync, inflateSync } from 'node:zlib';
import { Window } from 'happy-dom';
import { createAgentWorkflowFiles } from '../cli/agent-workflow-files.js';
import { createAgentWorkflow } from '../src/agent-workflow.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { blankDocument, blankLayer, node } from '../src/core.js';

const cli = fileURLToPath(new URL('../cli/agent-workflow.js', import.meta.url));
const Parser = new Window().DOMParser;
const initialized = { jsonrpc: '2.0', method: 'notifications/initialized' };
const hello = {
  protocolVersion: '2025-06-18',
  capabilities: {},
  clientInfo: { name: 'workflow-test', version: '1' },
};

async function fixture(t) {
  const parent = await fs.realpath(await fs.mkdtemp(path.join(tmpdir(), 'nect-workflow-test-')));
  const root = path.join(parent, 'workspace');
  await fs.mkdir(root);
  const children = new Set();
  t.after(async () => {
    for (const child of children) {
      child.kill();
      await once(child, 'close');
    }
    await fs.rm(parent, { recursive: true, force: true });
  });
  function track(child) {
    children.add(child);
    child.once('close', () => children.delete(child));
    return child;
  }
  function launch(chromiumPath) {
    const args = chromiumPath ? ['--chromium-path', chromiumPath] : [];
    const child = track(
      spawn(process.execPath, [cli, '--workspace', root, ...args], { windowsHide: true }),
    );
    child.stdin.on('error', () => {}); // An early exit is reported by receive/close with stderr.
    const closed = once(child, 'close');
    const lines = createInterface({ input: child.stdout })[Symbol.asyncIterator]();
    let id = 0,
      stderr = '';
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => (stderr += chunk));
    const send = (message) => child.stdin.write(`${JSON.stringify(message)}\n`);
    const raw = (bytes) => child.stdin.write(bytes);
    const receive = async () => {
      const line = await lines.next();
      assert.equal(line.done, false, `Unexpected EOF: ${stderr}`);
      return JSON.parse(line.value);
    };
    const rpc = async (method, params) => {
      send({ jsonrpc: '2.0', id: ++id, method, params });
      const reply = await receive();
      assert.equal(reply.id, id);
      assert.equal(reply.jsonrpc, '2.0');
      return reply;
    };
    const init = async () => {
      const { result } = await rpc('initialize', hello);
      assert.equal(result.protocolVersion, hello.protocolVersion);
      assert.deepEqual(result.capabilities, { tools: {} });
      send(initialized);
    };
    const tool = async (name, args, error) => {
      const { result } = await rpc('tools/call', { name: `nect_${name}`, arguments: args });
      assert.equal(result.content.length, 1);
      assert.equal(result.content[0].type, 'text');
      const reply = JSON.parse(result.content[0].text);
      assert.equal(result.isError, !!error, JSON.stringify(reply));
      assert.equal(reply.ok, !error, JSON.stringify(reply));
      if (error) {
        assert.equal(reply.error.code, error, JSON.stringify(reply));
        return reply.error;
      }
      return reply.result;
    };
    const stop = async () => {
      child.stdin.end();
      assert.deepEqual(await closed, [0, null], stderr);
      assert.equal(stderr, '');
      assert.equal(
        (await lines.next()).done,
        true,
        'unexpected notification response or stdout log',
      );
    };
    return { send, raw, receive, rpc, init, tool, stop };
  }
  return { root, parent, launch, track, file: (leaf) => path.join(root, leaf) };
}

async function svgFile(filename) {
  const bytes = await fs.readFile(filename);
  const svg = new Parser().parseFromString(bytes.toString('utf8'), 'image/svg+xml');
  assert.equal(svg.documentElement.localName, 'svg');
  return { bytes, svg: svg.documentElement, paths: [...svg.querySelectorAll('path')] };
}

const projectFile = async (filename) => {
  const bytes = await fs.readFile(filename);
  const project = JSON.parse(bytes);
  assert.ok(project.compositionSchemaVersion >= 1);
  const [comp] = project.compositions;
  assert.equal(comp.width, 377.5);
  assert.equal(comp.height, 253.125);
  return { bytes, project, comp };
};

test(
  'local MCP startup authorization, handshake, discovery and bounded frame recovery',
  { timeout: 20_000 },
  async (t) => {
    const { root, parent, launch } = await fixture(t);
    const junction = path.join(parent, 'alias');
    await fs.symlink(root, junction, process.platform === 'win32' ? 'junction' : 'dir');
    for (const [args, code] of [
      [[], 'USAGE'],
      [['--workspace', 'relative'], 'UNSAFE_PATH'],
      [['--workspace', '//remote/share'], 'UNSAFE_PATH'],
      [['--workspace', path.join(parent, 'absent')], 'NOT_FOUND'],
      [['--workspace', junction], 'UNSAFE_WORKSPACE'],
    ]) {
      const result = spawnSync(process.execPath, [cli, ...args], {
        input: '',
        encoding: 'utf8',
        timeout: 5000,
        windowsHide: true,
      });
      assert.equal(result.status, 1, result.stderr);
      assert.equal(result.stdout, '');
      assert.equal(JSON.parse(result.stderr).error.code, code);
    }
    const p = launch();
    assert.deepEqual((await p.rpc('ping')).result, {});
    assert.equal((await p.rpc('tools/list')).error.code, -32002);
    assert.equal((await p.rpc('initialize', {})).error.code, -32602);
    await p.init();
    const {
      result: { tools },
    } = await p.rpc('tools/list');
    assert.deepEqual(
      tools.map(({ name }) => name),
      [
        'inspect',
        'new',
        'open',
        'create_rectangle',
        'move_artwork',
        'rename_composition',
        'switch_composition',
        'undo',
        'redo',
        'save',
        'save_as',
        'export_svg',
        'export_png',
      ].map((name) => `nect_${name}`),
    );
    const newSchema = tools.find(({ name }) => name === 'nect_new').inputSchema;
    assert.deepEqual(newSchema.required, ['target', 'width', 'height']);
    assert.deepEqual(newSchema.properties.width, {
      type: 'number',
      exclusiveMinimum: 0,
      maximum: 100000,
    });
    assert.equal(newSchema.additionalProperties, false);
    assert.ok(
      !Object.hasOwn(
        tools.find(({ name }) => name === 'nect_open').inputSchema.properties,
        'width',
      ),
    );
    assert.equal((await p.rpc('tools/call', { name: 'dispatch' })).error.code, -32602);
    assert.equal(
      (await p.rpc('tools/call', { name: 'nect_inspect', arguments: [] })).error.code,
      -32602,
    );
    assert.equal((await p.rpc('evaluate', {})).error.code, -32601);
    p.raw('{\n');
    assert.equal((await p.receive()).error.code, -32700);
    p.raw(Buffer.from([0xff, 10]));
    assert.equal((await p.receive()).error.code, -32700);
    p.raw(`${'a'.repeat(16_384)}`);
    p.raw('b\n');
    assert.equal((await p.receive()).error.code, -32700);
    const control = await p.tool('inspect');
    assert.deepEqual(control, {
      target: null,
      fileName: null,
      dirty: false,
      savedRevision: null,
      lifecycleRevision: 0,
      history: null,
      compositions: [],
      composition: null,
    });
    await p.stop();
  },
);

test(
  'real stdio workflow: New → graph edit and render → history → Save/Save As → cold Open and SVG',
  { timeout: 30_000 },
  async (t) => {
    const { file, launch } = await fixture(t);
    const p = launch();
    await p.init();
    const control = await p.tool('inspect');
    const dims = { width: 377.5, height: 253.125 };
    await p.tool('open', { target: null, fileName: 'missing.json' }, 'NOT_FOUND');
    await p.tool(
      'create_rectangle',
      { target: { sessionId: 'absent', revision: 0, bindingRevision: 0 }, x: 10, y: 10 },
      'NO_DOCUMENT',
    );
    for (const [dimension, value] of [
      ['width', undefined],
      ['height', undefined],
      ['width', '377.5'],
      ['height', '253.125'],
      ['width', 0],
      ['height', -1],
      ['width', 100001],
      ['height', null],
    ]) {
      await p.tool('new', { target: null, ...dims, [dimension]: value }, 'INVALID_ARGUMENT');
      assert.deepEqual(await p.tool('inspect'), control);
    }
    p.raw(
      '{"jsonrpc":"2.0","id":"overflow","method":"tools/call","params":{"name":"nect_new","arguments":{"target":null,"width":1e999,"height":8}}}\n',
    );
    const overflow = await p.receive();
    assert.equal(overflow.id, 'overflow');
    assert.equal(JSON.parse(overflow.result.content[0].text).error.code, 'INVALID_ARGUMENT');
    assert.deepEqual(await p.tool('inspect'), control);

    const blank = await p.tool('new', { target: null, ...dims });
    assert.equal(blank.target.revision, 0);
    assert.equal(blank.dirty, true);
    const blankInfo = await p.tool('inspect');
    assert.equal(blankInfo.composition.width, dims.width);
    assert.equal(blankInfo.composition.height, dims.height);
    await p.tool('undo', { target: blank.target }, 'NOT_APPLICABLE');
    await p.tool(
      'move_artwork',
      { target: blank.target, layerId: blankInfo.composition.layers[0].id, dx: 5, dy: 3 },
      'EDIT_REJECTED',
    );
    await p.tool('save', { target: blank.target }, 'NAME_REQUIRED');
    await fs.writeFile(file('occupied.json'), 'existing bytes');
    await p.tool('save', { target: blank.target, fileName: 'occupied.json' }, 'OUTPUT_EXISTS');
    assert.equal(await fs.readFile(file('occupied.json'), 'utf8'), 'existing bytes');
    await p.tool('export_svg', { target: blank.target, fileName: 'blank.svg' });
    assert.equal((await svgFile(file('blank.svg'))).paths.length, 0);
    assert.deepEqual(await p.tool('inspect'), blankInfo);

    const edit = await p.tool('create_rectangle', {
      target: blank.target,
      x: 160,
      y: 130,
      name: 'Proof rectangle',
    });
    assert.equal(edit.changed, true);
    assert.equal(edit.target.revision, 1);
    const artwork = (await p.tool('inspect')).composition.layers.at(-1);
    assert.equal(artwork.name, 'Proof rectangle');
    assert.equal(artwork.id, edit.layerId);
    const [rectangle, fill, output] = artwork.nodes;
    assert.equal(rectangle.id, edit.nodeId);
    assert.deepEqual(
      artwork.nodes.map((n) => n.type),
      ['Rectangle', 'Fill', 'Output'],
    );
    assert.equal(fill.pathSourceId, rectangle.id);
    assert.equal(output.pathSourceId, fill.id);
    assert.equal(output.id, artwork.outputId);
    await p.tool('create_rectangle', { target: blank.target, x: 0, y: 0 }, 'STALE_TARGET');
    await p.tool('new', { target: null, ...dims, discard: true }, 'STALE_TARGET');
    await p.tool('export_svg', { target: edit.target, fileName: 'rect.svg' });
    const rendered = await svgFile(file('rect.svg'));
    assert.equal(rendered.svg.getAttribute('viewBox'), '0 0 377.5 253.125');
    assert.equal(rendered.svg.getAttribute('width'), '377.5');
    assert.equal(rendered.svg.getAttribute('height'), '253.125');
    assert.equal(rendered.paths.length, 1);
    assert.equal(rendered.paths[0].getAttribute('d'), 'M70,240v-220h180v220z');

    const undone = await p.tool('undo', { target: edit.target });
    assert.equal(undone.target.revision, 2);
    assert.deepEqual(undone.history, { revision: 2, canUndo: false, canRedo: true });
    assert.deepEqual((await p.tool('inspect')).composition.layers, blankInfo.composition.layers);
    await p.tool('export_svg', { target: undone.target, fileName: 'undone.svg' });
    assert.deepEqual(
      (await svgFile(file('undone.svg'))).bytes,
      (await svgFile(file('blank.svg'))).bytes,
    );
    const redone = await p.tool('redo', { target: undone.target });
    assert.equal(redone.target.revision, 3);
    assert.deepEqual((await p.tool('inspect')).composition.layers.at(-1), artwork);
    await p.tool('export_svg', { target: redone.target, fileName: 'redone.svg' });
    assert.deepEqual((await svgFile(file('redone.svg'))).bytes, rendered.bytes);
    const saved = await p.tool('save', { target: redone.target, fileName: 'original.json' });
    assert.equal(saved.dirty, false);
    assert.equal(saved.savedRevision, 3);
    assert.deepEqual(saved.history, redone.history);
    const first = await projectFile(file('original.json'));
    assert.deepEqual(first.comp.layers.at(-1).nodes[0].params.center.value, [160, 130]);

    const moved = await p.tool('move_artwork', {
      target: saved.target,
      layerId: artwork.id,
      dx: 20,
      dy: 2,
    });
    assert.equal(moved.target.revision, 4);
    assert.equal(moved.dirty, true);
    const cleans = await p.tool('undo', { target: moved.target });
    assert.equal(cleans.dirty, false);
    assert.equal(cleans.savedRevision, saved.savedRevision);
    const movesAgain = await p.tool('redo', { target: cleans.target });
    assert.equal(movesAgain.dirty, true);
    const bound = await p.tool('save', { target: movesAgain.target });
    assert.equal(bound.fileName, 'original.json');
    assert.equal(bound.dirty, false);
    const beforeAs = await projectFile(file('original.json'));
    assert.notDeepEqual(beforeAs.bytes, first.bytes);
    assert.deepEqual(beforeAs.comp.layers.at(-1).nodes[0].params.center.value, [180, 132]);
    const last = await p.tool('move_artwork', {
      target: bound.target,
      layerId: artwork.id,
      dx: 24,
      dy: -3,
    });
    assert.equal(last.dirty, true);
    await p.tool('export_svg', { target: last.target, fileName: 'unsaved.svg' });
    const unsaved = await svgFile(file('unsaved.svg'));
    assert.equal(unsaved.paths[0].getAttribute('d'), 'M114,239v-220h180v220z');
    assert.deepEqual((await projectFile(file('original.json'))).bytes, beforeAs.bytes);

    const copy = await p.tool('save_as', { target: last.target, fileName: 'copy.json' });
    assert.equal(copy.fileName, 'copy.json');
    assert.equal(copy.dirty, false);
    assert.equal(copy.savedRevision, last.lifecycleRevision);
    assert.deepEqual(copy.history, last.history);
    assert.equal(copy.target.revision, last.target.revision);
    assert.equal(copy.target.bindingRevision, last.target.bindingRevision + 1);
    await p.tool('save', { target: last.target }, 'STALE_TARGET');
    assert.deepEqual((await projectFile(file('original.json'))).bytes, beforeAs.bytes);
    const copied = await projectFile(file('copy.json'));
    assert.deepEqual(copied.comp.layers.at(-1).nodes[0].params.center.value, [204, 129]);
    assert.equal(copied.comp.layers.at(-1).nodes[0].id, edit.nodeId);
    await p.stop();

    const cold = launch();
    await cold.init();
    assert.deepEqual(await cold.tool('inspect'), control);
    await cold.tool('open', { target: null, fileName: 'copy.json', width: 20 }, 'INVALID_ARGUMENT');
    const reopened = await cold.tool('open', { target: null, fileName: 'copy.json' });
    assert.equal(reopened.target.revision, 0);
    assert.notEqual(reopened.target.sessionId, copy.target.sessionId);
    assert.equal(reopened.dirty, false);
    assert.equal(reopened.savedRevision, 0);
    assert.equal(reopened.fileName, 'copy.json');
    assert.deepEqual(reopened.history, { revision: 0, canUndo: false, canRedo: false });
    const coldInfo = await cold.tool('inspect');
    assert.deepEqual(
      [coldInfo.composition.width, coldInfo.composition.height],
      [dims.width, dims.height],
    );
    assert.deepEqual(coldInfo.composition.layers.at(-1), artwork);
    await cold.tool('undo', { target: reopened.target }, 'NOT_APPLICABLE');
    await cold.tool('redo', { target: reopened.target }, 'NOT_APPLICABLE');
    await cold.tool('export_svg', { target: reopened.target, fileName: 'reopened.svg' });
    assert.deepEqual((await svgFile(file('reopened.svg'))).bytes, unsaved.bytes);
    assert.deepEqual(await cold.tool('inspect'), coldInfo);
    const originalOpened = await cold.tool('open', {
      target: reopened.target,
      fileName: 'original.json',
    });
    assert.equal(originalOpened.target.revision, 0);
    assert.notEqual(originalOpened.target.sessionId, reopened.target.sessionId);
    await cold.tool('create_rectangle', { target: reopened.target, x: 10, y: 20 }, 'STALE_TARGET');
    await cold.tool('export_svg', { target: originalOpened.target, fileName: 'old-original.svg' });
    assert.equal(
      (await svgFile(file('old-original.svg'))).paths[0].getAttribute('d'),
      'M90,242v-220h180v220z',
    );
    await cold.stop();
  },
);

test(
  'workspace refusals, dirty transition cancellation, and locked edits preserve bytes and history',
  { timeout: 20_000 },
  async (t) => {
    const { file, root, parent, launch } = await fixture(t);
    const p = launch();
    await p.init();
    const dims = { width: 377.5, height: 253.125 };
    const blank = await p.tool('new', { target: null, ...dims });
    const edit = await p.tool('create_rectangle', { target: blank.target, x: 160, y: 130 });
    const { target } = await p.tool('save', { target: edit.target, fileName: 'real.json' });
    assert.deepEqual(target, { ...edit.target, bindingRevision: 1 });
    const saved = await projectFile(file('real.json'));
    saved.comp.layers.at(-1).locked = true;
    await fs.writeFile(file('locked.json'), JSON.stringify(saved.project));
    await fs.writeFile(file('broken.json'), '{bad json');
    await fs.writeFile(file('invalid-utf8.json'), Buffer.from([0xff]));
    await fs.writeFile(file('oversized.json'), '');
    await fs.truncate(file('oversized.json'), 50_000_001);
    saved.comp.height = 100001;
    await fs.writeFile(file('invalid-size.json'), JSON.stringify(saved.project));
    await fs.link(file('real.json'), file('linked.json'));
    await fs.symlink(
      parent,
      file('junction.json'),
      process.platform === 'win32' ? 'junction' : 'dir',
    );
    await fs.mkdir(file('occupied.json'));
    await fs.writeFile(file('exists.svg'), 'preserve SVG');
    await fs.writeFile(file('exists.png'), 'preserve PNG');
    const moved = await p.tool('move_artwork', { target, layerId: edit.layerId, dx: 20, dy: 2 });
    const before = await p.tool('inspect');
    assert.equal(before.dirty, true);
    await p.tool('open', { target: moved.target, fileName: 'locked.json' }, 'DIRTY_DOCUMENT');
    await p.tool('new', { target: moved.target, ...dims }, 'DIRTY_DOCUMENT');
    await p.tool(
      'new',
      { target: moved.target, width: '', height: 5, discard: true },
      'INVALID_ARGUMENT',
    );
    for (const [fileName, code] of [
      ['broken.json', 'INVALID_PROJECT'],
      ['invalid-size.json', 'INVALID_PROJECT'],
      ['invalid-utf8.json', 'INVALID_PROJECT'],
      ['oversized.json', 'FILE_TOO_LARGE'],
      ['missing.json', 'NOT_FOUND'],
      ['linked.json', 'UNSAFE_FILE'],
      ['junction.json', 'UNSAFE_FILE'],
    ])
      await p.tool('open', { target: moved.target, fileName, discard: true }, code);
    for (const bad of [
      '../outside.json',
      'missing/target.json',
      '..\\outside.json',
      'file.json:ads',
      'lpt1.json',
      'con.json',
      'a.json\n',
      'a.json ',
      'a.svg',
      'file;echo.json',
    ])
      await p.tool('save_as', { target: moved.target, fileName: bad }, 'UNSAFE_NAME');
    for (const fileName of ['occupied.json', 'real.json', 'locked.json'])
      await p.tool('save_as', { target: moved.target, fileName }, 'OUTPUT_EXISTS');
    await p.tool('save', { target: moved.target, fileName: 'locked.json' }, 'BINDING_MISMATCH');
    await p.tool('export_svg', { target: moved.target, fileName: 'exists.svg' }, 'OUTPUT_EXISTS');
    await p.tool('export_png', { target: moved.target, fileName: 'exists.png' }, 'OUTPUT_EXISTS');
    await p.tool(
      'export_png',
      { target: moved.target, fileName: 'unavailable.png' },
      'PNG_UNAVAILABLE',
    );
    await assert.rejects(fs.lstat(file('unavailable.png')), { code: 'ENOENT' });
    assert.equal(await fs.readFile(file('exists.svg'), 'utf8'), 'preserve SVG');
    assert.equal(await fs.readFile(file('exists.png'), 'utf8'), 'preserve PNG');
    await p.tool('save', { target: moved.target }, 'BINDING_CHANGED'); // extra link to live binding
    await fs.unlink(file('linked.json'));
    await fs.writeFile(file('real.json'), 'external edit');
    await p.tool('save', { target: moved.target }, 'BINDING_CHANGED');
    assert.equal(await fs.readFile(file('real.json'), 'utf8'), 'external edit');
    await fs.unlink(file('real.json'));
    await p.tool('save', { target: moved.target }, 'BINDING_CHANGED');
    assert.deepEqual(await p.tool('inspect'), before);
    assert.deepEqual((await fs.readdir(parent)).sort(), ['workspace']);

    const locked = await p.tool('open', {
      target: moved.target,
      fileName: 'locked.json',
      discard: true,
    });
    assert.equal(locked.dirty, false);
    assert.equal(locked.target.revision, 0);
    const lockedBefore = await p.tool('inspect');
    assert.equal(lockedBefore.composition.layers.at(-1).locked, true);
    const rejection = await p.tool(
      'move_artwork',
      { target: locked.target, layerId: edit.layerId, dx: 30, dy: 10 },
      'EDIT_REJECTED',
    );
    assert.match(rejection.message, /unlock/i);
    assert.deepEqual(await p.tool('inspect'), lockedBefore);
    // An opened file with normal permissions remains a legitimate live binding.
    const savedOpen = await p.tool('save', { target: locked.target });
    assert.equal(savedOpen.fileName, 'locked.json');
    assert.deepEqual(savedOpen.history, locked.history);
    const wide = await p.tool('new', { target: locked.target, width: 100000, height: 10 });
    assert.equal((await p.tool('inspect')).composition.width, 100000);
    await p.tool('export_png', { target: wide.target, fileName: 'too-wide.png' }, 'RASTER_SIZE');
    await assert.rejects(fs.lstat(file('too-wide.png')), { code: 'ENOENT' });
    const wideInfo = await p.tool('inspect');
    await fs.rename(root, path.join(parent, 'moved-workspace'));
    await fs.mkdir(root);
    await p.tool('save', { target: wide.target, fileName: 'escape.json' }, 'UNSAFE_WORKSPACE');
    assert.deepEqual(await fs.readdir(root), []);
    assert.deepEqual(await p.tool('inspect'), wideInfo);
    await p.stop();
  },
);

test(
  'Windows live binding held with FileShare.None is not overwritten or marked saved',
  {
    skip: process.platform !== 'win32',
    timeout: 20_000,
  },
  async (t) => {
    const { file, launch, track } = await fixture(t);
    const p = launch();
    await p.init();
    const blank = await p.tool('new', { target: null, width: 377.5, height: 253.125 });
    const saved = await p.tool('save', { target: blank.target, fileName: 'busy.json' });
    const original = await fs.readFile(file('busy.json'));
    const edit = await p.tool('create_rectangle', { target: saved.target, x: 160, y: 130 });
    const before = await p.tool('inspect');
    // Windows libuv handles share delete; use the inbox .NET file API for a real
    // exclusive share mode. The script is fixed; the synthetic path is only data.
    const powershell = path.join(
      process.env.SystemRoot ?? 'C:\\Windows',
      'System32',
      'WindowsPowerShell',
      'v1.0',
      'powershell.exe',
    );
    const lock = track(
      spawn(
        powershell,
        [
          '-NoProfile',
          '-NonInteractive',
          '-Command',
          '$f = [IO.File]::Open($env:NECT_LOCK_FIXTURE, "Open", "Read", "None"); try { [Console]::WriteLine("locked"); [void][Console]::ReadLine() } finally { $f.Dispose() }',
        ],
        { windowsHide: true, env: { ...process.env, NECT_LOCK_FIXTURE: file('busy.json') } },
      ),
    );
    const closed = once(lock, 'close');
    const lines = createInterface({ input: lock.stdout })[Symbol.asyncIterator]();
    assert.equal((await lines.next()).value, 'locked');
    p.send({
      jsonrpc: '2.0',
      id: 'busy-save',
      method: 'tools/call',
      params: { name: 'nect_save', arguments: { target: edit.target } },
    });
    const reply = await p.receive();
    assert.equal(reply.id, 'busy-save');
    assert.equal(reply.result.isError, true);
    assert.match(JSON.parse(reply.result.content[0].text).error.code, /^FILE_(BUSY|INACCESSIBLE)$/);
    assert.deepEqual(await p.tool('inspect'), before);
    lock.stdin.end('\n');
    assert.deepEqual(await closed, [0, null]);
    assert.deepEqual(await fs.readFile(file('busy.json')), original);
    const retried = await p.tool('save', { target: edit.target });
    assert.equal(retried.dirty, false);
    assert.notDeepEqual(await fs.readFile(file('busy.json')), original);
    await p.stop();
  },
);

// Decode the small Chromium RGBA fixture independently of Nect/raster.js.
function readRgbaPng(bytes) {
  assert.equal(bytes.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
  assert.equal(bytes.readUInt32BE(8), 13);
  assert.equal(bytes.toString('ascii', 12, 16), 'IHDR');
  const width = bytes.readUInt32BE(16),
    height = bytes.readUInt32BE(20);
  assert.deepEqual([...bytes.subarray(24, 29)], [8, 6, 0, 0, 0], 'noninterlaced RGBA8');
  const idat = [];
  let end = 0;
  for (let offset = 8; offset + 12 <= bytes.length; offset = end) {
    const size = bytes.readUInt32BE(offset);
    end = offset + 12 + size;
    assert.ok(end <= bytes.length);
    const type = bytes.toString('ascii', offset + 4, offset + 8);
    if (type === 'IDAT') idat.push(bytes.subarray(offset + 8, offset + 8 + size));
    if (type === 'IEND') {
      assert.equal(size, 0);
      break;
    }
  }
  assert.equal(end, bytes.length);
  const stride = 4 * width;
  const filtered = inflateSync(Buffer.concat(idat), { maxOutputLength: (stride + 1) * height });
  assert.equal(filtered.length, (stride + 1) * height);
  const pixels = Buffer.alloc(stride * height);
  const paeth = (left, up, upperLeft) => {
    const value = left + up - upperLeft;
    const a = Math.abs(value - left),
      b = Math.abs(value - up),
      c = Math.abs(value - upperLeft);
    return a <= b && a <= c ? left : b <= c ? up : upperLeft;
  };
  for (let y = 0; y < height; y++) {
    const row = y * (stride + 1),
      filter = filtered[row];
    assert.ok(filter <= 4);
    for (let x = 0; x < stride; x++) {
      const index = y * stride + x;
      const left = x >= 4 ? pixels[index - 4] : 0;
      const up = y ? pixels[index - stride] : 0;
      const upperLeft = y && x >= 4 ? pixels[index - stride - 4] : 0;
      const prediction = [0, left, up, (left + up) >> 1, paeth(left, up, upperLeft)][filter];
      pixels[index] = (filtered[row + 1 + x] + prediction) & 255;
    }
  }
  const pixel = (x, y) => [...pixels.subarray(y * stride + x * 4, y * stride + x * 4 + 4)];
  return { width, height, pixels, pixel };
}

const chromiumPath = process.env.NECT_WORKFLOW_CHROMIUM;
const imageTestOptions = {
  skip: !chromiumPath && 'Set NECT_WORKFLOW_CHROMIUM to installed Chromium for real image decode',
  timeout: 270_000,
};

test(
  'existing headless raster: real PNG dimensions, artwork color and transparent exterior, before Save and after cold Open',
  {
    skip:
      !chromiumPath && 'Set NECT_WORKFLOW_CHROMIUM to installed Chromium for the real raster check',
    timeout: 205_000,
  },
  async (t) => {
    const { file, launch, root } = await fixture(t);
    const p = launch(chromiumPath);
    await p.init();
    const blank = await p.tool('new', { target: null, width: 377.5, height: 253.125 });
    const edit = await p.tool('create_rectangle', { target: blank.target, x: 160, y: 130 });
    const before = await p.tool('inspect');
    const result = await p.tool('export_png', { target: edit.target, fileName: 'current.png' });
    assert.deepEqual(result.output, {
      fileName: 'current.png',
      format: 'png',
      width: 378,
      height: 253,
    });
    assert.deepEqual(await fs.readdir(root), ['current.png']);
    const first = await fs.readFile(file('current.png'));
    const decoded = readRgbaPng(first);
    assert.deepEqual([decoded.width, decoded.height], [378, 253]);
    assert.deepEqual(decoded.pixel(160, 130), [54, 67, 78, 255]);
    assert.equal(decoded.pixel(1, 1)[3], 0);
    assert.equal(decoded.pixel(265, 130)[3], 0);
    await p.tool('export_png', { target: edit.target, fileName: 'current.png' }, 'OUTPUT_EXISTS');
    assert.deepEqual(await fs.readFile(file('current.png')), first);
    assert.deepEqual(await p.tool('inspect'), before);
    await p.tool('save', { target: edit.target, fileName: 'first.json' });
    await p.stop();

    const cold = launch(chromiumPath);
    await cold.init();
    const opened = await cold.tool('open', { target: null, fileName: 'first.json' });
    await cold.tool('export_png', { target: opened.target, fileName: 'reopened.png' });
    const reopened = readRgbaPng(await fs.readFile(file('reopened.png')));
    assert.deepEqual(reopened.pixels, decoded.pixels);
    assert.deepEqual((await cold.tool('inspect')).history, opened.history);
    await cold.stop();
  },
);

function pngChunk(tag, data) {
  const bytes = Buffer.alloc(12 + data.length);
  bytes.writeUInt32BE(data.length);
  bytes.write(tag, 4, 'ascii');
  data.copy(bytes, 8);
  bytes.writeUInt32BE(crc32(bytes.subarray(4, -4)), bytes.length - 4);
  return bytes;
}

const rgbaRow = Buffer.from([0, 255, 0, 0, 255, 0, 255, 128, 255]);
const rgbaRows = Buffer.concat([rgbaRow, rgbaRow, rgbaRow]);
const rgbaZlib = deflateSync(rgbaRows);
const idat = (data) => pngChunk('IDAT', data);
const emptyIdat = idat(Buffer.alloc(0));

function fixturePng({
  width = 2,
  height = 3,
  format = [8, 6, 0, 0, 0],
  chunks = [idat(rgbaZlib)],
} = {}) {
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width);
  header.writeUInt32BE(height, 4);
  header.set(format, 8);
  return Buffer.concat([
    Buffer.from('89504e470d0a1a0a', 'hex'),
    pngChunk('IHDR', header),
    ...chunks,
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

// The original witness: legal 2×1 RGBA IHDR, correct CRCs, and only an empty IDAT.
const emptyIdatPng = fixturePng({ height: 1, chunks: [emptyIdat] });

function indexedPng() {
  // Nine one-bit indices per row: two bytes plus the filter byte.
  const compressed = deflateSync(Buffer.from([0, 0xaa, 0x80, 0, 0x55, 0x00]));
  return fixturePng({
    width: 9,
    height: 2,
    format: [1, 3, 0, 0, 0],
    chunks: [
      pngChunk('PLTE', Buffer.from([255, 0, 0, 0, 255, 128])),
      idat(compressed.subarray(0, 1)),
      emptyIdat,
      idat(compressed.subarray(1)),
    ],
  });
}

function adam7Png() {
  // 7×5, gray and alpha (two 16-bit samples per pixel). Hard-code pass
  // row widths for this fixture rather than reusing the validator geometry.
  const widths = [1, 1, 2, 2, 2, 4, 3, 3, 3, 7, 7];
  const rows = widths.map((width, i) => {
    const row = Buffer.alloc(1 + 4 * width, 0xff);
    row[0] = i % 5;
    return row;
  });
  return fixturePng({
    width: 7,
    height: 5,
    format: [16, 4, 0, 0, 1],
    chunks: [idat(deflateSync(Buffer.concat(rows)))],
  });
}

function picturedProject(bytes, placement = 'active') {
  const doc = { ...blankDocument(), width: 311.5, height: 197.25 };
  const picture = blankLayer('Synthetic PNG'),
    image = node('Image'),
    output = node('Image Output');
  const asset = (data) => ({
    name: 'picture.png',
    mime: 'image/png',
    width: data.readUInt32BE(16),
    height: data.readUInt32BE(20),
    data: data.toString('base64'),
  });
  doc.imageAssets =
    placement === 'unused'
      ? { 'image-picture': asset(fixturePng()), 'image-unused': asset(bytes) }
      : { 'image-picture': asset(bytes) };
  image.asset = 'image-picture';
  image.params.position.value = [12.5, 23.25];
  image.params.size.value = [45.5, 67.25];
  output.params.path.source = image.id;
  picture.nodes = [image, output];
  picture.output = output.id;
  delete picture.allowEmptyOutput;
  if (placement !== 'inactive') doc.layers.push(picture);
  const project = migrateLegacyDocumentToCompositionProject(doc);
  if (placement === 'inactive')
    project.compositions.push({
      id: 'inactive',
      name: 'Second',
      width: 100,
      height: 100,
      layers: [picture],
    });
  return serializeCompositionProject(project);
}

async function workspaceBytes({ root, file }) {
  const names = (await fs.readdir(root)).sort();
  return Promise.all(names.map(async (name) => [name, await fs.readFile(file(name))]));
}

test(
  'stdio Open needs Chromium for global PNGs; valid Open and SVG preserve independent image bytes and geometry',
  imageTestOptions,
  verifyStdioImageRoundtrip,
);

async function verifyStdioImageRoundtrip(t) {
  const workspace = await fixture(t);
  const { file, launch, root } = workspace;
  const png = fixturePng();
  const source = Buffer.from(picturedProject(png));
  await fs.writeFile(file('picture.json'), source);

  const withoutBrowser = launch();
  await withoutBrowser.init();
  const blank = await withoutBrowser.tool('inspect');
  const unavailable = await withoutBrowser.tool(
    'open',
    { target: null, fileName: 'picture.json' },
    'IMAGE_VALIDATION_UNAVAILABLE',
  );
  assert.match(unavailable.message, /--chromium-path/);
  assert.deepEqual(await withoutBrowser.tool('inspect'), blank);
  assert.deepEqual(await workspaceBytes(workspace), [['picture.json', source]]);
  await withoutBrowser.stop();

  const p = launch(chromiumPath);
  await p.init();
  const opened = await p.tool('open', { target: null, fileName: 'picture.json' });
  const before = await p.tool('inspect');
  assert.equal(opened.fileName, 'picture.json');
  assert.equal(opened.dirty, false);
  assert.deepEqual(opened.history, { revision: 0, canUndo: false, canRedo: false });
  assert.deepEqual([before.composition.width, before.composition.height], [311.5, 197.25]);
  assert.deepEqual(
    before.composition.layers.at(-1).nodes.map(({ type }) => type),
    ['Image', 'Image Output'],
  );
  await p.tool('export_svg', { target: opened.target, fileName: 'picture.svg' });
  const embedded = await assertPictureSvg(file('picture.svg'), png);
  const decoded = readRgbaPng(embedded);
  assert.deepEqual([decoded.width, decoded.height], [2, 3]);
  assert.deepEqual(decoded.pixel(0, 0), [255, 0, 0, 255]);
  assert.deepEqual(decoded.pixel(1, 2), [0, 255, 128, 255]);
  assert.deepEqual(await p.tool('inspect'), before);
  assert.deepEqual(await fs.readFile(file('picture.json')), source);
  assert.deepEqual((await fs.readdir(root)).sort(), ['picture.json', 'picture.svg']);
  await p.stop();
}

async function assertPictureSvg(filename, png) {
  const { svg } = await svgFile(filename);
  assert.equal(svg.getAttribute('viewBox'), '0 0 311.5 197.25');
  const images = svg.querySelectorAll('image');
  assert.equal(images.length, 1);
  const [image] = images;
  assert.deepEqual(
    ['x', 'y', 'width', 'height', 'preserveAspectRatio'].map((key) => image.getAttribute(key)),
    ['12.5', '23.25', '45.5', '67.25', 'none'],
  );
  const data = /^data:image\/png;base64,([A-Za-z0-9+/=]+)$/.exec(image.getAttribute('href'));
  assert.ok(data);
  const embedded = Buffer.from(data[1], 'base64');
  assert.deepEqual(embedded, png);
  return embedded;
}

test(
  'stdio Open and SVG accept packed indexed split-IDAT and 16-bit Adam7 PNGs',
  imageTestOptions,
  async (t) => {
    const { file, launch } = await fixture(t);
    const p = launch(chromiumPath);
    await p.init();
    let target = null;
    for (const [name, png] of [
      ['indexed', indexedPng()],
      ['adam7', adam7Png()],
    ]) {
      const fileName = `${name}.json`;
      const source = Buffer.from(picturedProject(png));
      await fs.writeFile(file(fileName), source);
      const opened = await p.tool('open', { target, fileName });
      assert.equal(opened.fileName, fileName);
      assert.equal(opened.dirty, false);
      target = opened.target;
      await p.tool('export_svg', { target, fileName: `${name}.svg` });
      await assertPictureSvg(file(`${name}.svg`), png);
      assert.deepEqual(await fs.readFile(file(fileName)), source);
    }
    await p.stop();
  },
);

test(
  'stdio Open rejects invalid IDAT and format data, including unused/inactive assets, without changing dirty binding or history',
  imageTestOptions,
  verifyDirtyOpenRejections,
);

async function verifyDirtyOpenRejections(t) {
  const workspace = await fixture(t);
  const { file, launch } = workspace;
  const badFilter = Buffer.concat([rgbaRow, Buffer.from([5]), rgbaRow.subarray(1), rgbaRow]);
  const badChecksum = Buffer.from(rgbaZlib);
  badChecksum[badChecksum.length - 1] ^= 1;
  const malformedData = {
    'bad-zlib': Buffer.from('not zlib'),
    'truncated-zlib': rgbaZlib.subarray(0, -2), // Even if all rows can be inflated.
    'bad-adler': badChecksum,
    'short-row': deflateSync(rgbaRows.subarray(0, -1)),
    'bad-filter': deflateSync(badFilter),
    'extra-output': deflateSync(Buffer.concat([rgbaRows, Buffer.alloc(65_536)])),
    'trailing-junk': Buffer.concat([rgbaZlib, Buffer.of(0)]),
    'second-stream': Buffer.concat([rgbaZlib, deflateSync(Buffer.alloc(0))]),
  };
  const malformedPngs = {
    'bad-depth': fixturePng({ format: [4, 6, 0, 0, 0] }),
    'bad-compression': fixturePng({ format: [8, 6, 1, 0, 0] }),
    'bad-filter-method': fixturePng({ format: [8, 6, 0, 1, 0] }),
    'bad-interlace': fixturePng({ format: [8, 6, 0, 0, 2] }),
    'no-palette': fixturePng({
      height: 1,
      format: [1, 3, 0, 0, 0],
      chunks: [idat(deflateSync(Buffer.from([0, 0])))],
    }),
    'noncontiguous-idat': fixturePng({
      chunks: [
        idat(rgbaZlib.subarray(0, 4)),
        pngChunk('tEXt', Buffer.from('A\0b')),
        idat(rgbaZlib.subarray(4)),
      ],
    }),
  };
  for (const placement of ['active', 'unused', 'inactive'])
    await fs.writeFile(file(`${placement}.json`), picturedProject(emptyIdatPng, placement));
  for (const [name, compressed] of Object.entries(malformedData))
    await fs.writeFile(
      file(`${name}.json`),
      picturedProject(fixturePng({ chunks: [idat(compressed)] })),
    );
  for (const [name, png] of Object.entries(malformedPngs))
    await fs.writeFile(file(`${name}.json`), picturedProject(png));
  await fs.writeFile(file('previous.svg'), 'keep old SVG bytes');
  await fs.writeFile(file('sentinel.png'), Buffer.from([1, 2, 3, 0]));
  const p = launch(chromiumPath);
  await p.init();
  const blank = await p.tool('new', { target: null, width: 377.5, height: 253.125 });
  const saved = await p.tool('save', { target: blank.target, fileName: 'bound.json' });
  const rect = await p.tool('create_rectangle', { target: saved.target, x: 160, y: 130 });
  const moved = await p.tool('move_artwork', {
    target: rect.target,
    layerId: rect.layerId,
    dx: 20,
    dy: 2,
  });
  const undone = await p.tool('undo', { target: moved.target });
  const before = await p.tool('inspect');
  assert.equal(before.fileName, 'bound.json');
  assert.equal(before.dirty, true);
  assert.equal(before.history.canUndo, true);
  assert.equal(before.history.canRedo, true);
  const original = await workspaceBytes(workspace);
  for (const name of [
    'active',
    'unused',
    'inactive',
    ...Object.keys(malformedData),
    ...Object.keys(malformedPngs),
  ]) {
    const fileName = `${name}.json`;
    const rejected = await p.tool(
      'open',
      { target: undone.target, fileName, discard: true },
      'IMAGE_REJECTED',
    );
    assert.match(rejected.message, /repair or replace/i);
    assert.doesNotMatch(rejected.message, /[\\/]/);
    assert.deepEqual(await p.tool('inspect'), before);
    assert.deepEqual(await workspaceBytes(workspace), original);
  }
  const back = await p.tool('undo', { target: undone.target });
  assert.equal(back.dirty, false);
  assert.equal(back.history.canRedo, true);
  const forward = await p.tool('redo', { target: back.target });
  assert.equal(forward.dirty, true);
  assert.equal(forward.history.canRedo, true);
  const last = await p.tool('redo', { target: forward.target });
  assert.equal(last.history.canRedo, false);
  assert.deepEqual(await workspaceBytes(workspace), original);
  const bound = await p.tool('save', { target: last.target });
  assert.equal(bound.fileName, before.fileName);
  assert.equal(bound.target.bindingRevision, before.target.bindingRevision);
  assert.equal(bound.dirty, false);
  assert.deepEqual(
    (await projectFile(file('bound.json'))).comp.layers.at(-1).nodes[0].params.center.value,
    [180, 132],
  );
  await p.stop();
}

test('still-PNG validation checks filter offsets across inflation chunks before needing Chromium', async (t) => {
  const { root } = await fixture(t);
  const files = await createAgentWorkflowFiles(root);
  const width = 255,
    height = 35,
    stride = width * 4 + 1;
  const rows = Buffer.alloc(height * stride, 255);
  for (let offset = 0; offset < rows.length; offset += stride) rows[offset] = 0;
  const validate = () => {
    const png = fixturePng({ width, height, chunks: [idat(deflateSync(rows))] });
    return files.validateImages({
      imageAssets: { picture: { width, height, data: png.toString('base64') } },
    });
  };
  assert.equal(await validate(), 'unavailable');
  rows[(height - 1) * stride] = 5; // After the first 32 KiB of output.
  assert.equal(await validate(), 'rejected');
});

test(
  'real SVG image boundary rejects the empty-IDAT witness without publishing, even without Chromium',
  { timeout: 90_000 },
  verifySvgImageRejection,
);

async function verifySvgImageRejection(t) {
  const workspace = await fixture(t);
  const { file, root } = workspace;
  const corrupt = emptyIdatPng;
  await fs.writeFile(file('corrupt.json'), picturedProject(corrupt));
  await fs.writeFile(file('old.svg'), 'never replace existing SVG');
  const original = await workspaceBytes(workspace);
  const files = await createAgentWorkflowFiles(root, { chromiumPath });
  const withoutBrowser = await createAgentWorkflowFiles(root);
  const seen = [];
  let publications = 0;
  let decoder = files;
  const workflow = createAgentWorkflow({
    files: {
      ...files,
      async validateImages(input) {
        seen.push(input);
        // Bypass only the Open preflight: normal stdio correctly rejects this project.
        return seen.length === 1 ? 'valid' : decoder.validateImages(input);
      },
      async exportArtwork(args) {
        publications++;
        return files.exportArtwork(args);
      },
    },
  });
  const opened = await workflow.dispatch('nect_open', { target: null, fileName: 'corrupt.json' });
  assert.equal(opened.ok, true, JSON.stringify(opened));
  const before = await workflow.dispatch('nect_inspect');
  const { target } = opened.result;
  for (const [fileName, code, port] of [
    ['old.svg', 'OUTPUT_EXISTS', files],
    ['fresh.svg', 'IMAGE_REJECTED', files],
    ['fresh.svg', 'IMAGE_REJECTED', withoutBrowser],
  ]) {
    decoder = port;
    const result = await workflow.dispatch('nect_export_svg', { target, fileName });
    assert.equal(result.ok, false);
    assert.equal(result.error.code, code, JSON.stringify(result));
    assert.equal(publications, 0);
    assert.deepEqual(await workflow.dispatch('nect_inspect'), before);
    assert.deepEqual(await workspaceBytes(workspace), original);
  }
  assert.equal(seen.length, 3);
  for (const { imageAssets } of seen.slice(1))
    assert.deepEqual(Object.values(imageAssets), [
      { data: corrupt.toString('base64'), width: 2, height: 1 },
    ]);
  await assert.rejects(fs.lstat(file('fresh.svg')), { code: 'ENOENT' });
}
