import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  createVariableDocumentPreparer,
  createVariablePreparationController,
  node,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { createLayerClipboardOwner } from '../src/layer-clipboard-owner.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text');
  text.text = 'AV';
  layer.nodes.push(text);
  const project = migrateLegacyDocumentToCompositionProject(doc),
    owner = createCompositionSessionOwner(project),
    bridge = createVariableDocumentPreparer();
  return { doc, layer, text, project, owner, bridge };
}

function edit(owner, f) {
  return {
    ...owner.state,
    type: 'set-text-content',
    payload: { layerId: f.layer.id, nodeId: f.text.id, text: 'AB' },
  };
}

test('stale prepared History cleanup cannot cancel a newer bridge ticket', async () => {
  const f = fixture();
  try {
    const old = f.owner.prepareCommand({ ...f.owner.state, type: 'undo' }, f.bridge),
      rejected = assert.rejects(old, /STALE/);
    f.owner.renameComposition(f.project.compositions[0].id, 'Concurrent');
    const newer = f.bridge.prepareDocument(f.owner.snapshot());
    await rejected;
    const ticket = await newer;
    assert.doesNotThrow(() => f.bridge.assertFresh(ticket));
  } finally {
    f.bridge.dispose();
  }
});

test('stale prepared text cleanup cannot cancel a newer bridge ticket', async () => {
  const f = fixture();
  try {
    const old = f.owner.prepareCommand(edit(f.owner, f), f.bridge),
      rejected = assert.rejects(old, /STALE/);
    let newer;
    await Promise.resolve().then(() => {
      f.owner.renameComposition(f.project.compositions[0].id, 'Concurrent');
      newer = f.bridge.prepareDocument(f.owner.snapshot());
    });
    await rejected;
    const ticket = await newer;
    assert.doesNotThrow(() => f.bridge.assertFresh(ticket));
  } finally {
    f.bridge.dispose();
  }
});

test('canceling an old prepared Project edit leaves a newer bridge ticket intact', async () => {
  const f = fixture();
  try {
    await f.owner.prepareCommand(edit(f.owner, f), f.bridge);
    const newer = await f.bridge.prepareDocument(f.owner.snapshot());
    f.owner.cancelPrepared(f.bridge);
    assert.doesNotThrow(() => f.bridge.assertFresh(newer));
  } finally {
    f.bridge.dispose();
  }
});

test('canceling old prepared History leaves a newer bridge ticket intact', async () => {
  const f = fixture();
  try {
    await f.owner.prepareCommand({ ...f.owner.state, type: 'undo' }, f.bridge);
    const newer = await f.bridge.prepareDocument(f.owner.snapshot());
    f.owner.cancelPrepared(f.bridge);
    assert.doesNotThrow(() => f.bridge.assertFresh(newer));
  } finally {
    f.bridge.dispose();
  }
});

test('canceling a pending Project edit leaves a newer bridge request intact', async () => {
  const f = fixture();
  try {
    const old = f.owner.prepareCommand(edit(f.owner, f), f.bridge),
      rejected = old.then(
        () => null,
        (error) => error,
      ),
      newer = f.bridge.prepareDocument(f.owner.snapshot());
    f.owner.cancelPrepared(f.bridge);
    assert.ok(await rejected);
    const ticket = await newer;
    assert.doesNotThrow(() => f.bridge.assertFresh(ticket));
  } finally {
    f.bridge.dispose();
  }
});

test('canceling pending History leaves a newer bridge request intact', async () => {
  const f = fixture();
  try {
    const old = f.owner.prepareCommand({ ...f.owner.state, type: 'undo' }, f.bridge),
      rejected = old.then(
        () => null,
        (error) => error,
      ),
      newer = f.bridge.prepareDocument(f.owner.snapshot());
    f.owner.cancelPrepared(f.bridge);
    assert.ok(await rejected);
    const ticket = await newer;
    assert.doesNotThrow(() => f.bridge.assertFresh(ticket));
  } finally {
    f.bridge.dispose();
  }
});

test('token-specific standalone cancellation leaves a newer bridge ticket intact', async () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  try {
    const token = await session.prepareCommand(edit(session, f), f.bridge),
      newer = await f.bridge.prepareDocument(session.snapshot());
    session.cancelPrepared(f.bridge, token);
    assert.doesNotThrow(() => f.bridge.assertFresh(newer));
  } finally {
    f.bridge.dispose();
  }
});

test('a fonts callback cannot let an older preparation replace a newer Session request', async () => {
  const bridge = createVariableDocumentPreparer();
  let reads = 0,
    newer;
  const session = new CommandSession(blankDocument(), {
    fonts() {
      if (++reads === 2)
        newer = session.prepareCommand({ ...session.state, type: 'undo' }, bridge).then(
          (token) => ({ token }),
          (error) => ({ error }),
        );
      return {};
    },
  });
  try {
    await assert.rejects(
      session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
      /STALE/,
    );
    assert.ok(newer);
    const result = await newer;
    assert.ifError(result.error);
    assert.equal(session.commitPrepared(result.token).changed, false);
  } finally {
    bridge.dispose();
  }
});

test('a fonts callback cannot let an older preparation replace a newer bridge ticket', async () => {
  const bridge = createVariableDocumentPreparer();
  let reads = 0,
    newer;
  const session = new CommandSession(blankDocument(), {
    fonts() {
      if (++reads === 2) newer = bridge.prepareDocument(session.snapshot());
      return {};
    },
  });
  try {
    await assert.rejects(
      session.prepareCommand({ ...session.state, type: 'undo' }, bridge),
      /STALE/,
    );
    assert.ok(newer);
    const ticket = await newer;
    assert.doesNotThrow(() => bridge.assertFresh(ticket));
  } finally {
    bridge.dispose();
  }
});

test('a replacement aborted during target access preserves an earlier prepared token', async () => {
  const f = fixture(),
    controller = createVariablePreparationController(),
    session = new CommandSession(f.doc),
    payload = { nodeId: f.text.id, text: 'AB' };
  Object.defineProperty(payload, 'layerId', {
    enumerable: true,
    get() {
      controller.abort();
      return f.layer.id;
    },
  });
  try {
    const token = await session.prepareCommand({ ...session.state, type: 'undo' }, f.bridge);
    await assert.rejects(
      session.prepareCommand({ ...session.state, type: 'set-text-content', payload }, f.bridge, {
        signal: controller.signal,
      }),
      /CANCELLED/,
    );
    assert.equal(session.commitPrepared(token).changed, false);
  } finally {
    f.bridge.dispose();
  }
});

for (const independent of [false, true]) {
  test(`a final Layer clipboard check rejects a newer ${independent ? 'separate' : 'shared'} bridge request`, async () => {
    const owner = createCompositionSessionOwner(
        migrateLegacyDocumentToCompositionProject(blankDocument()),
      ),
      bridge = createVariableDocumentPreparer(),
      nextBridge = independent ? createVariableDocumentPreparer() : bridge;
    let armed = false,
      checks = 0,
      newer;
    const clipboard = createLayerClipboardOwner({
      currentGeneration() {
        // This is the owner recheck after the inner Session has prepared Paste.
        if (armed && ++checks === 10)
          newer = owner.prepareCommand({ ...owner.state, type: 'undo' }, nextBridge).then(
            (token) => ({ token }),
            (error) => ({ error }),
          );
        return 1;
      },
    });
    const context = {
      owner,
      generation: 1,
      compositionId: owner.activeCompositionId,
      layerId: owner.snapshot().layers[0].id,
    };
    try {
      clipboard.copy(context);
      const token = clipboard.token(context);
      armed = true;
      await assert.rejects(
        owner.prepareCommand(
          { ...owner.state, type: 'paste-layer-snapshot', payload: { token } },
          bridge,
        ),
        /STALE/,
      );
      assert.ok(newer);
      const result = await newer;
      assert.ifError(result.error);
      assert.equal(owner.commitPrepared(result.token).changed, false);
    } finally {
      bridge.dispose();
      if (independent) nextBridge.dispose();
    }
  });
}
