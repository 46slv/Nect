import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { finiteNumber, fixedSvgSize, parseSvgPngArgs } from '../cli/svg-to-png-options.js';
import { pngDimensions } from '../src/raster.js';
import { publishPng, readSvgInput, requireNewOutput } from '../cli/png-output.js';

const env = { CHROMIUM_PATH: 'installed-chrome' };
const parse = (...args) => parseSvgPngArgs(['in.svg', 'out.png', ...args], env);
async function temporary(t) {
  const directory = await fs.mkdtemp(join(tmpdir(), 'nect-png-cli-'));
  t.after(() => fs.rm(directory, { recursive: true, force: true }));
  return directory;
}

test('CLI requires two explicit paths and installed Chrome', () => {
  assert.deepEqual(parse(), {
    input: 'in.svg',
    output: 'out.png',
    sizing: 1,
    ppi: null,
    chromiumPath: 'installed-chrome',
  });
  assert.equal(parse('--chromium-path', 'explicit').chromiumPath, 'explicit');
  assert.deepEqual(parseSvgPngArgs(['--help'], {}), { help: true });
  for (const args of [[], ['in.svg'], ['a', 'b', 'c']])
    assert.throws(() => parseSvgPngArgs(args, env), /Usage/);
  assert.throws(() => parseSvgPngArgs(['a', 'b'], {}), /CHROMIUM_PATH/);
  assert.throws(() => parse('--unknown'), /Unknown option/);
  assert.throws(() => parse('--chromium-path'), /installed Chrome/);
  assert.throws(() => parse('--chromium-path', 'a', '--chromium-path', 'b'), /once/);
});

test('strict finite scale/PPI, exclusive sizing and existing renderer bounds', () => {
  for (const value of ['NaN', 'Infinity', '-Infinity', '1junk', '0x10', '', ' 2', '1e999']) {
    assert.throws(() => finiteNumber(value), /finite decimal/);
    assert.throws(() => parse('--scale', value));
    assert.throws(() => parse('--ppi', value));
  }
  for (const value of ['0', '-1', '0.09', '8.01']) assert.throws(() => parse('--scale', value));
  for (const value of ['0', '-1', '0.00001', '1e20']) assert.throws(() => parse('--ppi', value));
  for (const args of [
    ['--scale', '2', '--ppi', '300'],
    ['--ppi', '300', '--scale', '2'],
    ['--scale', '1', '--scale', '1'],
    ['--ppi', '96', '--ppi', '96'],
  ])
    assert.throws(() => parse(...args), /mutually exclusive/);
  assert.throws(() => parse('--scale'), /finite decimal/);
  assert.equal(parse('--scale', '1e0').sizing, 1);
  assert.deepEqual(pngDimensions({ width: 32, height: 16 }, parse('--ppi', '300').sizing), {
    width: 100,
    height: 50,
  });
  assert.deepEqual(pngDimensions({ width: 10000, height: 1000 }, 0.5), {
    width: 5000,
    height: 500,
  });
  assert.throws(() => pngDimensions({ width: 8193, height: 1 }));
  assert.throws(() => pngDimensions({ width: 4001, height: 4000 }));
});

test('fixed SVG uses CSS-pixel viewBox once, even for mm/cm roots', () => {
  for (const [width, height] of [
    ['32', '16'],
    ['32px', '16px'],
    ['8.4mm', '4.2mm'],
    ['.84cm', '.42cm'],
  ])
    assert.deepEqual(fixedSvgSize({ width, height, viewBox: '0 0 32 16' }), {
      width: 32,
      height: 16,
    });
  for (const item of [
    {},
    { width: '100%', height: '100%', viewBox: '0 0 32 16' },
    { width: 'auto', height: '16', viewBox: '0 0 32 16' },
    { width: '32', height: '16' },
    { width: '32', height: '16', viewBox: '1 0 32 16' },
    { width: '32', height: '16', viewBox: '0 0 0 16' },
    { width: '32', height: '16', viewBox: '0 0 Infinity 16' },
    { width: '1e999', height: '16', viewBox: '0 0 32 16' },
    { width: '-32', height: '16', viewBox: '0 0 32 16' },
  ])
    assert.throws(() => fixedSvgSize(item));
});

test('bounded input is read-only and rejects invalid UTF-8/over-limit growth', async (t) => {
  const directory = await temporary(t),
    input = join(directory, 'source.svg');
  const bytes = Buffer.from('<svg>é</svg>');
  await fs.writeFile(input, bytes);
  assert.equal(await readSvgInput(input, bytes.length), bytes.toString());
  assert.deepEqual(await fs.readFile(input), bytes);
  await assert.rejects(readSvgInput(input, bytes.length - 1), /50 MB/);
  await fs.writeFile(input, Buffer.from([0xff]));
  await assert.rejects(readSvgInput(input, 20), /encoded data/);
  await assert.rejects(readSvgInput(directory, 20));
  await fs.writeFile(input, '12345');
  const growing = {
    open: async (...args) => {
      const handle = await fs.open(...args);
      return {
        stat: async () => ({ size: 1, isFile: () => true }),
        read: (...a) => handle.read(...a),
        close: () => handle.close(),
      };
    },
  };
  await assert.rejects(readSvgInput(input, 4, growing), /50 MB/);
});

test('exclusive publication preserves input/output and cleans its temp', async (t) => {
  const directory = await temporary(t),
    input = join(directory, 'source.svg'),
    output = join(directory, 'output.png');
  await fs.writeFile(input, 'source stays unchanged');
  await requireNewOutput(output);
  await publishPng(output, Buffer.from('complete bytes'));
  assert.equal(await fs.readFile(output, 'utf8'), 'complete bytes');
  assert.equal(await fs.readFile(input, 'utf8'), 'source stays unchanged');
  await assert.rejects(requireNewOutput(output), /already exists/);
  await assert.rejects(publishPng(output, Buffer.from('replacement')), { code: 'EEXIST' });
  assert.equal(await fs.readFile(output, 'utf8'), 'complete bytes');
  assert.deepEqual((await fs.readdir(directory)).sort(), ['output.png', 'source.svg']);
  await assert.rejects(requireNewOutput(join(directory, 'missing', 'output.png')), {
    code: 'ENOENT',
  });
});

test('competing publisher cannot clobber a file created after preflight', async (t) => {
  const directory = await temporary(t),
    output = join(directory, 'output.png');
  await requireNewOutput(output);
  await fs.writeFile(output, 'competitor', { flag: 'wx' });
  await assert.rejects(publishPng(output, Buffer.from('ours')), { code: 'EEXIST' });
  assert.equal(await fs.readFile(output, 'utf8'), 'competitor');
  assert.deepEqual(await fs.readdir(directory), ['output.png']);
});

test('write/close/link failures leave no partial output or unowned cleanup', async (t) => {
  const directory = await temporary(t),
    output = join(directory, 'output.png'),
    unrelated = '.output.png.not-ours.tmp';
  await fs.writeFile(join(directory, unrelated), 'keep');
  for (const failure of ['write', 'close', 'link']) {
    let closed = false;
    const io = {
      ...fs,
      open: async (...args) => {
        const handle = await fs.open(...args);
        return {
          writeFile: async (bytes) => {
            if (failure === 'write') {
              await handle.writeFile(bytes.subarray(0, 2));
              throw Error('write failure');
            }
            await handle.writeFile(bytes);
          },
          close: async () => {
            if (!closed) {
              closed = true;
              await handle.close();
              if (failure === 'close') throw Error('close failure');
            }
          },
        };
      },
      link: async (...args) => {
        if (failure === 'link') throw Error('hard links unsupported');
        await fs.link(...args);
      },
    };
    await assert.rejects(publishPng(output, Buffer.from('all bytes'), io));
    await assert.rejects(fs.lstat(output), { code: 'ENOENT' });
    assert.deepEqual(await fs.readdir(directory), [unrelated]);
  }
  const collision = {
    ...fs,
    open: async () => {
      throw Object.assign(Error('collision'), { code: 'EEXIST' });
    },
    unlink: async () => {
      assert.fail('must not unlink a temp we never created');
    },
  };
  await assert.rejects(publishPng(output, Buffer.from('bytes'), collision), { code: 'EEXIST' });
});

test('actual CLI rejects unsafe paths/sizes before Chrome without partial files', async (t) => {
  const directory = await temporary(t),
    input = join(directory, 'source.svg'),
    output = join(directory, 'output.png'),
    cli = fileURLToPath(new URL('../cli/svg-to-png.js', import.meta.url));
  await fs.writeFile(input, '<svg/>');
  const run = (args) =>
    spawnSync(process.execPath, [cli, ...args], {
      env: { ...process.env, CHROMIUM_PATH: join(directory, 'absent-chrome') },
      encoding: 'utf8',
    });
  assert.equal(run([input, output, '--ppi', 'NaN']).status, 1);
  await fs.writeFile(output, 'sentinel');
  assert.match(run([input, output]).stderr, /already exists/);
  assert.equal(await fs.readFile(output, 'utf8'), 'sentinel');
  await fs.unlink(output);
  const handle = await fs.open(input, 'w');
  await handle.truncate(50_000_001);
  await handle.close();
  const result = run([input, output]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /50 MB/);
  await assert.rejects(fs.lstat(output), { code: 'ENOENT' });
  assert.deepEqual(await fs.readdir(directory), ['source.svg']);
});
