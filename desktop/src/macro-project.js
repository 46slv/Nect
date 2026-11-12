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

// Candidate-only representation. The normal Project codec does not call this
// until the Macro node, versioned persistence and Session have a common owner.
// Its caller must first validate the whole Project as data-only JSON.
export function assertMacroProjectReferences(project) {
  const { registry } = definitionsRegistry(projectDefinitions(project));
  for (const composition of project.compositions)
    for (const layer of composition.layers)
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

  // Detached presets currently have no definition-closure contract.
  for (const preset of project.nodePresets || [])
    if (Array.isArray(preset.nodes) && preset.nodes.some((node) => node?.type === 'Macro'))
      throw Error('Macro node presets need a definition closure');
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
