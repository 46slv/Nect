import {
  assertCompatibleMacroDefinition,
  snapshotMacroDefinition,
  snapshotMacroInstance,
} from './macro-package.js';

// Candidate-only representation. The normal Project codec does not call this
// until the Macro node, versioned persistence and Session have a common owner.
// Its caller must first validate the whole Project as data-only JSON.
function macroRegistry(project) {
  const definitions = Object.hasOwn(project, 'macroDefinitions') ? project.macroDefinitions : [];
  if (!Array.isArray(definitions)) throw Error('Invalid Macro definitions');

  const registry = new Map();
  for (const value of definitions) {
    const definition = snapshotMacroDefinition(value);
    let revisions = registry.get(definition.id);
    if (!revisions) registry.set(definition.id, (revisions = new Map()));
    const existing = revisions.get(definition.revision);
    if (existing) {
      assertCompatibleMacroDefinition(existing, definition);
      throw Error('Duplicate Macro definition revision');
    }
    revisions.set(definition.revision, definition);
  }
  return registry;
}

export function assertMacroProjectReferences(project) {
  const registry = macroRegistry(project);
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
export function assertMacroProjectDefinitionTransition(before, candidate) {
  const previous = macroRegistry(before),
    next = macroRegistry(candidate);
  for (const [id, revisions] of previous)
    for (const [revision, definition] of revisions) {
      const retained = next.get(id)?.get(revision);
      if (!retained) throw Error('Macro definition removal is not supported');
      assertCompatibleMacroDefinition(definition, retained);
    }
}
