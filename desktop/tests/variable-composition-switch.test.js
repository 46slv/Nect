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

for (const mode of ['new-ticket', 'gesture', 'busy', 'late-abort']) {
  test(`final signal freshness rejects ${mode} reentry before any prepared text publication`, async () => {
    const fixture = await makeSavedProject();
    let busy = false,
      abortOnBusy = false;
    const controller = new AbortController();
    const { owner, bridge } = await openSaved(fixture, {
      busy: () => {
        if (abortOnBusy) {
          abortOnBusy = false;
          controller.abort();
        }
        return busy;
      },
    });
    const signal = controller.signal,
      remove = signal.removeEventListener.bind(signal),
      nativeAborted = Object.getOwnPropertyDescriptor(AbortSignal.prototype, 'aborted').get;
    let armed = false,
      detached = false,
      reads = 0,
      newer;
    signal.removeEventListener = (...args) => {
      remove(...args);
      if (armed) detached = true;
    };
    Object.defineProperty(signal, 'aborted', {
      get() {
        const aborted = nativeAborted.call(this);
        if (armed && detached && ++reads === 2) {
          if (mode === 'new-ticket') newer = bridge.prepareDocument(owner.snapshot());
          else if (mode === 'busy') busy = true;
          else if (mode === 'late-abort') abortOnBusy = true;
          else
            owner.dispatch({
              ...owner.state,
              type: 'begin-gesture',
              payload: {
                target: {
                  kind: 'nodes-translate',
                  layerId: fixture.layerB,
                  nodeIds: [fixture.textB],
                },
              },
            });
        }
        return aborted;
      },
    });
    try {
      owner.commitPreparedCompositionSwitch(
        await owner.prepareCompositionSwitch(fixture.secondId, bridge),
      );
      const definition = {
        format: 'nect.macro',
        formatVersion: 1,
        id: 'double',
        revision: 1,
        name: 'Double',
        input: { type: 'Path', nodeId: 'a', socket: 'path' },
        output: { type: 'Path', nodeId: 'b', port: null },
        nodes: ['a', 'b'].map((id) => ({
          id,
          type: 'Transform',
          params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
        })),
        edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
        parameters: [
          {
            id: 'scale',
            label: 'Scale',
            nodeId: 'a',
            key: 'scale',
            type: 'Scalar',
            defaultValue: 1,
          },
        ],
      };
      owner.dispatch({
        ...owner.state,
        type: 'register-macro-definition',
        payload: { definition },
      });
      const before = serializeCompositionProject(owner.projectSnapshot()),
        state = owner.state,
        history = owner.historyState,
        bank = bridge.stats().active,
        token = await owner.prepareCommand(
          {
            ...owner.state,
            type: 'set-text-content',
            payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
          },
          bridge,
          { signal },
        );
      armed = true;
      let failure;
      try {
        owner.commitPrepared(token);
      } catch (error) {
        failure = error;
      }
      const nextTicket = newer ? await newer : null;
      assert.match(failure?.message ?? '', /STALE|GESTURE_ACTIVE/);
      assert.equal(serializeCompositionProject(owner.projectSnapshot()) === before, true);
      assert.deepEqual(owner.state, state);
      assert.deepEqual(owner.historyState, history);
      assert.deepEqual(bridge.stats().active, bank);
      assert.equal(
        owner.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
        'AV',
      );
      assert.equal(owner.hasGesture, mode === 'gesture');
      if (nextTicket) assert.doesNotThrow(() => bridge.assertFresh(nextTicket));
      else assert.equal(bridge.stats().candidate, null);
    } finally {
      bridge.dispose();
    }
  });
}

test('an unregistered signal cannot replace an existing prepared document ticket', async () => {
  const fixture = await makeSavedProject(),
    project = deserializeCompositionProject(fixture.json, deserialize),
    document = activeCompositionDocument(project, fixture.secondId),
    bridge = newBridge();
  try {
    const ticket = await bridge.prepareDocument(document),
      proxySignal = new Proxy(new AbortController().signal, {});
    await assert.rejects(
      bridge.prepareDocument(document, { signal: proxySignal }),
      /INVALID_ABORT_SIGNAL/,
    );
    assert.doesNotThrow(() => bridge.assertFresh(ticket));
    bridge.activate(ticket);
    assert.equal(exportSVG(document, bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('an unregistered signal cannot invalidate a prepared text command', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const ticket = await owner.prepareCommand(
      {
        ...owner.state,
        type: 'set-text-content',
        payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
      },
      bridge,
    );
    await assert.rejects(
      owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AX' },
        },
        bridge,
        { signal: new AbortController().signal },
      ),
      /INVALID_ABORT_SIGNAL/,
    );
    owner.commitPrepared(ticket);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AW',
    );
  } finally {
    bridge.dispose();
  }
});

test('an unregistered signal cannot invalidate a prepared Composition switch', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  try {
    const ticket = await owner.prepareCompositionSwitch(fixture.secondId, bridge);
    await assert.rejects(
      owner.prepareCompositionSwitch(fixture.secondId, bridge, {
        signal: new Proxy(new AbortController().signal, {}),
      }),
      /INVALID_ABORT_SIGNAL/,
    );
    owner.commitPreparedCompositionSwitch(ticket);
    assert.equal(owner.activeCompositionId, fixture.secondId);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('prepared Composition switch rejects teardown reentry before publishing its font bank', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      owner.renameComposition(fixture.firstId, 'Concurrent');
    }
  };
  try {
    const bank = bridge.stats().active,
      token = await owner.prepareCompositionSwitch(fixture.secondId, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPreparedCompositionSwitch(token), /STALE/);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(owner.projectSnapshot().compositions[0].name, 'Concurrent');
    assert.deepEqual(bridge.stats().active, bank);
    assert.equal(bridge.stats().candidate, null);
  } finally {
    bridge.dispose();
  }
});

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

test('AbortSignal cleanup cannot publish a stale prepared History or font bank', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      owner.switchComposition(fixture.firstId);
    }
  };
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
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
    owner.switchComposition(fixture.secondId);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('prepared text teardown cannot expose its uncommitted edit to a reentrant Project edit', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false,
    observed;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      observed = owner.snapshot().layers[0].nodes.find((entry) => entry.id === fixture.textB).text;
      owner.dispatch({
        ...owner.state,
        type: 'set-document-settings',
        payload: { key: 'width', value: 777 },
      });
    }
  };
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const before = owner.snapshot(),
      beforeBank = bridge.stats().active,
      token = await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
        { signal },
      );
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    assert.equal(observed, 'AV');
    assert.equal(
      owner.snapshot().layers[0].nodes.find((entry) => entry.id === fixture.textB).text,
      'AV',
    );
    assert.equal(owner.snapshot().width, 777);
    assert.deepEqual(bridge.stats().active, beforeBank);
    assert.equal(bridge.stats().candidate, null);
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    assert.equal(owner.snapshot().width, before.width);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((entry) => entry.id === fixture.textB).text,
      'AV',
    );
  } finally {
    bridge.dispose();
  }
});

test('abort during prepared text teardown preserves Project, History and active font bank', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      controller.abort();
    }
  };
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const before = serializeCompositionProject(owner.projectSnapshot()),
      beforeHistory = owner.historyState,
      beforeBank = bridge.stats().active,
      token = await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
        { signal },
      );
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE|CANCEL/);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
    assert.equal(bridge.stats().candidate, null);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('reentrant prepared text supersedes the old ticket without losing the new one', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false,
    newer;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      newer = owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AX' },
        },
        bridge,
      );
    }
  };
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const token = await owner.prepareCommand(
      {
        ...owner.state,
        type: 'set-text-content',
        payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
      },
      bridge,
      { signal },
    );
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    const newToken = await newer;
    owner.commitPrepared(newToken);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((entry) => entry.id === fixture.textB).text,
      'AX',
    );
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    assert.equal(
      owner.snapshot().layers[0].nodes.find((entry) => entry.id === fixture.textB).text,
      'AV',
    );
  } finally {
    bridge.dispose();
  }
});

test('abort during staged History activation discards only the canceled bank', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      controller.abort();
    }
  };
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
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge, { signal });
    assert.notEqual(bridge.stats().candidate, null);
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE|CANCEL/);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
    assert.equal(bridge.stats().candidate, null);
  } finally {
    bridge.dispose();
  }
});

test('throwing teardown after abort still discards its own prepared History bank', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false,
    calls = 0;
  signal.removeEventListener = (...args) => {
    calls++;
    remove(...args);
    if (trigger) {
      trigger = false;
      controller.abort();
      throw Error('teardown failed');
    }
  };
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
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /teardown failed/);
    assert.equal(calls, 1, 'teardown is not blindly retried');
    assert.equal(bridge.stats().candidate, null);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
  } finally {
    bridge.dispose();
  }
});

test('cleanup of an older History ticket does not discard a newer ticket', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false,
    newer;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      newer = bridge.prepareDocument(
        activeCompositionDocument(owner.projectSnapshot(), fixture.firstId),
      );
    }
  };
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
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    const newTicket = await newer;
    assert.doesNotThrow(() => bridge.assertFresh(newTicket));
    assert.notEqual(bridge.stats().candidate, null);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
  } finally {
    bridge.dispose();
  }
});

test('a throwing old teardown cannot discard a newer ticket installed by its callback', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false,
    newer,
    calls = 0;
  signal.removeEventListener = (...args) => {
    calls++;
    remove(...args);
    if (trigger) {
      trigger = false;
      newer = bridge.prepareDocument(
        activeCompositionDocument(owner.projectSnapshot(), fixture.firstId),
      );
      throw Error('old teardown failed');
    }
  };
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
      token = await owner.prepareCommand({ ...owner.state, type: 'redo' }, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /old teardown failed/);
    const newTicket = await newer;
    assert.equal(calls, 1);
    assert.doesNotThrow(() => bridge.assertFresh(newTicket));
    assert.notEqual(bridge.stats().candidate, null);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
  } finally {
    bridge.dispose();
  }
});

test('prepared History captures its command before caller mutation and never invokes a getter', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController();
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
    const edited = exportSVG(owner.snapshot(), bridge.fonts());
    owner.commitPrepared(await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge));
    const command = { ...owner.state, type: 'redo' },
      expected = command.expectedRevision,
      token = await owner.prepareCommand(command, bridge, { signal: controller.signal });
    let called = 0;
    Object.defineProperty(command, 'expectedRevision', {
      enumerable: true,
      get() {
        called++;
        controller.abort();
        return expected;
      },
    });
    Object.defineProperty(command, 'type', {
      enumerable: true,
      get() {
        called++;
        controller.abort();
        return 'redo';
      },
    });
    Object.setPrototypeOf(command, {
      get payload() {
        called++;
        controller.abort();
        return {};
      },
    });
    owner.commitPrepared(token);
    assert.equal(called, 0);
    assert.equal(controller.signal.aborted, false);
    assert.equal(exportSVG(owner.snapshot(), bridge.fonts()), edited);
    assert.equal(
      owner
        .projectSnapshot()
        .compositions.find((item) => item.id === fixture.secondId)
        .layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AW',
    );

    const payloadGetter = { ...owner.state, type: 'set-text-content' };
    Object.defineProperty(payloadGetter, 'payload', {
      enumerable: true,
      get() {
        called++;
        payloadGetter.type = 'undo';
        controller.abort();
        return {};
      },
    });
    await assert.rejects(owner.prepareCommand(payloadGetter, bridge), /prepared command payload/i);
    assert.equal(called, 0);
    assert.equal(controller.signal.aborted, false);

    const nested = {
      ...owner.state,
      type: 'set-text-content',
      payload: { nodeId: fixture.textB, text: 'AX' },
    };
    Object.defineProperty(nested.payload, 'layerId', {
      enumerable: true,
      get() {
        nested.type = 'undo';
        return fixture.layerB;
      },
    });
    owner.commitPrepared(await owner.prepareCommand(nested, bridge));
    assert.equal(nested.type, 'undo');
    assert.equal(
      owner.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AX',
    );

    const invalid = { ...owner.state, type: 'undo' };
    Object.defineProperty(invalid, 'expectedRevision', {
      enumerable: true,
      get() {
        called++;
        return 0;
      },
    });
    await assert.rejects(owner.prepareCommand(invalid, bridge), /prepared command envelope/i);
    assert.equal(called, 0);

    const inherited = Object.assign(
      Object.create({
        get type() {
          called++;
          controller.abort();
          return 'undo';
        },
      }),
      owner.state,
    );
    await assert.rejects(owner.prepareCommand(inherited, bridge), /prepared command type/i);
    const hidden = { ...owner.state };
    Object.defineProperty(hidden, 'type', { value: 'undo', enumerable: false });
    await assert.rejects(owner.prepareCommand(hidden, bridge), /prepared command type/i);
    assert.equal(called, 0);
    assert.equal(controller.signal.aborted, false);
  } finally {
    bridge.dispose();
  }
});

test('stale prepared History discards its own bank before publication', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
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
    owner.switchComposition(fixture.firstId);
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    assert.equal(bridge.stats().candidate, null);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), beforeProject);
    assert.deepEqual(owner.historyState, beforeHistory);
    assert.deepEqual(bridge.stats().active, beforeBank);
  } finally {
    bridge.dispose();
  }
});

test('no-op prepared History rejects a reentrant teardown without overwriting it', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = new AbortController(),
    signal = controller.signal,
    remove = signal.removeEventListener.bind(signal);
  let trigger = false;
  signal.removeEventListener = (...args) => {
    remove(...args);
    if (trigger) {
      trigger = false;
      owner.createComposition({ name: 'Concurrent', width: 100, height: 200 });
    }
  };
  try {
    assert.equal(owner.historyState.canUndo, false);
    const beforeBank = bridge.stats().active,
      token = await owner.prepareCommand({ ...owner.state, type: 'undo' }, bridge, { signal });
    trigger = true;
    assert.throws(() => owner.commitPrepared(token), /STALE/);
    assert.equal(owner.projectSnapshot().compositions.at(-1).name, 'Concurrent');
    assert.deepEqual(bridge.stats().active, beforeBank);
    assert.equal(bridge.stats().candidate, null);
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
