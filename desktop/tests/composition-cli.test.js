import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { execFile } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { deserialize, exportSVG } from '../src/core.js';
import { parseFont, MAX_PROJECT_BYTES } from '../src/fonts.js';
import { deserializeExportDocument } from '../cli/project-input.js';
import { deserializeCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const exec = promisify(execFile);
const cli = fileURLToPath(new URL('../cli/export.js', import.meta.url));
const fixture = (name) => new URL(`./fixtures/cli-compositions/${name}`, import.meta.url);
const read = (name) => fs.readFile(fixture(name), 'utf8');
const fontBytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const fonts = {
  default: parseFont(
    fontBytes.buffer.slice(fontBytes.byteOffset, fontBytes.byteOffset + fontBytes.byteLength),
  ),
};
const run = (args, options = {}) =>
  exec(process.execPath, [cli, ...args], { timeout: 15000, ...options });
const exists = async (name) =>
  fs.access(name).then(
    () => true,
    () => false,
  );
async function temp(t) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'nect-composition-cli-'));
  t.after(() => fs.rm(dir, { recursive: true, force: true }));
  return dir;
}
async function rejects(args, pattern, options) {
  await assert.rejects(run(args, options), (error) => {
    assert.equal(error.code, 1);
    assert.equal(error.killed, false);
    assert.match(error.stderr, pattern);
    return true;
  });
}

test('legacy CLI and explicit A preserve byte output; B exports only its own geometry', async (t) => {
  const dir = await temp(t);
  const single = await read('single-document.json');
  const multi = await read('two-compositions.json');
  const input = path.join(dir, 'project.json');
  const legacyInput = path.join(dir, 'legacy.json');
  await fs.writeFile(input, multi);
  await fs.writeFile(legacyInput, single);
  const legacyOutput = path.join(dir, 'legacy.svg');
  await run([legacyInput, legacyOutput]);
  const legacy = await fs.readFile(legacyOutput, 'utf8');
  assert.equal(legacy, exportSVG(deserialize(single), fonts));
  const expected = {
    'composition-a': ['0 0 160 120', '#ff0000', 'M10,10h60l-60,40z', '#0000ff'],
    'composition-b': ['0 0 240 180', '#0000ff', 'M110,70h80l-40,60z', '#ff0000'],
  };
  for (const [id, [viewBox, fill, geometry, forbidden]] of Object.entries(expected)) {
    const output = path.join(dir, `${id}.svg`);
    await run(['--composition', id, input, output]);
    const actual = await fs.readFile(output, 'utf8');
    const project = deserializeCompositionProject(multi, deserialize);
    const owner = createCompositionSessionOwner(project, {
      activeCompositionId: id,
      fonts: () => fonts,
    });
    assert.equal(actual, exportSVG(owner.activeDocument(), fonts));
    assert.equal(actual.match(/viewBox="([^"]+)"/)[1], viewBox);
    assert.equal(actual.match(/\sfill="([^"]+)"/)[1], fill);
    assert.equal(actual.match(/\sd="([^"]+)"/)[1], geometry);
    assert.equal((actual.match(/<path\b/g) || []).length, 1);
    assert(!actual.includes(forbidden));
    if (id === 'composition-a') assert.equal(actual, legacy);
    else assert.notEqual(actual, legacy);
  }
  assert.equal(await fs.readFile(input, 'utf8'), multi);
  assert.equal(await fs.readFile(legacyInput, 'utf8'), single);
});

test('a sole Composition exports without a selector, with the legacy core upgrader retained', async (t) => {
  const dir = await temp(t);
  const project = JSON.parse(await read('two-compositions.json'));
  project.compositions.splice(1);
  const text = JSON.stringify(project);
  const input = path.join(dir, 'single-composition.json');
  const output = path.join(dir, 'single.svg');
  await fs.writeFile(input, text);
  await run([input, output]);
  assert.equal(
    await fs.readFile(output, 'utf8'),
    exportSVG(deserialize(await read('single-document.json')), fonts),
  );
  // schema 1 stores paint on Output. Both routes must use the existing migration.
  const old = JSON.parse(await read('single-document.json'));
  const layer = old.layers[0];
  const fill = layer.nodes.find((node) => node.type === 'Fill');
  const out = layer.nodes.find((node) => node.type === 'Output');
  out.params.path = { ...fill.params.path, type: 'Path' };
  out.params.fill = { ...fill.params.color };
  layer.nodes = layer.nodes.filter((node) => node !== fill);
  old.schemaVersion = 1;
  const legacyText = JSON.stringify(old);
  assert.equal(
    exportSVG(deserializeExportDocument(legacyText), fonts),
    exportSVG(deserialize(legacyText), fonts),
  );
});

test('invalid inactive content, malformed JSON and ambiguous/unknown IDs never write output', async (t) => {
  const dir = await temp(t);
  const sentinel = Buffer.from('keep existing output\r\n\0end');
  const cases = [
    ['invalid-inactive-graph.json', ['--composition', 'composition-a'], /Invalid parameter type/],
    [
      'invalid-inactive-dimensions.json',
      ['--composition', 'composition-a'],
      /Invalid Composition width/,
    ],
    ['invalid-json.txt', [], /JSON/],
    ['two-compositions.json', [], /specify --composition/],
    ['two-compositions.json', ['--composition', 'missing'], /Unknown Composition ID/],
    ['single-document.json', ['--composition', 'missing'], /Unknown Composition ID/],
  ];
  for (const [index, [name, args, error]] of cases.entries()) {
    await t.test(`${name} ${args.join(' ')}`, async () => {
      const text = await read(name);
      const input = path.join(dir, `${index}.json`);
      const output = path.join(dir, `${index}.svg`);
      await fs.writeFile(input, text);
      await rejects([...args, input, output], error);
      assert.equal(await exists(output), false);
      await fs.writeFile(output, sentinel);
      await rejects([...args, input, output], error);
      assert.deepEqual(await fs.readFile(output), sentinel);
      assert.equal(await fs.readFile(input, 'utf8'), text);
    });
  }
});

test('valid inputs refuse existing output, including when input and output are the same file', async (t) => {
  const dir = await temp(t);
  for (const [name, args] of [
    ['single-document.json', []],
    ['two-compositions.json', ['--composition', 'composition-b']],
  ]) {
    const text = await read(name);
    const input = path.join(dir, name);
    const output = `${input}.svg`;
    await fs.writeFile(input, text);
    await fs.writeFile(output, 'keep');
    await rejects([...args, input, output], /EEXIST/);
    assert.equal(await fs.readFile(output, 'utf8'), 'keep');
    await rejects([...args, input, input], /EEXIST/);
    assert.equal(await fs.readFile(input, 'utf8'), text);
  }
});

test('strict CLI options reject extra/duplicate/unknown arguments and support help and --', async (t) => {
  const dir = await temp(t);
  const input = path.join(dir, 'project.json');
  const output = path.join(dir, 'out.svg');
  await fs.writeFile(input, await read('single-document.json'));
  for (const args of [
    [input, output, 'extra'],
    ['--unknown', input, output],
    ['--composition', 'a', '--composition', 'b', input, output],
    ['--composition=', input, output],
    ['--composition'],
    [input],
  ]) {
    await rejects(args, /Usage:|option|composition|Composition/);
    assert.equal(await exists(output), false);
  }
  const help = await run(['--help']);
  assert.match(help.stdout, /--composition <id>/);
  await fs.writeFile(path.join(dir, '-input.json'), await read('single-document.json'));
  await run(['--', '-input.json', '-out.svg'], { cwd: dir });
  assert.equal(await exists(path.join(dir, '-out.svg')), true);
});

test('size limits cover file bytes and UTF-8 decoder bytes before parsing', async (t) => {
  const dir = await temp(t);
  const input = path.join(dir, 'oversized.json');
  const output = path.join(dir, 'out.svg');
  const handle = await fs.open(input, 'wx');
  try {
    await handle.truncate(MAX_PROJECT_BYTES + 1);
  } finally {
    await handle.close();
  }
  await rejects([input, output], /Project exceeds 50 MB/);
  assert.equal(await exists(output), false);
  await fs.writeFile(output, 'keep');
  await rejects([input, output], /Project exceeds 50 MB/);
  assert.equal(await fs.readFile(output, 'utf8'), 'keep');
  const unicode = 'あ'.repeat(Math.floor(MAX_PROJECT_BYTES / 3) + 1);
  assert(unicode.length < MAX_PROJECT_BYTES);
  assert.throws(() => deserializeExportDocument(unicode), /Project exceeds 50 MB/);
});
