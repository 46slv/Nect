import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, evaluate, node, setCharacterOffsets } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function doc(prefix, withText = false) {
  const d = blankDocument(),
    l = d.layers[0];
  d.schemaVersion = 65;
  l.id = `${prefix}-layer`;
  l.nodes[0].id = `${prefix}-output`;
  l.output = l.nodes[0].id;
  if (withText) {
    const text = node('Text');
    text.id = `${prefix}-text`;
    text.text = 'AV';
    text.characterPositions = setCharacterOffsets('AV', undefined, { c1: [3, 4] });
    l.nodes.push(text);
  }
  return d;
}

function project(document, id) {
  const result = migrateLegacyDocumentToCompositionProject(document);
  result.compositions[0].id = id;
  return result;
}

const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

test('Project Import does not allocate an incoming node at an incoming missing Reference target', () => {
  const destination = doc('destination', true),
    incoming = doc('source', true);
  const destinationVector = node('Vector'),
    sourceVector = node('Vector');
  destinationVector.id = sourceVector.id = 'c1';
  destination.layers[0].nodes.push(destinationVector);
  incoming.layers[0].nodes.push(sourceVector);
  const reference = node('Reference');
  reference.id = 'dangling';
  reference.reference = { nodeId: 'c1-import-1', portId: 'output' };
  incoming.layers[0].nodes.push(reference);
  const source = project(incoming, 'source-comp'),
    owner = createCompositionSessionOwner(project(destination, 'destination-comp'));
  const sourceBefore = structuredClone(source),
    before = serializeCompositionProject(owner.projectSnapshot());
  const result = send(owner, 'import-project-compositions', { sourceProject: source });
  const after = serializeCompositionProject(owner.projectSnapshot()),
    imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  assert.ok(imported.layers[0].nodes.some((n) => n.id === 'c1-import-2'));
  assert.equal(
    imported.layers[0].nodes.find((n) => n.id === 'dangling').reference.nodeId,
    'c1-import-1',
  );
  assert.throws(() => evaluate(imported.layers[0]).run('dangling'), /missing/i);
  assert.deepEqual(source, sourceBefore);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after), {
    activeCompositionId: imported.id,
  });
  assert.throws(() => evaluate(reopened.activeDocument().layers[0]).run('dangling'), /missing/i);
  send(owner, 'undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send(owner, 'redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
});

test('local missing references do not reserve IDs in another outer Composition', () => {
  const destination = doc('destination', true),
    incoming = doc('source');
  const reference = node('Reference'),
    sink = node('Value'),
    source = node('Vector');
  reference.id = 'dangling';
  reference.reference = { nodeId: 'c1', portId: 'output' };
  sink.id = 'sink';
  sink.params.value.expression = 'ref("c1","value")';
  source.id = 'c1';
  destination.layers[0].nodes.push(reference, sink);
  incoming.layers[0].nodes.push(source);
  const owner = createCompositionSessionOwner(project(destination, 'destination-comp'));
  const result = send(owner, 'import-project-compositions', {
    sourceProject: project(incoming, 'source-comp'),
  });
  const imported = owner
    .projectSnapshot()
    .compositions.find((c) => c.id === result.importedCompositionIds[0]);
  assert.ok(imported.layers[0].nodes.some((n) => n.id === 'c1'));
  const reopened = createCompositionSessionOwner(
    deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
  );
  const current = reopened.activeDocument();
  assert.throws(() => evaluate(current.layers[0], {}, {}, current).run('dangling'), /missing/i);
  assert.throws(
    () => evaluate(current.layers[0], {}, {}, current).parameterValue('sink', 'value'),
    /missing/i,
  );
});

for (const entry of [
  {
    version: 65,
    kind: 'composition',
    expression: 'ref(composition, "c1", "c2", "c3", "value")',
    type: 'Value',
  },
  {
    version: 65,
    kind: 'colorArtwork',
    expression: 'ref(colorArtwork, "c2", "c3", "value")',
    type: 'Color',
  },
  {
    version: 65,
    kind: 'colorComposition',
    expression: 'ref(colorComposition, "c1", "c2", "c3", "value")',
    type: 'Color',
  },
  {
    version: 65,
    kind: 'componentComposition',
    expression: 'ref(componentComposition, "c1", "c2", "c3", "value", "x")',
    type: 'Vector',
  },
  {
    version: 65,
    kind: 'componentComposition point',
    expression: 'ref(componentComposition, "c1", "c2", "c3", "c4.position", "x")',
    type: 'Curve',
  },
]) {
  test(`Project Import preserves a missing ${entry.kind} target at schema ${entry.version}`, () => {
    const destination = doc('destination', true),
      incoming = doc('source');
    destination.schemaVersion = entry.version;
    const text = destination.layers[0].nodes.find((n) => n.type === 'Text');
    text.text = 'ABCD';
    text.characterPositions = setCharacterOffsets(text.text, undefined, { c1: [3, 4] });
    const sink = node(entry.type === 'Color' ? 'Fill' : 'Value');
    sink.id = 'sink';
    const q = entry.type === 'Color' ? sink.params.color : sink.params.value;
    q.expression = entry.expression;
    destination.layers[0].nodes.push(sink);
    incoming.layers[0].id = 'c2';
    const target = node(entry.type);
    target.id = 'c3';
    if (entry.type === 'Curve') target.points[0].id = 'c4';
    incoming.layers[0].nodes.push(target);
    const source = project(incoming, 'c1'),
      owner = createCompositionSessionOwner(project(destination, 'destination-comp'));
    const before = serializeCompositionProject(owner.projectSnapshot()),
      sourceBefore = structuredClone(source);
    const result = send(owner, 'import-project-compositions', { sourceProject: source });
    const after = serializeCompositionProject(owner.projectSnapshot());
    const imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
    assert.equal(imported.id, entry.kind === 'colorArtwork' ? 'c1' : 'c1-import-1');
    assert.equal(imported.layers[0].id, 'c2-import-1');
    assert.ok(imported.layers[0].nodes.some((n) => n.id === 'c3-import-1'));
    if (entry.type === 'Curve')
      assert.equal(
        imported.layers[0].nodes.find((n) => n.type === 'Curve').points[0].id,
        'c4-import-1',
      );
    const retained = owner
      .projectSnapshot()
      .compositions[0].layers[0].nodes.find((n) => n.id === 'sink');
    assert.equal(
      (entry.type === 'Color' ? retained.params.color : retained.params.value).expression,
      entry.expression,
    );
    assert.deepEqual(source, sourceBefore);
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(after));
    const reloaded = reopened
      .projectSnapshot()
      .compositions[0].layers[0].nodes.find((n) => n.id === 'sink');
    assert.equal(
      (entry.type === 'Color' ? reloaded.params.color : reloaded.params.value).expression,
      entry.expression,
    );
    const reopenedDoc = reopened.activeDocument();
    assert.throws(
      () =>
        evaluate(reopenedDoc.layers[0], {}, reopenedDoc.imageAssets, reopenedDoc).parameterValue(
          'sink',
          entry.type === 'Color' ? 'color' : 'value',
        ),
      /missing/i,
    );
    send(owner, 'undo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
    send(owner, 'redo');
    assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  });
}

for (const missing of ['layer', 'node', 'point']) {
  test(`Project Import keeps a qualified reference with an existing owner and missing ${missing}`, () => {
    const currentDoc = doc('destination', true),
      targetDoc = doc('target'),
      sourceDoc = doc('source');
    currentDoc.schemaVersion = 65;
    const text = currentDoc.layers[0].nodes.find((n) => n.type === 'Text');
    text.text = 'ABCD';
    text.characterPositions = setCharacterOffsets(text.text, undefined, { c1: [1, 2] });
    const sink = node('Value');
    sink.id = 'sink';
    sink.params.value.expression =
      missing === 'layer'
        ? 'ref(composition, "c1", "c2", "c3", "value")'
        : missing === 'node'
          ? 'ref(componentComposition, "c1", "c2", "c3", "value", "x")'
          : 'ref(componentComposition, "c1", "c2", "c3", "c4.position", "x")';
    currentDoc.layers[0].nodes.push(sink);
    if (missing !== 'layer') targetDoc.layers[0].id = 'c2';
    if (missing === 'point') {
      const curve = node('Curve');
      curve.id = 'c3';
      targetDoc.layers[0].nodes.push(curve);
    }
    if (missing === 'layer') sourceDoc.layers[0].id = 'c2';
    const importedNode = node(
      missing === 'layer' ? 'Value' : missing === 'node' ? 'Vector' : 'Curve',
    );
    importedNode.id = missing === 'point' ? 'source-curve' : 'c3';
    if (missing === 'point') importedNode.points[0].id = 'c4';
    sourceDoc.layers[0].nodes.push(importedNode);
    const current = project(currentDoc, 'destination-comp');
    current.compositions.push(project(targetDoc, 'c1').compositions[0]);
    const owner = createCompositionSessionOwner(current);
    const result = send(owner, 'import-project-compositions', {
      sourceProject: project(sourceDoc, 'source-comp'),
    });
    const imported = owner
      .projectSnapshot()
      .compositions.find((c) => c.id === result.importedCompositionIds[0]);
    if (missing === 'layer') assert.equal(imported.layers[0].id, 'c2-import-1');
    if (missing === 'node') assert.ok(imported.layers[0].nodes.some((n) => n.id === 'c3-import-1'));
    if (missing === 'point')
      assert.equal(
        imported.layers[0].nodes.find((n) => n.type === 'Curve').points[0].id,
        'c4-import-1',
      );
    const reopened = createCompositionSessionOwner(
      deserializeCompositionProject(serializeCompositionProject(owner.projectSnapshot())),
    );
    const d = reopened.activeDocument();
    assert.equal(
      d.layers[0].nodes.find((n) => n.id === 'sink').params.value.expression,
      sink.params.value.expression,
    );
    assert.throws(
      () => evaluate(d.layers[0], {}, d.imageAssets, d).parameterValue('sink', 'value'),
      /missing/i,
    );
  });
}
