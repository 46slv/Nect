import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, connect, node, validate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { planMacroInsertion, sameMacroInsertionTarget } from '../src/macro-insert-plan.js';

function definition() {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'double',
    revision: 1,
    name: 'Double',
    input: { type: 'Path', nodeId: 'a', socket: 'path' },
    output: { type: 'Path', nodeId: 'b', port: null },
    nodes: ['a', 'b'].map((id) => ({
      id,
      type: 'Transform',
      params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
    })),
    edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
    parameters: [
      { id: 'scale', label: 'Scale', nodeId: 'a', key: 'scale', type: 'Scalar', defaultValue: 1 },
    ],
  };
}

const pin = { definitionId: 'double', revision: 1 };

function fixture(shape = 'Rectangle') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node(shape),
    transform = node('Transform'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 66;
  doc.macroDefinitions = [definition()];
  layer.nodes = [source, transform, fill, output];
  layer.output = output.id;
  connect(layer, source.id, transform.id, 'path');
  connect(layer, transform.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  validate(doc);
  return { doc, layer, source, transform, fill, output };
}

const plan = (f, selected = pin) => planMacroInsertion(f.doc, f.layer.id, selected);

test('plans the exact Rectangle edge and immutable definition pin without mutating authored data', () => {
  const f = fixture(),
    before = structuredClone(f.doc),
    result = plan(f);
  assert.equal(result.supported, true, result.reason);
  assert.deepEqual(result.target, {
    layerId: f.layer.id,
    outputNodeId: f.output.id,
    consumerNodeId: f.transform.id,
    inputKey: 'path',
    producerNodeId: f.source.id,
    producerPort: null,
    definitionId: 'double',
    definitionRevision: 1,
  });
  assert.deepEqual(f.doc, before);
});

test('the chosen exact definition must already be registered in schema 66', () => {
  const f = fixture();
  assert.equal(plan(f, { definitionId: 'double', revision: 2 }).code, 'missing-definition');
  assert.equal(plan(f, { definitionId: 'missing', revision: 1 }).code, 'missing-definition');
  f.doc.schemaVersion = 65;
  assert.equal(plan(f).code, 'missing-definition');
  f.doc.schemaVersion = 67;
  assert.equal(plan(f).code, 'missing-definition');
});

test('pin accessors and malformed pins are refused without invoking them', () => {
  const f = fixture();
  let called = 0;
  const selected = { revision: 1 };
  Object.defineProperty(selected, 'definitionId', {
    enumerable: true,
    get() {
      called++;
      return 'double';
    },
  });
  assert.equal(plan(f, selected).code, 'invalid-definition');
  assert.equal(called, 0);
  assert.equal(
    plan(f, { definitionId: 'double', revision: 1, extra: true }).code,
    'invalid-definition',
  );
});

test('other shapes, missing terminal Transform, lock, bypass and hidden artwork are refused', () => {
  const circle = fixture('Circle');
  assert.equal(plan(circle).code, 'unsupported-route');
  const direct = fixture();
  connect(direct.layer, direct.source.id, direct.fill.id, 'path');
  assert.equal(plan(direct).code, 'unsupported-route');
  const locked = fixture();
  locked.transform.params.path.locked = true;
  assert.equal(plan(locked).code, 'unavailable-edge');
  const bypassed = fixture();
  bypassed.transform.bypassed = true;
  assert.equal(plan(bypassed).supported, false);
  const hidden = fixture();
  hidden.layer.visible = false;
  assert.equal(plan(hidden).code, 'hidden-artwork');
});

test('shared consumers and driven route inputs are rejected without rewiring', () => {
  const shared = fixture(),
    branch = node('Transform');
  shared.layer.nodes.push(branch);
  connect(shared.layer, shared.source.id, branch.id, 'path');
  const before = structuredClone(shared.doc);
  assert.equal(plan(shared).code, 'shared-route');
  assert.deepEqual(shared.doc, before);
  const driven = fixture();
  driven.transform.params.scale.expression = '1';
  assert.equal(plan(driven).code, 'driven-route');
});

test('a reference from an inactive Composition is detected in the real Project context', () => {
  const f = fixture(),
    project = migrateLegacyDocumentToCompositionProject(f.doc),
    other = blankDocument(),
    value = node('Value');
  value.params.value.expression = `ref(composition, "${project.compositions[0].id}", "${f.layer.id}", "${f.transform.id}", "scale")`;
  other.layers[0].nodes.push(value);
  const otherComposition = migrateLegacyDocumentToCompositionProject(other).compositions[0];
  otherComposition.id = 'inactive-composition';
  project.compositions.push(otherComposition);
  const owner = createCompositionSessionOwner(project),
    before = owner.projectSnapshot();
  const result = planMacroInsertion(owner.snapshot(), f.layer.id, pin);
  assert.equal(result.code, 'shared-route');
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.projectRevision, 0);
});

test('the captured target comparison checks every edge endpoint and definition revision', () => {
  const f = fixture(),
    target = plan(f).target;
  assert.ok(sameMacroInsertionTarget(target, structuredClone(target)));
  for (const key of Object.keys(target)) {
    const changed = structuredClone(target);
    changed[key] = key === 'definitionRevision' ? 2 : 'changed';
    assert.equal(sameMacroInsertionTarget(target, changed), false, key);
  }
});
