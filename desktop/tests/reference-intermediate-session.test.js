import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  layer,
  node,
  point,
  connect,
  setReference,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture() {
  const doc = document(),
    current = doc.layers[0];
  // The unrelated Composition uses the current blank-document grid contract.
  doc.schemaVersion = 27;
  const source = node('Vector'),
    alternate = node('Vector'),
    spare = node('Value');
  const reference = node('Reference'),
    shape = node('Circle'),
    other = node('Circle');
  const join = node('Join Geometry'),
    offset = node('Closed Path Offset'),
    output = node('Output');
  const open = node('Curve'),
    invalidOffset = node('Closed Path Offset');
  source.params.value.value = [120, 210];
  alternate.params.value.value = [220, 180];
  shape.params.radius.value = 30;
  other.params.radius.value = 20;
  other.params.center.value = [350, 220];
  offset.params.amount.value = 2;
  open.points = [point(0, 0), point(20, 10)];
  open.params.closed.value = false;
  invalidOffset.params.amount.value = 0;
  current.nodes = [
    source,
    alternate,
    spare,
    reference,
    shape,
    other,
    join,
    offset,
    output,
    open,
    invalidOffset,
  ];
  current.output = output.id;
  setReference(current, reference.id, { nodeId: source.id, portId: 'output' });
  connect(current, reference.id, shape.id, 'center');
  connect(current, shape.id, join.id, 'a');
  connect(current, other.id, join.id, 'b');
  connect(current, join.id, offset.id, 'path');
  connect(current, offset.id, output.id, 'path');
  connect(current, open.id, invalidOffset.id, 'path');
  const independent = layer('Independent artwork'),
    independentShape = node('Circle'),
    independentOutput = node('Output'),
    foreign = node('Vector');
  independent.nodes = [independentShape, independentOutput, foreign];
  independent.output = independentOutput.id;
  connect(independent, independentShape.id, independentOutput.id, 'path');
  doc.layers.push(independent);
  return {
    doc,
    current,
    source,
    alternate,
    spare,
    reference,
    shape,
    join,
    offset,
    invalidOffset,
    independent,
    foreign,
  };
}

function runtime(mode, f, document = f.doc) {
  if (mode === 'CommandSession') return new CommandSession(document);
  const subject = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(document),
  );
  const first = subject.activeCompositionId;
  subject.createComposition({ name: 'Unrelated composition', width: 320, height: 240 });
  subject.switchComposition(first);
  return subject;
}
const send = (subject, f, type, payload = {}) =>
  subject.dispatch({ ...subject.state, type, payload: { layerId: f.current.id, ...payload } });
const record = (subject, f) =>
  subject.snapshot().layers[0].nodes.find((n) => n.id === f.reference.id).reference;
const descriptor = (f) => ({ nodeId: f.source.id, portId: 'output' });
function diagnosed(subject, f, code) {
  const doc = subject.snapshot(),
    result = evaluate(doc.layers[0]);
  assert.equal(result.errorCode, code);
  for (const target of [f.reference, f.join, f.offset])
    assert.throws(
      () => result.run(target.id),
      (error) => error.code === code,
    );
  assert.throws(() => exportSVG(doc), /Reference target/);
  assert.equal(evaluate(doc.layers[1]).error, null, 'independent artwork remains evaluable');
  assert.match(exportSVG({ ...doc, layers: [doc.layers[1]] }), /<svg/);
}

for (const mode of ['CommandSession', 'CompositionSessionOwner']) {
  for (const action of ['delete-node', 'delete-nodes', 'clear']) {
    test(`${mode}: ${action} retains explicit Reference diagnostics through bounded Join/Offset, history and reload`, () => {
      const f = fixture(),
        subject = runtime(mode, f);
      assert.equal(evaluate(subject.snapshot().layers[0]).error, null);
      const before = subject.snapshot(),
        depth = subject.historyView.past.length;
      const unrelated = subject.projectSnapshot?.().compositions.at(-1);
      const payload =
        action === 'clear'
          ? { nodeId: f.reference.id, target: null }
          : action === 'delete-node'
            ? { nodeId: f.source.id }
            : { nodeIds: [f.source.id, f.spare.id] };
      assert.doesNotThrow(() =>
        send(subject, f, action === 'clear' ? 'set-reference' : action, payload),
      );
      const changed = subject.snapshot(),
        code = action === 'clear' ? 'REFERENCE_TARGET_UNSET' : 'REFERENCE_TARGET_MISSING';
      assert.equal(subject.historyView.past.length, depth + 1);
      assert.deepEqual(record(subject, f), action === 'clear' ? null : descriptor(f));
      if (action !== 'clear') assert.ok(!changed.layers[0].nodes.some((n) => n.id === f.source.id));
      if (action === 'delete-nodes')
        assert.ok(!changed.layers[0].nodes.some((n) => n.id === f.spare.id));
      assert.deepEqual(changed.layers[1], before.layers[1]);
      diagnosed(subject, f, code);
      if (unrelated) assert.deepEqual(subject.projectSnapshot().compositions.at(-1), unrelated);
      send(subject, f, 'undo');
      assert.deepEqual(subject.snapshot(), before);
      assert.equal(subject.historyView.past.length, depth);
      send(subject, f, 'redo');
      assert.deepEqual(subject.snapshot(), changed);
      assert.equal(subject.historyView.past.length, depth + 1);
      const reloaded =
        mode === 'CommandSession'
          ? new CommandSession(deserialize(serialize(changed)))
          : createCompositionSessionOwner(
              JSON.parse(serializeCompositionProject(subject.projectSnapshot())),
            );
      assert.deepEqual(record(reloaded, f), action === 'clear' ? null : descriptor(f));
      diagnosed(reloaded, f, code);
      const reloadedDepth = reloaded.historyView.past.length;
      send(reloaded, f, 'set-reference', {
        nodeId: f.reference.id,
        target: { nodeId: f.alternate.id, portId: 'output' },
      });
      assert.deepEqual(evaluate(reloaded.snapshot().layers[0]).run(f.reference.id), [220, 180]);
      assert.equal(evaluate(reloaded.snapshot().layers[0]).error, null);
      assert.match(exportSVG(reloaded.snapshot()), /<svg/);
      assert.equal(reloaded.historyView.past.length, reloadedDepth + 1);
      send(reloaded, f, 'undo');
      diagnosed(reloaded, f, code);
      send(reloaded, f, 'redo');
      assert.equal(evaluate(reloaded.snapshot().layers[0]).error, null);
      if (unrelated) assert.deepEqual(reloaded.projectSnapshot().compositions.at(-1), unrelated);
    });
  }

  test(`${mode}: malformed, wrong-type/port, cross-layer and cyclic targets still reject atomically`, () => {
    const f = fixture(),
      subject = runtime(mode, f);
    const invalid = [
      { nodeId: f.source.id, portId: 'output', extra: true },
      { nodeId: f.source.id, portId: 'not-a-port' },
      { nodeId: f.spare.id, portId: 'output' },
      { nodeId: f.source.id, portId: 'position' },
      { nodeId: f.foreign.id, portId: 'output' },
      { nodeId: f.reference.id, portId: 'output' },
      { nodeId: 'genuinely-missing-target', portId: 'output' },
    ];
    for (const target of invalid) {
      const before = subject.snapshot(),
        depth = subject.historyView.past.length;
      assert.throws(() => send(subject, f, 'set-reference', { nodeId: f.reference.id, target }));
      assert.deepEqual(subject.snapshot(), before);
      assert.equal(subject.historyView.past.length, depth);
    }
    const before = subject.snapshot();
    assert.throws(
      () =>
        send(subject, f, 'connect', {
          sourceId: f.reference.id,
          nodeId: f.source.id,
          key: 'value',
        }),
      /cycle/i,
    );
    assert.deepEqual(subject.snapshot(), before);
  });

  test(`${mode}: missing Reference does not admit unrelated bounded-geometry failures`, () => {
    const f = fixture();
    f.current.nodes = f.current.nodes.filter((n) => n.id !== f.source.id);
    const subject = runtime(mode, f),
      before = subject.snapshot(),
      depth = subject.historyView.past.length;
    assert.throws(
      () =>
        send(subject, f, 'set-parameter', { nodeId: f.invalidOffset.id, key: 'amount', value: 5 }),
      /closed/i,
    );
    assert.deepEqual(subject.snapshot(), before);
    assert.equal(subject.historyView.past.length, depth);
  });

  test(`${mode}: unrelated expression errors and locked layers remain atomic`, () => {
    const f = fixture(),
      subject = runtime(mode, f),
      before = subject.snapshot();
    assert.throws(() =>
      send(subject, f, 'set-parameter-expression', {
        nodeId: f.shape.id,
        key: 'radius',
        expression: '1 / 0',
      }),
    );
    assert.deepEqual(subject.snapshot(), before);
    const locked = subject.snapshot();
    locked.layers[0].locked = true;
    const lockedSubject = runtime(mode, f, locked),
      lockedBefore = lockedSubject.snapshot();
    assert.throws(
      () => send(lockedSubject, f, 'set-reference', { nodeId: f.reference.id, target: null }),
      /locked/i,
    );
    assert.deepEqual(lockedSubject.snapshot(), lockedBefore);
    assert.throws(() => send(lockedSubject, f, 'delete-node', { nodeId: f.source.id }), /locked/i);
    assert.deepEqual(lockedSubject.snapshot(), lockedBefore);
  });
}
