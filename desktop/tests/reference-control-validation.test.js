import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  setReference,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { assertRepeaterControls, repeaterSettings } from '../src/repeater.js';
import { featherFixture } from './fixtures/mask-feather.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0];
  doc.schemaVersion = 50;
  const source = node('Vector'),
    alternate = node('Vector'),
    reference = node('Reference');
  const count = node('Value'),
    shape = node('Circle'),
    repeater = node('Geometry Repeater'),
    output = node('Output');
  source.params.value.value = [10, 20];
  alternate.params.value.value = [30, 40];
  count.params.value.value = 3;
  repeater.operation = 'ae-2d';
  layer.nodes = [source, alternate, reference, count, shape, repeater, output];
  layer.output = output.id;
  setReference(layer, reference.id, { nodeId: source.id, portId: 'output' });
  connect(layer, reference.id, repeater.id, 'position');
  connect(layer, count.id, repeater.id, 'count');
  connect(layer, shape.id, repeater.id, 'path');
  connect(layer, repeater.id, output.id, 'path');
  return { doc, layer, source, alternate, reference, count, repeater };
}
const runtime = (mode, doc) =>
  mode === 'CommandSession'
    ? new CommandSession(doc)
    : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
const send = (s, f, type, payload) =>
  s.dispatch({ ...s.state, type, payload: { layerId: f.layer.id, ...payload } });
const state = (s) => ({
  document: s.snapshot(),
  project: s.projectSnapshot?.(),
  envelope: s.state,
  past: [...s.historyView.past],
  future: [...s.historyView.future],
});
function rejectCopies(s, f) {
  const before = state(s);
  assert.throws(
    () => send(s, f, 'set-parameter', { nodeId: f.count.id, key: 'value', value: 257 }),
    /Copies.*0 to 256/,
  );
  assert.deepEqual(
    state(s),
    before,
    'invalid independent Copies must not mutate document, revision or history',
  );
}
function diagnosed(s, f, code) {
  const doc = s.snapshot(),
    r = evaluate(doc.layers[0]);
  assert.equal(r.errorCode, code);
  assert.throws(
    () => r.run(f.repeater.id),
    (e) => e.code === code,
  );
  assert.throws(() => exportSVG(doc), /Reference target/);
}

for (const code of ['REFERENCE_TARGET_UNSET', 'REFERENCE_TARGET_MISSING']) {
  test(`${code}: every resolved Repeater control is validated without substituting missing values`, () => {
    const f = fixture();
    const values = Object.fromEntries(
      Object.entries(f.repeater.params).map(([key, q]) => [key, q.value]),
    );
    const invalid = {
      count: 257,
      offset: 257,
      anchor: [NaN, 0],
      position: [0],
      scale: [1001, 100],
      alternateScale: 0,
      alternateFill: 1,
      alternateColor: { hex: 'invalid', alpha: 1 },
      alternateStroke: 1,
      alternateStrokeColor: { hex: '#000000', alpha: 2 },
      rotation: NaN,
      startOpacity: -1,
      endOpacity: 101,
      copiesAbove: 1,
    };
    for (const missing of ['count', 'position', 'scale', 'alternateColor']) {
      const absent = Object.assign(new Error('diagnosed missing control'), { code });
      const read = (settings) => (key) => {
        if (key === missing) throw absent;
        return settings[key];
      };
      assert.equal(
        assertRepeaterControls(read(values), (e) => e.code === code),
        undefined,
      );
      assert.throws(
        () => repeaterSettings(read(values)),
        (e) => e === absent,
        'normal evaluation still fails with the original diagnostic',
      );
      for (const [key, value] of Object.entries(invalid)) {
        if (key === missing) continue;
        assert.throws(
          () => assertRepeaterControls(read({ ...values, [key]: value }), (e) => e.code === code),
          `${key} remains required while ${missing} is unresolved`,
        );
      }
    }
    assert.throws(
      () =>
        assertRepeaterControls(
          (key) => {
            if (key === 'position') throw Object.assign(new Error('missing Position'), { code });
            if (key !== 'endOpacity') return values[key];
            throw new Error('unrelated evaluation error');
          },
          (e) => e.code === code,
        ),
      /unrelated evaluation error/,
    );
  });
}
for (const mode of ['CommandSession', 'CompositionSessionOwner']) {
  for (const action of ['clear', 'delete-node']) {
    test(`${mode}: ${action} geometry Reference does not bypass independent Feather validation on the same Mask`, () => {
      const m = featherFixture(3),
        source = node('Vector'),
        reference = node('Reference'),
        control = node('Value');
      source.params.value.value = [40, 40];
      control.params.value.value = 3;
      m.layer.nodes.push(source, reference, control);
      setReference(m.layer, reference.id, { nodeId: source.id, portId: 'output' });
      connect(m.layer, reference.id, m.shape.id, 'center');
      connect(m.layer, control.id, m.mask.id, 'feather');
      const f = { ...m, source, reference },
        s = runtime(mode, m.doc);
      send(
        s,
        f,
        action === 'clear' ? 'set-reference' : action,
        action === 'clear' ? { nodeId: reference.id, target: null } : { nodeId: source.id },
      );
      const before = state(s);
      assert.throws(
        () => send(s, f, 'set-parameter', { nodeId: control.id, key: 'value', value: 100000 }),
        /Feather/,
      );
      assert.deepEqual(state(s), before);
      send(s, f, 'set-parameter', { nodeId: control.id, key: 'value', value: 4 });
      assert.equal(s.historyView.past.length, before.past.length + 1);
      assert.equal(
        evaluate(s.snapshot().layers[0], {}, m.doc.imageAssets).errorCode,
        action === 'clear' ? 'REFERENCE_TARGET_UNSET' : 'REFERENCE_TARGET_MISSING',
      );
    });
    test(`${mode}: ${action} Position Reference never suppresses same-Repeater Copies validation`, () => {
      const f = fixture(),
        s = runtime(mode, f.doc);
      assert.equal(evaluate(s.snapshot().layers[0]).error, null);
      rejectCopies(s, f); // The original reviewer negative control with a valid Reference.
      send(
        s,
        f,
        action === 'clear' ? 'set-reference' : action,
        action === 'clear' ? { nodeId: f.reference.id, target: null } : { nodeId: f.source.id },
      );
      const code = action === 'clear' ? 'REFERENCE_TARGET_UNSET' : 'REFERENCE_TARGET_MISSING';
      diagnosed(s, f, code);
      rejectCopies(s, f);
      const depth = s.historyView.past.length;
      send(s, f, 'set-parameter', { nodeId: f.count.id, key: 'value', value: 4 });
      assert.equal(s.historyView.past.length, depth + 1, 'a legal independent edit commits once');
      diagnosed(s, f, code);
      send(s, f, 'undo', {});
      assert.equal(evaluate(s.snapshot().layers[0]).parameterValue(f.repeater.id, 'count'), 3);
      send(s, f, 'redo', {});
      assert.equal(evaluate(s.snapshot().layers[0]).parameterValue(f.repeater.id, 'count'), 4);
      const reloaded =
        mode === 'CommandSession'
          ? runtime(mode, deserialize(serialize(s.snapshot())))
          : createCompositionSessionOwner(
              JSON.parse(serializeCompositionProject(s.projectSnapshot())),
            );
      diagnosed(reloaded, f, code);
      rejectCopies(reloaded, f);
      send(reloaded, f, 'set-reference', {
        nodeId: f.reference.id,
        target: { nodeId: f.alternate.id, portId: 'output' },
      });
      assert.equal(evaluate(reloaded.snapshot().layers[0]).error, null);
      assert.deepEqual(
        evaluate(reloaded.snapshot().layers[0]).parameterValue(f.repeater.id, 'position'),
        [30, 40],
      );
      assert.match(exportSVG(reloaded.snapshot()), /<svg/);
      rejectCopies(reloaded, f);
      send(reloaded, f, 'undo', {});
      diagnosed(reloaded, f, code);
      send(reloaded, f, 'redo', {});
      assert.equal(evaluate(reloaded.snapshot().layers[0]).error, null);
    });
    test(`${mode}: ${action} Position Reference preserves later and relational control checks on the same Repeater`, () => {
      const f = fixture();
      const opacity = node('Value'),
        scale = node('Vector');
      opacity.params.value.value = 100;
      scale.params.value.value = [100, 100];
      f.layer.nodes.push(opacity, scale);
      connect(f.layer, opacity.id, f.repeater.id, 'endOpacity');
      connect(f.layer, scale.id, f.repeater.id, 'scale');
      const s = runtime(mode, f.doc);
      send(
        s,
        f,
        action === 'clear' ? 'set-reference' : action,
        action === 'clear' ? { nodeId: f.reference.id, target: null } : { nodeId: f.source.id },
      );
      for (const [nodeId, value, pattern] of [
        [opacity.id, 101, /Opacity/],
        [scale.id, [1001, 100], /Scale/],
      ]) {
        const before = state(s);
        assert.throws(() => send(s, f, 'set-parameter', { nodeId, key: 'value', value }), pattern);
        assert.deepEqual(state(s), before);
      }
      send(s, f, 'set-parameter', { nodeId: scale.id, key: 'value', value: [-100, 100] });
      const before = state(s);
      assert.throws(
        () => send(s, f, 'set-parameter', { nodeId: f.repeater.id, key: 'offset', value: 0.5 }),
        /fractional Offset/,
      );
      assert.deepEqual(state(s), before);
    });
  }
}
