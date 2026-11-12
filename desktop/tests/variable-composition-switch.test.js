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
  createVariablePreparationController,
  CommandSession,
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

async function openWithBlockedWorker(fixture) {
  const project = deserializeCompositionProject(fixture.json, deserialize);
  let reachedWorker;
  const blocked = new Promise((resolve) => {
    reachedWorker = resolve;
  });
  let hold = true;
  const bridge = createVariableDocumentPreparer({
    workerFactory() {
      const transport = workerFactory(),
        send = transport.postMessage;
      transport.postMessage = (data) => {
        if (hold) {
          hold = false;
          reachedWorker();
        } else send(data);
      };
      return transport;
    },
  });
  const first = activeCompositionDocument(project, fixture.firstId);
  bridge.activate(await bridge.prepareDocument(first));
  const makeOwner = () =>
    createCompositionSessionOwner(project, {
      activeCompositionId: fixture.firstId,
      fonts: () => bridge.fonts(),
    });
  return { bridge, blocked, owner: makeOwner(), anotherOwner: makeOwner() };
}

test('public signal accessors cannot interpose on Worker preparation or Project publication', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture);
  const controller = createVariablePreparationController(),
    signal = controller.signal;
  let calls = 0;
  Object.defineProperty(signal, 'aborted', {
    get() {
      calls++;
      return false;
    },
  });
  signal.addEventListener = () => calls++;
  signal.removeEventListener = () => calls++;
  const abortedSymbol = Object.getOwnPropertySymbols(signal).find(
    (symbol) => String(symbol) === 'Symbol(kAborted)',
  );
  if (abortedSymbol)
    Object.defineProperty(signal, abortedSymbol, {
      configurable: true,
      get() {
        calls++;
        return false;
      },
    });
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    const revision = owner.projectRevision,
      token = await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
        { signal },
      );
    owner.commitPrepared(token);
    assert.equal(calls, 0);
    assert.equal(owner.projectRevision, revision + 1);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AW',
    );
    assert.equal(
      owner
        .projectSnapshot()
        .compositions[1].layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AW',
    );
  } finally {
    bridge.dispose();
  }
});

test('standalone activation ignores public signal teardown hooks', async () => {
  const fixture = await makeSavedProject(),
    project = deserializeCompositionProject(fixture.json, deserialize),
    document = activeCompositionDocument(project, fixture.secondId),
    bridge = newBridge(),
    controller = createVariablePreparationController();
  let calls = 0;
  controller.signal.removeEventListener = () => {
    calls++;
    throw Error('public teardown was called');
  };
  try {
    const ticket = await bridge.prepareDocument(document, { signal: controller.signal });
    bridge.activate(ticket);
    assert.equal(calls, 0);
    assert.equal(exportSVG(document, bridge.fonts()), fixture.expectedB);
    assert.equal(bridge.stats().candidate, null);
  } finally {
    bridge.dispose();
  }
});

test('shared AbortSignal prototype changes cannot interpose on the private Worker signal', async () => {
  const fixture = await makeSavedProject(),
    { owner, bridge } = await openSaved(fixture),
    controller = createVariablePreparationController(),
    prototype = AbortSignal.prototype,
    descriptors = Object.fromEntries(
      ['aborted', 'addEventListener', 'removeEventListener'].map((key) => [
        key,
        Object.getOwnPropertyDescriptor(prototype, key),
      ]),
    );
  let calls = 0;
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    Object.defineProperty(prototype, 'aborted', {
      configurable: true,
      get() {
        calls++;
        return false;
      },
    });
    for (const key of ['addEventListener', 'removeEventListener'])
      Object.defineProperty(prototype, key, {
        configurable: true,
        value() {
          calls++;
          throw Error('mutable signal prototype was called');
        },
      });
    const token = await owner.prepareCommand(
      {
        ...owner.state,
        type: 'set-text-content',
        payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
      },
      bridge,
      { signal: controller.signal },
    );
    owner.commitPrepared(token);
    assert.equal(calls, 0);
    assert.equal(
      owner.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
      'AW',
    );
  } finally {
    for (const [key, descriptor] of Object.entries(descriptors)) {
      if (descriptor) Object.defineProperty(prototype, key, descriptor);
      else delete prototype[key];
    }
    bridge.dispose();
  }
});

test('a caller abort during commit preserves the Project and active font bank', async () => {
  const fixture = await makeSavedProject(),
    controller = createVariablePreparationController();
  let armed = false,
    staged = false,
    observedFreshness = false,
    abortedLate = false;
  const { owner, bridge } = await openSaved(fixture, {
    busy: () => {
      if (armed && observedFreshness) {
        armed = false;
        abortedLate = true;
        controller.abort();
      }
      return false;
    },
  });
  const stage = bridge.stageActivation.bind(bridge),
    assertFresh = bridge.assertFresh.bind(bridge);
  bridge.stageActivation = (ticket) => {
    stage(ticket);
    staged = true;
  };
  bridge.assertFresh = (ticket) => {
    assertFresh(ticket);
    if (staged) observedFreshness = true;
  };
  try {
    owner.commitPreparedCompositionSwitch(
      await owner.prepareCompositionSwitch(fixture.secondId, bridge),
    );
    staged = false;
    observedFreshness = false;
    const before = serializeCompositionProject(owner.projectSnapshot()),
      history = owner.historyState,
      bank = bridge.stats().active,
      token = await owner.prepareCommand(
        {
          ...owner.state,
          type: 'set-text-content',
          payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
        },
        bridge,
        { signal: controller.signal },
      );
    armed = true;
    assert.throws(() => owner.commitPrepared(token), /STALE|CANCEL/);
    assert.equal(abortedLate, true);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()) === before, true);
    assert.deepEqual(owner.historyState, history);
    assert.deepEqual(bridge.stats().active, bank);
    assert.equal(bridge.stats().candidate, null);
  } finally {
    bridge.dispose();
  }
});

test('canceling an in-flight Composition switch stops only its child preparation', async () => {
  const fixture = await makeSavedProject(),
    { bridge, blocked, owner } = await openWithBlockedWorker(fixture),
    parent = createVariablePreparationController();
  try {
    const pending = owner.prepareCompositionSwitch(fixture.secondId, bridge, {
      signal: parent.signal,
    });
    const result = pending.then(
      () => null,
      (error) => error,
    );
    await blocked;
    owner.cancelPreparedCompositionSwitch(bridge);
    assert.match((await result).message, /CANCEL|STALE/);
    assert.equal(parent.signal.aborted, false);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(bridge.stats().pending, false);
  } finally {
    bridge.dispose();
  }
});

test('parent abort at the final busy check cannot publish a prepared Composition', async () => {
  const fixture = await makeSavedProject(),
    parent = createVariablePreparationController();
  let armed = false,
    staged = false,
    observedFreshness = false,
    abortedLate = false;
  const { owner, bridge } = await openSaved(fixture, {
    busy: () => {
      if (armed && observedFreshness) {
        armed = false;
        abortedLate = true;
        parent.abort();
      }
      return false;
    },
  });
  const stage = bridge.stageActivation.bind(bridge),
    assertFresh = bridge.assertFresh.bind(bridge);
  bridge.stageActivation = (ticket) => {
    stage(ticket);
    staged = true;
  };
  bridge.assertFresh = (ticket) => {
    assertFresh(ticket);
    if (staged) observedFreshness = true;
  };
  try {
    const before = serializeCompositionProject(owner.projectSnapshot()),
      history = owner.historyState,
      bank = bridge.stats().active,
      token = await owner.prepareCompositionSwitch(fixture.secondId, bridge, {
        signal: parent.signal,
      });
    armed = true;
    assert.throws(() => owner.commitPreparedCompositionSwitch(token), /STALE|CANCEL/);
    assert.equal(abortedLate, true);
    assert.equal(parent.signal.aborted, true);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(serializeCompositionProject(owner.projectSnapshot()) === before, true);
    assert.deepEqual(owner.historyState, history);
    assert.deepEqual(bridge.stats().active, bank);
    assert.equal(bridge.stats().candidate, null);
  } finally {
    bridge.dispose();
  }
});

test('canceling an old Composition request leaves a newer shared-parent request intact', async () => {
  const fixture = await makeSavedProject(),
    { bridge, blocked, owner, anotherOwner } = await openWithBlockedWorker(fixture),
    parent = createVariablePreparationController();
  try {
    const first = owner.prepareCompositionSwitch(fixture.secondId, bridge, {
      signal: parent.signal,
    });
    const firstResult = first.then(
      () => null,
      (error) => error,
    );
    await blocked;
    const second = anotherOwner.prepareCompositionSwitch(fixture.secondId, bridge, {
      signal: parent.signal,
    });
    owner.cancelPreparedCompositionSwitch(bridge);
    assert.match((await firstResult).message, /CANCEL|STALE/);
    const ticket = await second;
    anotherOwner.commitPreparedCompositionSwitch(ticket);
    assert.equal(parent.signal.aborted, false);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.equal(anotherOwner.activeCompositionId, fixture.secondId);
    assert.equal(exportSVG(anotherOwner.snapshot(), bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('an unregistered signal cannot replace an existing prepared document ticket', async () => {
  const fixture = await makeSavedProject(),
    project = deserializeCompositionProject(fixture.json, deserialize),
    document = activeCompositionDocument(project, fixture.secondId),
    bridge = newBridge();
  try {
    const ticket = await bridge.prepareDocument(document),
      proxySignal = new Proxy(createVariablePreparationController().signal, {});
    await assert.rejects(
      bridge.prepareDocument(document, { signal: proxySignal }),
      /INVALID_ABORT_SIGNAL/,
    );
    let accessorCalls = 0;
    const options = Object.defineProperty({}, 'signal', {
      enumerable: true,
      get() {
        accessorCalls++;
        return createVariablePreparationController().signal;
      },
    });
    await assert.rejects(bridge.prepareDocument(document, options), /INVALID_ABORT_SIGNAL/);
    assert.equal(accessorCalls, 0);
    assert.doesNotThrow(() => bridge.assertFresh(ticket));
    bridge.activate(ticket);
    assert.equal(exportSVG(document, bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

test('reentry while inspecting signal options cannot replace the newer ticket', async () => {
  const fixture = await makeSavedProject(),
    project = deserializeCompositionProject(fixture.json, deserialize),
    document = activeCompositionDocument(project, fixture.secondId),
    bridge = newBridge();
  let newer;
  const options = new Proxy(
    {},
    {
      getOwnPropertyDescriptor(target, key) {
        if (key === 'signal' && !newer) newer = bridge.prepareDocument(document);
        return Reflect.getOwnPropertyDescriptor(target, key);
      },
    },
  );
  try {
    await assert.rejects(bridge.prepareDocument(document, options), /STALE/);
    const ticket = await newer;
    assert.doesNotThrow(() => bridge.assertFresh(ticket));
    bridge.activate(ticket);
    assert.equal(exportSVG(document, bridge.fonts()), fixture.expectedB);
  } finally {
    bridge.dispose();
  }
});

for (const operation of ['text', 'Composition switch']) {
  test(`${operation} options cannot supersede a newer bridge preparation`, async () => {
    const fixture = await makeSavedProject();
    const { owner, bridge } = await openSaved(fixture);
    const project = deserializeCompositionProject(fixture.json, deserialize),
      target = activeCompositionDocument(project, fixture.secondId);
    let newer;
    const options = new Proxy(
      {},
      {
        getOwnPropertyDescriptor(object, key) {
          if (key === 'signal' && !newer) newer = bridge.prepareDocument(target);
          return Reflect.getOwnPropertyDescriptor(object, key);
        },
      },
    );
    try {
      const attempted =
        operation === 'text'
          ? owner.prepareCommand(
              {
                ...owner.state,
                type: 'set-text-content',
                payload: { layerId: 'unused', nodeId: 'unused', text: 'A' },
              },
              bridge,
              options,
            )
          : owner.prepareCompositionSwitch(fixture.secondId, bridge, options);
      await assert.rejects(attempted, /STALE/);
      const ticket = await newer;
      assert.doesNotThrow(() => bridge.assertFresh(ticket));
      assert.equal(owner.activeCompositionId, fixture.firstId);
    } finally {
      bridge.dispose();
    }
  });
}

for (const kind of ['Project owner', 'standalone Session']) {
  test(`${kind} keeps a bridge request started by its busy callback`, async () => {
    const fixture = await makeSavedProject(),
      project = deserializeCompositionProject(fixture.json, deserialize),
      document = activeCompositionDocument(project, fixture.secondId),
      bridge = newBridge();
    let armed = false,
      newer;
    const busy = () => {
      if (armed) {
        armed = false;
        newer = bridge.prepareDocument(document);
      }
      return false;
    };
    try {
      bridge.activate(await bridge.prepareDocument(document));
      const session =
        kind === 'Project owner'
          ? createCompositionSessionOwner(project, {
              activeCompositionId: fixture.secondId,
              fonts: () => bridge.fonts(),
              busy,
            })
          : new CommandSession(document, { fonts: () => bridge.fonts(), busy });
      armed = true;
      await assert.rejects(
        session.prepareCommand(
          {
            ...session.state,
            type: 'set-text-content',
            payload: { layerId: fixture.layerB, nodeId: fixture.textB, text: 'AW' },
          },
          bridge,
        ),
        /STALE/,
      );
      const ticket = await newer;
      assert.doesNotThrow(() => bridge.assertFresh(ticket));
      assert.equal(
        session.snapshot().layers[0].nodes.find((item) => item.id === fixture.textB).text,
        'AV',
      );
    } finally {
      bridge.dispose();
    }
  });
}

test('a busy callback switching Composition cannot retarget a prepared History request', async () => {
  const fixture = await makeSavedProject();
  let armed = false,
    owner;
  const opened = await openSaved(fixture, {
    busy: () => {
      if (armed) {
        armed = false;
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
    owner.renameComposition(fixture.secondId, 'Edited');
    const history = owner.historyState,
      bank = bridge.stats().active,
      command = { ...owner.state, type: 'undo' };
    armed = true;
    await assert.rejects(owner.prepareCommand(command, bridge), /STALE/);
    assert.equal(owner.activeCompositionId, fixture.firstId);
    assert.deepEqual(owner.historyState, history);
    assert.deepEqual(bridge.stats().active, bank);
    assert.equal(bridge.stats().candidate, null);
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
        signal: new Proxy(createVariablePreparationController().signal, {}),
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

test('prepared History captures its command before caller mutation and never invokes a getter', async () => {
  const fixture = await makeSavedProject();
  const { owner, bridge } = await openSaved(fixture, { macroProjectAdmission: () => false });
  const controller = createVariablePreparationController();
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
    const controller = createVariablePreparationController();
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
