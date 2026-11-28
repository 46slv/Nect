import fs from 'node:fs/promises';
import { constants } from 'node:fs';
import { createHash, randomUUID } from 'node:crypto';
import { execFile } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { constants as zlib, createInflate } from 'node:zlib';
import { AgentWorkflowError } from '../src/agent-workflow.js';
import { MAX_PROJECT_BYTES } from '../src/fonts.js';
import { decodeImage, MAX_IMAGE_PIXELS, MAX_IMAGE_SIDE, pngSize } from '../src/images.js';

const run = promisify(execFile);
const rasterCli = fileURLToPath(new URL('./svg-to-png.js', import.meta.url));
const MAX_PNG_BYTES = 70_000_000;
const imageOrigin = 'https://nect-image-preflight.invalid';
const imageModulePaths = ['/src/raster.js', '/src/images.js', '/src/artboard-units.js'];
const statOptions = { bigint: true };
const fail = (code, message) => {
  throw new AgentWorkflowError(code, message);
};
const identity = (stat) => `${stat.dev}:${stat.ino}`;
const stamp = (stat) => `${identity(stat)}:${stat.size}:${stat.mtimeNs}:${stat.mode}`;
const duringRead = (stat) => `${stamp(stat)}:${stat.ctimeNs}`;
const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');

function localPath(input) {
  const windows = process.platform === 'win32';
  if (
    typeof input !== 'string' ||
    !path.isAbsolute(input) ||
    /[\x00-\x1f]/.test(input) ||
    /^[\\/]{2}/.test(input) ||
    (windows && (!/^[a-z]:[\\/]/i.test(input) || /[:<>"|?*]/.test(input.slice(2)))) ||
    input
      .split(/[\\/]/)
      .some((part) => part === '.' || part === '..' || (windows && /[. ]$/.test(part)))
  )
    fail('UNSAFE_PATH', 'Supply an absolute local filesystem path.');
  return path.resolve(input);
}

function leaf(name, extension) {
  const suffix = `.${extension}`;
  const stem =
    typeof name === 'string' && name.endsWith(suffix) ? name.slice(0, -suffix.length) : '';
  if (
    !/^[a-z0-9]/i.test(stem) ||
    /[^a-z0-9_-]/i.test(stem) ||
    name.length > 80 ||
    /^(?:con|prn|aux|nul|com[0-9]|lpt[0-9])$/i.test(stem)
  )
    fail(
      'UNSAFE_NAME',
      `Use a simple workspace leaf with a .${extension} extension; no device names.`,
    );
  return name;
}

async function directoryInfo(directory) {
  const stat = await fs.lstat(directory, statOptions);
  if (!stat.isDirectory() || stat.isSymbolicLink() || (await fs.realpath(directory)) !== directory)
    fail('UNSAFE_WORKSPACE', 'Workspace and parent must be real, canonical directories.');
  if (process.getuid && stat.uid !== BigInt(process.getuid()))
    fail('UNSAFE_WORKSPACE', 'Workspace and parent must be owned by the launching user.');
  return identity(stat);
}

async function safeFile(filename) {
  const info = await fs.lstat(filename, statOptions);
  if (
    !info.isFile() ||
    info.isSymbolicLink() ||
    info.nlink !== 1n ||
    (await fs.realpath(filename)) !== filename
  )
    fail('UNSAFE_FILE', 'Only canonical regular, single-link files are accepted.');
  if (process.getuid && info.uid !== BigInt(process.getuid()))
    fail('UNSAFE_FILE', 'Workspace file must be owned by the launching user.');
  return info;
}

// Guarded equivalent of readSvgInput: cap even a growing file, and compare
// pathname, handle, identity and metadata before consuming any bytes.
async function readFileBounded(filename, limit) {
  const initial = await safeFile(filename);
  const handle = await fs.open(
    filename,
    constants.O_RDONLY | (constants.O_NOFOLLOW ?? 0) | (constants.O_NONBLOCK ?? 0),
  );
  try {
    const opened = await handle.stat(statOptions);
    if (!opened.isFile() || opened.nlink !== 1n || duringRead(opened) !== duringRead(initial))
      fail('FILE_CHANGED', 'File changed during read.');
    if (opened.size > BigInt(limit))
      fail('FILE_TOO_LARGE', 'File exceeds the workflow byte limit.');
    const bytes = Buffer.alloc(Number(opened.size) + 1);
    let total = 0;
    while (total < bytes.length) {
      const { bytesRead } = await handle.read(bytes, total, bytes.length - total, null);
      if (!bytesRead) break;
      total += bytesRead;
    }
    if (
      BigInt(total) !== opened.size ||
      duringRead(await safeFile(filename)) !== duringRead(opened) ||
      duringRead(await handle.stat(statOptions)) !== duringRead(opened)
    )
      fail('FILE_CHANGED', 'File changed during read.');
    return { bytes: bytes.subarray(0, total), stamp: stamp(opened) };
  } finally {
    await handle.close();
  }
}

async function fresh(filename) {
  try {
    await fs.lstat(filename); // A dangling symlink also reserves the name.
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  fail('OUTPUT_EXISTS', 'Destination already exists; choose a fresh name.');
}

async function io(action) {
  try {
    return await action();
  } catch (error) {
    if (error instanceof AgentWorkflowError) throw error;
    if (error.code === 'ENOENT') fail('NOT_FOUND', 'Workspace file or directory was not found.');
    if (error.code === 'EEXIST')
      fail('OUTPUT_EXISTS', 'Destination already exists; choose a fresh name.');
    if (['EBUSY', 'EAGAIN', 'ETXTBSY'].includes(error.code))
      fail('FILE_BUSY', 'Workspace file is busy.');
    if (['EACCES', 'EPERM', 'EROFS'].includes(error.code))
      fail('FILE_INACCESSIBLE', 'Workspace file is not writable/readable or is in use.');
    fail('FILE_IO', 'Workspace file operation failed.');
  }
}

async function withinDeadline(promise, timeout) {
  let timer;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(Error('Image validation timed out')), timeout);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

// PNG color type -> [samples per pixel, legal sample depths].
const pngFormats = new Map([
  [0, [1, [1, 2, 4, 8, 16]]],
  [2, [3, [8, 16]]],
  [3, [1, [1, 2, 4, 8]]],
  [4, [2, [8, 16]]],
  [6, [4, [8, 16]]],
]);
// [first x, first y, x step, y step] in on-wire pass order.
const adam7 = [
  [0, 0, 8, 8],
  [4, 0, 8, 8],
  [0, 4, 4, 8],
  [2, 0, 4, 4],
  [0, 2, 2, 4],
  [1, 0, 2, 2],
  [0, 1, 1, 2],
];
// Even RGBA16 uses only eight bytes per pixel, plus one filter byte per pass row.
const maxScanlineBytes = MAX_IMAGE_PIXELS * 8 + MAX_IMAGE_SIDE * adam7.length;

async function checkScanlines(compressed, passes, expected) {
  // Backpressure bounds buffering independently of the claimed output size.
  // Never allocate a full inflated image; stop in the first oversized chunk.
  const chunkSize = Math.min(32 * 1024, Math.max(zlib.Z_MIN_CHUNK, expected + 1));
  const inflater = createInflate({
    chunkSize,
    highWaterMark: chunkSize,
    finishFlush: zlib.Z_FINISH,
  });
  let offset = 0,
    nextFilter = 0,
    pass = 0,
    rows = passes[0].rows;
  try {
    inflater.end(compressed);
    for await (const chunk of inflater) {
      const end = offset + chunk.length;
      if (end > expected) return false;
      while (nextFilter < end) {
        if (chunk[nextFilter - offset] > 4) return false;
        nextFilter += passes[pass].stride;
        if (--rows === 0 && ++pass < passes.length) rows = passes[pass].rows;
      }
      offset = end;
    }
    // Z_FINISH requires the end marker and checksum. Inflate can leave trailing
    // input unconsumed; neither junk nor a second zlib member belongs in IDAT.
    return offset === expected && inflater.bytesWritten === compressed.length;
  } catch (error) {
    if (['Z_DATA_ERROR', 'Z_NEED_DICT', 'Z_BUF_ERROR'].includes(error.code)) return false;
    throw error;
  } finally {
    inflater.destroy();
  }
}

// Check the still-image stream, not pixel values or ancillary chunk semantics.
async function hasPngImageData({ data, width, height }) {
  let bytes;
  try {
    const decoded = decodeImage(data);
    // Shared checks bound bytes and dimensions and verify chunk extents/CRCs
    // before this adapter reads IHDR/PLTE/IDAT fields. Also excludes animation.
    const size = pngSize(decoded);
    if (size.width !== width || size.height !== height) return false;
    bytes = Buffer.from(decoded.buffer, decoded.byteOffset, decoded.byteLength);
  } catch {
    return false;
  }
  const [depth, color, compression, filter, interlace] = bytes.subarray(24, 29);
  const format = pngFormats.get(color);
  if (!format?.[1].includes(depth) || compression !== 0 || filter !== 0 || interlace > 1)
    return false;

  const passes = [];
  let expected = 0;
  for (const [x, y, dx, dy] of interlace ? adam7 : [[0, 0, 1, 1]]) {
    if (width <= x || height <= y) continue; // Empty passes have no filter bytes either.
    const columns = Math.ceil((width - x) / dx),
      rows = Math.ceil((height - y) / dy),
      stride = 1 + Math.ceil((columns * format[0] * depth) / 8);
    expected += stride * rows;
    passes.push({ stride, rows });
  }
  if (expected < 1 || expected > maxScanlineBytes) return false;

  let compressed,
    length = 0,
    palette = false,
    sawIdat = false,
    closedIdat = false;
  for (let at = 8; at < bytes.length; ) {
    const size = bytes.readUInt32BE(at),
      body = at + 8,
      tag = bytes.toString('latin1', at + 4, body);
    if (tag === 'IDAT') {
      if (closedIdat || (color === 3 && !palette)) return false;
      sawIdat = true;
      if (size) {
        // One bounded buffer, without creating a list entry per chunk.
        compressed ??= Buffer.allocUnsafe(bytes.length);
        length += bytes.copy(compressed, length, body, body + size);
      }
    } else {
      if (sawIdat) closedIdat = true;
      if (tag === 'PLTE') {
        const maxEntries = color === 3 ? 2 ** depth : 256;
        if (
          palette ||
          sawIdat ||
          color === 0 ||
          color === 4 ||
          !size ||
          size % 3 ||
          size / 3 > maxEntries
        )
          return false;
        palette = true;
      }
    }
    at = body + size + 4;
  }
  if (!length) return false;
  return checkScanlines(compressed.subarray(0, length), passes, expected);
}

async function preflightEmbeddedImages(chromiumPath, imageAssets) {
  const assets = Object.values(imageAssets).map(({ data, width, height }) => ({
    data,
    width,
    height,
  }));
  if (!assets.length) return 'valid';
  // Chromium may decode through malformed IDAT. Reject static data failures
  // first, even without a browser; data that passes still needs a real decode.
  try {
    for (const asset of assets) if (!(await hasPngImageData(asset))) return 'rejected';
  } catch {
    return 'unavailable';
  }
  if (!chromiumPath) return 'unavailable';
  let browser,
    status = 'unavailable';
  try {
    // As in svg-to-png.js, load only known checkout modules, fulfilled in-memory
    // with no server and no file URLs. Never load the project or SVG as a page.
    const sources = new Map(
      await Promise.all(
        imageModulePaths.map(async (modulePath) => [
          imageOrigin + modulePath,
          await fs.readFile(new URL(`..${modulePath}`, import.meta.url), 'utf8'),
        ]),
      ),
    );
    const { chromium } = await import('playwright-core');
    browser = await chromium.launch({
      executablePath: chromiumPath,
      headless: true,
      chromiumSandbox: true,
      timeout: 15_000,
      // Fail closed even outside intercepted page traffic; no working proxy
      // or DNS, and no implicit loopback bypass.
      args: [
        '--host-resolver-rules=MAP * ~NOTFOUND',
        '--proxy-server=http://nect-no-network.invalid:9',
        '--proxy-bypass-list=<-loopback>',
        '--disable-quic',
      ],
    });
    let unexpected = false;
    const result = await withinDeadline(
      (async () => {
        const context = await browser.newContext({ serviceWorkers: 'block' });
        await context.route('**/*', async (route) => {
          const request = route.request();
          if (request.method() === 'GET') {
            if (request.url() === imageOrigin + '/')
              return route.fulfill({
                contentType: 'text/html',
                headers: {
                  'Content-Security-Policy': "default-src 'none'; script-src 'self'; img-src data:",
                },
                body: '<!doctype html><title>Nect image preflight</title><link rel="icon" href="data:,">',
              });
            const source = sources.get(request.url());
            if (source !== undefined)
              return route.fulfill({ contentType: 'text/javascript', body: source });
          }
          unexpected = true;
          await route.abort('blockedbyclient');
        });
        const page = await context.newPage();
        page.on('pageerror', () => (unexpected = true));
        await page.goto(imageOrigin + '/');
        return page.evaluate(async (imageAssets) => {
          const { preflightImages } = await import('/src/raster.js');
          if (typeof Image !== 'function' || typeof Image.prototype.decode !== 'function')
            return 'unavailable';
          try {
            await preflightImages({ imageAssets });
            return 'valid';
          } catch {
            return 'rejected';
          }
        }, assets);
      })(),
      30_000,
    );
    if (!unexpected && browser.isConnected()) status = result;
  } catch {
    // Browser, module or evaluation failures are unavailable, not bad assets.
  } finally {
    if (browser) {
      try {
        // Playwright force-kills if graceful close exceeds its 30 s deadline.
        await withinDeadline(browser.close(), 35_000);
      } catch {
        status = 'unavailable';
      }
    }
  }
  return status;
}

/** Only the launcher can choose the root or executable. This is a dedicated,
 * nonconcurrent workspace contract, not a sandbox against a local user racing IO. */
export async function createAgentWorkflowFiles(workspace, { chromiumPath } = {}) {
  const root = localPath(workspace),
    parent = path.dirname(root);
  if (parent === root) fail('UNSAFE_WORKSPACE', 'Choose a dedicated workspace, not a drive root.');
  const [parentId, rootId] = await io(() =>
    Promise.all([directoryInfo(parent), directoryInfo(root)]),
  );
  const assertRoot = async () => {
    if ((await directoryInfo(parent)) !== parentId || (await directoryInfo(root)) !== rootId)
      fail('UNSAFE_WORKSPACE', 'Workspace or its parent was replaced.');
  };
  let chromium;
  if (chromiumPath !== undefined) {
    chromium = await io(async () => {
      const executable = localPath(await fs.realpath(localPath(chromiumPath)));
      if (!(await fs.stat(executable)).isFile())
        fail('PNG_UNAVAILABLE', 'Provide an installed Chromium executable.');
      return executable;
    });
  }

  const bindings = new WeakSet();
  const bind = (fileName, stamp, digest, cleanupWarning) => {
    const binding = Object.freeze({
      token: Symbol(),
      fileName,
      stamp,
      digest,
      cleanupWarning,
    });
    bindings.add(binding);
    return binding;
  };
  const location = (fileName, extension) => path.join(root, leaf(fileName, extension));

  async function assertBinding(filename, binding) {
    let current;
    try {
      current = await readFileBounded(filename, MAX_PROJECT_BYTES);
    } catch (error) {
      if (
        ['ENOENT', 'ENOTDIR', 'ELOOP', 'FILE_CHANGED', 'UNSAFE_FILE', 'FILE_TOO_LARGE'].includes(
          error.code,
        )
      )
        fail(
          'BINDING_CHANGED',
          'The bound file was removed, replaced or externally changed. Use Save As.',
        );
      throw error;
    }
    if (current.stamp !== binding.stamp || digest(current.bytes) !== binding.digest)
      fail(
        'BINDING_CHANGED',
        'The bound file was removed, replaced or externally changed. Use Save As.',
      );
  }

  async function publish(filename, bytes, binding = null) {
    await assertRoot();
    if (!binding) await fresh(filename);
    const temporary = path.join(root, `.nect-workflow-${randomUUID()}.tmp`);
    const handle = await fs.open(temporary, 'wx', 0o600);
    let result;
    try {
      try {
        await handle.writeFile(bytes);
        await handle.sync();
      } finally {
        await handle.close();
      }
      const stagedStamp = stamp(await safeFile(temporary));
      await assertRoot();
      if (binding) {
        await assertBinding(filename, binding);
        // Only this exact live binding is eligible for replacement. This is not
        // an atomic compare-and-swap against a competing same-user writer.
        await fs.rename(temporary, filename);
      } else {
        // Same no-clobber publication as png-output.js. A complete file becomes
        // visible in one link; no unsafe copy/rename fallback on unsupported FS.
        await fs.link(temporary, filename);
      }
      result = { stamp: stagedStamp };
    } finally {
      if (!result || !binding) {
        try {
          await fs.unlink(temporary);
        } catch {
          // Do not mask a primary write failure, nor report a published file as unsaved.
          if (result) result.cleanupWarning = 'Output was published, but staging cleanup failed.';
        }
      }
    }
    return result;
  }

  return Object.freeze({
    open: (fileName) =>
      io(async () => {
        const filename = location(fileName, 'json');
        await assertRoot();
        const record = await readFileBounded(filename, MAX_PROJECT_BYTES);
        let contents;
        try {
          contents = new TextDecoder('utf-8', { fatal: true }).decode(record.bytes);
        } catch {
          fail('INVALID_PROJECT', 'Project must be valid UTF-8 JSON.');
        }
        return { contents, binding: bind(fileName, record.stamp, digest(record.bytes)) };
      }),
    writeProject: ({ contents, binding, fileName }) =>
      io(async () => {
        if (
          binding &&
          (!bindings.has(binding) || (fileName !== undefined && fileName !== binding.fileName))
        )
          fail('BINDING_MISMATCH', 'Save requires this workspace session’s current binding.');
        fileName = binding?.fileName ?? fileName;
        const filename = location(fileName, 'json');
        if (Buffer.byteLength(contents) > MAX_PROJECT_BYTES)
          fail('FILE_TOO_LARGE', 'Project exceeds 50 MB.');
        const bytes = Buffer.from(contents),
          hash = digest(bytes);
        const record = await publish(filename, bytes, binding);
        if (binding) bindings.delete(binding);
        return bind(fileName, record.stamp, hash, record.cleanupWarning);
      }),
    release: (binding) => {
      bindings.delete(binding);
    },
    validateImages: ({ imageAssets }) => preflightEmbeddedImages(chromium, imageAssets),
    assertFresh: (fileName, format) =>
      io(async () => {
        if (format !== 'svg' && format !== 'png')
          fail('UNSUPPORTED_FORMAT', 'Expected SVG or PNG.');
        const filename = location(fileName, format);
        await assertRoot();
        await fresh(filename);
      }),
    exportArtwork: ({ fileName, format, svg }) =>
      io(async () => {
        if (format !== 'svg' && format !== 'png')
          fail('UNSUPPORTED_FORMAT', 'Expected SVG or PNG.');
        const filename = location(fileName, format);
        if (format === 'svg') return (await publish(filename, Buffer.from(svg))).cleanupWarning;
        if (!chromium)
          fail('PNG_UNAVAILABLE', 'Launch with --chromium-path to enable headless PNG.');
        await assertRoot();
        await fresh(filename);
        // Run the existing headless raster adapter in private staging. In particular,
        // it uses readSvgInput, the existing rasterizeSVG, and publishPng. The final
        // destination never receives a partial or failed raster attempt.
        const scratch = await fs.mkdtemp(path.join(root, '.nect-raster-'));
        let published;
        try {
          const source = path.join(scratch, 'artwork.svg'),
            png = path.join(scratch, 'artwork.png');
          await fs.writeFile(source, svg, { flag: 'wx', mode: 0o600 });
          try {
            await run(process.execPath, [rasterCli, source, png, '--chromium-path', chromium], {
              shell: false,
              windowsHide: true,
              timeout: 90_000,
              maxBuffer: 64 * 1024,
            });
          } catch {
            fail('RASTER_FAILED', 'The existing headless SVG-to-PNG adapter failed or timed out.');
          }
          const { bytes } = await readFileBounded(png, MAX_PNG_BYTES);
          published = await publish(filename, bytes);
        } finally {
          try {
            await fs.rm(scratch, { recursive: true, force: true });
          } catch {
            if (published)
              published.cleanupWarning = 'PNG was published, but staging cleanup failed.';
          }
        }
        return published.cleanupWarning;
      }),
  });
}
