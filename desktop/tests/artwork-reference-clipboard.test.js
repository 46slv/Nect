import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function fixture(kind = 'document', { paused = false, missing = false, point = false } = {}) {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Source'),
    elsewhere = blankLayer('Other'),
    sink = node('Value'),
    local = node('Value'),
    downstream = node('Value'),
    driver = point ? node('Curve') : node('Value');
  target.id = 'art-target';
  source.id = 'art-source';
  elsewhere.id = 'art-other';
  sink.id = 'sink';
  local.id = 'local';
  downstream.id = 'downstream';
  downstream.params.value.expression = 'ref("sink", "value") + 1';
  driver.id = 'node-collision';
  if (point) driver.points[0].id = 'anchor';
  else driver.params.value.value = 7;
  target.nodes.push(local, sink, downstream);
  source.nodes.push(driver);
  doc.layers.push(source, elsewhere);
  doc.schemaVersion = 62;
  const sourceId = missing ? 'not-present' : driver.id;
  sink.params.value.expression = point
    ? `ref(artwork, "art-source", "${sourceId}", "anchor.position", "x")`
    : `ref(artwork, "art-source", "${sourceId}", "value")`;
  if (paused) connect(target, local.id, sink.id, 'value');
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  return { session, send, doc, target, source, driver, sink, local, downstream };
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: copying qualified nodes within the same Artwork retains the live source and one-step history`, () => {
    const f = fixture(kind);
    const original = serialize(f.session.snapshot());
    const copied = f.send('copy-nodes', {
      layerId: f.target.id,
      nodeIds: [f.sink.id, f.downstream.id],
    });
    assert.equal(copied.externalInputCount, 1);
    assert.equal(serialize(f.session.snapshot()), original);
    f.send('set-parameter', { layerId: f.source.id, nodeId: f.driver.id, key: 'value', value: 9 });
    const beforePaste = serialize(f.session.snapshot()),
      past = f.session.historyView.past.length;
    const { nodeIds } = f.send('paste-nodes', { layerId: f.target.id });
    assert.notEqual(nodeIds[0], f.sink.id);
    const saved = f.session.snapshot();
    assert.equal(
      saved.layers[0].nodes.find((n) => n.id === nodeIds[0]).params.value.expression,
      f.sink.params.value.expression,
    );
    assert.match(
      saved.layers[0].nodes.find((n) => n.id === nodeIds[1]).params.value.expression,
      new RegExp(nodeIds[0]),
    );
    assert.equal(evaluate(saved.layers[0], {}, {}, saved).parameterValue(nodeIds[0], 'value'), 9);
    assert.equal(evaluate(saved.layers[0], {}, {}, saved).parameterValue(nodeIds[1], 'value'), 10);
    assert.equal(f.session.historyView.past.length, past + 1);
    const encoded = serialize(saved);
    assert.equal(serialize(deserialize(encoded)), encoded);
    f.send('undo');
    assert.equal(serialize(f.session.snapshot()), beforePaste);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), encoded);
    assert.equal(f.send('paste-nodes', { layerId: f.target.id }).nodeIds.length, 2);
  });

  test(`${kind}: History cannot revive a clipboard whose foreign source disappeared during Undo`, () => {
    const f = fixture(kind),
      created = f.send('create-node', { layerId: f.source.id, nodeType: 'Value' });
    f.send('set-parameter-expression', {
      layerId: f.target.id,
      nodeId: f.sink.id,
      key: 'value',
      expression: `ref(artwork, "${f.source.id}", "${created.nodeId}", "value")`,
    });
    f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
    f.send('undo');
    f.send('undo');
    f.send('redo');
    f.send('redo');
    const before = serialize(f.session.snapshot()),
      state = f.session.state;
    assert.throws(() => f.send('paste-nodes', { layerId: f.target.id }), /source.*copy again/i);
    assert.equal(serialize(f.session.snapshot()), before);
    assert.deepEqual(f.session.state, state);
  });
}

test('local copied Curve point identities remap while the foreign source stays linked', () => {
  const f = fixture(),
    doc = f.session.snapshot(),
    target = doc.layers[0],
    curve = node('Curve'),
    consumer = node('Value');
  curve.id = 'local-curve';
  curve.points[0].id = 'local-point';
  consumer.id = 'mixed-consumer';
  consumer.params.value.expression =
    'ref("local-curve", "local-point.position", "x") + ref(artwork, "art-source", "node-collision", "value")';
  target.nodes.push(curve, consumer);
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('copy-nodes', { layerId: target.id, nodeIds: [curve.id, consumer.id] });
  const ids = send('paste-nodes', { layerId: target.id }).nodeIds,
    saved = session.snapshot(),
    nodes = saved.layers[0].nodes,
    copyCurve = nodes.find((n) => n.id === ids[0]),
    copyConsumer = nodes.find((n) => n.id === ids[1]);
  assert.notEqual(copyCurve.id, curve.id);
  assert.notEqual(copyCurve.points[0].id, curve.points[0].id);
  assert.match(copyConsumer.params.value.expression, new RegExp(copyCurve.id));
  assert.match(copyConsumer.params.value.expression, new RegExp(copyCurve.points[0].id));
  assert.match(
    copyConsumer.params.value.expression,
    /ref\(artwork, "art-source", "node-collision", "value"\)/,
  );
  assert.equal(
    evaluate(saved.layers[0], {}, {}, saved).parameterValue(copyConsumer.id, 'value'),
    curve.points[0].position.value[0] + 7,
  );
});

test('qualified clipboard refuses another Artwork without touching pending Redo', () => {
  const f = fixture();
  f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  f.send('set-parameter', { layerId: f.source.id, nodeId: f.driver.id, key: 'value', value: 9 });
  f.send('undo');
  const before = serialize(f.session.snapshot()),
    state = f.session.state,
    future = f.session.historyView.future;
  assert.throws(() => f.send('paste-nodes', { layerId: 'art-other' }), /original Artwork/i);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.deepEqual(f.session.state, state);
  assert.deepEqual(f.session.historyView.future, future);
  f.send('redo');
  assert.equal(
    f.session.snapshot().layers[1].nodes.find((n) => n.id === f.driver.id).params.value.value,
    9,
  );
});

test('local sockets with layerId extension metadata still require their original graph', () => {
  const f = fixture('document', { paused: true }),
    doc = f.session.snapshot();
  const sink = doc.layers[0].nodes.find((n) => n.id === f.sink.id);
  sink.params.value.expression = '';
  sink.params.value.layerId = 'metadata-only';
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const copied = send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  assert.equal(copied.externalInputCount, 1);
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(() => send('paste-nodes', { layerId: 'art-other' }), /unselected input/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
  const { nodeIds } = send('paste-nodes', { layerId: f.target.id });
  const pasted = session.snapshot().layers[0].nodes.find((n) => n.id === nodeIds[0]);
  assert.equal(pasted.params.value.source, f.local.id);
  assert.equal(pasted.params.value.layerId, 'metadata-only');
});

test('source rename, value, visibility and lock changes keep qualified clipboard valid', () => {
  const f = fixture();
  f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  f.send('rename', { layerId: f.source.id, nodeId: f.driver.id, name: 'Renamed source' });
  f.send('set-parameter', { layerId: f.source.id, nodeId: f.driver.id, key: 'value', value: 13 });
  f.send('set-layer-state', { layerId: f.source.id, key: 'visible', value: false });
  f.send('set-layer-state', { layerId: f.source.id, key: 'locked', value: true });
  const { nodeIds } = f.send('paste-nodes', { layerId: f.target.id });
  const doc = f.session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(nodeIds[0], 'value'), 13);
});

test('cancelled previews, failed edits and no-ops keep a copied reference usable', () => {
  const f = fixture();
  f.send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  assert.throws(() =>
    f.send('set-parameter', {
      layerId: f.source.id,
      nodeId: f.driver.id,
      key: 'value',
      value: Infinity,
    }),
  );
  const before = serialize(f.session.snapshot()),
    revision = f.session.state.expectedRevision;
  f.send('set-parameter', { layerId: f.source.id, nodeId: f.driver.id, key: 'value', value: 7 });
  let gesture = f.send('begin-gesture', {
    target: { kind: 'graph-translate', layerId: f.source.id, nodeIds: [f.driver.id] },
  });
  gesture = f.send('update-gesture', { ...gesture, update: { delta: [10, 10] } });
  f.send('cancel-gesture', gesture);
  assert.equal(serialize(f.session.snapshot()), before);
  assert.equal(f.session.state.expectedRevision, revision);
  const pasted = f.send('paste-nodes', { layerId: f.target.id });
  assert.equal(pasted.nodeIds.length, 1);
});

test('source expiry is visible before a commit callback can paste', () => {
  const f = fixture();
  let session,
    armed = false,
    observed;
  session = new CommandSession(f.doc, {
    onCommitted: () => {
      if (!armed) return;
      try {
        session.dispatch({
          ...session.state,
          type: 'paste-nodes',
          payload: { layerId: f.target.id },
        });
      } catch (error) {
        observed = error.message;
      }
    },
  });
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('copy-nodes', { layerId: f.target.id, nodeIds: [f.sink.id] });
  send('delete-node', { layerId: f.target.id, nodeId: f.sink.id });
  armed = true;
  send('delete-node', { layerId: f.source.id, nodeId: f.driver.id });
  assert.match(observed, /source.*copy again/i);
  assert.equal(
    session.snapshot().layers[0].nodes.some((n) => n.id === f.sink.id),
    false,
  );
});

test('same-ID Text to outline replacement expires a copied source even after Undo', () => {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Text source'),
    sink = node('Value'),
    text = node('Text');
  target.id = 'art-target';
  source.id = 'art-source';
  sink.id = 'sink';
  text.id = 'text-source';
  text.text = 'A';
  sink.params.value.expression = 'ref(artwork, "art-source", "text-source", "position", "x")';
  target.nodes.push(sink);
  source.nodes.push(text);
  doc.layers.push(source);
  doc.schemaVersion = 62;
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
  const session = new CommandSession(doc, { fonts: () => ({ default: font }) });
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('copy-nodes', { layerId: target.id, nodeIds: [sink.id] });
  send('delete-node', { layerId: target.id, nodeId: sink.id });
  send('bake-text-outline', { layerId: source.id, nodeId: text.id });
  assert.equal(session.snapshot().layers[1].nodes.find((n) => n.id === text.id).type, 'Multi Path');
  send('undo');
  const before = serialize(session.snapshot()),
    future = session.historyView.future;
  assert.throws(() => send('paste-nodes', { layerId: target.id }), /source.*copy again/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.historyView.future, future);
});

test('removing a materialized legacy parameter expires its clipboard reference', () => {
  const doc = blankDocument(),
    target = doc.layers[0],
    source = blankLayer('Mask source'),
    sink = node('Value'),
    mask = node('Mask');
  target.id = 'art-target';
  source.id = 'art-source';
  sink.id = 'sink';
  mask.id = 'mask';
  delete mask.params.feather;
  target.nodes.push(sink);
  source.nodes.push(mask);
  doc.layers.push(source);
  doc.schemaVersion = 62;
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('set-parameter', { layerId: source.id, nodeId: mask.id, key: 'feather', value: 1 });
  send('set-parameter-expression', {
    layerId: target.id,
    nodeId: sink.id,
    key: 'value',
    expression: 'ref(artwork, "art-source", "mask", "feather")',
  });
  send('copy-nodes', { layerId: target.id, nodeIds: [sink.id] });
  send('undo');
  send('undo');
  assert.equal(
    Object.hasOwn(
      session.snapshot().layers[1].nodes.find((n) => n.id === mask.id).params,
      'feather',
    ),
    false,
  );
  send('redo');
  const before = serialize(session.snapshot()),
    future = session.historyView.future;
  assert.throws(() => send('paste-nodes', { layerId: target.id }), /source.*copy again/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.historyView.future, future);
});

for (const paused of [false, true]) {
  test(`qualified clipboard expires after its source disappears, including paused=${paused}`, () => {
    const f = fixture('document', { paused });
    f.send('copy-nodes', {
      layerId: f.target.id,
      nodeIds: paused ? [f.local.id, f.sink.id] : [f.sink.id],
    });
    f.send('delete-node', { layerId: f.target.id, nodeId: f.sink.id });
    f.send('delete-node', { layerId: f.source.id, nodeId: f.driver.id });
    f.send('undo');
    const before = serialize(f.session.snapshot()),
      state = f.session.state,
      future = f.session.historyView.future;
    assert.throws(() => f.send('paste-nodes', { layerId: f.target.id }), /source.*copy again/i);
    assert.equal(serialize(f.session.snapshot()), before);
    assert.deepEqual(f.session.state, state);
    assert.deepEqual(f.session.historyView.future, future);
    f.send('redo');
    const random = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
    try {
      assert.equal(
        f.send('create-node', { layerId: f.source.id, nodeType: 'Value' }).nodeId,
        f.driver.id,
      );
    } finally {
      random.mock.restore();
    }
    const afterReuse = serialize(f.session.snapshot());
    assert.throws(() => f.send('paste-nodes', { layerId: f.target.id }), /source.*copy again/i);
    assert.equal(serialize(f.session.snapshot()), afterReuse);
  });
}

test('a missing source at copy or a deleted Curve point cannot be silently revived', () => {
  const missing = fixture('document', { missing: true });
  missing.send('copy-nodes', { layerId: missing.target.id, nodeIds: [missing.local.id] });
  missing.send('set-parameter', {
    layerId: missing.target.id,
    nodeId: missing.local.id,
    key: 'value',
    value: 4,
  });
  missing.send('undo');
  const initial = serialize(missing.session.snapshot()),
    state = missing.session.state,
    future = missing.session.historyView.future;
  assert.throws(
    () => missing.send('copy-nodes', { layerId: missing.target.id, nodeIds: [missing.sink.id] }),
    /source.*missing/i,
  );
  assert.equal(serialize(missing.session.snapshot()), initial);
  assert.deepEqual(missing.session.state, state);
  assert.deepEqual(missing.session.historyView.future, future);
  const pasted = missing.send('paste-nodes', { layerId: missing.target.id }).nodeIds;
  assert.equal(
    missing.session.snapshot().layers[0].nodes.find((n) => n.id === pasted[0]).params.value
      .expression,
    missing.local.params.value.expression,
  );

  const point = fixture('document', { point: true });
  point.send('copy-nodes', { layerId: point.target.id, nodeIds: [point.sink.id] });
  point.send('delete-node', { layerId: point.target.id, nodeId: point.sink.id });
  point.send('delete-point', {
    layerId: point.source.id,
    nodeId: point.driver.id,
    pointId: 'anchor',
  });
  point.send('undo');
  const before = serialize(point.session.snapshot()),
    pointFuture = point.session.historyView.future;
  assert.throws(
    () => point.send('paste-nodes', { layerId: point.target.id }),
    /source.*copy again/i,
  );
  assert.equal(serialize(point.session.snapshot()), before);
  assert.deepEqual(point.session.historyView.future, pointFuture);
});
