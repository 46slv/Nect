import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { execFile } from 'node:child_process';
import {
  document,
  node,
  deserialize,
  serialize,
  exportSVG,
  createVariableDocumentPreparer,
} from '../src/core.js';
import { makeVariableFontAsset, parseFont } from '../src/fonts.js';
import { createNodeVariableWorkerTransport } from '../cli/variable-font-node.js';

const exec = promisify(execFile);
const cli = new URL('../cli/export.js', import.meta.url);
const fixture = await fs.readFile(
  new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
);
const defaultBytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const asBuffer = (bytes) =>
  bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
const defaults = { default: parseFont(asBuffer(defaultBytes)) };
function scene(axes) {
  const doc = document(),
    text = node('Text'),
    output = node('Output');
  text.text = 'AV Width';
  text.params.size.value = 100;
  output.params.path.source = text.id;
  doc.layers[0].nodes = [text, output];
  doc.layers[0].output = output.id;
  if (axes) {
    const font = makeVariableFontAsset(asBuffer(fixture), 'Roboto-variable.ttf');
    doc.schemaVersion = 25;
    doc.fontAssets = { [font.id]: font.asset };
    text.font = font.id;
    text.variation = { version: 1, fontId: font.id, axes };
  }
  return doc;
}
async function oracle(doc) {
  const transport = createNodeVariableWorkerTransport();
  const bridge = createVariableDocumentPreparer({ workerFactory: transport.workerFactory });
  try {
    const ticket = await bridge.prepareDocument(deserialize(serialize(doc)));
    const prepared = bridge.preparedDocument(ticket);
    bridge.activate(ticket);
    return exportSVG(prepared, { ...defaults, ...bridge.fonts() });
  } finally {
    bridge.dispose();
    await transport.dispose();
  }
}
async function temp(t) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'nect-variable-cli-'));
  t.after(() => fs.rm(dir, { recursive: true, force: true }));
  return dir;
}
const run = (input, output) =>
  exec(process.execPath, [cli.pathname, input, output], { timeout: 15000 });

test('cold CLI prepares real weight/width axes exactly and exits; repeated export is deterministic', async (t) => {
  const dir = await temp(t),
    outputs = [];
  for (const [index, axes] of [
    { wght: 100, wdth: 75 },
    { wght: 900, wdth: 100 },
  ].entries()) {
    const doc = scene(axes),
      input = path.join(dir, `${index}.json`),
      output = path.join(dir, `${index}.svg`);
    await fs.writeFile(input, serialize(doc));
    await run(input, output);
    const actual = await fs.readFile(output, 'utf8');
    assert.equal(actual, await oracle(doc));
    outputs.push(actual);
    const repeated = path.join(dir, `${index}-again.svg`);
    await run(input, repeated);
    assert.equal(await fs.readFile(repeated, 'utf8'), actual);
  }
  assert.notEqual(outputs[0], outputs[1]);
});

test('static CLI remains byte-exact with the existing default font resolver', async (t) => {
  const dir = await temp(t),
    doc = scene(),
    input = path.join(dir, 'static.json'),
    output = path.join(dir, 'static.svg');
  await fs.writeFile(input, serialize(doc));
  await run(input, output);
  assert.equal(await fs.readFile(output, 'utf8'), exportSVG(deserialize(serialize(doc)), defaults));
});

test('invalid axes, unsupported mapping and unsupported font leave outputs untouched and exit', async (t) => {
  const dir = await temp(t),
    input = path.join(dir, 'invalid.json'),
    output = path.join(dir, 'existing.svg');
  const invalidAxes = scene({ wght: 901 });
  const unsupportedMapping = scene({ wght: 400 });
  unsupportedMapping.layers[0].nodes[0].text = 'ffi';
  const unsupportedFont = structuredClone(scene({ wght: 400 }));
  Object.values(unsupportedFont.fontAssets)[0].data = defaultBytes.toString('base64');
  for (const [doc, error] of [
    [invalidAxes, /INVALID_AXES/],
    [unsupportedMapping, /UNSUPPORTED_MAPPING/],
    [unsupportedFont, /VARIABLE|FONT|TABLE/],
  ]) {
    await fs.writeFile(input, serialize(doc));
    await fs.writeFile(output, 'preserve existing output');
    await assert.rejects(run(input, output), (e) => {
      assert.equal(e.code, 1);
      assert.match(e.stderr, error);
      return true;
    });
    assert.equal(await fs.readFile(output, 'utf8'), 'preserve existing output');
    const absent = path.join(dir, 'must-not-exist.svg');
    await assert.rejects(run(input, absent));
    await assert.rejects(fs.stat(absent), { code: 'ENOENT' });
  }
  await fs.writeFile(input, serialize(scene({ wght: 400 })));
  await assert.rejects(run(input, output), (e) => e.code === 1 && /EEXIST/.test(e.stderr));
  assert.equal(await fs.readFile(output, 'utf8'), 'preserve existing output');
});

test('real worker timeout cleans up, disposal is idempotent, unprepared export fails closed', async () => {
  const transport = createNodeVariableWorkerTransport();
  const bridge = createVariableDocumentPreparer({
    workerFactory: transport.workerFactory,
    timeout: 1,
  });
  const doc = scene({ wght: 900 });
  assert.throws(() => exportSVG(doc, defaults), { code: 'PREPARATION_REQUIRED' });
  try {
    await assert.rejects(bridge.prepareDocument(doc), { code: 'TIMEOUT' });
    assert.equal(bridge.stats().worker, false);
    assert.equal(bridge.stats().pending, false);
  } finally {
    bridge.dispose();
    await transport.dispose();
    await transport.dispose();
  }
  assert.throws(() => transport.workerFactory(), /DISPOSED/);
});

test(
  'Node transport propagates endpoint failure and still awaits termination',
  { timeout: 5000 },
  async () => {
    const transport = createNodeVariableWorkerTransport();
    try {
      const worker = transport.workerFactory();
      const failure = new Promise((resolve) => {
        worker.onerror = resolve;
      });
      // Invalid protocol input crashes the endpoint; this exercises worker error/exit wiring.
      worker.postMessage(null);
      assert.ok((await failure) instanceof Error);
    } finally {
      await transport.dispose();
    }
  },
);
