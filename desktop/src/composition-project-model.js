const ID_PATTERN = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;
const MAX_DIMENSION = 100000;
const MAX_COMPOSITIONS = 100;
const MAX_LAYERS = 100;
const MAX_NODES_PER_LAYER = 2000;
const MAX_TEXT_LENGTH = 100000;

export const COMPOSITION_SCHEMA_VERSION = 1;

function isRecord(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}

function assertJsonTree(value, path = '$', ancestors = new Set()) {
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return;
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) throw Error(`Invalid JSON number at ${path}`);
    return;
  }
  if (typeof value !== 'object') throw Error(`Invalid JSON value at ${path}`);
  if (ancestors.has(value)) throw Error(`Cyclic JSON value at ${path}`);
  if (!Array.isArray(value) && !isRecord(value)) throw Error(`Invalid JSON object at ${path}`);
  if (Object.getOwnPropertySymbols(value).length)
    throw Error(`Symbol keys are not supported at ${path}`);

  ancestors.add(value);
  if (Array.isArray(value)) {
    for (let index = 0; index < value.length; index++) {
      if (!Object.hasOwn(value, index)) throw Error(`Sparse JSON array at ${path}[${index}]`);
      assertJsonTree(value[index], `${path}[${index}]`, ancestors);
    }
  } else {
    for (const [key, child] of Object.entries(value))
      assertJsonTree(child, `${path}.${key}`, ancestors);
  }
  ancestors.delete(value);
}

function cloneJson(value) {
  assertJsonTree(value);
  return JSON.parse(JSON.stringify(value));
}

function assertText(value, label) {
  if (typeof value !== 'string' || value.length > MAX_TEXT_LENGTH) throw Error(`Invalid ${label}`);
}

function assertId(value, label) {
  if (typeof value !== 'string' || !ID_PATTERN.test(value)) throw Error(`Invalid ${label} ID`);
}

function assertUniqueId(value, label, ids) {
  assertId(value, label);
  if (ids.has(value)) throw Error(`Duplicate ID: ${value}`);
  ids.add(value);
}

function assertDimensions(width, height) {
  for (const [name, value] of [
    ['width', width],
    ['height', height],
  ]) {
    if (!Number.isFinite(value) || value <= 0 || value > MAX_DIMENSION)
      throw Error(`Invalid Composition ${name}`);
  }
}

function assertDocumentEnvelope(project) {
  if (!isRecord(project)) throw Error('Invalid Composition project');
  if (
    project.compositionSchemaVersion !== COMPOSITION_SCHEMA_VERSION ||
    !Number.isInteger(project.schemaVersion) ||
    project.schemaVersion < 1
  )
    throw Error('Unsupported Composition project version');
  assertText(project.name, 'project name');
  if (
    Object.hasOwn(project, 'width') ||
    Object.hasOwn(project, 'height') ||
    Object.hasOwn(project, 'layers') ||
    Object.hasOwn(project, 'editor')
  )
    throw Error('Composition project cannot use legacy root document fields');
  if (
    !Array.isArray(project.compositions) ||
    project.compositions.length < 1 ||
    project.compositions.length > MAX_COMPOSITIONS
  )
    throw Error('Invalid Compositions');
}

function assertGlobalEntityIds(project) {
  const ids = new Set();
  const compositions = project.compositions;
  for (const composition of compositions) {
    if (!isRecord(composition)) throw Error('Invalid Composition');
    assertUniqueId(composition.id, 'Composition', ids);
    for (const guide of composition.editor?.guides || []) {
      if (!isRecord(guide)) throw Error('Invalid guide');
      assertUniqueId(guide.id, 'Guide', ids);
    }
    for (const layer of composition.layers || []) {
      if (!isRecord(layer)) throw Error('Invalid Layer');
      assertUniqueId(layer.id, 'Layer', ids);
      if (!Array.isArray(layer.nodes)) throw Error('Invalid node graph');
      for (const node of layer.nodes) {
        if (!isRecord(node)) throw Error('Invalid node');
        assertUniqueId(node.id, 'Node', ids);
        if (Array.isArray(node.points)) {
          for (const point of node.points) {
            if (!isRecord(point)) throw Error('Invalid point');
            assertUniqueId(point.id, 'Point', ids);
          }
        }
      }
    }
  }

  for (const preset of project.nodePresets || []) {
    if (!isRecord(preset)) throw Error('Invalid node preset');
    assertUniqueId(preset.id, 'Node preset', ids);
  }
}

function assertCompositionProjectFields(project) {
  assertDocumentEnvelope(project);
  for (const composition of project.compositions) {
    assertText(composition.name, 'Composition name');
    assertDimensions(composition.width, composition.height);
    if (
      !Array.isArray(composition.layers) ||
      composition.layers.length < 1 ||
      composition.layers.length > MAX_LAYERS
    )
      throw Error('Invalid Composition layers');
    for (const layer of composition.layers) {
      if (
        !isRecord(layer) ||
        !Array.isArray(layer.nodes) ||
        layer.nodes.length > MAX_NODES_PER_LAYER
      )
        throw Error('Invalid Layer node graph');
    }
  }
  assertGlobalEntityIds(project);
  assertLayerReferenceScope(project);
}

function assertLayerReferenceScope(project) {
  const layerOwners = new Map();
  for (const composition of project.compositions)
    for (const layer of composition.layers) layerOwners.set(layer.id, composition.id);

  for (const composition of project.compositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        if (node.type === 'Composition Reference') {
          // Creation and clearing a source deliberately leave this node unbound.
          if (node.type === 'Composition Reference' && node.compositionReference === null) continue;
          const targetId = node.compositionReference?.compositionId;
          if (!layerOwners.has(targetId)) throw Error('Composition Reference target is missing');
          if (layerOwners.get(targetId) !== composition.id)
            throw Error('Composition Reference cannot cross outer Compositions');
        }
}

export function validateCompositionProject(project) {
  assertJsonTree(project);
  assertCompositionProjectFields(project);
  return project;
}

export function serializeCompositionProject(project) {
  validateCompositionProject(project);
  return JSON.stringify(project);
}

export function deserializeCompositionProject(text, deserializeLegacyDocument = null) {
  if (typeof text !== 'string') throw Error('Composition project JSON must be text');
  const parsed = JSON.parse(text);
  if (
    isRecord(parsed) &&
    (Object.hasOwn(parsed, 'compositionSchemaVersion') || Object.hasOwn(parsed, 'compositions'))
  )
    return validateCompositionProject(parsed);
  const legacy =
    typeof deserializeLegacyDocument === 'function' ? deserializeLegacyDocument(text) : parsed;
  return migrateLegacyDocumentToCompositionProject(legacy);
}

function legacyIds(document) {
  const ids = new Set();
  for (const layer of document.layers) {
    assertUniqueId(layer.id, 'Layer', ids);
    for (const node of layer.nodes) {
      assertUniqueId(node.id, 'Node', ids);
      for (const point of node.points || []) assertUniqueId(point.id, 'Point', ids);
    }
  }
  for (const guide of document.editor?.guides || []) assertUniqueId(guide.id, 'Guide', ids);
  for (const preset of document.nodePresets || []) assertUniqueId(preset.id, 'Node preset', ids);
  return ids;
}

function assertLegacyDocument(document) {
  if (!isRecord(document)) throw Error('Invalid legacy Document');
  if (
    Object.hasOwn(document, 'compositionSchemaVersion') ||
    Object.hasOwn(document, 'compositions')
  )
    throw Error('Refusing to migrate a Composition project more than once');
  if (!Number.isInteger(document.schemaVersion) || document.schemaVersion < 1)
    throw Error('Unsupported legacy Document version');
  assertText(document.name, 'project name');
  assertDimensions(document.width, document.height);
  if (
    !Array.isArray(document.layers) ||
    document.layers.length < 1 ||
    document.layers.length > MAX_LAYERS
  )
    throw Error('Invalid legacy Layers');
  for (const layer of document.layers) {
    if (!isRecord(layer) || !Array.isArray(layer.nodes) || layer.nodes.length > MAX_NODES_PER_LAYER)
      throw Error('Invalid legacy node graph');
  }
  return legacyIds(document);
}

function nextCompositionId(ids) {
  for (let index = 1; ; index++) {
    const candidate = `legacy-composition-${index}`;
    if (!ids.has(candidate)) return candidate;
  }
}

export function migrateLegacyDocumentToCompositionProject(document) {
  const copy = cloneJson(document);
  const ids = assertLegacyDocument(copy);
  const { width, height, layers, editor, ...project } = copy;
  const compositionId = nextCompositionId(ids);
  const migrated = {
    ...project,
    compositionSchemaVersion: COMPOSITION_SCHEMA_VERSION,
    compositions: [
      {
        id: compositionId,
        name: 'Composition 1',
        width,
        height,
        ...(editor === undefined ? {} : { editor }),
        layers,
      },
    ],
  };
  return validateCompositionProject(migrated);
}

export function activeCompositionDocument(project, compositionId) {
  const copy = cloneJson(validateCompositionProject(project));
  const composition = copy.compositions.find((item) => item.id === compositionId);
  if (!composition) throw Error('Unknown active Composition ID');
  const projectFields = { ...copy };
  delete projectFields.compositionSchemaVersion;
  delete projectFields.compositions;
  return {
    ...projectFields,
    width: composition.width,
    height: composition.height,
    ...(composition.editor === undefined ? {} : { editor: composition.editor }),
    layers: composition.layers,
  };
}

export function applyActiveCompositionDocument(project, compositionId, editedDocument) {
  const base = cloneJson(validateCompositionProject(project));
  const targetIndex = base.compositions.findIndex((item) => item.id === compositionId);
  if (targetIndex < 0) throw Error('Unknown active Composition ID');
  const edited = cloneJson(editedDocument);
  assertLegacyDocument(edited);
  const { width, height, layers, editor, ...editedProjectFields } = edited;
  const projectFields = { ...base };
  delete projectFields.compositionSchemaVersion;
  delete projectFields.compositions;
  Object.assign(projectFields, editedProjectFields);
  const compositions = base.compositions.map((composition, index) => {
    if (index !== targetIndex) return composition;
    const updated = { ...composition, width, height, layers };
    if (editor === undefined) delete updated.editor;
    else updated.editor = editor;
    return updated;
  });
  return validateCompositionProject({
    ...projectFields,
    compositionSchemaVersion: base.compositionSchemaVersion,
    compositions,
  });
}
