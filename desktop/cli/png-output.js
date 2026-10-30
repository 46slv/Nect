import fs from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import { basename, dirname, join } from 'node:path';

export async function requireNewOutput(output, io = fs) {
  if (!(await io.stat(dirname(output))).isDirectory()) throw Error('Output directory must exist');
  try {
    await io.lstat(output); // A dangling symlink also counts as an existing output.
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  throw Error('Output already exists; choose a new path');
}

// No ordinary rename: it would overwrite a competing writer's finished output.
// A same-directory hard link atomically publishes the closed, complete file or
// fails without clobbering. Unsupported filesystems fail closed; no copy fallback.
export async function publishPng(output, bytes, io = fs) {
  const temporary = join(dirname(output), `.${basename(output)}.${randomUUID()}.tmp`);
  let handle,
    owned = false;
  try {
    handle = await io.open(temporary, 'wx', 0o600);
    owned = true;
    await handle.writeFile(bytes);
    await handle.close();
    handle = null;
    await io.link(temporary, output);
  } finally {
    try {
      await handle?.close();
    } finally {
      if (owned) await io.unlink(temporary);
    }
  }
}

export async function readSvgInput(input, maxBytes, io = fs) {
  const handle = await io.open(input, 'r');
  try {
    const stat = await handle.stat();
    if (!stat.isFile()) throw Error('SVG input must be a regular file');
    if (stat.size > maxBytes) throw Error('SVG input exceeds 50 MB');
    // Read at most the limit plus one byte even if the source grows after stat.
    const chunks = [];
    let total = 0;
    while (true) {
      const buffer = Buffer.alloc(Math.min(64 * 1024, maxBytes + 1 - total));
      const { bytesRead } = await handle.read(buffer);
      if (!bytesRead) break;
      chunks.push(buffer.subarray(0, bytesRead));
      total += bytesRead;
      if (total > maxBytes) throw Error('SVG input exceeds 50 MB');
    }
    return new TextDecoder('utf-8', { fatal: true }).decode(Buffer.concat(chunks));
  } finally {
    await handle.close();
  }
}
