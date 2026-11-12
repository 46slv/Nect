import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { Worker } from 'node:worker_threads';
import {
  blankDocument,
  node,
  deserialize,
  exportSVG,
  createVariableDocumentPreparer,
} from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

const file = fs.readFileSync(
  new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
);
const asset = makeVariableFontAsset(
  file.buffer.slice(file.byteOffset, file.byteOffset + file.byteLength),
  'Roboto.ttf',
);
const workerFactory = () => {
  const worker = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url));
  const transport = {
    postMessage: (data) => worker.postMessage(data),
    terminate: () => worker.terminate(),
  };
  worker.on('message', (data) => transport.onmessage?.({ data }));
  worker.on('error', (error) => transport.onerror?.(error));
  return transport;
};
const newBridge = () => createVariableDocumentPreparer({ workerFactory });
const snapshot = (owner, bridge) => ({
  project: serializeCompositionProject(owner.projectSnapshot()),
  state: owner.state,
  history: owner.historyState,
  active: owner.activeCompositionId,
  svg: exportSVG(owner.snapshot(), bridge.fonts()),
  bank: bridge.stats().active,
});

async function makeSavedProject({ firstText = false } = {}) {
  const first = blankDocument(),
    second = blankDocument();
  const textB = node('Text');
  textB.text = 'AV';
  second.layers[0].nodes.push(textB);
  second.layers[0].nodes[0].params.path.source = textB.id;
  let textA;
  if (firstText) {
    textA = node('Text');
    textA.text = 'AA';
    first.layers[0].nodes.push(textA);
    first.layers[0].nodes[0].params.path.source = textA.id;
  }
  const project = migrateLegacyDocumentToCompositionProject(first);
  const firstId = project.compositions[0].id;
  const secondComposition = migrateLegacyDocumentToCompositionProject(second).compositions[0];
  secondComposition.id = 'variable-composition-b';
  project.compositions.push(secondComposition);
  const secondId = secondComposition.id;
  const bridge = newBridge();
  try {
    const owner = createCompositionSessionOwner(project, {
      activeCompositionId: secondId,
      fonts: () => bridge.fonts(),
    });
    const setVariable = async (text, layerId) => {
      const token = await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-variable-font',
          payload: { layerId, nodeId: text.id, fontId: asset.id, axes: { wght: 550 }, asset },
        },
        bridge,
      );
      owner.commitPrepared(token);
    };
    await setVariable(textB, second.layers[0].id);
    const expectedB = exportSVG(owner.snapshot(), bridge.fonts());
    if (firstText) {
      owner.switchComposition(firstId);
      await setVariable(textA, first.layers[0].id);
    }
    return {
      json: serializeCompositionProject(owner.projectSnapshot()),
      expectedB,
      firstId,
      secondId,
      layerB: second.layers[0].id,
      textB: textB.id,
    };
  } finally {
    bridge.dispose();
  }
}

async function openSaved(fixture, options = {}) {
  const project = deserializeCompositionProject(fixture.json, deserialize),
    bridge = newBridge();
  const first = activeCompositionDocument(project, fixture.firstId);
  bridge.activate(await bridge.prepareDocument(first));
  return {
    bridge,
    owner: createCompositionSessionOwner(project, {
      activeCompositionId: fixture.firstId,
      fonts: () => bridge.fonts(),
      ...options,
    }),
  };
}

test('a reentrant Macro classification cannot publish a stale prepared History font bank', async () => {
  const fixture = await makeSavedProject();
  let owner,
    trigger = false;
  const opened = await openSaved(fixture, {
    macroProjectAdmission: () => {
      if (trigger) {
        trigger = false;
        owner.switchComposition(fixture.firstId);
      }
      return false;
    },
  });
  owner = opened.owner;
  const { bridge } = opened;
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    owner.commitPrepared(
      await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
      ),
    );
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    const beforeProject = serializeCompositionProject(owner.projectSnapshot()),
      beforeHistory = owner.historyState,
      beforeBank = bridge.stats().active,
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge);
    assert.equal(beforeHistory.canRedo, true);
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
  } finally {
    bridge.dispose();
  }
});

test('prepared History with Macro admission retains live fonts and the node clipboard', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const created = owner.dispatch({
      ...owner.state,
      type: 'create-node',
      payload: { layerId: fixture.layerB, nodeType: 'Value' },
    });
    owner.dispatch({
      ...owner.state,
      type: 'copy-nodes',
      payload: { layerId: fixture.layerB, nodeIds: [created.nodeId] },
    });
    owner.commitPrepared(
      await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
      ),
    );
    const edited = exportSVG(owner.snapshot(), bridge.fonts());
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge));
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), edited);
    const pasted = owner.dispatch({
      ...owner.state,
      type: 'paste-nodes',
      payload: { layerId: fixture.layerB },
    });
    assert.equal(pasted.nodeIds.length, 1);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((entry) => entry.id === pasted.nodeIds[0]).type,
      'Value',
    );
  } finally {
    bridge.dispose();
  }
});

test('real variable Text in a second Composition opens, edits, round-trips and switches without History edits', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  try {
    owner.renameComposition(fixture.firstId, 'First changed');
    owner.dispatch({ ...owner.state, type: 'undo' });
    const before = snapshot(owner, bridge);
    assert.equal(before.history.canRedo, true);
    let reset = 0;
    assert.throws(
      () => owner.switchComposition(fixture.secondId, { beforeCommit: () => reset++ }),
      { code: 'PREPARATION_REQUIRED' },
    );
    assert.equal(reset, 0);
    assert.deepEqual(snapshot(owner, bridge), before);
    const token = await owner.prepareCompositionSwitch(fixture.secondId, bridge);
    assert.deepEqual(snapshot(owner, bridge), before);
    owner.commitPreparedCompositionSwitch(token, { beforeCommit: () => reset++ });
    assert.equal(reset, 1);
    assert.equal(owner.activeCompositionId, fixture.secondId);
    assert.deepEqual(owner.historyState, before.history);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before.project);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
    owner.commitPrepared(
      await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
      ),
    );
    const edited = exportSVG(owner.snapshot(), bridge.fonts());
    assert.notEqual(edited, fixture.expectedB);
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge));
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), edited);
    const saved = serializeCompositionProject(owner.projectSnapshot());
    owner.switchComposition(fixture.firstId);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    const reopened = await openSaved({ ...fixture, json: saved });
    try {
      reopened.owner.commitPreparedCompositionSwitch(
        await reopened.owner.prepareCompositionSwitch(fixture.secondId, reopened.bridge),
      );
      assert.equal(exportSVG(reopened.owner.snapshot(), reopened.bridge.fonts()), edited);
    } finally {
      reopened.bridge.dispose();
    }
  } finally {
    bridge.dispose();
  }
});

test('failed, cancelled, stale and superseded preparations preserve the active bank and History', async () => {
  const fixture = await makeSavedProject({ firstText: true });
  const { owner, bridge } = await openSaved(fixture);
  try {
    const before = snapshot(owner, bridge);
    const controller = new AbortController();
    const canceled = owner.prepareCompositionSwitch(fixture.secondId, bridge, {
      signal: controller.signal,
    });
    controller.abort();
    owner.cancelPreparedCompositionSwitch(bridge);
    await assert.rejects(canceled);
    assert.deepEqual(snapshot(owner, bridge), before);

    const broken = deserializeCompositionProject(fixture.json, deserialize);
    broken.compositions[1].layers[0].nodes.find((entry) => entry.id === fixture.textB).text = 'ffi';
    const brokenOwner = createCompositionSessionOwner(broken, { fonts: () => bridge.fonts() });
    await assert.rejects(brokenOwner.prepareCompositionSwitch(fixture.secondId, bridge), {
      code: 'UNSUPPORTED_MAPPING',
    });
    assert.deepEqual(snapshot(owner, bridge), before);

    const stale = await owner.prepareCompositionSwitch(fixture.secondId, bridge);
    owner.renameComposition(fixture.firstId, 'Changed during preparation');
    const changed = snapshot(owner, bridge);
    assert.throws(() => owner.commitPreparedCompositionSwitch(stale), /STALE_REVISION/);
    owner.cancelPreparedCompositionSwitch(bridge);
    assert.deepEqual(snapshot(owner, bridge), changed);

    const removed = await owner.prepareCompositionSwitch(fixture.secondId, bridge);
    owner.deleteComposition(fixture.secondId);
    const afterRemoval = snapshot(owner, bridge);
    assert.throws(() => owner.commitPreparedCompositionSwitch(removed), /STALE_REVISION/);
    owner.cancelPreparedCompositionSwitch(bridge);
    assert.deepEqual(snapshot(owner, bridge), afterRemoval);
    owner.dispatch({ ...owner.state, type: 'undo' });

    const old = owner.prepareCompositionSwitch(fixture.secondId, bridge);
    const newest = owner.prepareCompositionSwitch(fixture.secondId, bridge);
    await assert.rejects(old);
    const current = await newest;
    const latestBefore = snapshot(owner, bridge);
    owner.commitPreparedCompositionSwitch(current);
    assert.equal(owner.activeCompositionId, fixture.secondId);
    assert.deepEqual(owner.historyState, latestBefore.history);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});
