// Curve point keys are embedded in the existing, deliberately restricted ref grammar.
export const CURVE_POINT_REFERENCE_SCHEMA_VERSION = 60;
export const CURVE_HANDLE_REFERENCE_SCHEMA_VERSION = 61;
export const ARTWORK_REFERENCE_SCHEMA_VERSION = 62;
export const COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION = 63;
export const QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION = 64;
export const MAX_DOCUMENT_SCHEMA_VERSION = QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION;
export const CURVE_POINT_COMPOSITION_SCHEMA_VERSION = 2;

const pointKey = /^([a-zA-Z0-9][a-zA-Z0-9:_-]{0,180})\.(position|handleA|handleB)$/;
const refArguments = /\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"(\s*,\s*)"([a-zA-Z0-9:_.-]+)"/g;
const pointRefArguments =
  /\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"\s*,\s*"(x|y|length)"\s*\)/g;
// This grammar was rejected by the old expression parser. The pure project
// wrapper could nevertheless retain it as opaque text at earlier versions.
const artworkRefArguments =
  /\bref\s*\(\s*artwork\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"(?:\s*,\s*"(x|y|length)")?\s*\)/g;

const compositionRefArguments =
  /\bref\s*\(\s*composition\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"\s*\)/g;
// These reserved words were opaque in wrappers through schema 63.
const colorArtworkArguments =
  /\bref\s*\(\s*colorArtwork\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"\s*\)/g;
const colorCompositionArguments =
  /\bref\s*\(\s*colorComposition\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"\s*\)/g;
export const hasQualifiedColorReferenceSyntax = (source) =>
  /\bref\s*\(\s*(?:colorArtwork|colorComposition)\b/.test(source);
export function qualifiedColorReferences(source) {
  return [
    ...[...source.matchAll(colorArtworkArguments)].map((m) => ({
      qualifier: 'colorArtwork',
      layerId: m[1],
      nodeId: m[2],
      key: m[3],
    })),
    ...[...source.matchAll(colorCompositionArguments)].map((m) => ({
      qualifier: 'colorComposition',
      compositionId: m[1],
      layerId: m[2],
      nodeId: m[3],
      key: m[4],
    })),
  ];
}
export function nodesHaveQualifiedColorReferenceSyntax(nodes) {
  return nodes.some((node) =>
    allExpressionParameters(node).some((q) => hasQualifiedColorReferenceSyntax(q.expression)),
  );
}
export function qualifiedColorReferencesInLayers(layers) {
  return layers.flatMap((layer) =>
    layer.nodes.flatMap((node) =>
      allExpressionParameters(node).flatMap((q) => qualifiedColorReferences(q.expression)),
    ),
  );
}
export function qualifiedColorReferencesInProject(project) {
  return project.compositions.flatMap((composition) =>
    qualifiedColorReferencesInLayers(composition.layers),
  );
}

export const hasCompositionPropertyReferenceSyntax = (source) =>
  /\bref\s*\(\s*composition\b/.test(source);
export function compositionPropertyReferences(source) {
  return [...source.matchAll(compositionRefArguments)].map((match) => ({
    compositionId: match[1],
    layerId: match[2],
    nodeId: match[3],
    key: match[4],
  }));
}
export function nodesHaveCompositionPropertyReferenceSyntax(nodes) {
  return nodes.some((node) =>
    allExpressionParameters(node).some((q) => hasCompositionPropertyReferenceSyntax(q.expression)),
  );
}
export function compositionPropertyReferencesInProject(project) {
  return project.compositions.flatMap((composition) =>
    composition.layers.flatMap((layer) =>
      layer.nodes.flatMap((node) =>
        allExpressionParameters(node).flatMap((q) => compositionPropertyReferences(q.expression)),
      ),
    ),
  );
}

export function artworkExpressionReferences(source) {
  return [...source.matchAll(artworkRefArguments)].map((match) => ({
    layerId: match[1],
    nodeId: match[2],
    key: match[3],
    component: match[4],
  }));
}
export const hasArtworkReferenceSyntax = (source) => /\bref\s*\(\s*artwork\b/.test(source);
export function nodesHaveArtworkReferenceSyntax(nodes) {
  return nodes.some((node) =>
    expressionParameters(node, { includeUnsupported: true }).some((q) =>
      hasArtworkReferenceSyntax(q.expression),
    ),
  );
}

export function nodesHaveArtworkReferences(nodes) {
  return nodes.some((node) =>
    expressionParameters(node, { includeUnsupported: true }).some(
      (q) => artworkExpressionReferences(q.expression).length,
    ),
  );
}
export function artworkReferencesInLayers(layers) {
  return layers.flatMap((layer) =>
    layer.nodes.flatMap((node) =>
      expressionParameters(node, { includeUnsupported: true }).flatMap((q) =>
        artworkExpressionReferences(q.expression),
      ),
    ),
  );
}
export function reserveDanglingArtworkReferenceIds(layers, occupied) {
  const byLayer = new Map(layers.map((layer) => [layer.id, layer]));
  for (const ref of artworkReferencesInLayers(layers)) {
    const layer = byLayer.get(ref.layerId),
      owner = layer?.nodes.find((node) => node.id === ref.nodeId);
    const point = curvePointReference(ref.key, ref.component);
    if (!layer) occupied.add(ref.layerId);
    if (!owner) occupied.add(ref.nodeId);
    if (point && (!owner || owner.type !== 'Curve' || !owner.points.some((p) => p.id === point.id)))
      occupied.add(point.id);
  }
}
export function assertNoImportedArtworkReferenceRebind(
  sourceLayers,
  destinationLayers,
  { outerComposition = false } = {},
) {
  const source = new Map(sourceLayers.map((layer) => [layer.id, layer]));
  const destination = new Map(destinationLayers.map((layer) => [layer.id, layer]));
  for (const ref of artworkReferencesInLayers(sourceLayers)) {
    if (source.has(ref.layerId)) continue;
    if (outerComposition && destination.has(ref.layerId))
      throw Error('Import would retarget a missing Artwork layer');
    const target = destination.get(ref.layerId)?.nodes.find((node) => node.id === ref.nodeId);
    const point = curvePointReference(ref.key, ref.component);
    if (
      target &&
      (!point || (target.type === 'Curve' && target.points.some((p) => p.id === point.id)))
    )
      throw Error('Import would retarget a missing Artwork reference');
  }
}

// Map only identities owned by the referenced copied Artwork and Curve.
export function remapArtworkExpressionReferences(sourceLayers, copiedLayers) {
  const layers = new Map();
  sourceLayers.forEach((source, index) => {
    const copy = copiedLayers[index],
      nodes = new Map();
    source.nodes.forEach((node, i) => {
      const next = copy.nodes[i],
        points = new Map();
      if (node.type === 'Curve')
        node.points.forEach((point, j) => points.set(point.id, next.points[j].id));
      nodes.set(node.id, { id: next.id, points });
    });
    layers.set(source.id, { id: copy.id, nodes });
  });
  for (const layer of copiedLayers)
    for (const node of layer.nodes)
      for (const q of expressionParameters(node, { includeUnsupported: true }))
        q.expression = q.expression.replace(
          artworkRefArguments,
          (match, layerId, nodeId, key, component) => {
            const target = layers.get(layerId),
              owner = target?.nodes.get(nodeId);
            if (!target) return match;
            const point = curvePointReference(key, component);
            const newKey =
              point && owner?.points.has(point.id)
                ? `${owner.points.get(point.id)}.${point.field}`
                : key;
            const parts =
              /^(\bref\s*\(\s*artwork\s*,\s*")[^"]+("\s*,\s*")[^"]+("\s*,\s*")[^"]+("[\s\S]*)$/.exec(
                match,
              );
            return `${parts[1]}${target.id}${parts[2]}${owner?.id || nodeId}${parts[3]}${newKey}${parts[4]}`;
          },
        );
}

export function curvePointReference(key, component) {
  const match = pointKey.exec(key);
  if (!match || !['x', 'y', ...(match[2] === 'position' ? [] : ['length'])].includes(component))
    return null;
  return {
    id: match[1],
    field: match[2],
    schemaVersion:
      match[2] === 'position'
        ? CURVE_POINT_REFERENCE_SCHEMA_VERSION
        : CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  };
}

export function curvePointId(key) {
  return pointKey.exec(key)?.[1] ?? null;
}

export function allExpressionParameters(node) {
  const result = Object.values(node.params || {});
  for (const point of node.points || []) result.push(point.position, point.handleA, point.handleB);
  for (const edit of node.edits || []) result.push(...Object.values(edit.params || {}));
  return result.filter((q) => q && typeof q.expression === 'string' && q.expression);
}

export function expressionParameters(node, { includeUnsupported = false } = {}) {
  const result = Object.values(node.params || {});
  for (const point of node.points || []) result.push(point.position, point.handleA, point.handleB);
  for (const edit of node.edits || []) result.push(...Object.values(edit.params || {}));
  return result.filter(
    (q) =>
      q &&
      (['Scalar', 'Vector2', 'Color'].includes(q.type) ||
        (includeUnsupported && ['Path', 'PaintedPath', 'Image', 'Boolean'].includes(q.type))) &&
      typeof q.expression === 'string' &&
      q.expression,
  );
}

export function expressionCurvePointReferenceVersion(source) {
  let version = 0;
  for (const match of source.matchAll(pointRefArguments))
    version = Math.max(version, curvePointReference(match[2], match[3])?.schemaVersion || 0);
  return version;
}

export function expressionHasCurvePointReference(source) {
  return expressionCurvePointReferenceVersion(source) > 0;
}

// A missing owner's handle x/y was already retainable at schema 60. Length,
// and handle components owned by a real Curve, require schema 61. Check the
// source layer at Copy time, including paused and unsupported descriptors.
export function clipboardReferenceSchemaVersion(nodes, sourceNodes) {
  const owners = new Map(sourceNodes.map((node) => [node.id, node]));
  let version = CURVE_POINT_REFERENCE_SCHEMA_VERSION;
  for (const node of nodes)
    for (const q of allExpressionParameters(node)) {
      if (hasArtworkReferenceSyntax(q.expression)) version = ARTWORK_REFERENCE_SCHEMA_VERSION;
      for (const match of q.expression.matchAll(pointRefArguments)) {
        const point = curvePointReference(match[2], match[3]);
        if (
          point?.schemaVersion === CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
          (match[3] === 'length' || owners.get(match[1])?.type === 'Curve')
        )
          version = Math.max(version, CURVE_HANDLE_REFERENCE_SCHEMA_VERSION);
      }
    }
  return version;
}

export function nodesCurvePointReferenceVersion(
  nodes,
  sourceSchemaVersion = MAX_DOCUMENT_SCHEMA_VERSION,
) {
  let version = 0;
  for (const node of nodes)
    for (const q of expressionParameters(node))
      for (const match of q.expression.matchAll(pointRefArguments)) {
        const required = curvePointReference(match[2], match[3])?.schemaVersion || 0;
        if (required <= sourceSchemaVersion) version = Math.max(version, required);
      }
  return version;
}

// Preserve the schema 60 codec's pre-existing position-key check. Handle-shaped
// keys were accepted as opaque unresolved text by that codec, even where the
// stricter Document Session rejected them. Only the Session can interpret them.
export function nodesKnownCurvePointReferenceVersion(nodes) {
  const owners = new Map(nodes.map((node) => [node.id, node]));
  for (const node of nodes)
    for (const q of expressionParameters(node))
      for (const match of q.expression.matchAll(refArguments))
        if (owners.get(match[1])?.type === 'Curve' && pointKey.exec(match[3])?.[2] === 'position')
          return CURVE_POINT_REFERENCE_SCHEMA_VERSION;
  return 0;
}

// A pre-61 wrapped source can carry handle-shaped text that its old codec never
// interpreted. Importing it into a schema 61 Document would change its meaning.
export function nodesHaveLegacyCurveHandleReference(nodes) {
  const owners = new Map(nodes.map((node) => [node.id, node]));
  return nodes.some((node) =>
    expressionParameters(node, { includeUnsupported: true }).some((q) =>
      [...q.expression.matchAll(pointRefArguments)].some((match) => {
        const ref = curvePointReference(match[2], match[3]);
        return (
          !!ref &&
          ref.field !== 'position' &&
          (match[3] === 'length' || owners.get(match[1])?.type === 'Curve')
        );
      }),
    ),
  );
}

// Source and copy are paired before any expression is rewritten. A point key
// changes only when that exact point belongs to the referenced copied Curve.
export function remapExpressionReferences(
  sourceNodes,
  copiedNodes,
  sourceSchemaVersion = MAX_DOCUMENT_SCHEMA_VERSION,
) {
  const owners = new Map();
  sourceNodes.forEach((source, index) => {
    const copy = copiedNodes[index],
      points = new Map();
    if (source.type === 'Curve')
      source.points.forEach((point, i) => points.set(point.id, copy.points[i].id));
    owners.set(source.id, { id: copy.id, points });
  });
  for (const node of copiedNodes)
    for (const q of expressionParameters(node))
      q.expression = q.expression.replace(refArguments, (match, id, comma, key, offset, source) => {
        const owner = owners.get(id);
        if (!owner) return match;
        const component = /^\s*,\s*"(x|y|length)"\s*\)/.exec(
            source.slice(offset + match.length),
          )?.[1],
          point = curvePointReference(key, component),
          replacement =
            point && point.schemaVersion <= sourceSchemaVersion && owner.points.has(point.id)
              ? `${owner.points.get(point.id)}.${point.field}`
              : key;
        return match.replace(`"${id}"${comma}"${key}"`, `"${owner.id}"${comma}"${replacement}"`);
      });
}

// Prevent a new imported ID from accidentally reviving a dangling reference.
export function danglingExpressionIds(nodes) {
  const byId = new Map(nodes.map((node) => [node.id, node])),
    reserved = new Set(),
    pointIds = new Map();
  const ownsPoint = (owner, point) => {
    if (!pointIds.has(owner.id)) pointIds.set(owner.id, new Set(owner.points.map((p) => p.id)));
    return pointIds.get(owner.id).has(point);
  };
  for (const node of nodes)
    for (const q of expressionParameters(node, { includeUnsupported: true }))
      for (const match of q.expression.matchAll(refArguments)) {
        const owner = byId.get(match[1]),
          point = curvePointId(match[3]);
        if (!['Scalar', 'Vector2', 'Color'].includes(q.type)) {
          const component = /^\s*,\s*"(x|y|length)"\s*\)/.exec(
            q.expression.slice(match.index + match[0].length),
          )?.[1];
          if (!curvePointReference(match[3], component)) continue;
        }
        if (!owner) reserved.add(match[1]);
        if (owner?.type === 'Curve' && point && !ownsPoint(owner, point)) reserved.add(point);
      }
  return reserved;
}

function pointReferences(nodes) {
  const refs = new Map();
  for (const node of nodes)
    for (const q of expressionParameters(node, { includeUnsupported: true }))
      for (const match of q.expression.matchAll(pointRefArguments)) {
        const point = curvePointReference(match[2], match[3]);
        if (point) refs.set(`${match[1]}\0${point.id}`, { owner: match[1], point: point.id });
      }
  return refs;
}

export function curvePointReferenceOwners(nodes) {
  return [...new Set([...pointReferences(nodes).values()].map((ref) => ref.owner))];
}

// Detached Presets and clipboard snapshots can retain a previously missing
// owner. Never let a coincidentally matching destination Curve resolve it.
export function assertNoExternalPointReferenceRebind(
  sourceNodes,
  targetNodes,
  preservedOwners = new Set(),
) {
  const internal = new Set(sourceNodes.map((node) => node.id)),
    targets = new Map(targetNodes.map((node) => [node.id, node])),
    pointIds = new Map();
  for (const { owner, point } of pointReferences(sourceNodes).values()) {
    if (internal.has(owner) || preservedOwners.has(owner)) continue;
    const target = targets.get(owner);
    if (target?.type !== 'Curve') continue;
    if (!pointIds.has(owner)) pointIds.set(owner, new Set(target.points.map((item) => item.id)));
    if (pointIds.get(owner).has(point))
      throw Error('Copy would retarget a missing Curve point reference');
  }
}

// Undo/Redo restores snapshots directly. New edits must never revive a deleted
// target merely because a newly allocated point happens to reuse its ID.
export function assertNoAccidentalCurvePointRebind(before, candidate, clipboard = null) {
  const layerIds = new Set((before.layers || []).map((layer) => layer.id));
  if (clipboard?.sourceLayerId) layerIds.add(clipboard.sourceLayerId);
  for (const layerId of layerIds) {
    const oldLayer = before.layers?.find((item) => item.id === layerId),
      layer = candidate.layers?.find((item) => item.id === layerId);
    if (!layer) continue;
    const oldRefs = pointReferences(oldLayer?.nodes || []),
      clipboardRefs =
        clipboard?.sourceLayerId === layerId ? pointReferences(clipboard.nodes) : new Map(),
      internal = new Set((clipboard?.nodes || []).map((node) => node.id));
    for (const [key, ref] of clipboardRefs) if (!internal.has(ref.owner)) oldRefs.set(key, ref);
    if (!oldRefs.size) continue;
    const afterRefs = pointReferences(layer.nodes),
      oldNodes = new Map((oldLayer?.nodes || []).map((node) => [node.id, node])),
      newNodes = new Map(layer.nodes.map((node) => [node.id, node])),
      oldPoints = new Map(),
      newPoints = new Map();
    for (const [key, ref] of clipboardRefs) if (!internal.has(ref.owner)) afterRefs.set(key, ref);
    const hasPoint = (owner, point, cache) => {
      if (owner?.type !== 'Curve') return false;
      if (!cache.has(owner.id)) cache.set(owner.id, new Set(owner.points.map((item) => item.id)));
      return cache.get(owner.id).has(point);
    };
    for (const [key, ref] of oldRefs) {
      if (!afterRefs.has(key)) continue;
      const oldOwner = oldNodes.get(ref.owner),
        newOwner = newNodes.get(ref.owner),
        existed = hasPoint(oldOwner, ref.point, oldPoints),
        nowExists = hasPoint(newOwner, ref.point, newPoints);
      if (!existed && nowExists)
        throw Error('New point ID would retarget a missing Curve reference');
    }
  }
}
