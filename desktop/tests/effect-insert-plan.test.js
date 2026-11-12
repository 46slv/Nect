import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { blankDocument, blankLayer, connect, node } from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import {
  planTransformEffect,
  sameTransformEffectTarget,
  TRANSFORM_EFFECT_ID,
} from '../src/effect-insert-plan.js';

function fixture(type = 'Rectangle') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node(type),
    fill = node('Fill'),
    output = node('Output');
  layer.nodes = [source, fill, output];
  layer.output = output.id;
  connect(layer, source.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, fill, output };
}

for (const type of ['Circle', 'Ellipse', 'Rectangle', 'Polygon', 'Star'])
  test(`Transform plan identifies the filled ${type} edge without mutation`, () => {
    const f = fixture(type),
      before = structuredClone(f.doc),
      result = planTransformEffect(f.doc, f.layer.id);
    assert.equal(result.supported, true, result.reason);
    assert.deepEqual(result.target, {
      effectId: TRANSFORM_EFFECT_ID,
      layerId: f.layer.id,
      outputNodeId: f.output.id,
      consumerNodeId: f.fill.id,
      inputKey: 'path',
      producerNodeId: f.source.id,
      producerPort: null,
    });
    assert.deepEqual(f.doc, before);
  });

test('Transform plan accepts supported unattached Text with the actual font', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    f = fixture('Text');
  f.source.text = 'Nect';
  const result = planTransformEffect(f.doc, f.layer.id, { fonts: { default: font } });
  assert.equal(result.supported, true, result.reason);
  assert.equal(result.target.producerNodeId, f.source.id);
});

test('Transform plan refuses locked, hidden, bypassed and unavailable routes without mutation', () => {
  for (const change of [
    (f) => (f.layer.locked = true),
    (f) => (f.layer.visible = false),
    (f) => (f.source.locked = true),
    (f) => (f.fill.params.path.locked = true),
    (f) => (f.fill.bypassed = true),
    (f) => delete f.fill.params.path.source,
    (f) => (f.fill.params.path.sourcePort = 'position'),
  ]) {
    const f = fixture();
    change(f);
    const before = structuredClone(f.doc);
    assert.equal(planTransformEffect(f.doc, f.layer.id).supported, false);
    assert.deepEqual(f.doc, before);
  }
});

test('Transform plan refuses existing Transform, unsupported paint and multiple consumers', () => {
  {
    const f = fixture(),
      transform = node('Transform');
    f.layer.nodes.push(transform);
    connect(f.layer, f.source.id, transform.id, 'path');
    connect(f.layer, transform.id, f.fill.id, 'path');
    assert.equal(planTransformEffect(f.doc, f.layer.id).supported, false);
  }
  {
    const f = fixture(),
      gradient = node('Linear Gradient Fill');
    f.layer.nodes.push(gradient);
    connect(f.layer, f.source.id, gradient.id, 'path');
    connect(f.layer, gradient.id, f.output.id, 'path');
    assert.equal(planTransformEffect(f.doc, f.layer.id).supported, false);
  }
  {
    const f = fixture(),
      other = node('Fill');
    f.layer.nodes.push(other);
    connect(f.layer, f.source.id, other.id, 'path');
    const result = planTransformEffect(f.doc, f.layer.id);
    assert.equal(result.supported, false);
    assert.equal(result.code, 'shared-route');
  }
});

test('Transform plan refuses another Layer referencing the artwork', () => {
  const f = fixture(),
    other = blankLayer('Consumer'),
    reference = node('Composition Reference');
  reference.compositionReference = { compositionId: f.layer.id };
  other.nodes.push(reference);
  f.doc.layers.push(other);
  const result = planTransformEffect(f.doc, f.layer.id);
  assert.equal(result.supported, false);
  assert.equal(result.code, 'shared-route');
});

test('Transform plan inspects consumers in another Composition through the bound Project', () => {
  const f = fixture('Circle'),
    project = migrateLegacyDocumentToCompositionProject(f.doc),
    other = blankDocument(),
    value = node('Value');
  project.compositions[0].id = 'source-composition';
  other.layers[0].nodes.push(value);
  project.compositions.push({
    id: 'other-composition',
    name: 'Other',
    width: other.width,
    height: other.height,
    layers: other.layers,
    editor: other.editor,
  });
  const owner = createCompositionSessionOwner(project, {
    activeCompositionId: 'other-composition',
  });
  owner.dispatch({
    ...owner.state,
    type: 'set-parameter-expression',
    payload: {
      layerId: other.layers[0].id,
      nodeId: value.id,
      key: 'value',
      expression: `ref(composition, "source-composition", "${f.layer.id}", "${f.source.id}", "radius")`,
    },
  });
  owner.switchComposition('source-composition');
  const before = owner.projectSnapshot(),
    result = planTransformEffect(owner.historyView.doc, f.layer.id);
  assert.equal(result.supported, false);
  assert.equal(result.code, 'shared-route');
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('Transform edge comparison rejects a changed identity or source port', () => {
  const f = fixture(),
    target = planTransformEffect(f.doc, f.layer.id).target;
  assert.equal(sameTransformEffectTarget(target, { ...target }), true);
  for (const key of [
    'effectId',
    'layerId',
    'outputNodeId',
    'consumerNodeId',
    'inputKey',
    'producerNodeId',
    'producerPort',
  ])
    assert.equal(sameTransformEffectTarget(target, { ...target, [key]: 'changed' }), false, key);
  assert.equal(sameTransformEffectTarget(target, null), false);
  assert.equal(sameTransformEffectTarget({}, {}), false);
});
