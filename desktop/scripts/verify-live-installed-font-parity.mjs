import assert from 'node:assert/strict';
import { createInterface } from 'node:readline';
import { createHash, randomUUID } from 'node:crypto';
import { spawn, execFileSync } from 'node:child_process';
import { readFile, readdir, mkdir, copyFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
const [exePath, repoPath, shapePath, expectedDigest] = process.argv.slice(2);
assert.equal(process.platform, 'win32');
assert.ok(
  /^[a-f0-9]{64}$/i.test(expectedDigest ?? ''),
  'Pass the reviewed acceptance executable SHA-256',
);
const exe = resolve(exePath),
  repo = resolve(repoPath),
  base = dirname(exe);
const sha = (buf) => createHash('sha256').update(buf).digest('hex');
const digest = sha(await readFile(exe));
assert.equal(digest, expectedDigest.toLowerCase());
const { chromium } = await import(
  pathToFileURL(join(repo, 'desktop', 'node_modules', 'playwright-core', 'index.mjs')).href
);
const { document, node, serialize, CommandSession } = await import(
  pathToFileURL(join(repo, 'desktop', 'src', 'core.js')).href
);
const rows = JSON.parse(await readFile(shapePath, 'utf8'));
const raw = rows.find(
  (r) =>
    r.face.family === 'Meiryo' &&
    r.face.weight === 400 &&
    r.face.style === 0 &&
    r.writingMode === 'horizontal-tb' &&
    r.wrapWidth === 160,
);
assert.ok(raw);
const doc = document(),
  layer = doc.layers[0],
  text = node('Text', 110, 80),
  fill = node('Fill', 330, 100);
text.text = raw.text;
text.params.size.value = raw.size;
text.params.lineSpacing.value = raw.lineSpacing;
text.params.letterSpacing.value = raw.tracking;
text.kerning = raw.kerning;
text.alignment = ['start', 'center', 'end'][raw.alignment];
text.params.position.value = [200, 140];
fill.params.path.source = text.id;
layer.nodes.push(text, fill);
layer.nodes.find((n) => n.id === layer.output).params.path.source = fill.id;
const seedOwner = new CommandSession(doc);
seedOwner.dispatch({
  ...seedOwner.state,
  type: 'set-os-font-text',
  payload: {
    layerId: layer.id,
    nodeId: text.id,
    outline: raw,
    settings: {
      ligatures: raw.ligatures,
      textWrap: { width: raw.wrapWidth, policy: raw.wrapPolicy },
    },
  },
});
const seed = serialize(seedOwner.snapshot());
const value = '日本語 A1（12）';
const pause = (ms) => new Promise((r) => setTimeout(r, ms));
async function seek(root, depth) {
  if (depth < 0) return null;
  for (const d of await readdir(root, { withFileTypes: true }).catch(() => [])) {
    const next = join(root, d.name);
    if (d.name === 'DevToolsActivePort' && d.isFile()) return next;
    if (
      d.isDirectory() &&
      !d.isSymbolicLink() &&
      /^(nect-svg-acceptance-|webview2|EBWebView|Default)/.test(d.name)
    ) {
      const got = await seek(next, depth - 1);
      if (got) return got;
    }
  }
}
async function run(gui) {
  const home = join(base, 'g19-meiryo-parity-' + randomUUID());
  await mkdir(home);
  const binary = join(home, 'nect-svg-desktop.exe');
  await copyFile(exe, binary);
  const env = {
    ...process.env,
    TEMP: home,
    TMP: home,
    TMPDIR: home,
    NECT_ACCEPTANCE_EXE: binary,
    NECT_ACCEPTANCE_SHA256: digest,
    NECT_MCP_PREVIEW_READ_ONLY: '0',
    NECT_ACCEPTANCE_ARTWORK: '1',
    NECT_ACCEPTANCE_TEXT: '1',
    NECT_SVG_ACCEPTANCE_JP_FIXTURE: '0',
    WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS:
      '--remote-debugging-address=127.0.0.1 --remote-debugging-port=0',
  };
  for (const k of [
    'WEBVIEW2_PIPE_FOR_SCRIPT_DEBUGGER',
    'NECT_MCP_LOCAL_PIPE_PROOF',
    'NECT_SVG_ACCEPTANCE_ROOT',
  ])
    delete env[k];
  const p = spawn(
    'python',
    ['-B', join(repo, 'desktop', 'scripts', 'live-editor-acceptance-mcp.py')],
    { cwd: home, env, windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] },
  );
  let last = '',
    waiters = new Map(),
    count = 0,
    browser = null;
  p.stderr.on('data', (b) => (last = (last + b.toString()).slice(-1600)));
  p.on('error', (err) => (last = String(err)));
  createInterface({ input: p.stdout }).on('line', (line) => {
    const packet = JSON.parse(line),
      cb = waiters.get(packet.id);
    if (cb) {
      waiters.delete(packet.id);
      cb(packet);
    }
  });
  const send = (obj) => p.stdin.write(JSON.stringify({ jsonrpc: '2.0', ...obj }) + '\n');
  const call = (method, params = {}) =>
    new Promise((ok, no) => {
      const id = ++count;
      const timer = setTimeout(() => {
        waiters.delete(id);
        no(Error('Timed out ' + method + ' ' + last));
      }, 16000);
      waiters.set(id, (r) => {
        clearTimeout(timer);
        if (r.error) no(Error(JSON.stringify(r.error)));
        else ok(r.result);
      });
      send({ id, method, params });
    });
  const tool = async (name, args, status) => {
    const r = await call('tools/call', { name: 'nect_test_' + name, arguments: args });
    assert.equal(r.structuredContent?.status, status, JSON.stringify(r));
    return r.structuredContent;
  };
  try {
    await call('initialize', {
      protocolVersion: '2025-06-18',
      capabilities: {},
      clientInfo: { name: 'isolated-qa', version: '1' },
    });
    send({ method: 'notifications/initialized' });
    await tool('inspect', {}, 'ok');
    let marker = null;
    for (let i = 0; i < 60 && !marker; i++) {
      marker = await seek(home, 5);
      if (!marker) await pause(100);
    }
    assert.ok(marker, 'Only test-owned WebView can be inspected');
    const [n, route] = (await readFile(marker, 'utf8')).trim().split(/\r?\n/);
    assert.match(route, /^\/devtools\/browser\//);
    const port = Number(n);
    assert.ok(port > 1024 && port < 65536);
    const listening = execFileSync('netstat.exe', ['-ano', '-p', 'tcp'], {
      encoding: 'utf8',
      windowsHide: true,
    })
      .split(/\r?\n/)
      .map((x) => x.trim().split(/\s+/))
      .filter((a) => a[1]?.endsWith(':' + port) && a[3] === 'LISTENING');
    assert.ok(
      listening.length &&
        listening.every((a) => a[1] === '127.0.0.1:' + port || a[1] === '[::1]:' + port),
    );
    browser = await chromium.connectOverCDP('http://127.0.0.1:' + port, { timeout: 6000 });
    const pages = browser
      .contexts()
      .flatMap((c) => c.pages())
      .filter((p) => {
        try {
          return new URL(p.url()).hostname === 'tauri.localhost';
        } catch {
          return false;
        }
      });
    assert.equal(pages.length, 1);
    const [page] = pages;
    await page.evaluate((seed) => window.nect.load(seed), seed);
    const getNode = () =>
      page.evaluate(
        ({ l, n }) =>
          window.nect
            .snapshot()
            .layers.find((x) => x.id === l)
            .nodes.find((x) => x.id === n),
        { l: layer.id, n: text.id },
      );
    assert.deepEqual((await getNode()).osFont.face, raw.face);
    const inspect = async () => (await tool('inspect', {}, 'ok')).target;
    let current = await inspect();
    if (gui) {
      const { ensureWorkspacePanel } = await import(
        pathToFileURL(join(repo, 'desktop', 'tests', 'helpers', 'workspace-panels.js')).href
      );
      await ensureWorkspacePanel(page, 'inspector');
      await page
        .locator('.graph .node[data-id="' + text.id + '"] .node-head .type-tag')
        .click({ timeout: 6000 });
      await page
        .locator('.inspector-body textarea[data-os-text="' + text.id + '"]')
        .first()
        .fill(value, { timeout: 6000 });
      await page
        .locator('.inspector-body button[data-apply-os-text="' + text.id + '"]')
        .first()
        .click({ timeout: 6000 });
      await page.waitForFunction(
        ({ l, n, value }) =>
          window.nect
            .snapshot()
            .layers.find((x) => x.id === l)
            .nodes.find((x) => x.id === n)?.text === value,
        { l: layer.id, n: text.id, value },
        { timeout: 10000 },
      );
    } else {
      const args = { target: current, layerId: layer.id, nodeId: text.id, text: value };
      await tool('set_text_content', args, 'changed');
      await tool('set_text_content', args, 'stale-target');
    }
    const updated = await getNode();
    assert.equal(updated.text, value);
    assert.deepEqual(updated.osFont.face, raw.face);
    assert.equal(updated.osFont.source, undefined);
    current = await inspect();
    const undo = await tool('undo', { target: current }, 'changed');
    assert.equal((await getNode()).text, raw.text);
    await tool('redo', { target: undo.target }, 'changed');
    assert.deepEqual(await getNode(), updated);
    current = await inspect();
    await tool('save_copy', { target: current }, 'accepted-by-writer');
    await tool('reopen_copy', { target: current }, 'reopened');
    assert.deepEqual(await getNode(), updated);
    current = await inspect();
    await tool('export_svg', { target: current }, 'accepted-by-writer');
    await tool('export_png', { target: current }, 'accepted-by-writer');
    const dirs = (await readdir(home, { withFileTypes: true })).filter(
      (d) => d.isDirectory() && /^nect-svg-acceptance-[a-f0-9]{32}$/.test(d.name),
    );
    assert.equal(dirs.length, 1);
    const file = (role) => readFile(join(home, dirs[0].name, 'documents', role));
    const svg = await file('live-api-export.svg'),
      png = await file('live-api-export.png');
    assert.match(svg.toString(), /<path\b/);
    assert.equal(png.toString('hex', 0, 8), '89504e470d0a1a0a');
    return { svg: sha(svg), png: sha(png), node: updated };
  } finally {
    p.stdin.end();
    for (let i = 0; i < 35 && p.exitCode === null && p.signalCode === null; i++) await pause(100);
    if (browser) await browser.close().catch(() => {});
    if (p.exitCode === null && p.signalCode === null) {
      p.kill();
      throw Error('Owned verification bridge did not exit');
    }
  }
}
const viaGui = await run(true),
  viaMcp = await run(false);
assert.deepEqual(viaGui, viaMcp);
console.log(
  JSON.stringify({
    face: viaGui.node.osFont.face.family,
    style: viaGui.node.osFont.face.style,
    weight: viaGui.node.osFont.face.weight,
    guiVsMcp: true,
    undoRedo: true,
    copyReopened: true,
    svg: viaGui.svg,
    png: viaGui.png,
  }),
);
