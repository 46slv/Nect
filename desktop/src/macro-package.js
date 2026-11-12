import { snapshotNodeDeclaration } from './node-foundation.js';
import { isVector2 } from './node-value-validation.js';
import { transformValues } from './transform-affine.js';

// Project admission and the bounded Session route use pinned definitions from
// this validator. The first definition is one Path through two literal
// Transforms; the Effects workflow and external library are still separate.
const idPattern = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;
const scalarKeys = new Set(['scale', 'rotation']);
const hasId = (value) => typeof value === 'string' && idPattern.test(value);
const finite = (value) => typeof value === 'number' && Number.isFinite(value);
const label = (value) =>
  typeof value === 'string' && value.trim().length > 0 && value.length <= 100;
const plainRecord = (value) => !!value && typeof value === 'object' && !Array.isArray(value);

function exactKeys(value, expected, what) {
  if (
    !plainRecord(value) ||
    Object.keys(value).length !== expected.length ||
    expected.some((key) => !Object.hasOwn(value, key))
  )
    throw Error(`Invalid ${what}`);
}

function assertTransformParameters(params) {
  // Use the same affine checks as the existing Transform evaluator, including
  // combinations of finite inputs whose resulting matrix would overflow.
  // A finite matrix alone does not validate a particular Path; Session apply
  // and edits must separately evaluate that Path before an atomic commit.
  transformValues({ params }, (key) => params[key]);
}

function assertEffectiveTransforms(definition, overrides = {}) {
  const values = new Map(definition.nodes.map((item) => [item.id, { ...item.params }]));
  for (const parameter of definition.parameters)
    values.get(parameter.nodeId)[parameter.key] = Object.hasOwn(overrides, parameter.id)
      ? overrides[parameter.id]
      : parameter.defaultValue;
  for (const params of values.values()) assertTransformParameters(params);
}

function validateDefinition(definition) {
  exactKeys(
    definition,
    [
      'format',
      'formatVersion',
      'id',
      'revision',
      'name',
      'input',
      'output',
      'nodes',
      'edges',
      'parameters',
    ],
    'Macro definition',
  );
  if (
    definition.format !== 'nect.macro' ||
    definition.formatVersion !== 1 ||
    !hasId(definition.id) ||
    !Number.isSafeInteger(definition.revision) ||
    definition.revision < 1 ||
    !label(definition.name)
  )
    throw Error('Invalid Macro definition identity');

  if (!Array.isArray(definition.nodes) || definition.nodes.length !== 2)
    throw Error('Macro requires two Transform nodes');
  const nodes = new Map();
  for (const item of definition.nodes) {
    exactKeys(item, ['id', 'type', 'params'], 'Macro node');
    if (!hasId(item.id) || nodes.has(item.id) || item.type !== 'Transform')
      throw Error('Invalid or duplicate Macro node');
    exactKeys(item.params, ['anchor', 'offset', 'scale', 'rotation'], 'Transform parameters');
    if (
      !isVector2(item.params.anchor) ||
      !isVector2(item.params.offset) ||
      !finite(item.params.scale) ||
      !finite(item.params.rotation)
    )
      throw Error('Invalid Transform literal');
    assertTransformParameters(item.params);
    nodes.set(item.id, item);
  }

  exactKeys(definition.input, ['type', 'nodeId', 'socket'], 'Macro input');
  exactKeys(definition.output, ['type', 'nodeId', 'port'], 'Macro output');
  const input = definition.input,
    output = definition.output;
  if (
    input.type !== 'Path' ||
    output.type !== 'Path' ||
    input.socket !== 'path' ||
    output.port !== null ||
    !nodes.has(input.nodeId) ||
    !nodes.has(output.nodeId) ||
    input.nodeId === output.nodeId
  )
    throw Error('Invalid Macro Path interface');
  if (!Array.isArray(definition.edges) || definition.edges.length !== 1)
    throw Error('Invalid Macro graph');
  const edge = definition.edges[0];
  exactKeys(edge, ['sourceNodeId', 'sourcePort', 'targetNodeId', 'targetSocket'], 'Macro edge');
  if (
    edge.sourceNodeId !== input.nodeId ||
    edge.sourcePort !== null ||
    edge.targetNodeId !== output.nodeId ||
    edge.targetSocket !== 'path'
  )
    throw Error('Invalid Macro graph');

  if (!Array.isArray(definition.parameters) || !definition.parameters.length)
    throw Error('Macro needs a published Scalar');
  const ids = new Set(),
    targets = new Set();
  for (const parameter of definition.parameters) {
    exactKeys(
      parameter,
      ['id', 'label', 'nodeId', 'key', 'type', 'defaultValue'],
      'published parameter',
    );
    const target = `${parameter.nodeId}\u0000${parameter.key}`;
    if (
      !hasId(parameter.id) ||
      ids.has(parameter.id) ||
      !label(parameter.label) ||
      parameter.type !== 'Scalar' ||
      !nodes.has(parameter.nodeId) ||
      !scalarKeys.has(parameter.key) ||
      !finite(parameter.defaultValue) ||
      targets.has(target)
    )
      throw Error('Invalid or duplicate published parameter');
    ids.add(parameter.id);
    targets.add(target);
  }
  assertEffectiveTransforms(definition);
}

// Snapshot prevents caller changes from altering a pinned revision after validation.
export function snapshotMacroDefinition(value) {
  const definition = snapshotNodeDeclaration(value);
  validateDefinition(definition);
  return definition;
}

export function parseMacroDefinition(text) {
  if (typeof text !== 'string' || text.length > 262144)
    throw Error('Invalid Macro definition text');
  return snapshotMacroDefinition(JSON.parse(text));
}

function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (!plainRecord(value)) return value;
  return Object.fromEntries(
    Object.keys(value)
      .sort()
      .map((key) => [key, canonical(value[key])]),
  );
}

// A revision is immutable. Different revisions may coexist for pinned instances.
export function assertCompatibleMacroDefinition(existing, incoming) {
  const a = snapshotMacroDefinition(existing),
    b = snapshotMacroDefinition(incoming);
  const sameRevision = a.id === b.id && a.revision === b.revision;
  if (sameRevision && JSON.stringify(canonical(a)) !== JSON.stringify(canonical(b)))
    throw Error('Macro definition ID and revision conflict');
  return sameRevision;
}

export function snapshotMacroInstance(value, definitionValue) {
  const definition = snapshotMacroDefinition(definitionValue),
    instance = snapshotNodeDeclaration(value);
  exactKeys(instance, ['id', 'definitionId', 'definitionRevision', 'overrides'], 'Macro instance');
  if (
    !hasId(instance.id) ||
    instance.definitionId !== definition.id ||
    instance.definitionRevision !== definition.revision ||
    !plainRecord(instance.overrides)
  )
    throw Error('Invalid or unpinned Macro instance');
  const parameters = new Map(definition.parameters.map((item) => [item.id, item]));
  for (const [id, value] of Object.entries(instance.overrides))
    if (!parameters.has(id) || !finite(value)) throw Error('Invalid Macro override');
  assertEffectiveTransforms(definition, instance.overrides);
  return instance;
}

// Resolve by stable public IDs. An explicit zero is an override, not a missing value.
export function resolveMacroInstance(definitionValue, instanceValue) {
  const definition = snapshotMacroDefinition(definitionValue),
    instance = snapshotMacroInstance(instanceValue, definition);
  return Object.freeze(
    definition.parameters.map((parameter) =>
      Object.freeze({
        parameterId: parameter.id,
        nodeId: parameter.nodeId,
        key: parameter.key,
        value: Object.hasOwn(instance.overrides, parameter.id)
          ? instance.overrides[parameter.id]
          : parameter.defaultValue,
      }),
    ),
  );
}

export function setMacroOverride(definition, instanceValue, parameterId, value) {
  if (typeof parameterId !== 'string') throw Error('Missing published parameter');
  const validated = snapshotMacroDefinition(definition),
    instance = snapshotMacroInstance(instanceValue, validated);
  if (!validated.parameters.some((parameter) => parameter.id === parameterId))
    throw Error('Missing published parameter');
  return snapshotMacroInstance(
    { ...instance, overrides: { ...instance.overrides, [parameterId]: value } },
    validated,
  );
}

export function resetMacroOverride(definition, instanceValue, parameterId) {
  const instance = snapshotMacroInstance(instanceValue, definition),
    validated = snapshotMacroDefinition(definition);
  if (!validated.parameters.some((parameter) => parameter.id === parameterId))
    throw Error('Missing published parameter');
  const overrides = { ...instance.overrides };
  delete overrides[parameterId];
  return snapshotMacroInstance({ ...instance, overrides }, validated);
}
