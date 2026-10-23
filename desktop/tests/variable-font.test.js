import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import { Worker } from 'node:worker_threads';
import { build } from 'esbuild';
import { createVariableBackend } from '../src/variable-font/backend.js';
import { createVariablePreparer } from '../src/variable-font/prepare.js';
import { validateVariableBytes } from '../src/variable-font/validate.js';
import { variableRunToCubicPath } from '../src/variable-font/cubic-path.js';
import { canonicalAxes, LIMITS } from '../src/variable-font/contract.js';
import { parseFont } from '../src/fonts.js';
const fixture = fs
  .readdirSync(new URL('./fixtures/variable-font/', import.meta.url))
  .find((x) => x.endsWith('.ttf'));
const bytes = new Uint8Array(
  fs.readFileSync(new URL(`./fixtures/variable-font/${fixture}`, import.meta.url)),
);
const id = createHash('sha256').update(bytes).digest('hex');
const nodeWorker = () => {
  const w = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url));
  const bridge = { postMessage: (data) => w.postMessage(data), terminate: () => w.terminate() };
  w.on('message', (data) => bridge.onmessage?.({ data }));
  w.on('error', (e) => bridge.onerror?.(e));
  return bridge;
};
function table(bytes, tag) {
  const v = new DataView(bytes.buffer);
  for (let i = 0; i < v.getUint16(4); i++) {
    const p = 12 + i * 16;
    if (String.fromCharCode(...bytes.slice(p, p + 4)) === tag)
      return new DataView(bytes.buffer, v.getUint32(p + 8), v.getUint32(p + 12));
  }
  throw Error(tag);
}
test('licensed real variations, canonical coordinates, composites, deterministic cubic bridge', () => {
  assert.equal(id, 'd7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134');
  const metadata = validateVariableBytes(bytes),
    b = createVariableBackend();
  assert.deepEqual(canonicalAxes(metadata.axes, { wght: 400 }).sparse, {});
  const values = [100, 400, 900, 550].map((wght) =>
    b.prepare(id, bytes, { text: 'A', axes: { wght } }),
  );
  assert.deepEqual(
    values.slice(0, 3).map((r) => r.advance),
    [1223, 1336, 1395],
  );
  assert.notDeepEqual(values[0].commands, values[2].commands);
  assert.notDeepEqual(values[1].commands, values[3].commands);
  assert.notEqual(
    b.prepare(id, bytes, { text: 'A', axes: { wdth: 75 } }).advance,
    values[1].advance,
  );
  const composite = b.prepare(id, bytes, { text: 'éé AV', axes: { wght: 550 } });
  assert.equal(composite.glyphs.length, 5);
  assert.equal(composite.glyphs[1].sourceStart, 1);
  assert.deepEqual(
    createVariableBackend().prepare(id, bytes, { text: 'éé AV', axes: { wght: 550 } }),
    composite,
  );
  const cubic = variableRunToCubicPath(composite);
  assert(cubic.contours.length);
  assert(cubic.contours.every((c) => c.closed));
  assert(
    cubic.contours
      .flatMap((c) => c.points)
      .every((p) => [...p.position, ...p.handleA, ...p.handleB].every(Number.isFinite)),
  );
  for (let i = 0; i < 40; i++) b.prepare(id, bytes, { text: 'A', axes: { wght: 100 + i * 10 } });
  assert.deepEqual(b.prepare(id, bytes, { text: 'A', axes: { wght: 400 } }), values[1]);
});
test('fail closed axes, layout eligibility, malformed metadata, rights, and output bounds', () => {
  const b = createVariableBackend();
  for (const axes of [{ wght: NaN }, { wght: Infinity }, { wght: 99 }, { oops: 1 }, []])
    assert.throws(() => b.prepare(id, bytes, { text: 'A', axes }), /INVALID_AXES/);
  for (const text of ['ffi', 'x\u0301', 'אב', '\ud800', 'A\nB'])
    assert.throws(() => b.prepare(id, bytes, { text }), /UNSUPPORTED_MAPPING/);
  assert.throws(() => b.prepare(id, bytes, { text: 'A', direction: 'rtl' }), /UNSUPPORTED_OPTIONS/);
  assert.throws(() => b.prepare(id, bytes, { text: 'A'.repeat(LIMITS.text + 1) }), /TEXT_LIMIT/);
  assert.throws(() => b.prepare(id, bytes, { text: '@'.repeat(1024) }), /OUTPUT_LIMIT/);
  for (const mutate of [
    (b) => table(b, 'fvar').setUint16(8, 100),
    (b) => table(b, 'fvar').setUint8(19, 32),
    (b) => table(b, 'fvar').setInt32(20, 999 * 65536),
    (b) => table(b, 'gvar').setUint16(4, 7),
    (b) => table(b, 'OS/2').setUint16(8, 2),
    (b) => table(b, 'maxp').setUint16(4, 65535),
  ]) {
    const copy = bytes.slice();
    mutate(copy);
    assert.throws(() => validateVariableBytes(copy));
  }
  assert.throws(() => parseFont(bytes.slice().buffer), /Variable fonts are unsupported/);
});
test('real worker preparation, immutable byte copy, cache miss, cleanup', async () => {
  const p = createVariablePreparer({ workerFactory: nodeWorker });
  try {
    const input = bytes.slice(),
      asset = await p.register(input);
    input.fill(0);
    assert.throws(() => p.read(asset, { text: 'A' }), /PREPARATION_REQUIRED/);
    const result = await p.prepare(asset, { text: 'A' });
    assert.equal(result.advance, 1336);
    assert(Object.isFrozen(result.commands));
    assert.equal(p.read(asset, { text: 'A' }), result);
    assert.equal(p.stats().bytes, bytes.length);
    p.releaseWorker();
    assert.equal(p.stats().worker, false);
  } finally {
    p.dispose();
  }
  assert.equal(p.stats().bytes, 0);
});
test('timeout, abort, stale revisions, ignored obsolete messages, concurrency, no fallback', async () => {
  let workers = [];
  const factory = () => {
    const w = {
      terminate() {
        this.stopped = true;
      },
      postMessage(data) {
        this.request = data;
      },
    };
    workers.push(w);
    return w;
  };
  const p = createVariablePreparer({ workerFactory: factory, timeout: 25 }),
    asset = await p.register(bytes);
  await assert.rejects(p.prepare(asset, { text: 'A' }), /TIMEOUT/);
  assert(workers[0].stopped);
  const control = new AbortController(),
    cancel = p.prepare(asset, { text: 'A' }, { signal: control.signal });
  control.abort();
  await assert.rejects(cancel, /CANCELLED/);
  const stale = p.prepare(asset, { text: 'A' });
  p.setRevision(1);
  await assert.rejects(stale, /STALE_RESULT/);
  const next = p.prepare(asset, { text: 'A' });
  await assert.rejects(p.prepare(asset, { text: 'B' }), /BUSY/);
  workers.at(-1).onmessage({ data: { id: 999, revision: 0, result: {} } });
  await assert.rejects(next, /TIMEOUT/);
  p.dispose();
  await assert.rejects(p.prepare(asset, { text: 'A' }), /DISPOSED/);
  const broken = createVariablePreparer({
    workerFactory: () => {
      throw Error('NO_WORKER');
    },
  });
  const a = await broken.register(bytes);
  await assert.rejects(broken.prepare(a, { text: 'A' }), /NO_WORKER/);
  broken.dispose();
});
test('browser worker resolves pinned browser entry without Node externals or codegen', async () => {
  const result = await build({
    entryPoints: ['src/variable-font/worker.js'],
    bundle: true,
    platform: 'browser',
    format: 'esm',
    target: 'es2022',
    write: false,
    metafile: true,
  });
  assert(
    Object.keys(result.metafile.inputs).some((p) => p.endsWith('fontkit/dist/browser-module.mjs')),
  );
  assert(!Object.values(result.metafile.outputs).some((o) => o.imports.some((i) => i.external)));
  assert(
    !/\b(?:eval|Function)\s*\(|createRequire|node:fs|node:module/.test(result.outputFiles[0].text),
  );
});

test('parent axes validation cannot be bypassed by ready cache; Buffer ownership', async () => {
  const p = createVariablePreparer({ workerFactory: nodeWorker });
  try {
    const input = Buffer.from(bytes),
      asset = await p.register(input);
    input.fill(0);
    await p.prepare(asset, { text: 'A' });
    for (const axes of [null, [], NaN, {}, { wght: NaN }]) {
      if (axes && Object.getPrototypeOf(axes) === Object.prototype && !Object.keys(axes).length)
        continue;
      await assert.rejects(p.prepare(asset, { text: 'A', axes }), /INVALID_AXES/);
      assert.throws(() => p.read(asset, { text: 'A', axes }), /INVALID_AXES/);
    }
    for (let i = 0; i < 12; i++)
      await p.prepare(asset, { text: 'A', axes: { wght: 100 + i * 20 } });
    assert.equal(p.stats().runs, 8);
    assert(p.stats().cachedCommands <= LIMITS.cachedCommands);
  } finally {
    p.dispose();
  }
});

test('old worker errors cannot abort newer preparation; global worker slot is bounded', async () => {
  const workers = [],
    factory = () => {
      const w = {
        terminate() {},
        postMessage(data) {
          this.request = data;
        },
      };
      workers.push(w);
      return w;
    };
  const p = createVariablePreparer({ workerFactory: factory, timeout: 50 }),
    q = createVariablePreparer({ workerFactory: factory, timeout: 50 });
  const a = await p.register(bytes),
    b = await q.register(bytes);
  const c = new AbortController(),
    first = p.prepare(a, { text: 'A' }, { signal: c.signal });
  c.abort();
  await assert.rejects(first, /CANCELLED/);
  const next = p.prepare(a, { text: 'A' });
  workers[0].onerror();
  workers[0].onmessageerror();
  await assert.rejects(q.prepare(b, { text: 'A' }), /WORKER_LIMIT/);
  await assert.rejects(next, /TIMEOUT/);
  p.dispose();
  q.dispose();
});

test('asset count, input bytes and concurrent reservation are bounded', async () => {
  const p = createVariablePreparer();
  try {
    await assert.rejects(p.register(new Uint8Array(LIMITS.bytes + 1)), /FONT_BYTES/);
    const registrations = Array.from({ length: 9 }, (_, i) => {
      const b = bytes.slice();
      b[b.length - 1] = i;
      return p.register(b);
    });
    const result = await Promise.allSettled(registrations);
    assert.equal(result.filter((x) => x.status === 'fulfilled').length, 8);
    assert.equal(p.stats().assets, 8);
    assert.equal(result[8].reason.code, 'ASSET_LIMIT');
  } finally {
    p.dispose();
  }
});
