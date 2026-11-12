import {
  assertCompatibleMacroDefinition,
  snapshotMacroDefinition,
  snapshotMacroInstance,
} from './macro-package.js';

function dataArray(values, label) {
  if (!Array.isArray(values) || Object.getPrototypeOf(values) !== Array.prototype)
    throw Error(`Invalid ${label}`);
  const descriptors = Object.getOwnPropertyDescriptors(values);
  for (const key of Reflect.ownKeys(descriptors)) {
    if (key === 'length') continue;
    if (typeof key !== 'string' || !/^(0|[1-9][0-9]*)$/.test(key) || Number(key) >= values.length)
      throw Error(`Invalid ${label}`);
  }
  const result = [];
  for (let index = 0; index < values.length; index++) {
    const descriptor = descriptors[String(index)];
    if (!descriptor || !descriptor.enumerable || !Object.hasOwn(descriptor, 'value'))
      throw Error(`Invalid ${label}`);
    result.push(descriptor.value);
  }
  return result;
}

function ownEnumerableValue(record, key, label, { optional = false } = {}) {
  if (!record || typeof record !== 'object' || Array.isArray(record))
    throw Error(`Invalid ${label}`);
  const descriptor = Object.getOwnPropertyDescriptor(record, key);
  if (!descriptor && optional) return undefined;
  if (!descriptor || !descriptor.enumerable || !Object.hasOwn(descriptor, 'value'))
    throw Error(`Invalid enumerable ${label}`);
  return descriptor.value;
}

function assertMacroLayersEnvelope(layers) {
  for (const layer of dataArray(layers, 'Macro Composition layers')) {
    const nodes = ownEnumerableValue(layer, 'nodes', 'Macro Layer nodes');
    for (const node of dataArray(nodes, 'Macro Layer nodes')) {
      ownEnumerableValue(node, 'id', 'Macro candidate node ID');
      const type = ownEnumerableValue(node, 'type', 'Macro candidate node type');
      if (typeof type !== 'string') throw Error('Invalid Macro candidate node type');
      if (type !== 'Macro') continue;
      const pin = ownEnumerableValue(node, 'macro', 'Macro instance pin');
      if (!pin || typeof pin !== 'object' || Array.isArray(pin))
        throw Error('Invalid Macro instance pin');
      const keys = Reflect.ownKeys(pin);
      if (
        keys.length !== 3 ||
        keys.some((key) => !['definitionId', 'definitionRevision', 'overrides'].includes(key))
      )
        throw Error('Invalid Macro instance pin');
      for (const key of keys) ownEnumerableValue(pin, key, 'Macro instance pin field');
    }
  }
}

function assertMacroPresetsEnvelope(document) {
  const presets = ownEnumerableValue(document, 'nodePresets', 'Macro Project presets', {
    optional: true,
  });
  if (presets !== undefined)
    for (const preset of dataArray(presets, 'Macro Project presets')) {
      const nodes = ownEnumerableValue(preset, 'nodes', 'Macro preset nodes', { optional: true });
      if (nodes !== undefined)
        for (const node of dataArray(nodes, 'Macro preset nodes'))
          ownEnumerableValue(node, 'type', 'Macro preset node type');
    }
}

// Inspect the typed route before the existing codec reads any hidden payload.
export function assertMacroCandidateEnvelope(project) {
  ownEnumerableValue(project, 'macroDefinitions', 'Macro definitions', { optional: true });
  const compositions = ownEnumerableValue(project, 'compositions', 'Macro Project compositions');
  for (const composition of dataArray(compositions, 'Macro Project compositions'))
    assertMacroLayersEnvelope(
      ownEnumerableValue(composition, 'layers', 'Macro Composition layers'),
    );
  assertMacroPresetsEnvelope(project);
}

export function assertMacroDocumentEnvelope(document) {
  ownEnumerableValue(document, 'macroDefinitions', 'Macro definitions', { optional: true });
  assertMacroLayersEnvelope(ownEnumerableValue(document, 'layers', 'Macro Document layers'));
  assertMacroPresetsEnvelope(document);
}

// JSON.stringify invokes own or inherited toJSON methods, including hidden
// ones, and silently drops hidden object fields and extra array properties.
// Candidate admission rejects those cases without calling a getter. The
// ordinary codec deliberately retains its old behavior.
export function assertNoMacroCandidateJsonCallbacks(value) {
  if (!value || typeof value !== 'object') return;
  let current = value;
  while (current) {
    const descriptor = Object.getOwnPropertyDescriptor(current, 'toJSON');
    if (descriptor) {
      if (!Object.hasOwn(descriptor, 'value') || typeof descriptor.value === 'function')
        throw Error('Macro candidate cannot use a toJSON callback');
      break;
    }
    current = Object.getPrototypeOf(current);
  }
  const descriptors = Object.getOwnPropertyDescriptors(value);
  if (Array.isArray(value)) {
    for (const key of Object.getOwnPropertyNames(descriptors)) {
      if (key === 'length') continue;
      if (!/^(0|[1-9][0-9]*)$/.test(key) || Number(key) >= value.length)
        throw Error('Macro candidate cannot serialize extra array data');
    }
    for (let index = 0; index < value.length; index++)
      assertNoMacroCandidateJsonCallbacks(descriptors[String(index)].value);
  } else {
    for (const key of Object.getOwnPropertyNames(descriptors)) {
      if (!descriptors[key].enumerable)
        throw Error('Macro candidate cannot serialize hidden object data');
      assertNoMacroCandidateJsonCallbacks(descriptors[key].value);
    }
  }
}

function definitionsRegistry(definitions) {
  const registry = new Map();
  const snapshots = [];
  for (const value of dataArray(definitions, 'Macro definitions')) {
    const definition = snapshotMacroDefinition(value);
    let revisions = registry.get(definition.id);
    if (!revisions) registry.set(definition.id, (revisions = new Map()));
    const existing = revisions.get(definition.revision);
    if (existing) {
      assertCompatibleMacroDefinition(existing, definition);
      throw Error('Duplicate Macro definition revision');
    }
    revisions.set(definition.revision, definition);
    snapshots.push(definition);
  }
  return { registry, snapshots };
}

function projectDefinitions(project) {
  return Object.hasOwn(project, 'macroDefinitions') ? project.macroDefinitions : [];
}

// Import is idempotent for the same immutable revision and atomic on conflict.
// This only merges validated definition data; it does not commit a Project.
export function mergeMacroDefinitions(existing, incoming) {
  const { registry, snapshots } = definitionsRegistry(existing);
  for (const value of dataArray(incoming, 'Macro definitions')) {
    const definition = snapshotMacroDefinition(value);
    let revisions = registry.get(definition.id);
    if (!revisions) registry.set(definition.id, (revisions = new Map()));
    const previous = revisions.get(definition.revision);
    if (previous) {
      assertCompatibleMacroDefinition(previous, definition);
      continue;
    }
    revisions.set(definition.revision, definition);
    snapshots.push(definition);
  }
  return Object.freeze(snapshots);
}

// Schema 66 validates every stored instance pin against the Project's
// definitions. Its caller first validates the whole Project as data-only JSON.
function assertMacroLayerReferences(layers, registry) {
  for (const layer of layers)
    for (const node of layer.nodes) {
      if (node.type !== 'Macro') continue;
      const pin = node.macro;
      if (
        !pin ||
        typeof pin !== 'object' ||
        Array.isArray(pin) ||
        Object.keys(pin).length !== 3 ||
        !Object.hasOwn(pin, 'definitionId') ||
        !Object.hasOwn(pin, 'definitionRevision') ||
        !Object.hasOwn(pin, 'overrides') ||
        typeof pin.definitionId !== 'string' ||
        !Number.isSafeInteger(pin.definitionRevision)
      )
        throw Error('Invalid Macro instance pin');
      const definition = registry.get(pin.definitionId)?.get(pin.definitionRevision);
      if (!definition) throw Error('Macro definition for instance pin is missing');
      snapshotMacroInstance({ id: node.id, ...pin }, definition);
    }
}

function assertNoMacroPresets(project) {
  // Detached presets currently have no definition-closure contract.
  for (const preset of project.nodePresets || [])
    if (Array.isArray(preset.nodes) && preset.nodes.some((node) => node?.type === 'Macro'))
      throw Error('Macro node presets need a definition closure');
}

export function assertMacroProjectReferences(project) {
  const { registry } = definitionsRegistry(projectDefinitions(project));
  for (const composition of project.compositions)
    assertMacroLayerReferences(composition.layers, registry);
  assertNoMacroPresets(project);
}

export function assertMacroDocumentReferences(document) {
  const { registry } = definitionsRegistry(projectDefinitions(document));
  assertMacroLayerReferences(document.layers, registry);
  assertNoMacroPresets(document);
}

// A later revision may be added, but the same ID/revision must not acquire
// different content. Until definition reuse and History are specified, an
// edit cannot remove definitions and later reuse their old identities.
export function assertMacroProjectDefinitionTransition(before, candidate, retainedDefinitionSets) {
  const { registry: previous } = definitionsRegistry(projectDefinitions(before)),
    { registry: next } = definitionsRegistry(projectDefinitions(candidate));
  for (const [id, revisions] of previous)
    for (const [revision, definition] of revisions) {
      const retained = next.get(id)?.get(revision);
      if (!retained) throw Error('Macro definition removal is not supported');
      assertCompatibleMacroDefinition(definition, retained);
    }
  // Undo/Redo snapshots can retain an identity absent from the current state.
  // Only the owning Session can supply the complete set of retained pins.
  for (const values of dataArray(retainedDefinitionSets, 'retained Macro definition sets')) {
    const { registry: retained } = definitionsRegistry(values);
    for (const [id, revisions] of retained)
      for (const [revision, definition] of revisions) {
        const incoming = next.get(id)?.get(revision);
        if (incoming) assertCompatibleMacroDefinition(definition, incoming);
      }
  }
}
