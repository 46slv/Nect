import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, blankDocument, node, serialize } from '../src/core.js';

test('composition import is atomic, remaps repeated IDs, and is one undo step', () => {
  const source = blankDocument();
  const session = new CommandSession(blankDocument());
  const before = serialize(session.snapshot());
  session.dispatch({
    ...session.state,
    type: 'import-composition',
    payload: { sourceText: serialize(source), compositionId: source.layers[0].id },
  });
  assert.equal(session.snapshot().layers.length, 2);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(session.snapshot().layers.length, 2);
});

test('invalid composition import leaves the session untouched', () => {
  const session = new CommandSession(blankDocument());
  const before = serialize(session.snapshot());
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'import-composition',
      payload: { sourceText: '{}', compositionId: 'missing' },
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
});

test('composition dependencies and embedded image assets follow the imported composition', () => {
  const source = blankDocument();
  const dependency = blankDocument().layers[0];
  dependency.name = 'Referenced';
  const ref = node('Composition Reference');
  ref.compositionReference = { compositionId: dependency.id };
  const image = node('Image');
  image.asset = 'image-test';
  source.layers[0].nodes.push(ref, image);
  source.layers.push(dependency);
  source.schemaVersion = 31;
  source.imageAssets = {
    'image-test': {
      name: 'pixel.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=',
    },
  };
  const session = new CommandSession(blankDocument());
  session.dispatch({
    ...session.state,
    type: 'import-composition',
    payload: { sourceText: serialize(source), compositionId: source.layers[0].id },
  });
  const imported = session.snapshot();
  assert.equal(imported.layers.length, 3);
  assert.ok(imported.layers.some((layer) => layer.name === 'Referenced'));
  assert.ok(imported.imageAssets['image-test']);
});

test('missing dependency or image asset rejects atomically', () => {
  const source = blankDocument();
  const dependency = blankDocument().layers[0];
  const ref = node('Composition Reference');
  ref.compositionReference = { compositionId: dependency.id };
  source.layers[0].nodes.push(ref);
  source.layers.push(dependency);
  source.schemaVersion = 31;
  const session = new CommandSession(blankDocument());
  const before = serialize(session.snapshot());
  const missingDependency = structuredClone(source);
  missingDependency.layers.pop();
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'import-composition',
      payload: {
        sourceText: JSON.stringify(missingDependency),
        compositionId: source.layers[0].id,
      },
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
  const missingAsset = structuredClone(source);
  const image = node('Image');
  image.asset = 'missing';
  missingAsset.layers[0].nodes.push(image);
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'import-composition',
      payload: { sourceText: JSON.stringify(missingAsset), compositionId: source.layers[0].id },
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
});

test('composition import is blocked during busy, gesture, or text composition and then commits once', () => {
  const source = blankDocument();
  const payload = { sourceText: serialize(source), compositionId: source.layers[0].id };
  let busy = true;
  const session = new CommandSession(blankDocument(), { busy: () => busy });
  const unchanged = serialize(session.snapshot());
  assert.throws(
    () => session.dispatch({ ...session.state, type: 'import-composition', payload }),
    /GESTURE_ACTIVE/,
  );
  assert.equal(serialize(session.snapshot()), unchanged);
  assert.equal(session.historyView.past.length, 0);
  busy = false;
  const layer = session.snapshot().layers[0];
  const target = layer.nodes[0];
  const gesture = session.dispatch({
    ...session.state,
    type: 'begin-gesture',
    payload: { target: { kind: 'graph-translate', layerId: layer.id, nodeIds: [target.id] } },
  });
  assert.throws(
    () => session.dispatch({ ...session.state, type: 'import-composition', payload }),
    /GESTURE_ACTIVE/,
  );
  session.dispatch({
    ...session.state,
    type: 'cancel-gesture',
    payload: { gestureId: gesture.gestureId },
  });
  session.dispatch({ ...session.state, type: 'import-composition', payload });
  assert.equal(session.snapshot().layers.length, 2);
  assert.equal(session.historyView.past.length, 1);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), unchanged);
});
