// Curve point keys are embedded in the existing, deliberately restricted ref grammar.
export const CURVE_POINT_REFERENCE_SCHEMA_VERSION = 60;
export const CURVE_POINT_COMPOSITION_SCHEMA_VERSION = 2;

const pointKey = /^([a-zA-Z0-9][a-zA-Z0-9:_-]{0,180})\.position$/;
const refArguments = /\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"(\s*,\s*)"([a-zA-Z0-9:_.-]+)"/g;
const pointRefArguments =
  /\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"\s*,\s*"([a-zA-Z0-9:_.-]+)"\s*,\s*"(?:x|y)"\s*\)/g;

export function curvePointId(key) {
  return pointKey.exec(key)?.[1] ?? null;
}

export function expressionParameters(node) {
  const result = Object.values(node.params || {});
  for (const point of node.points || []) result.push(point.position, point.handleA, point.handleB);
  for (const edit of node.edits || []) result.push(...Object.values(edit.params || {}));
  return result.filter(
    (q) =>
      q &&
      ['Scalar', 'Vector2', 'Color'].includes(q.type) &&
      typeof q.expression === 'string' &&
      q.expression,
  );
}

export function expressionHasCurvePointReference(source) {
  return [...source.matchAll(pointRefArguments)].some((match) => curvePointId(match[2]));
}

export function nodesHaveCurvePointReferences(nodes) {
  return nodes.some((node) =>
    expressionParameters(node).some((q) => expressionHasCurvePointReference(q.expression)),
  );
}

// Before schema 60, a reference to a deleted node could use any parameter key.
// Only a Curve that is actually present proves this spelling has the new meaning.
export function nodesHaveKnownCurvePointReferences(nodes) {
  const owners = new Map(nodes.map((node) => [node.id, node]));
  return nodes.some((node) =>
    expressionParameters(node).some((q) =>
      [...q.expression.matchAll(refArguments)].some(
        (match) => curvePointId(match[3]) && owners.get(match[1])?.type === 'Curve',
      ),
    ),
  );
}

// Source and copy are paired before any expression is rewritten. A point key
// changes only when that exact point belongs to the referenced copied Curve.
export function remapExpressionReferences(sourceNodes, copiedNodes) {
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
      q.expression = q.expression.replace(refArguments, (match, id, comma, key) => {
        const owner = owners.get(id);
        if (!owner) return match;
        const point = curvePointId(key),
          replacement =
            point && owner.points.has(point) ? `${owner.points.get(point)}.position` : key;
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
    for (const q of expressionParameters(node))
      for (const match of q.expression.matchAll(refArguments)) {
        const owner = byId.get(match[1]),
          point = curvePointId(match[3]);
        if (!owner) reserved.add(match[1]);
        if (owner?.type === 'Curve' && point && !ownsPoint(owner, point)) reserved.add(point);
      }
  return reserved;
}

function pointReferences(nodes) {
  const refs = new Map();
  for (const node of nodes)
    for (const q of expressionParameters(node))
      for (const match of q.expression.matchAll(pointRefArguments)) {
        const point = curvePointId(match[2]);
        if (point) refs.set(`${match[1]}\0${point}`, { owner: match[1], point });
      }
  return refs;
}

export function curvePointReferenceOwners(nodes) {
  return [...new Set([...pointReferences(nodes).values()].map((ref) => ref.owner))];
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
