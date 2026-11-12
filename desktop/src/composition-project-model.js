import { validateColorSlots, validateColorSlotBinding } from './color-slots.js';
import { validateLayerFolders } from './layer-folders.js';
import {
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  hasComponentCompositionReferenceSyntax,
  nodesHaveComponentCompositionReferenceSyntax,
  hasQualifiedColorReferenceSyntax,
  nodesHaveQualifiedColorReferenceSyntax,
  hasCompositionPropertyReferenceSyntax,
  nodesHaveCompositionPropertyReferenceSyntax,
  allExpressionParameters,
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  MAX_DOCUMENT_SCHEMA_VERSION,
  nodesKnownCurvePointReferenceVersion,
  nodesHaveArtworkReferenceSyntax,
  nodesHaveLegacyCurveHandleReference,
  expressionParameters,
  hasArtworkReferenceSyntax,
} from './curve-point-reference.js';
import { qualifiedReferencesForCodec } from './expression-parser.js';
const ID_PATTERN = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;
const MAX_DIMENSION = 100000;
export const MAX_COMPOSITIONS = 100;
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
  // Inspect even non-enumerable properties before later schema validation can
  // read a required field. JSON traversal must never invoke an accessor.
  for (const key of Object.getOwnPropertyNames(value)) {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor || !Object.hasOwn(descriptor, 'value'))
      throw Error(`Unsupported JSON accessor at ${path}.${key}`);
  }
  if (Array.isArray(value)) {
    for (let index = 0; index < value.length; index++) {
      const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
      if (!descriptor) throw Error(`Sparse JSON array at ${path}[${index}]`);
      if (!Object.hasOwn(descriptor, 'value'))
        throw Error(`Unsupported JSON accessor at ${path}[${index}]`);
      assertJsonTree(descriptor.value, `${path}[${index}]`, ancestors);
    }
  } else {
    for (const key of Object.keys(value)) {
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      assertJsonTree(descriptor.value, `${path}.${key}`, ancestors);
    }
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
    ![COMPOSITION_SCHEMA_VERSION, CURVE_POINT_COMPOSITION_SCHEMA_VERSION].includes(
      project.compositionSchemaVersion,
    ) ||
    !Number.isInteger(project.schemaVersion) ||
    project.schemaVersion < 1 ||
    project.schemaVersion > MAX_DOCUMENT_SCHEMA_VERSION ||
    project.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION !==
      (project.compositionSchemaVersion === CURVE_POINT_COMPOSITION_SCHEMA_VERSION)
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
  const folderIds = new Set();
  for (const slot of validateColorSlots(project).values())
    assertUniqueId(slot.id, 'Color slot', ids);
  const compositions = project.compositions;
  for (const composition of compositions) {
    if (!isRecord(composition)) throw Error('Invalid Composition');
    assertUniqueId(composition.id, 'Composition', ids);
    for (const guide of composition.editor?.guides || []) {
      if (!isRecord(guide)) throw Error('Invalid guide');
      assertUniqueId(guide.id, 'Guide', ids);
    }
    for (const folder of composition.editor?.layerFolders || []) {
      assertUniqueId(folder.id, 'Folder', ids);
      folderIds.add(folder.id);
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

  for (const composition of compositions)
    for (const layer of composition.layers || [])
      for (const group of layer.nodeGroups || [])
        if (folderIds.has(group.id)) throw Error('Folder ID collides with a node group');

  for (const preset of project.nodePresets || []) {
    if (!isRecord(preset)) throw Error('Invalid node preset');
    assertUniqueId(preset.id, 'Node preset', ids);
  }
  return ids;
}

// A narrow create-only preflight for explicit IDs. Inactive Composition entities
// participate in the same namespace as global shared Colors. Call before the
// inner Document Session commits, not after a project-adapter rollback.
export function assertFreshCompositionEntityIds(project, proposedIds) {
  const ids = assertGlobalEntityIds(project);
  // Document validation also registers graph-organization group IDs. Include
  // inactive groups here without broadening the legacy project validator.
  for (const composition of project.compositions)
    for (const layer of composition.layers)
      for (const group of layer.nodeGroups || []) ids.add(group.id);
  for (const id of proposedIds) assertUniqueId(id, 'Proposed entity', ids);
}

function assertCompositionProjectFields(project) {
  assertDocumentEnvelope(project);
  const oldQualifiedReferences = (q) =>
    qualifiedReferencesForCodec(q, {
      allowComponentComposition:
        project.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
    });
  let pointReferenceVersion = 0;
  for (const composition of project.compositions)
    for (const layer of composition.layers || [])
      pointReferenceVersion = Math.max(
        pointReferenceVersion,
        nodesKnownCurvePointReferenceVersion(layer.nodes || []),
      );
  for (const preset of project.nodePresets || [])
    pointReferenceVersion = Math.max(
      pointReferenceVersion,
      nodesKnownCurvePointReferenceVersion(preset.nodes || []),
    );
  if (project.schemaVersion < pointReferenceVersion)
    throw Error(`Curve point references require schemaVersion ${pointReferenceVersion}`);
  for (const composition of project.compositions) {
    assertText(composition.name, 'Composition name');
    assertDimensions(composition.width, composition.height);
    if (
      !Array.isArray(composition.layers) ||
      composition.layers.length < 1 ||
      composition.layers.length > MAX_LAYERS
    )
      throw Error('Invalid Composition layers');
    validateLayerFolders({ editor: composition.editor, layers: composition.layers });
    for (const layer of composition.layers) {
      if (
        !isRecord(layer) ||
        !Array.isArray(layer.nodes) ||
        layer.nodes.length > MAX_NODES_PER_LAYER
      )
        throw Error('Invalid Layer node graph');
    }
  }
  const slots = validateColorSlots(project);
  for (const composition of project.compositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        for (const q of Object.values(node.params || {})) validateColorSlotBinding(q, slots);
  for (const preset of project.nodePresets || [])
    for (const node of preset.nodes || [])
      for (const q of Object.values(node.params || {})) validateColorSlotBinding(q, slots);
  assertGlobalEntityIds(project);
  assertLayerReferenceScope(project);
  if (project.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION) {
    const layerOwners = new Map(
      project.compositions.flatMap((composition) =>
        composition.layers.map((layer) => [layer.id, composition.id]),
      ),
    );
    const nodeOwners = new Map(
      project.compositions.flatMap((composition) =>
        composition.layers.flatMap((layer) => layer.nodes.map((node) => [node.id, layer.id])),
      ),
    );
    for (const composition of project.compositions)
      for (const layer of composition.layers)
        for (const node of layer.nodes)
          for (const q of expressionParameters(node, { includeUnsupported: true })) {
            if (!hasArtworkReferenceSyntax(q.expression)) continue;
            for (const ref of oldQualifiedReferences(q)) {
              if (!ref.layerId || ref.compositionId || ref.qualifier) continue;
              if (ref.layerId === layer.id)
                throw Error('Artwork reference requires a different layer');
              if (layerOwners.has(ref.layerId) && layerOwners.get(ref.layerId) !== composition.id)
                throw Error('Artwork reference cannot cross outer Compositions');
              if (nodeOwners.has(ref.nodeId) && nodeOwners.get(ref.nodeId) !== ref.layerId)
                throw Error('Artwork reference node is in a different layer');
            }
          }
    if (
      (project.nodePresets || []).some((preset) =>
        nodesHaveArtworkReferenceSyntax(preset.nodes || []),
      )
    )
      throw Error('Node presets do not support Artwork references');
  }
  if (project.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) {
    const compositions = new Map(project.compositions.map((c) => [c.id, c]));
    const layerOwners = new Map(
      project.compositions.flatMap((c) => c.layers.map((l) => [l.id, c.id])),
    );
    const nodeOwners = new Map(
      project.compositions.flatMap((c) =>
        c.layers.flatMap((l) => l.nodes.map((n) => [n.id, l.id])),
      ),
    );
    for (const composition of project.compositions)
      for (const layer of composition.layers)
        for (const node of layer.nodes)
          for (const q of allExpressionParameters(node)) {
            if (!hasCompositionPropertyReferenceSyntax(q.expression)) continue;
            if (!['Scalar', 'Color'].includes(q.type))
              throw Error('Composition references require a Scalar expression');
            for (const ref of oldQualifiedReferences(q)) {
              if (!ref.compositionId || ref.qualifier || ref.componentComposition) continue;
              if (ref.type !== 'Scalar' || ref.component !== undefined)
                throw Error('Composition references require a Scalar property');
              if (ref.compositionId === composition.id)
                throw Error('Composition reference requires a different outer Composition');
              if (
                layerOwners.has(ref.layerId) &&
                layerOwners.get(ref.layerId) !== ref.compositionId
              )
                throw Error('Composition reference Artwork is in a different Composition');
              if (nodeOwners.has(ref.nodeId) && nodeOwners.get(ref.nodeId) !== ref.layerId)
                throw Error('Composition reference node is in a different Artwork');
              const target = compositions
                .get(ref.compositionId)
                ?.layers.find((l) => l.id === ref.layerId);
              if (
                target &&
                !target.nodes.some((n) => n.id === ref.nodeId) &&
                nodeOwners.has(ref.nodeId)
              )
                throw Error('Composition reference node is in a different Artwork');
            }
          }
    if (
      (project.nodePresets || []).some((preset) =>
        nodesHaveCompositionPropertyReferenceSyntax(preset.nodes || []),
      )
    )
      throw Error('Node presets do not support Composition property references');
  }
  if (project.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) {
    const layerOwners = new Map(
      project.compositions.flatMap((c) => c.layers.map((l) => [l.id, c.id])),
    );
    const nodeOwners = new Map(
      project.compositions.flatMap((c) =>
        c.layers.flatMap((l) => l.nodes.map((n) => [n.id, l.id])),
      ),
    );
    for (const composition of project.compositions)
      for (const layer of composition.layers)
        for (const node of layer.nodes)
          for (const q of allExpressionParameters(node)) {
            if (!hasQualifiedColorReferenceSyntax(q.expression)) continue;
            if (q.type !== 'Color')
              throw Error('Qualified Color references require a Color expression');
            for (const ref of oldQualifiedReferences(q)) {
              if (!ref.qualifier) continue;
              if (ref.type !== 'Color' || ref.component !== undefined)
                throw Error('Qualified Color references require a Color property');
              const sourceComposition = ref.compositionId || composition.id;
              if (ref.qualifier === 'colorArtwork' && ref.layerId === layer.id)
                throw Error('Color Artwork reference requires a different layer');
              if (ref.qualifier === 'colorComposition' && ref.compositionId === composition.id)
                throw Error('Color Composition reference requires a different outer Composition');
              if (
                layerOwners.has(ref.layerId) &&
                layerOwners.get(ref.layerId) !== sourceComposition
              )
                throw Error('Color reference Artwork is in a different Composition');
              if (nodeOwners.has(ref.nodeId) && nodeOwners.get(ref.nodeId) !== ref.layerId)
                throw Error('Color reference node is in a different Artwork');
            }
          }
    if (
      (project.nodePresets || []).some((preset) =>
        nodesHaveQualifiedColorReferenceSyntax(preset.nodes || []),
      )
    )
      throw Error('Node presets do not support qualified Color references');
  }
  if (project.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) {
    const layerOwners = new Map(
      project.compositions.flatMap((c) => c.layers.map((l) => [l.id, c.id])),
    );
    const nodeOwners = new Map(
      project.compositions.flatMap((c) =>
        c.layers.flatMap((l) => l.nodes.map((n) => [n.id, l.id])),
      ),
    );
    for (const composition of project.compositions)
      for (const layer of composition.layers)
        for (const node of layer.nodes)
          for (const q of allExpressionParameters(node)) {
            if (!hasComponentCompositionReferenceSyntax(q.expression)) continue;
            if (!['Scalar', 'Vector2', 'Color'].includes(q.type))
              throw Error('Component Composition references require a numeric expression');
            for (const ref of qualifiedReferencesForCodec(q)) {
              if (!ref.componentComposition) continue;
              if (ref.type !== 'Scalar' || !ref.component)
                throw Error('Component Composition references require a numeric component');
              if (ref.compositionId === composition.id)
                throw Error(
                  'Component Composition reference requires a different outer Composition',
                );
              if (
                layerOwners.has(ref.layerId) &&
                layerOwners.get(ref.layerId) !== ref.compositionId
              )
                throw Error('Component reference Artwork is in a different Composition');
              if (nodeOwners.has(ref.nodeId) && nodeOwners.get(ref.nodeId) !== ref.layerId)
                throw Error('Component reference node is in a different Artwork');
            }
          }
    if (
      (project.nodePresets || []).some((preset) =>
        nodesHaveComponentCompositionReferenceSyntax(preset.nodes || []),
      )
    )
      throw Error('Node presets do not support Component Composition references');
  }
}

export function assertNoLegacyComponentCompositionReferencePromotion(project) {
  if (project.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) return;
  if (
    project.compositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveComponentCompositionReferenceSyntax(layer.nodes)),
    ) ||
    (project.nodePresets || []).some((preset) =>
      nodesHaveComponentCompositionReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error(
      'Legacy opaque Component Composition reference text cannot be promoted to schemaVersion 65',
    );
}
export function assertNoLegacyComponentCompositionDocumentPromotion(document) {
  if (document.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) return;
  if (
    document.layers.some((layer) => nodesHaveComponentCompositionReferenceSyntax(layer.nodes)) ||
    (document.nodePresets || []).some((preset) =>
      nodesHaveComponentCompositionReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error(
      'Legacy opaque Component Composition reference text cannot be promoted to schemaVersion 65',
    );
}

export function assertNoLegacyQualifiedColorReferencePromotion(project) {
  if (project.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) return;
  if (
    project.compositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveQualifiedColorReferenceSyntax(layer.nodes)),
    ) ||
    (project.nodePresets || []).some((preset) =>
      nodesHaveQualifiedColorReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error(
      'Legacy opaque qualified Color reference text cannot be promoted to schemaVersion 64',
    );
}
export function assertNoLegacyQualifiedColorDocumentPromotion(document) {
  if (document.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) return;
  if (
    document.layers.some((layer) => nodesHaveQualifiedColorReferenceSyntax(layer.nodes)) ||
    (document.nodePresets || []).some((preset) =>
      nodesHaveQualifiedColorReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error(
      'Legacy opaque qualified Color reference text cannot be promoted to schemaVersion 64',
    );
}

export function assertNoLegacyCompositionPropertyReferencePromotion(project) {
  if (project.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) return;
  if (
    project.compositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveCompositionPropertyReferenceSyntax(layer.nodes)),
    ) ||
    (project.nodePresets || []).some((preset) =>
      nodesHaveCompositionPropertyReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error('Legacy opaque Composition reference text cannot be promoted to schemaVersion 63');
}

export function assertNoLegacyCompositionPropertyDocumentPromotion(document) {
  if (document.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) return;
  if (
    document.layers.some((layer) => nodesHaveCompositionPropertyReferenceSyntax(layer.nodes)) ||
    (document.nodePresets || []).some((preset) =>
      nodesHaveCompositionPropertyReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error('Legacy opaque Composition reference text cannot be promoted to schemaVersion 63');
}

// Version promotion must not turn previously opaque wrapper text into a live
// dependency, including text in inactive Compositions and connected sockets.
export function assertNoLegacyArtworkReferencePromotion(project) {
  if (project.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION) return;
  if (
    project.compositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveArtworkReferenceSyntax(layer.nodes)),
    ) ||
    (project.nodePresets || []).some((preset) =>
      nodesHaveArtworkReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error('Legacy opaque Artwork reference text cannot be promoted to schemaVersion 62');
}
export function assertNoLegacyArtworkDocumentPromotion(document) {
  if (document.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION) return;
  if (
    document.layers.some((layer) => nodesHaveArtworkReferenceSyntax(layer.nodes)) ||
    (document.nodePresets || []).some((preset) =>
      nodesHaveArtworkReferenceSyntax(preset.nodes || []),
    )
  )
    throw Error('Legacy opaque Artwork reference text cannot be promoted to schemaVersion 62');
}
export function assertNoLegacyHandleReferencePromotion(project) {
  if (project.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION) return;
  if (
    project.compositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveLegacyCurveHandleReference(layer.nodes)),
    ) ||
    (project.nodePresets || []).some((preset) =>
      nodesHaveLegacyCurveHandleReference(preset.nodes || []),
    )
  )
    throw Error('Legacy unresolved Curve handle expression cannot be promoted to schemaVersion 61');
}
export function assertNoLegacyHandleDocumentPromotion(document) {
  if (document.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION) return;
  if (
    document.layers.some((layer) => nodesHaveLegacyCurveHandleReference(layer.nodes)) ||
    (document.nodePresets || []).some((preset) =>
      nodesHaveLegacyCurveHandleReference(preset.nodes || []),
    )
  )
    throw Error('Legacy unresolved Curve handle expression cannot be promoted to schemaVersion 61');
}

function assertLayerReferenceScope(project) {
  const layerOwners = new Map();
  for (const composition of project.compositions)
    for (const layer of composition.layers) layerOwners.set(layer.id, composition.id);

  for (const composition of project.compositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        if (['Composition Reference', 'SVG'].includes(node.type)) {
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
  for (const slot of validateColorSlots(document).values())
    assertUniqueId(slot.id, 'Color slot', ids);
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
  if (
    !Number.isInteger(document.schemaVersion) ||
    document.schemaVersion < 1 ||
    document.schemaVersion > MAX_DOCUMENT_SCHEMA_VERSION
  )
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
    compositionSchemaVersion:
      copy.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION
        ? CURVE_POINT_COMPOSITION_SCHEMA_VERSION
        : COMPOSITION_SCHEMA_VERSION,
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
  projectFields.schemaVersion = Math.max(base.schemaVersion, edited.schemaVersion);
  if (projectFields.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyComponentCompositionReferencePromotion(base);
    assertNoLegacyComponentCompositionDocumentPromotion(edited);
  }
  if (projectFields.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyQualifiedColorReferencePromotion(base);
    assertNoLegacyQualifiedColorDocumentPromotion(edited);
  }
  if (projectFields.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyCompositionPropertyReferencePromotion(base);
    assertNoLegacyCompositionPropertyDocumentPromotion(edited);
  }
  if (projectFields.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyArtworkReferencePromotion(base);
    assertNoLegacyArtworkDocumentPromotion(edited);
  }
  if (projectFields.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyHandleReferencePromotion(base);
    assertNoLegacyHandleDocumentPromotion(edited);
  }
  // This optional project-global packet can be explicitly cleared through the
  // active Document projection. Do not resurrect it from the base globals.
  if (!Object.hasOwn(editedProjectFields, 'keyColorRecipe')) delete projectFields.keyColorRecipe;
  const compositions = base.compositions.map((composition, index) => {
    if (index !== targetIndex) return composition;
    const updated = { ...composition, width, height, layers };
    if (editor === undefined) delete updated.editor;
    else updated.editor = editor;
    return updated;
  });
  return validateCompositionProject({
    ...projectFields,
    compositionSchemaVersion:
      projectFields.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION
        ? CURVE_POINT_COMPOSITION_SCHEMA_VERSION
        : base.compositionSchemaVersion,
    compositions,
  });
}
