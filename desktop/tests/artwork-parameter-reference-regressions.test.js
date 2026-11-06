import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  evaluate,
  validate,
  serialize,
  deserialize,
  previewColorExpression,
  expressionReferenceTargetsForDocument,
  CommandSession,
  nodeDependencies,
  parameter,
  booleanPendingInputKey,
} from '../src/core.js';
import {
  applyActiveCompositionDocument,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { parseFont } from '../src/fonts.js';

function fixture() {
  const doc = blankDocument(),
    a = doc.layers[0],
    b = blankLayer('source');
  a.id = 'art-a';
  b.id = 'art-b';
  const sink = node('Value'),
    driver = node('Value'),
    curve = node('Curve');
  sink.id = 'sink';
  driver.id = 'driver';
  curve.id = 'curve';
  driver.params.value.value = 7;
  curve.points[0].id = 'anchor';
  curve.points[0].handleA.value = [3, 4];
  a.nodes.push(sink);
  b.nodes.push(driver, curve);
  doc.layers.push(b);
  return { doc, a, b, sink, driver, curve };
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const qualified = 'ref(artwork, "art-b", "driver", "value")';

function localWireMetadataFixture(cycle = false) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    a = node('Value'),
    b = node('Value');
  layer.id = 'local-artwork';
  a.id = 'wire-a';
  b.id = 'wire-b';
  a.params.value.value = 7;
  b.params.value.source = a.id;
  a.params.value.layerId = 'extension-metadata';
  b.params.value.layerId = 'extension-metadata';
  if (cycle) a.params.value.source = b.id;
  layer.nodes.push(a, b);
  return { doc, layer, a, b };
}

test('local wire metadata cannot hide a cycle during validation or reopen', () => {
  const f = localWireMetadataFixture(true);
  assert.throws(() => validate(f.doc), /cycle/i);
  assert.throws(() => new CommandSession(f.doc), /cycle/i);
  assert.throws(() => deserialize(JSON.stringify(f.doc)), /cycle/i);
});

for (const kind of ['document', 'composition']) {
  test(`${kind} rejects a metadata-bearing local wire cycle without consuming Redo`, () => {
    const f = localWireMetadataFixture();
    const session =
      kind === 'document'
        ? new CommandSession(f.doc)
        : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc));
    send(session, 'set-document-settings', { key: 'width', value: f.doc.width + 1 });
    send(session, 'undo');
    const before = session.snapshot(),
      state = session.state,
      future = session.historyView.future;
    assert.throws(
      () =>
        send(session, 'connect', {
          layerId: f.layer.id,
          nodeId: f.a.id,
          key: 'value',
          sourceId: f.b.id,
        }),
      /cycle/i,
    );
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.deepEqual(session.historyView.future, future);
    send(session, 'redo');
    assert.equal(session.snapshot().width, f.doc.width + 1);
  });
}

test('local wire extension metadata is not interpreted as an Artwork address', () => {
  const f = localWireMetadataFixture();
  f.b.params.value.parameterKey = 'value';
  assert.equal(
    nodeDependencies(f.b).find((dep) => dep.source === f.a.id),
    f.b.params.value,
  );
  const saved = serialize(f.doc),
    doc = deserialize(saved),
    layer = doc.layers[0];
  assert.equal(evaluate(layer, {}, {}, doc).run(f.b.id), 7);
  assert.equal(layer.nodes.find((n) => n.id === f.b.id).params.value.layerId, 'extension-metadata');
  const session = new CommandSession(doc);
  send(session, 'set-parameter', { layerId: layer.id, nodeId: f.a.id, key: 'value', value: 9 });
  const edited = session.snapshot();
  assert.equal(evaluate(edited.layers[0], {}, {}, edited).run(f.b.id), 9);
});

test('wire metadata is not a parameter address and does not leave cached output stale', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Math'),
    sink = node('Value');
  source.id = 'math';
  sink.id = 'sink';
  source.params.a.value = 7;
  source.params.b.value = 2;
  sink.params.value.source = source.id;
  sink.params.value.layerId = 'extension-metadata';
  sink.params.value.parameterKey = 'a';
  layer.nodes.push(source, sink);
  const reopened = deserialize(JSON.stringify(doc)),
    l = reopened.layers[0];
  assert.equal(evaluate(l, {}, {}, reopened).run(sink.id), 9);
  l.nodes.find((n) => n.id === source.id).params.b.value = 5;
  const changed = evaluate(l, {}, {}, reopened);
  assert.equal(changed.run(source.id), 12);
  assert.equal(changed.run(sink.id), 12);

  l.nodes.find((n) => n.id === sink.id).params.value.parameterKey = 'extension-key';
  assert.equal(evaluate(l, {}, {}, reopened).run(sink.id), 12);
});

test('wire metadata cannot replace a selected scalar output port', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    vector = node('Vector'),
    separate = node('Separate Vector'),
    sink = node('Value');
  vector.params.value.value = [11, 22];
  layer.nodes.push(vector, separate, sink);
  connect(layer, vector.id, separate.id, 'vector');
  connect(layer, separate.id, sink.id, 'value', false, 'x');
  sink.params.value.layerId = 'extension-metadata';
  sink.params.value.parameterKey = 'extension-key';
  const reopened = deserialize(JSON.stringify(doc)),
    l = reopened.layers[0];
  assert.equal(evaluate(l, {}, {}, reopened).run(sink.id), 11);
  l.nodes.find((n) => n.id === vector.id).params.value.value = [13, 22];
  assert.equal(evaluate(l, {}, {}, reopened).run(sink.id), 13);
});

test('Text outline baking treats a default-port wire with metadata as a wire', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill');
  text.text = 'A';
  layer.nodes.push(text, fill);
  connect(layer, text.id, fill.id, 'path');
  fill.params.path.parameterKey = 'extension-key';
  const session = new CommandSession(doc, { fonts: () => ({ default: testFont() }) });
  const before = session.snapshot();
  send(session, 'bake-text-outline', { layerId: layer.id, nodeId: text.id });
  assert.equal(session.snapshot().layers[0].nodes.find((n) => n.id === text.id).type, 'Multi Path');
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
});

function testFont() {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  return parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
}

function expectAtomicFailure(session, action, pattern) {
  const before = session.snapshot(),
    state = { ...session.state },
    future = structuredClone(session.historyView.future);
  assert.throws(action, pattern);
  assert.deepEqual(session.snapshot(), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyView.future, future);
}

test('legacy dangling IDs stay local when another imported Composition owns a maximum-length ID', () => {
  const longId = 'n'.repeat(181),
    a = blankDocument(),
    b = blankDocument();
  a.layers[0].id = 'max-id-layer';
  b.layers[0].id = 'missing-id-layer';
  const existing = node('Value'),
    missing = node('Value');
  existing.id = longId;
  missing.id = 'missing-id-sink';
  missing.params.value.expression = `ref("${longId}", "value")`;
  a.layers[0].nodes.push(existing);
  b.layers[0].nodes.push(missing);
  const source = migrateLegacyDocumentToCompositionProject(a),
    other = migrateLegacyDocumentToCompositionProject(b).compositions[0],
    destination = migrateLegacyDocumentToCompositionProject(blankDocument());
  source.compositions[0].id = 'max-id-composition';
  source.compositions[0].name = 'maximum';
  other.id = 'missing-id-composition';
  other.name = 'missing';
  for (const compositions of [
    [source.compositions[0], other],
    [other, source.compositions[0]],
  ]) {
    const result = prepareProjectCompositionsImport(destination, { ...source, compositions }, [
      'max-id-composition',
      'missing-id-composition',
    ]).project;
    const importedA = result.compositions.find((item) => item.name === 'maximum'),
      importedB = result.compositions.find((item) => item.name === 'missing');
    assert.ok(importedA.layers[0].nodes.some((item) => item.id === longId));
    const value = importedB.layers[0].nodes.find((item) => item.id === 'missing-id-sink');
    assert.equal(value.params.value.expression, missing.params.value.expression);
    assert.throws(
      () => evaluate(importedB.layers[0]).parameterValue(value.id, 'value'),
      /missing/i,
    );
  }
});

test('old Color preview and Apply both reject an explicit same-Artwork reference', () => {
  const f = fixture(),
    color = node('Color'),
    local = node('Value');
  color.id = 'local-color';
  local.id = 'local-value';
  local.params.value.value = 0.4;
  f.a.nodes.push(color, local);
  f.doc.schemaVersion = 27;
  const expression = 'alpha(value, ref(artwork, "art-a", "local-value", "value"))',
    session = new CommandSession(f.doc);
  assert.throws(
    () => previewColorExpression(f.a, color.id, 'value', expression, {}, {}, f.doc),
    /different layer/i,
  );
  expectAtomicFailure(
    session,
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-a',
        nodeId: color.id,
        key: 'value',
        expression,
      }),
    /different layer/i,
  );
});

test('resuming a paused foreign expression validates its destination atomically', () => {
  const f = fixture(),
    local = node('Value');
  local.id = 'local-driver';
  local.params.value.value = 8;
  f.a.nodes.push(local);
  f.driver.params.value.value = 2;
  const session = new CommandSession(f.doc),
    sink = { layerId: 'art-a', nodeId: 'sink', key: 'value' },
    foreign = { layerId: 'art-b', nodeId: 'driver', key: 'value' },
    localValue = { layerId: 'art-a', nodeId: 'local-driver', key: 'value' };
  send(session, 'set-parameter-expression', { ...sink, expression: `1 / ${qualified}` });
  send(session, 'connect', { ...sink, sourceId: local.id });
  send(session, 'set-parameter', { ...foreign, value: 0 });
  send(session, 'set-parameter', { ...localValue, value: 9 });
  send(session, 'undo');
  expectAtomicFailure(session, () => send(session, 'disconnect', sink), /finite|zero|divide/i);
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === local.id).params.value.value,
    9,
  );
  send(session, 'set-parameter', { ...foreign, value: 4 });
  send(session, 'disconnect', sink);
  let doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value'), 0.25);
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.source,
    local.id,
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value'), 0.25);
});

test('foreign Resample count enforces bounds on Apply and resumed expressions', () => {
  const f = fixture(),
    resample = node('Resample Path'),
    local = node('Value');
  resample.id = 'resample';
  local.id = 'local-driver';
  local.params.value.value = 8;
  f.a.nodes.push(resample, local);
  f.driver.params.value.value = 1;
  const session = new CommandSession(f.doc),
    target = { layerId: 'art-a', nodeId: resample.id, key: 'count' },
    foreign = { layerId: 'art-b', nodeId: 'driver', key: 'value' };
  expectAtomicFailure(
    session,
    () =>
      send(session, 'set-parameter-expression', {
        ...target,
        expression: qualified,
      }),
    /Resample count/i,
  );
  send(session, 'set-parameter', { ...foreign, value: 7 });
  send(session, 'set-parameter-expression', { ...target, expression: qualified });
  send(session, 'connect', { ...target, sourceId: local.id });
  send(session, 'set-parameter', { ...foreign, value: 1 });
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: local.id, key: 'value', value: 9 });
  send(session, 'undo');
  expectAtomicFailure(session, () => send(session, 'disconnect', target), /Resample count/i);
  send(session, 'redo');
  send(session, 'set-parameter', { ...foreign, value: 4 });
  send(session, 'disconnect', target);
  let doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'count'), 4);
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === resample.id).params.count.source,
    local.id,
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'count'), 4);
});

test('switching operations validates a newly active foreign property even without an input', () => {
  const f = fixture(),
    resample = node('Resample Path'),
    local = node('Value');
  resample.id = 'resample';
  local.id = 'local-value';
  resample.operation = 'count';
  resample.params.spacing.expression = qualified;
  f.driver.params.value.value = -1;
  f.a.nodes.push(resample, local);
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    target = {
      layerId: 'art-a',
      nodeId: resample.id,
      operation: 'spacing',
    };
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: local.id, key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(session, () => send(session, 'set-operation', target), /Resample spacing/i);
  send(session, 'redo');
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-operation', target);
  let doc = session.snapshot();
  assert.equal(doc.layers[0].nodes.find((n) => n.id === resample.id).operation, 'spacing');
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'spacing'), 2);
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === resample.id).operation,
    'count',
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'spacing'), 2);
});

test('newly active Resample spacing rejects a preexisting direct-read error', () => {
  const f = fixture(),
    resample = node('Resample Path');
  resample.id = 'resample';
  resample.operation = 'count';
  resample.params.spacing.expression = `1 / ${qualified}`;
  f.driver.params.value.value = 0;
  f.a.nodes.push(resample);
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    target = {
      layerId: 'art-a',
      nodeId: resample.id,
      operation: 'spacing',
    };
  assert.throws(() => evaluate(f.a, {}, {}, f.doc).run(resample.id), /Connect a Path input/i);
  assert.throws(
    () => evaluate(f.a, {}, {}, f.doc).parameterValue(resample.id, 'spacing'),
    /finite|zero|divide/i,
  );
  expectAtomicFailure(session, () => send(session, 'set-operation', target), /finite|zero|divide/i);
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-operation', target);
  let doc = deserialize(serialize(session.snapshot()));
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'spacing'), 0.5);
  assert.throws(
    () => evaluate(doc.layers[0], {}, {}, doc).run(resample.id),
    /Connect a Path input/i,
  );
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === resample.id).operation,
    'count',
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'spacing'), 0.5);
});

test('activating Pucker manual center rejects an already-invalid foreign property atomically', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat');
  pucker.id = 'pucker';
  pucker.params.center.expression = `1 / ${qualified}; 0`;
  f.a.nodes.push(pucker);
  f.driver.params.value.value = 0;
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    target = { layerId: 'art-a', nodeId: pucker.id, key: 'autoCenter' };
  assert.throws(
    () => evaluate(f.a, {}, {}, f.doc).parameterValue(pucker.id, 'center'),
    /finite|zero|divide/i,
  );
  assert.throws(() => evaluate(f.a, {}, {}, f.doc).run(pucker.id), /Connect a Path input/i);
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(
    session,
    () => send(session, 'set-parameter', { ...target, value: false }),
    /finite|zero|divide/i,
  );
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === 'sink').params.value.value,
    11,
  );
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-parameter', { ...target, value: false });
  let doc = deserialize(serialize(session.snapshot()));
  assert.equal(doc.layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.value, false);
  assert.deepEqual(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    [0.5, 0],
  );
  assert.throws(() => evaluate(doc.layers[0], {}, {}, doc).run(pucker.id), /Connect a Path input/i);
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.value,
    true,
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.deepEqual(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    [0.5, 0],
  );
});

test('restoring bypass activates foreign controls without excusing an old direct-read error', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat');
  pucker.id = 'pucker';
  pucker.bypassed = true;
  pucker.params.amount.expression = `1 / ${qualified}`;
  f.a.nodes.push(pucker);
  f.driver.params.value.value = 0;
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    target = { layerId: 'art-a', nodeId: pucker.id, bypassed: false };
  assert.throws(
    () => evaluate(f.a, {}, {}, f.doc).parameterValue(pucker.id, 'amount'),
    /finite|zero|divide/i,
  );
  expectAtomicFailure(
    session,
    () => send(session, 'set-node-bypassed', target),
    /finite|zero|divide/i,
  );
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-node-bypassed', target);
  let doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'amount'), 0.5);
  send(session, 'undo');
  assert.equal(session.snapshot().layers[0].nodes.find((n) => n.id === pucker.id).bypassed, true);
  send(session, 'redo');
  doc = deserialize(serialize(session.snapshot()));
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'amount'), 0.5);
});

test('connecting a Boolean input validates newly effective foreign controls', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat'),
    toggle = node('Toggle');
  pucker.id = 'pucker';
  toggle.id = 'toggle';
  toggle.params.value.value = false;
  pucker.params.center.expression = `1 / ${qualified}; 0`;
  f.a.nodes.push(pucker, toggle);
  f.driver.params.value.value = 0;
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    target = {
      layerId: 'art-a',
      nodeId: pucker.id,
      key: 'autoCenter',
      sourceId: toggle.id,
    };
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(session, () => send(session, 'connect', target), /finite|zero|divide/i);
  send(session, 'redo');
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'connect', target);
  let doc = session.snapshot();
  assert.deepEqual(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    [0.5, 0],
  );
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.source,
    undefined,
  );
  send(session, 'redo');
  doc = deserialize(serialize(session.snapshot()));
  assert.equal(
    doc.layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.source,
    toggle.id,
  );
});

test('an upstream Boolean edit validates only a newly effective conditional input', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat'),
    toggle = node('Toggle');
  pucker.id = 'pucker';
  toggle.id = 'toggle';
  toggle.params.value.value = true;
  pucker.params.center.expression = `1 / ${qualified}; 0`;
  f.a.nodes.push(pucker, toggle);
  f.driver.params.value.value = 0;
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    wire = {
      layerId: 'art-a',
      nodeId: pucker.id,
      key: 'autoCenter',
      sourceId: toggle.id,
    },
    control = { layerId: 'art-a', nodeId: toggle.id, key: 'value' };
  // A true source leaves center inactive, even though its direct read fails.
  send(session, 'connect', wire);
  let doc = session.snapshot();
  assert.throws(() => evaluate(doc.layers[0], {}, {}, doc).run(pucker.id), /Connect a Path input/i);
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: 'sink', key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(
    session,
    () => send(session, 'set-parameter', { ...control, value: false }),
    /finite|zero|divide/i,
  );
  send(session, 'redo');
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-parameter', { ...control, value: false });
  doc = deserialize(serialize(session.snapshot()));
  assert.deepEqual(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    [0.5, 0],
  );
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === toggle.id).params.value.value,
    true,
  );
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === toggle.id).params.value.value,
    false,
  );
});

test('an upstream Boolean edit activates a driven Transform width using the same rule', () => {
  const f = fixture(),
    transform = node('Transform'),
    toggle = node('Toggle');
  transform.id = 'transform';
  toggle.id = 'toggle';
  toggle.params.value.value = true;
  transform.params.uniformScale = {
    type: 'Boolean',
    value: true,
    source: toggle.id,
    expression: '',
    locked: false,
  };
  transform.params.scaleWidth = {
    type: 'Scalar',
    value: 1,
    expression: `1 / ${qualified}`,
    locked: false,
  };
  f.a.nodes.push(transform, toggle);
  f.driver.params.value.value = 0;
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    control = {
      layerId: 'art-a',
      nodeId: toggle.id,
      key: 'value',
    };
  expectAtomicFailure(
    session,
    () => send(session, 'set-parameter', { ...control, value: false }),
    /finite|zero|divide/i,
  );
  send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 2 });
  send(session, 'set-parameter', { ...control, value: false });
  let doc = session.snapshot();
  assert.equal(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(transform.id, 'scaleWidth'),
    0.5,
  );
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === toggle.id).params.value.value,
    true,
  );
  send(session, 'redo');
  doc = deserialize(serialize(session.snapshot()));
  assert.equal(
    evaluate(doc.layers[0], {}, {}, doc).parameterValue(transform.id, 'scaleWidth'),
    0.5,
  );
});

test('activating a property with a missing foreign target preserves the dangling diagnostic', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat');
  pucker.id = 'pucker';
  pucker.params.center.expression = 'ref(artwork, "missing-art", "missing-value", "value"); 0';
  f.a.nodes.push(pucker);
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter', {
    layerId: 'art-a',
    nodeId: pucker.id,
    key: 'autoCenter',
    value: false,
  });
  let doc = deserialize(serialize(session.snapshot()));
  assert.throws(
    () => evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    /missing/i,
  );
  send(session, 'undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.value,
    true,
  );
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(doc.layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.value, false);
});

test('legacy same-Artwork incomplete Pucker edits retain their previous behavior', () => {
  const f = fixture(),
    pucker = node('Pucker & Bloat'),
    driver = node('Value');
  pucker.id = 'pucker';
  driver.id = 'local-driver';
  driver.params.value.value = 0;
  pucker.params.center.expression = '1 / ref("local-driver", "value"); 0';
  f.a.nodes.push(pucker, driver);
  f.doc.schemaVersion = 61;
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter', {
    layerId: 'art-a',
    nodeId: pucker.id,
    key: 'autoCenter',
    value: false,
  });
  const doc = deserialize(serialize(session.snapshot()));
  assert.equal(doc.schemaVersion, 61);
  assert.equal(doc.layers[0].nodes.find((n) => n.id === pucker.id).params.autoCenter.value, false);
  assert.throws(
    () => evaluate(doc.layers[0], {}, {}, doc).parameterValue(pucker.id, 'center'),
    /finite|zero|divide/i,
  );
});

test('a foreign consumer validates an inactive Resample property when its source changes', () => {
  const f = fixture(),
    resample = node('Resample Path'),
    local = node('Value');
  resample.id = 'resample';
  local.id = 'local-value';
  resample.operation = 'count';
  f.b.nodes.push(resample);
  f.a.nodes.push(local);
  f.driver.params.value.value = 2;
  resample.params.spacing.expression = '1 / ref("driver", "value")';
  f.sink.params.value.expression = 'ref(artwork, "art-b", "resample", "spacing")';
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    foreign = { layerId: 'art-b', nodeId: 'driver', key: 'value' };
  assert.equal(evaluate(f.a, {}, {}, f.doc).parameterValue('sink', 'value'), 0.5);
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: local.id, key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(
    session,
    () => send(session, 'set-parameter', { ...foreign, value: 0 }),
    /finite|zero|divide/i,
  );
  send(session, 'redo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === local.id).params.value.value,
    11,
  );
  send(session, 'set-parameter', { ...foreign, value: 4 });
  let doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value'), 0.25);
  send(session, 'undo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value'), 0.5);
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('sink', 'value'), 0.25);
});

test('a renderable Text output rejects a foreign size that would break it', () => {
  const f = fixture(),
    text = node('Text'),
    local = node('Value'),
    font = testFont();
  text.id = 'driven-text';
  text.text = 'a';
  local.id = 'local-value';
  f.a.nodes.push(text, local);
  const fonts = { default: font },
    session = new CommandSession(f.doc, { fonts: () => fonts }),
    foreign = { layerId: 'art-b', nodeId: 'driver', key: 'value' };
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: text.id,
    key: 'size',
    expression: qualified,
  });
  assert.doesNotThrow(() =>
    evaluate(session.snapshot().layers[0], fonts, {}, session.snapshot()).run(text.id),
  );
  send(session, 'set-parameter', { layerId: 'art-a', nodeId: local.id, key: 'value', value: 11 });
  send(session, 'undo');
  expectAtomicFailure(
    session,
    () => send(session, 'set-parameter', { ...foreign, value: -1 }),
    /size|positive/i,
  );
  send(session, 'redo');
  send(session, 'set-parameter', { ...foreign, value: 8 });
  let doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], fonts, {}, doc).parameterValue(text.id, 'size'), 8);
  assert.doesNotThrow(() => evaluate(doc.layers[0], fonts, {}, doc).run(text.id));
  send(session, 'undo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], fonts, {}, doc).parameterValue(text.id, 'size'), 7);
  send(session, 'redo');
  doc = session.snapshot();
  assert.equal(evaluate(doc.layers[0], fonts, {}, doc).parameterValue(text.id, 'size'), 8);
});

test('many driven Curve descriptors use their effective values without changing fallbacks', () => {
  const f = fixture(),
    template = f.curve.points[0];
  f.b.nodes.splice(f.b.nodes.indexOf(f.curve), 1);
  f.a.nodes.push(f.curve);
  f.curve.points = Array.from({ length: 300 }, (_, i) => {
    const point = structuredClone(template);
    point.id = `point-${i}`;
    point.position.value = [2, 3];
    point.position.expression = `${qualified}; 0`;
    return point;
  });
  f.doc.schemaVersion = 62;
  const session = new CommandSession(f.doc),
    source = { layerId: 'art-b', nodeId: 'driver', key: 'value' };
  const read = () => {
    const doc = session.snapshot(),
      curve = doc.layers[0].nodes.find((n) => n.id === 'curve');
    assert.deepEqual(curve.points[299].position.value, [2, 3]);
    return evaluate(doc.layers[0], {}, {}, doc).parameterValue('curve', 'point-299.position');
  };
  assert.deepEqual(read(), [7, 0]);
  send(session, 'set-parameter', { ...source, value: 8 });
  assert.deepEqual(read(), [8, 0]);
  send(session, 'undo');
  assert.deepEqual(read(), [7, 0]);
  send(session, 'redo');
  assert.deepEqual(read(), [8, 0]);
});

test('foreign consumers follow legacy Transform virtual properties when their backing value changes', () => {
  for (const [key, backing, initial, invalid, valid, component] of [
    ['position', 'anchor', [7, 0], [1, 0], [8, 0], ', "x"'],
    ['scaleWidth', 'scale', 7, 1, 8, ''],
  ]) {
    const f = fixture(),
      transform = node('Transform'),
      resample = node('Resample Path');
    transform.id = 'legacy-transform';
    resample.id = 'resample';
    transform.params[backing].value = initial;
    f.b.nodes.push(transform);
    f.a.nodes.push(resample);
    f.doc.schemaVersion = 62;
    resample.params.count.expression = `ref(artwork, "art-b", "legacy-transform", "${key}"${component})`;
    const session = new CommandSession(f.doc),
      source = {
        layerId: 'art-b',
        nodeId: transform.id,
        key: backing,
      };
    expectAtomicFailure(
      session,
      () => send(session, 'set-parameter', { ...source, value: invalid }),
      /Resample count/i,
    );
    send(session, 'set-parameter', { ...source, value: valid });
    let doc = session.snapshot();
    assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'count'), 8);
    send(session, 'undo');
    doc = session.snapshot();
    assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'count'), 7);
    send(session, 'redo');
    doc = session.snapshot();
    assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue(resample.id, 'count'), 8);
  }
});

test('the first qualified Color preview uses a private promoted candidate and agrees with Apply', () => {
  const f = fixture(),
    color = node('Color');
  color.id = 'color';
  f.a.nodes.push(color);
  f.driver.params.value.value = 0.4;
  const old = structuredClone(f.doc),
    expression = `alpha(value, ${qualified})`;
  assert.equal(previewColorExpression(f.a, 'color', 'value', expression, {}, {}, f.doc).alpha, 0.4);
  assert.deepEqual(f.doc, old);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'color',
    key: 'value',
    expression,
  });
  const saved = session.snapshot();
  assert.equal(saved.schemaVersion, 62);
  assert.equal(
    evaluate(saved.layers[0], {}, {}, saved).parameterValue('color', 'value').alpha,
    0.4,
  );
  const invalid = `alpha(value, ${qualified} + 1)`;
  assert.throws(
    () => previewColorExpression(f.a, 'color', 'value', invalid, {}, {}, f.doc),
    /alpha/i,
  );
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-a',
        nodeId: 'color',
        key: 'value',
        expression: invalid,
      }),
    /alpha/i,
  );
  assert.deepEqual(session.snapshot(), saved);
  send(session, 'undo');
  assert.equal(session.snapshot().schemaVersion, old.schemaVersion);
  send(session, 'redo');
  assert.equal(session.snapshot().schemaVersion, 62);
});

test('applying an old Document to a modern wrapper rejects opaque qualified text before promotion', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'sink',
    key: 'value',
    expression: qualified,
  });
  const modern = migrateLegacyDocumentToCompositionProject(session.snapshot()),
    original = serializeCompositionProject(modern);
  for (const kind of ['Scalar', 'Path']) {
    const old = structuredClone(f.doc);
    old.schemaVersion = 61;
    if (kind === 'Scalar')
      old.layers[0].nodes.find((n) => n.id === 'sink').params.value.expression = qualified;
    else {
      const curve = node('Curve'),
        transform = node('Transform');
      old.layers[0].nodes.push(curve, transform);
      connect(old.layers[0], curve.id, transform.id, 'path');
      transform.params.path.expression = qualified;
    }
    assert.throws(
      () => applyActiveCompositionDocument(modern, modern.compositions[0].id, old),
      /opaque|promot/i,
    );
    assert.equal(serializeCompositionProject(modern), original);
  }
  const cleanOld = structuredClone(f.doc);
  cleanOld.schemaVersion = 61;
  assert.equal(
    applyActiveCompositionDocument(modern, modern.compositions[0].id, cleanOld).schemaVersion,
    62,
  );
});

test('multi-Composition import reserves all selected dangling layer, node and point IDs before allocation', () => {
  const f = fixture();
  f.doc.schemaVersion = 62;
  const missingNode = node('Value'),
    missingPoint = node('Value');
  missingNode.id = 'missing-node-sink';
  missingPoint.id = 'missing-point-sink';
  f.sink.params.value.expression = 'ref(artwork, "conflict-layer-import-1", "missing", "value")';
  missingNode.params.value.expression = 'ref(artwork, "art-b", "conflict-node-import-1", "value")';
  missingPoint.params.value.expression =
    'ref(artwork, "art-b", "curve", "conflict-point-import-1.handleA", "x")';
  f.a.nodes.push(missingNode, missingPoint);
  const sourceA = migrateLegacyDocumentToCompositionProject(f.doc);
  sourceA.compositions[0].id = 'comp-a';
  sourceA.compositions[0].name = 'A';
  const other = blankDocument();
  other.layers[0].id = 'conflict-layer';
  const otherNode = node('Value'),
    otherCurve = node('Curve');
  otherNode.id = 'conflict-node';
  otherCurve.id = 'other-curve';
  otherCurve.points[0].id = 'conflict-point';
  other.layers[0].nodes.push(otherNode, otherCurve);
  const sourceB = migrateLegacyDocumentToCompositionProject(other).compositions[0];
  sourceB.id = 'comp-b';
  sourceB.name = 'B';
  const destination = blankDocument();
  destination.layers[0].id = 'conflict-layer';
  const destNode = node('Value'),
    destCurve = node('Curve');
  destNode.id = 'conflict-node';
  destCurve.id = 'dest-curve';
  destCurve.points[0].id = 'conflict-point';
  destination.layers[0].nodes.push(destNode, destCurve);
  const destinationProject = migrateLegacyDocumentToCompositionProject(destination);
  for (const order of [
    [sourceA.compositions[0], sourceB],
    [sourceB, sourceA.compositions[0]],
  ]) {
    const source = { ...sourceA, compositions: order };
    const result = prepareProjectCompositionsImport(destinationProject, source, [
      'comp-a',
      'comp-b',
    ]).project;
    const copiedA = result.compositions.find((c) => c.name === 'A'),
      copiedB = result.compositions.find((c) => c.name === 'B');
    const bLayer = copiedB.layers[0],
      bNode = bLayer.nodes.find((n) => n.type === 'Value'),
      bCurve = bLayer.nodes.find((n) => n.type === 'Curve');
    assert.equal(bLayer.id, 'conflict-layer-import-2');
    assert.equal(bNode.id, 'conflict-node-import-2');
    assert.equal(bCurve.points[0].id, 'conflict-point-import-2');
    const expressions = copiedA.layers
      .flatMap((layer) => layer.nodes)
      .filter((n) => n.type === 'Value')
      .map((n) => n.params.value.expression);
    assert.ok(expressions.some((s) => s.includes('conflict-layer-import-1')));
    assert.ok(expressions.some((s) => s.includes('conflict-node-import-1')));
    assert.ok(expressions.some((s) => s.includes('conflict-point-import-1')));
  }
});

test('legacy import preflights inactive dangling identities before changing the inner Session', () => {
  for (const kind of ['layer', 'node', 'point']) {
    const active = blankDocument(),
      activeValue = node('Value');
    active.layers[0].id = 'active-layer';
    activeValue.id = 'active-value';
    active.layers[0].nodes.push(activeValue);
    active.schemaVersion = 62;
    const project = migrateLegacyDocumentToCompositionProject(active);
    project.compositions[0].id = 'active-comp';
    const inactive = blankDocument(),
      sink = node('Value');
    inactive.layers[0].id = 'inactive-layer';
    sink.id = 'inactive-sink';
    inactive.layers[0].nodes.push(sink);
    const sourceLayer = blankLayer('inactive source');
    sourceLayer.id = 'inactive-source';
    const sourceCurve = node('Curve');
    sourceCurve.id = 'inactive-curve';
    sourceCurve.points[0].id = 'existing-point';
    sourceLayer.nodes.push(sourceCurve);
    inactive.layers.push(sourceLayer);
    inactive.schemaVersion = 62;
    sink.params.value.expression =
      kind === 'layer'
        ? 'ref(artwork, "incoming-layer", "incoming-node", "value")'
        : kind === 'node'
          ? 'ref(artwork, "inactive-source", "node-collision", "value")'
          : 'ref(artwork, "inactive-source", "inactive-curve", "incoming-point.position", "x")';
    const inactiveComp = migrateLegacyDocumentToCompositionProject(inactive).compositions[0];
    inactiveComp.id = 'inactive-comp';
    project.compositions.push(inactiveComp);
    const owner = createCompositionSessionOwner(project, { activeCompositionId: 'active-comp' });
    send(owner, 'set-parameter', {
      layerId: 'active-layer',
      nodeId: 'active-value',
      key: 'value',
      value: 9,
    });
    send(owner, 'undo');
    const before = owner.projectSnapshot(),
      state = { ...owner.state },
      future = structuredClone(owner.historyView.future),
      projectRevision = owner.projectRevision;
    const imported = blankDocument();
    imported.layers[0].id = kind === 'layer' ? 'incoming-layer' : 'import-layer';
    const incoming = node('Value'),
      incomingCurve = node('Curve');
    incoming.id = kind === 'node' ? 'node-collision' : 'incoming-node';
    incomingCurve.id = 'incoming-curve';
    incomingCurve.points[0].id = 'incoming-point';
    imported.layers[0].nodes.push(incoming, incomingCurve);
    assert.throws(
      () =>
        send(owner, 'import-composition', {
          sourceText: serialize(imported),
          compositionId: imported.layers[0].id,
        }),
      /inactive Composition reference/i,
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.state, state);
    assert.equal(owner.projectRevision, projectRevision);
    assert.deepEqual(owner.historyView.future, future);
    if (kind === 'node') {
      const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
      try {
        assert.throws(
          () => send(owner, 'create-node', { layerId: 'active-layer', nodeType: 'Value' }),
          /inactive Composition reference/i,
        );
      } finally {
        fake.mock.restore();
      }
      assert.deepEqual(owner.projectSnapshot(), before);
      assert.deepEqual(owner.state, state);
      assert.deepEqual(owner.historyView.future, future);
    }
    if (kind === 'layer') {
      imported.layers[0].id = 'safe-layer';
      incoming.id = 'inactive-sink';
      assert.throws(
        () =>
          send(owner, 'import-composition', {
            sourceText: serialize(imported),
            compositionId: imported.layers[0].id,
          }),
        /inactive Composition reference|existing identity/i,
      );
      assert.deepEqual(owner.projectSnapshot(), before);
      assert.deepEqual(owner.state, state);
      assert.deepEqual(owner.historyView.future, future);
      const crossOuter = blankDocument(),
        crossSink = node('Value');
      crossOuter.schemaVersion = 62;
      crossOuter.layers[0].id = 'cross-outer-import';
      crossSink.id = 'cross-sink';
      crossSink.params.value.expression =
        'ref(artwork, "inactive-source", "inactive-curve", "existing-point.position", "x")';
      crossOuter.layers[0].nodes.push(crossSink);
      assert.throws(
        () =>
          send(owner, 'import-composition', {
            sourceText: serialize(crossOuter),
            compositionId: crossOuter.layers[0].id,
          }),
        /outer Compositions/i,
      );
      assert.deepEqual(owner.projectSnapshot(), before);
      assert.deepEqual(owner.state, state);
      assert.deepEqual(owner.historyView.future, future);
    }
    send(owner, 'redo');
    assert.equal(
      owner.snapshot().layers[0].nodes.find((n) => n.id === 'active-value').params.value.value,
      9,
    );
    send(owner, 'undo');
    imported.layers[0].id = 'safe-layer';
    incoming.id = 'safe-node';
    incomingCurve.points[0].id = 'safe-point';
    send(owner, 'import-composition', {
      sourceText: serialize(imported),
      compositionId: imported.layers[0].id,
    });
    assert.ok(owner.snapshot().layers.some((layer) => layer.id === 'safe-layer'));
    send(owner, 'undo');
    assert.ok(!owner.snapshot().layers.some((layer) => layer.id === 'safe-layer'));
    send(owner, 'redo');
    assert.ok(owner.snapshot().layers.some((layer) => layer.id === 'safe-layer'));
  }
});

test('project identity preflight preserves Text-local character IDs in normal and prepared edits', async () => {
  const active = blankDocument(),
    text = node('Text');
  active.schemaVersion = 62;
  active.layers[0].id = 'active-layer';
  text.id = 'active-text';
  text.text = 'A';
  text.characterPositions = {
    version: 1,
    basis: 'A',
    units: [{ id: 'c1', text: 'A' }],
    offsets: {},
    nextId: 2,
  };
  active.layers[0].nodes.push(text);
  const project = migrateLegacyDocumentToCompositionProject(active);
  project.compositions[0].id = 'active-comp';
  const inactive = blankDocument(),
    sink = node('Value'),
    source = blankLayer('source');
  inactive.schemaVersion = 62;
  inactive.layers[0].id = 'inactive-layer';
  source.id = 'inactive-source';
  sink.id = 'inactive-sink';
  sink.params.value.expression = 'ref(artwork, "inactive-source", "c2", "value")';
  inactive.layers[0].nodes.push(sink);
  inactive.layers.push(source);
  const second = migrateLegacyDocumentToCompositionProject(inactive).compositions[0];
  second.id = 'inactive-comp';
  project.compositions.push(second);
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'active-comp' });
  send(owner, 'set-text-content', { layerId: 'active-layer', nodeId: 'active-text', text: 'AB' });
  assert.deepEqual(
    owner
      .snapshot()
      .layers[0].nodes.find((n) => n.id === 'active-text')
      .characterPositions.units.map((unit) => unit.id),
    ['c2', 'c3'],
  );
  send(owner, 'undo');
  assert.equal(owner.snapshot().layers[0].nodes.find((n) => n.id === 'active-text').text, 'A');
  send(owner, 'redo');
  assert.equal(owner.snapshot().layers[0].nodes.find((n) => n.id === 'active-text').text, 'AB');
  send(owner, 'undo');
  const bridge = {
    prepareDocument: async () => ({}),
    preparedFonts: () => ({}),
    assertFresh() {},
    activate() {},
    cancel() {},
  };
  const token = await owner.prepareCommand(
    {
      ...owner.state,
      type: 'set-text-content',
      payload: { layerId: 'active-layer', nodeId: 'active-text', text: 'AB' },
    },
    bridge,
  );
  owner.commitPrepared(token);
  assert.deepEqual(
    owner
      .snapshot()
      .layers[0].nodes.find((n) => n.id === 'active-text')
      .characterPositions.units.map((unit) => unit.id),
    ['c2', 'c3'],
  );
  send(owner, 'undo');
  assert.equal(owner.snapshot().layers[0].nodes.find((n) => n.id === 'active-text').text, 'A');
  send(owner, 'redo');
  assert.equal(owner.snapshot().layers[0].nodes.find((n) => n.id === 'active-text').text, 'AB');
});

test('an inactive legacy local reference stays local when another Composition allocates the same ID', () => {
  const active = blankDocument();
  active.schemaVersion = 61;
  active.layers[0].id = 'active-layer';
  const project = migrateLegacyDocumentToCompositionProject(active);
  project.compositions[0].id = 'active-comp';
  const inactive = blankDocument(),
    sink = node('Value');
  inactive.schemaVersion = 61;
  inactive.layers[0].id = 'inactive-layer';
  sink.id = 'inactive-sink';
  sink.params.value.expression = 'ref("node-collision", "value")';
  inactive.layers[0].nodes.push(sink);
  const second = migrateLegacyDocumentToCompositionProject(inactive).compositions[0];
  second.id = 'inactive-comp';
  project.compositions.push(second);
  const owner = createCompositionSessionOwner(project, { activeCompositionId: 'active-comp' });
  const fake = mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
  try {
    const result = send(owner, 'create-node', { layerId: 'active-layer', nodeType: 'Value' });
    assert.equal(result.nodeId, 'node-collision');
  } finally {
    fake.mock.restore();
  }
  const savedInactive = owner
    .projectSnapshot()
    .compositions.find((comp) => comp.id === 'inactive-comp').layers[0];
  assert.throws(() => evaluate(savedInactive).parameterValue('inactive-sink', 'value'), /missing/i);
  send(owner, 'undo');
  assert.ok(!owner.snapshot().layers[0].nodes.some((n) => n.id === 'node-collision'));
  send(owner, 'redo');
  assert.ok(owner.snapshot().layers[0].nodes.some((n) => n.id === 'node-collision'));
});

test('promotion of an existing schema 60 wrapper refuses opaque handle text on all descriptors', () => {
  for (const kind of [
    'Scalar',
    'Vector2',
    'Color',
    'Path',
    'PaintedPath',
    'Image',
    'Boolean',
    'Point',
    'Remap',
  ]) {
    const old = fixture();
    old.doc.schemaVersion = 60;
    const expression = 'ref("curve", "anchor.handleA", "x")';
    if (kind === 'Point') old.curve.points[0].position.expression = expression;
    else if (kind === 'Remap') {
      const remap = node('Path Remap');
      remap.edits = [
        {
          pointId: 'old-edit',
          params: { position: { type: 'Vector2', value: [0, 0], expression, locked: false } },
        },
      ];
      old.b.nodes.push(remap);
    } else {
      const types = {
        Scalar: ['Value', 'value'],
        Vector2: ['Vector', 'value'],
        Color: ['Color', 'value'],
        Path: ['Transform', 'path'],
        PaintedPath: ['Fill', 'path'],
        Image: ['Keyer', 'path'],
        Boolean: ['Toggle', 'value'],
      };
      const [type, key] = types[kind],
        descriptor = node(type);
      assert.equal(descriptor.params[key].type, kind);
      descriptor.params[key].expression = expression;
      old.b.nodes.push(descriptor);
    }
    const current = migrateLegacyDocumentToCompositionProject(old.doc);
    const unchanged = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
    unchanged.id = 'other-existing';
    current.compositions.unshift(unchanged);
    const before = serializeCompositionProject(current);
    const modernFixture = fixture(),
      session = new CommandSession(modernFixture.doc);
    send(session, 'set-parameter-expression', {
      layerId: 'art-a',
      nodeId: 'sink',
      key: 'value',
      expression: qualified,
    });
    const source = migrateLegacyDocumentToCompositionProject(session.snapshot());
    assert.throws(() => prepareProjectCompositionsImport(current, source), /handle|promot/i);
    assert.equal(serializeCompositionProject(current), before);
    const clean = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
    clean.id = 'unselected-clean';
    source.compositions.push(clean);
    const imported = prepareProjectCompositionsImport(current, source, [clean.id]).project;
    assert.equal(imported.schemaVersion, 60);
    assert.ok(serializeCompositionProject(imported).includes('anchor.handleA'));
  }
});

test('picker checks a foreign dependency against the edited property while preserving same-layer exclusion', () => {
  const f = fixture(),
    rectangle = node('Rectangle'),
    vector = node('Vector');
  rectangle.id = 'rect';
  vector.id = 'vector';
  f.a.nodes.push(rectangle);
  f.b.nodes.push(vector);
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-b',
    nodeId: 'vector',
    key: 'value',
    expression: 'ref(artwork, "art-a", "rect", "center", "x"); 0',
  });
  const doc = session.snapshot();
  const targets = (key) =>
    expressionReferenceTargetsForDocument(doc, 'art-a', 'rect', 'Scalar', key);
  assert.ok(targets('size').some((t) => t.nodeId === 'vector' && t.key === 'value'));
  assert.ok(!targets('center').some((t) => t.nodeId === 'vector' && t.key === 'value'));
  assert.ok(
    !expressionReferenceTargetsForDocument(doc, 'art-a', 'rect').some((t) => t.nodeId === 'vector'),
  );
  assert.ok(!targets('size').some((t) => t.nodeId === 'rect'));
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'rect',
    key: 'size',
    expression: 'ref(artwork, "art-b", "vector", "value", "x"); 20',
  });
  const before = session.snapshot();
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: 'art-a',
        nodeId: 'rect',
        key: 'center',
        expression: 'ref(artwork, "art-b", "vector", "value", "x"); 0',
      }),
    /cycle/i,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('graph parameter indexing preserves first-match and virtual fallback semantics per snapshot', () => {
  const f = fixture(),
    key = 'anchor.position';
  const first = {
    type: 'Vector2',
    value: [3, 4],
    expression: 'ref(artwork, "art-a", "sink", "value"); 0',
    locked: false,
  };
  f.curve.params[key] = first;
  assert.strictEqual(parameter(f.curve, key), first);
  const targets = () =>
    expressionReferenceTargetsForDocument(f.doc, 'art-a', 'sink', 'Scalar', 'value');
  assert.ok(!targets().some((target) => target.nodeId === 'curve' && target.key === key));
  first.expression = '';
  f.curve.points[0].position.expression = 'ref(artwork, "art-a", "sink", "value"); 0';
  assert.ok(targets().some((target) => target.nodeId === 'curve' && target.key === key));
  const read = () => evaluate(f.b, {}, {}, f.doc, null, true).parameterValue('curve', key);
  assert.deepEqual(read(), [3, 4]);
  f.curve.params[key] = { ...first, value: [8, 9] };
  assert.deepEqual(read(), [8, 9]);
  f.curve.params[key] = first;
  first.expression = 'ref(artwork, "art-a", "sink", "value"); 0';
  assert.ok(!targets().some((target) => target.nodeId === 'curve' && target.key === key));
  const transform = node('Transform');
  delete transform.params.position;
  delete transform.params.scaleWidth;
  transform.params.anchor.value = [11, 12];
  transform.params.scale.value = 2;
  assert.deepEqual(parameter(transform, 'position').value, [11, 12]);
  assert.equal(parameter(transform, 'scaleWidth').value, 2);
  const mask = node('Mask');
  delete mask.params.feather;
  delete mask.params.luminance;
  assert.equal(parameter(mask, 'feather').value, 0);
  assert.equal(parameter(mask, 'luminance').value, false);
  const boolean = node('Boolean');
  delete boolean.booleanInputOrder;
  boolean.params.b.locked = true;
  assert.equal(parameter(boolean, booleanPendingInputKey).type, 'Path');
});

test('unconnected Resample duplication and deleting an intended Composition target remain undoable', () => {
  const f = fixture(),
    resample = node('Resample Path'),
    composition = node('Composition Reference');
  resample.id = 'resample';
  composition.id = 'comp-ref';
  composition.compositionReference = { compositionId: 'art-b' };
  f.a.nodes.push(resample, composition);
  f.doc.schemaVersion = 31;
  connect(f.a, 'comp-ref', f.a.output, 'path');
  const session = new CommandSession(f.doc);
  send(session, 'set-parameter-expression', {
    layerId: 'art-a',
    nodeId: 'resample',
    key: 'count',
    expression: qualified,
  });
  const before = session.snapshot();
  const { nodeIds } = send(session, 'duplicate-nodes', { layerId: 'art-a', nodeIds: ['resample'] });
  assert.equal(nodeIds.length, 1);
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
  send(session, 'redo');
  send(session, 'delete-layer', { layerId: 'art-b' });
  const missing = session.snapshot();
  assert.equal(
    evaluate(missing.layers[0], {}, {}, missing).errorCode,
    'COMPOSITION_TARGET_MISSING',
  );
  send(session, 'undo');
  assert.ok(session.snapshot().layers.some((l) => l.id === 'art-b'));
  send(session, 'redo');
  assert.ok(!session.snapshot().layers.some((l) => l.id === 'art-b'));
});

test('Text outline and its Position output can be active together while a real Position cycle rejects', () => {
  const doc = blankDocument(),
    a = doc.layers[0],
    b = blankLayer('B');
  const text = node('Text'),
    circle = node('Circle'),
    fill = node('Fill'),
    vector = node('Vector'),
    driver = node('Value');
  doc.schemaVersion = 62;
  doc.layers.push(b);
  a.nodes.push(text, circle, fill, vector);
  b.nodes.push(driver);
  text.text = 'a';
  text.params.position.value = [17, 18];
  const contour = evaluate(a, {}, {}, doc).run(circle.id).contours[0];
  text.pathAttachment = {
    nodeId: circle.id,
    contourId: contour.id,
    startMode: 'distance',
    start: 0,
    reversed: false,
  };
  connect(a, text.id, vector.id, 'value', false, 'position');
  connect(a, text.id, fill.id, 'path');
  connect(a, fill.id, a.output, 'path');
  driver.params.value.expression = `ref(artwork, "${a.id}", "${vector.id}", "value", "x")`;
  const fonts = { default: testFont() },
    session = new CommandSession(doc, { fonts: () => fonts });
  const expression = `ref(artwork, "${b.id}", "${driver.id}", "value")`;
  send(session, 'set-parameter-expression', {
    layerId: a.id,
    nodeId: circle.id,
    key: 'radius',
    expression,
  });
  let snapshot = session.snapshot(),
    result = evaluate(snapshot.layers[0], fonts, snapshot.imageAssets, snapshot);
  assert.equal(result.error, null);
  assert.equal(result.parameterValue(circle.id, 'radius'), 17);
  const stable = session.snapshot(),
    state = session.state;
  assert.throws(
    () =>
      send(session, 'set-parameter-expression', {
        layerId: a.id,
        nodeId: text.id,
        key: 'position',
        expression: `${expression}; 0`,
      }),
    /cycle/i,
  );
  assert.deepEqual(session.snapshot(), stable);
  assert.deepEqual(session.state, state);
  send(session, 'undo');
  snapshot = session.snapshot();
  assert.equal(
    snapshot.layers[0].nodes.find((n) => n.id === circle.id).params.radius.expression,
    '',
  );
  send(session, 'redo');
  snapshot = session.snapshot();
  assert.equal(evaluate(snapshot.layers[0], fonts, snapshot.imageAssets, snapshot).error, null);
});

function legacyDepthFixture(count = 9, depth = 28) {
  const doc = blankDocument();
  doc.schemaVersion = 58;
  for (let i = 1; i < count; i++) doc.layers.push(blankLayer(`Depth ${i}`));
  for (let i = doc.layers.length - 1; i >= 0; i--) {
    const layer = doc.layers[i];
    let current;
    if (i === doc.layers.length - 1) {
      const circle = node('Circle'),
        fill = node('Fill');
      layer.nodes.push(circle, fill);
      connect(layer, circle.id, fill.id, 'path');
      current = fill;
    } else {
      current = node('Composition Reference');
      current.compositionReference = { compositionId: doc.layers[i + 1].id };
      layer.nodes.push(current);
    }
    for (let j = 0; j < depth; j++) {
      const transform = node('Transform');
      layer.nodes.push(transform);
      connect(layer, current.id, transform.id, 'path');
      transform.bypassed = true;
      current = transform;
    }
    connect(layer, current.id, layer.output, 'path');
  }
  return doc;
}

test('legacy bypass chains retain per-layer limits and a separate global evaluation depth limit', () => {
  const doc = legacyDepthFixture();
  validate(doc);
  const result = evaluate(doc.layers[0], {}, {}, doc);
  assert.equal(result.error, null);
  assert.ok(result.path);
  const qualifiedDoc = structuredClone(doc),
    sink = node('Value'),
    driver = node('Value');
  sink.id = 'qualified-depth-sink';
  driver.id = 'qualified-depth-driver';
  qualifiedDoc.layers[0].nodes.push(sink);
  qualifiedDoc.layers.at(-1).nodes.push(driver);
  qualifiedDoc.schemaVersion = 62;
  sink.params.value.expression = `ref(artwork, "${qualifiedDoc.layers.at(-1).id}", "${driver.id}", "value")`;
  validate(qualifiedDoc);
  assert.equal(evaluate(qualifiedDoc.layers[0], {}, {}, qualifiedDoc).error, null);
  const deeper = legacyDepthFixture(19, 28);
  const blocked = evaluate(deeper.layers[0], {}, {}, deeper);
  assert.match(blocked.error, /Evaluation depth exceeds the supported limit/);
  const depthSink = node('Value'),
    depthDriver = node('Value');
  deeper.layers[0].nodes.push(depthSink);
  deeper.layers.at(-1).nodes.push(depthDriver);
  deeper.schemaVersion = 62;
  depthSink.params.value.expression = `ref(artwork, "${deeper.layers.at(-1).id}", "${depthDriver.id}", "value")`;
  assert.throws(() => validate(deeper), /Graph depth exceeds the supported limit/);
  const tooDeep = legacyDepthFixture(1, 28),
    layer = tooDeep.layers[0],
    output = layer.nodes.find((n) => n.id === layer.output);
  let source = output.params.path.source;
  for (let i = 0; i < 230; i++) {
    const transform = node('Transform');
    transform.paintMode = true;
    transform.params.path.type = 'PaintedPath';
    transform.params.path.source = source;
    transform.bypassed = true;
    layer.nodes.push(transform);
    source = transform.id;
  }
  output.params.path.source = source;
  assert.match(evaluate(layer, {}, {}, tooDeep).error, /Graph depth exceeds 256 nodes/);
});

test('parameter recursion uses per-layer limits while retaining cross-layer cycle detection', () => {
  const doc = blankDocument();
  doc.schemaVersion = 62;
  for (let i = 0; i < 9; i++) {
    const layer = i ? blankLayer(`expression ${i}`) : doc.layers[0];
    layer.id = `expression-layer-${i}`;
    if (i) doc.layers.push(layer);
    for (let j = 0; j < 29; j++) {
      const value = node('Value');
      value.id = `value-${i}-${j}`;
      layer.nodes.push(value);
    }
  }
  for (let i = 0; i < 9; i++)
    for (let j = 0; j < 29; j++) {
      const value = doc.layers[i].nodes.find((n) => n.id === `value-${i}-${j}`);
      if (j < 28) value.params.value.expression = `ref("value-${i}-${j + 1}", "value")`;
      else if (i < 8)
        value.params.value.expression = `ref(artwork, "expression-layer-${i + 1}", "value-${i + 1}-0", "value")`;
      else value.params.value.value = 7;
    }
  validate(doc);
  assert.equal(evaluate(doc.layers[0], {}, {}, doc).parameterValue('value-0-0', 'value'), 7);
  const last = doc.layers[8].nodes.find((n) => n.id === 'value-8-28');
  last.params.value.expression = 'ref(artwork, "expression-layer-0", "value-0-0", "value")';
  assert.throws(() => validate(structuredClone(doc)), /cycle/i);
  assert.throws(
    () => evaluate(doc.layers[0], {}, {}, doc).parameterValue('value-0-0', 'value'),
    /cycle/i,
  );
});

test('unconnected Trim and unavailable Text preserve the baseline driven-value behavior', () => {
  for (const type of ['Trim Path', 'Text']) {
    const f = fixture(),
      sink = node(type);
    f.a.nodes.push(sink);
    f.doc.schemaVersion = 37;
    const key = type === 'Trim Path' ? 'start' : 'size';
    if (type === 'Trim Path') sink.params.end.value = 0.5;
    f.driver.params.value.value = type === 'Trim Path' ? 0.2 : 7;
    const session = new CommandSession(f.doc);
    send(session, 'set-parameter-expression', {
      layerId: 'art-a',
      nodeId: sink.id,
      key,
      expression: qualified,
    });
    send(session, 'set-parameter', {
      layerId: 'art-b',
      nodeId: 'driver',
      key: 'value',
      value: type === 'Trim Path' ? 0.8 : -1,
    });
    const doc = session.snapshot();
    assert.equal(
      evaluate(doc.layers[0], {}, {}, doc).parameterValue(sink.id, key),
      type === 'Trim Path' ? 0.8 : -1,
    );
  }
});

test('editing a referenced source does not re-render unrelated Text', () => {
  const f = fixture(),
    unrelated = blankLayer('unrelated'),
    text = node('Text'),
    fill = node('Fill');
  const sameLayerText = node('Text');
  sameLayerText.id = 'same-layer-text';
  sameLayerText.textWrap = { width: 300 };
  sameLayerText.text = 'unrelated same layer';
  f.a.nodes.push(sameLayerText);
  f.doc.schemaVersion = 31;
  unrelated.id = 'unrelated';
  text.id = 'unrelated-text';
  text.text = 'unrelated text';
  unrelated.nodes.push(text, fill);
  connect(unrelated, text.id, fill.id, 'path');
  connect(unrelated, fill.id, unrelated.output, 'path');
  f.doc.layers.push(unrelated);
  const font = testFont(),
    original = font.charToGlyphIndex;
  let calls = 0;
  const spy = mock.method(font, 'charToGlyphIndex', function (...args) {
    calls++;
    return original.apply(this, args);
  });
  try {
    assert.equal(evaluate(unrelated, { default: font }, {}, f.doc).error, null);
    assert.ok(calls > 0);
    calls = 0;
    const session = new CommandSession(f.doc, { fonts: () => ({ default: font }) });
    send(session, 'set-parameter-expression', {
      layerId: 'art-a',
      nodeId: 'sink',
      key: 'value',
      expression: qualified,
    });
    calls = 0;
    send(session, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 8 });
    assert.equal(calls, 0);
    const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(f.doc), {
      fonts: () => ({ default: font }),
    });
    send(owner, 'set-parameter-expression', {
      layerId: 'art-a',
      nodeId: 'sink',
      key: 'value',
      expression: qualified,
    });
    calls = 0;
    send(owner, 'set-parameter', { layerId: 'art-b', nodeId: 'driver', key: 'value', value: 8 });
    assert.equal(calls, 0);
  } finally {
    spy.mock.restore();
  }
});
