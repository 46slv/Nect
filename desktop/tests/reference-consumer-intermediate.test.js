import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
  document,
  layer,
  node,
  connect,
  setReference,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { featherFixture } from './fixtures/mask-feather.js';

const bytes = readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const fonts = {
  default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
};
function fixture(kind) {
  const maskFixture = kind === 'Mask' ? featherFixture(3) : null;
  const doc = maskFixture?.doc || document(),
    current = doc.layers[0];
  doc.schemaVersion = Math.max(doc.schemaVersion, 50);
  const source = node('Vector'),
    alternate = node('Vector'),
    reference = node('Reference');
  source.params.value.value = [120, 210];
  alternate.params.value.value = [220, 180];
  current.nodes.push(source, alternate, reference);
  setReference(current, reference.id, { nodeId: source.id, portId: 'output' });
  let consumer;
  if (maskFixture) {
    consumer = maskFixture.mask;
    connect(current, reference.id, maskFixture.shape.id, 'center');
  } else {
    consumer = node(kind);
    const fill = node('Fill'),
      output = node('Output');
    current.nodes.push(consumer, fill, output);
    current.output = output.id;
    if (kind === 'Text') {
      consumer.text = 'Alpha beta gamma';
      consumer.textWrap = { width: 160 };
      connect(current, reference.id, consumer.id, 'position');
    } else {
      const shape = node('Circle');
      consumer.operation = 'ae-2d';
      consumer.params.count.value = 3;
      current.nodes.push(shape);
      connect(current, shape.id, consumer.id, 'path');
      connect(current, reference.id, consumer.id, 'position');
    }
    connect(current, consumer.id, fill.id, 'path');
    connect(current, fill.id, output.id, 'path');
  }
  const independent = layer('Independent artwork'),
    shape = node('Circle'),
    output = node('Output');
  independent.nodes = [shape, output];
  independent.output = output.id;
  connect(independent, shape.id, output.id, 'path');
  doc.layers.push(independent);
  return { doc, current, source, alternate, reference, consumer };
}
function runtime(mode, doc) {
  const options = { fonts: () => fonts };
  if (mode === 'CommandSession') return new CommandSession(doc, options);
  return createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc), options);
}
const send = (subject, f, type, payload) =>
  subject.dispatch({ ...subject.state, type, payload: { layerId: f.current.id, ...payload } });
function diagnosed(subject, f, code) {
  const doc = subject.snapshot(),
    result = evaluate(doc.layers[0], fonts, doc.imageAssets, doc);
  assert.equal(result.errorCode, code);
  for (const target of [f.reference, f.consumer])
    assert.throws(
      () => result.run(target.id),
      (error) => error.code === code,
    );
  assert.throws(() => exportSVG(doc, fonts), /Reference target/);
  assert.equal(evaluate(doc.layers.at(-1), fonts).error, null);
  assert.match(exportSVG({ ...doc, layers: [doc.layers.at(-1)] }, fonts), /<svg/);
}
for (const mode of ['CommandSession', 'CompositionSessionOwner']) {
  for (const kind of ['Text', 'Geometry Repeater', 'Mask']) {
    for (const action of ['delete-node', 'delete-nodes', 'clear']) {
      test(`${mode}: ${kind} preserves diagnosed Reference ${action}, history, reload and retarget`, () => {
        const f = fixture(kind),
          subject = runtime(mode, f.doc);
        assert.equal(
          evaluate(subject.snapshot().layers[0], fonts, f.doc.imageAssets, f.doc).error,
          null,
        );
        const before = subject.snapshot(),
          depth = subject.historyView.past.length;
        send(
          subject,
          f,
          action === 'clear' ? 'set-reference' : action,
          action === 'clear'
            ? { nodeId: f.reference.id, target: null }
            : action === 'delete-node'
              ? { nodeId: f.source.id }
              : { nodeIds: [f.source.id] },
        );
        const changed = subject.snapshot(),
          code = action === 'clear' ? 'REFERENCE_TARGET_UNSET' : 'REFERENCE_TARGET_MISSING';
        assert.deepEqual(
          changed.layers[0].nodes.find((n) => n.id === f.reference.id).reference,
          action === 'clear' ? null : { nodeId: f.source.id, portId: 'output' },
        );
        assert.deepEqual(changed.layers.at(-1), before.layers.at(-1));
        assert.equal(subject.historyView.past.length, depth + 1);
        diagnosed(subject, f, code);
        send(subject, f, 'undo', {});
        assert.deepEqual(subject.snapshot(), before);
        send(subject, f, 'redo', {});
        assert.deepEqual(subject.snapshot(), changed);
        const reloaded =
          mode === 'CommandSession'
            ? runtime(mode, deserialize(serialize(changed)))
            : createCompositionSessionOwner(
                JSON.parse(serializeCompositionProject(subject.projectSnapshot())),
                { fonts: () => fonts },
              );
        diagnosed(reloaded, f, code);
        send(reloaded, f, 'set-reference', {
          nodeId: f.reference.id,
          target: { nodeId: f.alternate.id, portId: 'output' },
        });
        assert.equal(evaluate(reloaded.snapshot().layers[0], fonts, f.doc.imageAssets).error, null);
        assert.match(exportSVG(reloaded.snapshot(), fonts), /<svg/);
        send(reloaded, f, 'undo', {});
        diagnosed(reloaded, f, code);
        send(reloaded, f, 'redo', {});
        assert.equal(evaluate(reloaded.snapshot().layers[0], fonts, f.doc.imageAssets).error, null);
      });
    }
    test(`${mode}: missing Reference in ${kind} does not bypass other consumers or malformed wrap`, () => {
      const f = fixture(kind);
      f.current.nodes = f.current.nodes.filter((n) => n.id !== f.source.id);
      const extra = node('Geometry Repeater'),
        scalar = node('Value'),
        text = node('Text');
      extra.operation = 'ae-2d';
      scalar.params.value.value = 3;
      text.text = 'Valid wrap';
      text.textWrap = { width: 160 };
      f.current.nodes.push(extra, scalar, text);
      connect(f.current, scalar.id, extra.id, 'count');
      const subject = runtime(mode, f.doc),
        before = subject.snapshot(),
        depth = subject.historyView.past.length;
      assert.throws(
        () => send(subject, f, 'set-parameter', { nodeId: scalar.id, key: 'value', value: 257 }),
        /Copies.*0 to 256/i,
      );
      assert.deepEqual(subject.snapshot(), before);
      assert.throws(() =>
        send(subject, f, 'set-text-wrap', { nodeId: text.id, wrap: { width: -1 } }),
      );
      assert.deepEqual(subject.snapshot(), before);
      assert.equal(subject.historyView.past.length, depth);
    });
  }
}
