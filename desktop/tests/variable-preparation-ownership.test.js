import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  createVariableDocumentPreparer,
  node,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

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
