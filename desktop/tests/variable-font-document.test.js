import { createCharacterPositions } from '../src/character-identity.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { Worker } from 'node:worker_threads';
import {
  document,
  node,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
  createVariableDocumentPreparer,
} from '../src/core.js';
import { makeVariableFontAsset, makeFontAsset, resolveFonts } from '../src/fonts.js';
const fixture = fs.readFileSync(
  new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
);
const bytes = () =>
  fixture.buffer.slice(fixture.byteOffset, fixture.byteOffset + fixture.byteLength);
const workerFactory = () => {
  const w = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url));
  const bridge = { postMessage: (data) => w.postMessage(data), terminate: () => w.terminate() };
  w.on('message', (data) => bridge.onmessage?.({ data }));
  w.on('error', (e) => bridge.onerror?.(e));
  return bridge;
};
function scene(count = 1) {
  const doc = document(),
    text = node('Text'),
    out = node('Output');
  const asset = makeVariableFontAsset(bytes(), 'Roboto.ttf');
  doc.schemaVersion = 25;
  doc.fontAssets = { [asset.id]: asset.asset };
  text.font = asset.id;
  text.text = 'AV';
  text.variation = { version: 1, fontId: asset.id, axes: {} };
  text.params.size.value = 100;
  out.params.path.source = text.id;
  doc.layers[0].nodes = [text, out];
  doc.layers[0].output = out.id;
  for (let i = 1; i < count; i++) {
    const extra = node('Text');
    Object.assign(extra, {
      font: asset.id,
      text: 'A'.repeat(i + 1),
      variation: { version: 1, fontId: asset.id, axes: {} },
    });
    doc.layers[0].nodes.push(extra);
  }
  return { doc, text, asset, layerId: doc.layers[0].id };
}
async function open(doc) {
  const bridge = createVariableDocumentPreparer({ workerFactory });
  const ticket = await bridge.prepareDocument(doc);
  bridge.activate(ticket);
  const session = new CommandSession(doc, { fonts: () => bridge.fonts() });
  return { bridge, session };
}
const svg = (session, bridge) => exportSVG(session.snapshot(), bridge.fonts());
const edit = (session, bridge, payload) =>
  session.prepareCommand({ ...session.state, type: 'set-variable-font', payload }, bridge);

test('variable document real axes, immutable bytes, exact cache, JSON reopen and history', async () => {
  const { doc, text, asset, layerId } = scene();
  assert.throws(() => new CommandSession(doc), { code: 'PREPARATION_REQUIRED' });
  assert.equal(evaluate(doc.layers[0], resolveFonts(doc)).errorCode, 'PREPARATION_REQUIRED');
  assert.throws(() => exportSVG(doc), { code: 'PREPARATION_REQUIRED' });
  const { bridge, session } = await open(doc);
  try {
    const initial = svg(session, bridge),
      outputs = [];
    for (const wght of [100, 400, 900, 550]) {
      const token = await edit(session, bridge, {
        layerId,
        nodeId: text.id,
        fontId: asset.id,
        axes: { wght },
      });
      session.commitPrepared(token);
      outputs.push(svg(session, bridge));
      assert.equal(session.snapshot().fontAssets[asset.id].data, asset.asset.data);
      assert.equal(bridge.stats().assets, 1);
    }
    assert.equal(outputs[1], initial);
    assert.equal(new Set(outputs).size, 4);
    const beforeWidth = svg(session, bridge);
    session.commitPrepared(
      await edit(session, bridge, {
        layerId,
        nodeId: text.id,
        fontId: asset.id,
        axes: { wdth: 75 },
      }),
    );
    assert.notEqual(svg(session, bridge), beforeWidth);
    session.commitPrepared(
      await session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
    );
    assert.equal(svg(session, bridge), beforeWidth);
    session.commitPrepared(
      await session.prepareCommand({ ...session.state, type: 'redo' }, bridge),
    );
    const saved = serialize(session.snapshot()),
      expected = svg(session, bridge);
    bridge.dispose();
    const reopened = await open(deserialize(saved));
    try {
      assert.equal(svg(reopened.session, reopened.bridge), expected);
    } finally {
      reopened.bridge.dispose();
    }
  } finally {
    bridge.dispose();
  }
});

test('candidate failure/cancel/full cache replacement leaves active export exact; stale commit rejects', async () => {
  const { doc, text, asset, layerId } = scene(8);
  const { bridge, session } = await open(doc);
  try {
    const before = svg(session, bridge);
    const replacement = session.snapshot();
    for (const n of replacement.layers[0].nodes)
      if (n.type === 'Text') n.variation.axes = { wght: 900 };
    await bridge.prepareDocument(replacement);
    bridge.cancel();
    assert.equal(svg(session, bridge), before);
    const brokenReplacement = structuredClone(replacement);
    for (const n of brokenReplacement.layers[0].nodes)
      if (n.type === 'Text') n.variation.axes = { wght: 100 };
    brokenReplacement.layers[0].nodes.at(-1).text = 'ffi';
    await assert.rejects(bridge.prepareDocument(brokenReplacement), {
      code: 'UNSUPPORTED_MAPPING',
    });
    assert.equal(svg(session, bridge), before);
    await assert.rejects(
      edit(session, bridge, { layerId, nodeId: text.id, fontId: asset.id, axes: { NOPE: 1 } }),
      { code: 'INVALID_AXES' },
    );
    assert.equal(svg(session, bridge), before);
    const token = await edit(session, bridge, {
      layerId,
      nodeId: text.id,
      fontId: asset.id,
      axes: { wght: 550 },
    });
    session.dispatch({
      ...session.state,
      type: 'set-parameter',
      payload: { layerId, nodeId: text.id, key: 'size', value: 120 },
    });
    assert.throws(() => session.commitPrepared(token), /STALE_REVISION/);
    assert.match(svg(session, bridge), /<path/);
    const controller = new AbortController();
    controller.abort();
    await assert.rejects(bridge.prepareDocument(replacement, { signal: controller.signal }), {
      code: 'CANCELLED',
    });
    assert.match(svg(session, bridge), /<path/);
  } finally {
    bridge.dispose();
  }
});

test('specific variable unsupported modes and unprepared synchronous text edit are atomic', async () => {
  const { doc, text, asset, layerId } = scene();
  const { bridge, session } = await open(doc);
  try {
    const before = serialize(session.snapshot());
    assert.throws(
      () =>
        session.dispatch({
          ...session.state,
          type: 'set-text-content',
          payload: { layerId, nodeId: text.id, text: 'AA' },
        }),
      { code: 'PREPARATION_REQUIRED' },
    );
    assert.equal(serialize(session.snapshot()), before);
    for (const [settings, code] of [
      [{ writingMode: 'vertical-rl' }, 'VARIABLE_VERTICAL_UNSUPPORTED'],
      [{ textWrap: { width: 100 } }, 'VARIABLE_WRAP_UNSUPPORTED'],
      [{ kerning: false }, 'VARIABLE_FEATURES_UNSUPPORTED'],
    ]) {
      await assert.rejects(
        session.prepareCommand(
          {
            ...session.state,
            type: 'set-text-settings',
            payload: { layerId, nodeId: text.id, settings },
          },
          bridge,
        ),
        new RegExp(code),
      );
    }
    await assert.rejects(
      session.prepareCommand(
        {
          ...session.state,
          type: 'set-text-content',
          payload: { layerId, nodeId: text.id, text: 'ffi' },
        },
        bridge,
      ),
      { code: 'UNSUPPORTED_MAPPING' },
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.match(svg(session, bridge), /<path/);
    const changed = session.snapshot();
    changed.layers[0].nodes[0].variation.fontId = 'font-other';
    assert.throws(() => deserialize(serialize(changed)), { code: 'VARIABLE_FONT_MISMATCH' });
  } finally {
    bridge.dispose();
  }
});

test('prepared preview is detached, position/size reuse units, bake freezes current variable outline', async () => {
  const { doc, text, asset, layerId } = scene();
  const { bridge, session } = await open(doc);
  try {
    const token = await edit(session, bridge, {
      layerId,
      nodeId: text.id,
      fontId: asset.id,
      axes: { wght: 550 },
    });
    const preview = session.preparedSnapshot(token);
    const previewSVG = exportSVG(preview.document, preview.fonts);
    assert.notEqual(previewSVG, svg(session, bridge));
    preview.document.layers[0].nodes[0].text = 'bad';
    session.commitPrepared(token);
    assert.equal(svg(session, bridge), previewSVG);
    const run = bridge.fonts()[asset.id].readVariable({ text: 'AV', axes: { wght: 550 } });
    const stats = bridge.stats();
    session.dispatch({
      ...session.state,
      type: 'set-parameter',
      payload: { layerId, nodeId: text.id, key: 'size', value: 150 },
    });
    session.dispatch({
      ...session.state,
      type: 'set-parameter',
      payload: { layerId, nodeId: text.id, key: 'position', value: [30, 40] },
    });
    assert.equal(bridge.fonts()[asset.id].readVariable({ text: 'AV', axes: { wght: 550 } }), run);
    assert.equal(bridge.stats().runs, stats.runs);
    const beforeBake = svg(session, bridge);
    session.dispatch({
      ...session.state,
      type: 'bake-text-outline',
      payload: { layerId, nodeId: text.id },
    });
    assert.equal(svg(session, bridge), beforeBake);
    assert.equal(session.snapshot().layers[0].nodes[0].id, text.id);
    assert.equal(session.snapshot().layers[0].nodes[0].type, 'Multi Path');
    session.commitPrepared(
      await session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
    );
    bridge.dispose();
    assert.throws(
      () =>
        session.dispatch({
          ...session.state,
          type: 'bake-text-outline',
          payload: { layerId, nodeId: text.id },
        }),
      { code: 'PREPARATION_REQUIRED' },
    );
  } finally {
    bridge.dispose();
  }
});

test('known unit metrics, canonical defaults, immutable bytes, explicit static font switch and locks', async () => {
  const { doc, text, asset, layerId } = scene();
  text.text = 'A';
  const { bridge, session } = await open(doc);
  try {
    for (const [wght, advance] of [
      [100, 1223],
      [400, 1336],
      [900, 1395],
    ]) {
      session.commitPrepared(
        await edit(session, bridge, { layerId, nodeId: text.id, fontId: asset.id, axes: { wght } }),
      );
      const r = bridge.fonts()[asset.id].readVariable({ text: 'A', axes: { wght } });
      assert.equal(r.advance, advance);
      const layout = evaluate(
        session.snapshot().layers[0],
        resolveFonts(session.snapshot(), bridge.fonts()),
      ).textLayout(text.id);
      assert.equal(layout.glyphAdvances[0], (advance * 100) / 2048);
      if (wght === 400) assert.deepEqual(session.snapshot().layers[0].nodes[0].variation.axes, {});
    }
    const b = fs.readFileSync(
      new URL('./fixtures/fonts/NectJPFixture-Regular.otf', import.meta.url),
    );
    const staticAsset = makeFontAsset(
      b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength),
      'static.otf',
      session.snapshot().fontAssets,
    );
    session.commitPrepared(
      await edit(session, bridge, {
        layerId,
        nodeId: text.id,
        fontId: staticAsset.id,
        axes: null,
        asset: staticAsset,
      }),
    );
    assert.equal(session.snapshot().layers[0].nodes[0].variation, undefined);
    assert.match(svg(session, bridge), /<path/);
    session.commitPrepared(
      await session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
    );
    assert.equal(session.snapshot().layers[0].nodes[0].variation.fontId, asset.id);
    const locked = session.snapshot();
    locked.layers[0].locked = true;
    const ticket = await bridge.prepareDocument(locked);
    assert.match(exportSVG(bridge.preparedDocument(ticket), bridge.preparedFonts(ticket)), /<path/);
    bridge.activate(ticket);
    const lockedSession = new CommandSession(locked, { fonts: () => bridge.fonts() });
    await assert.rejects(
      edit(lockedSession, bridge, {
        layerId,
        nodeId: text.id,
        fontId: asset.id,
        axes: { wght: 100 },
      }),
      /locked/,
    );
  } finally {
    bridge.dispose();
  }
});

test('candidate abort releases only its bank; superseding failed preparation invalidates prior ticket', async () => {
  const { doc, text, asset, layerId } = scene();
  const { bridge, session } = await open(doc);
  try {
    const before = svg(session, bridge);
    const controller = new AbortController();
    const token = await session.prepareCommand(
      {
        ...session.state,
        type: 'set-variable-font',
        payload: { layerId, nodeId: text.id, fontId: asset.id, axes: { wght: 100 } },
      },
      bridge,
      { signal: controller.signal },
    );
    assert.equal(bridge.stats().candidate.runs, 1);
    controller.abort();
    assert.equal(bridge.stats().candidate, null);
    assert.throws(() => session.commitPrepared(token), /STALE_RESULT/);
    assert.equal(svg(session, bridge), before);
    const ticket = await bridge.prepareDocument(doc);
    const invalid = structuredClone(doc);
    invalid.layers[0].nodes[0].variation.axes = { wght: Infinity };
    await assert.rejects(bridge.prepareDocument(invalid), { code: 'INVALID_AXES' });
    assert.throws(() => bridge.activate(ticket), /STALE_RESULT/);
    assert.equal(svg(session, bridge), before);
    const tooMany = scene(9).doc;
    await assert.rejects(bridge.prepareDocument(tooMany), { code: 'VARIABLE_DOCUMENT_RUN_LIMIT' });
    assert.equal(svg(session, bridge), before);
  } finally {
    bridge.dispose();
  }
});

test('axes and verified text edits retain logical character identity; latest async candidate wins', async () => {
  const { doc, text, asset, layerId } = scene();
  text.characterPositions = createCharacterPositions(text.text);
  const originalIds = text.characterPositions.units.map((u) => u.id);
  const { bridge, session } = await open(doc);
  try {
    const obsolete = edit(session, bridge, {
      layerId,
      nodeId: text.id,
      fontId: asset.id,
      axes: { wght: 100 },
    });
    const rejection = assert.rejects(obsolete, /STALE/);
    const latest = await edit(session, bridge, {
      layerId,
      nodeId: text.id,
      fontId: asset.id,
      axes: { wght: 900 },
    });
    await rejection;
    session.commitPrepared(latest);
    assert.deepEqual(
      session.snapshot().layers[0].nodes[0].characterPositions.units.map((u) => u.id),
      originalIds,
    );
    const token = await session.prepareCommand(
      {
        ...session.state,
        type: 'set-text-content',
        payload: {
          layerId,
          nodeId: text.id,
          text: 'AAV',
          selectionStart: 1,
          selectionEnd: 1,
          inputType: 'insertText',
        },
      },
      bridge,
    );
    session.commitPrepared(token);
    const units = session.snapshot().layers[0].nodes[0].characterPositions.units;
    assert.equal(units[0].id, originalIds[0]);
    assert.equal(units[2].id, originalIds[1]);
    session.commitPrepared(
      await session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
    );
    assert.deepEqual(
      session.snapshot().layers[0].nodes[0].characterPositions.units.map((u) => u.id),
      originalIds,
    );
    assert.equal(session.snapshot().fontAssets[asset.id].data, asset.asset.data);
  } finally {
    bridge.dispose();
  }
});
