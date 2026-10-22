export { inverseAffineDelta, inverseAffinePoint } from './character-geometry.js';
import { evaluate, parameter } from './core.js';
import {
  identityMatrix,
  multiplyMatrices,
  transformCharacterPoint,
  inverseAffineDelta,
} from './character-geometry.js';
const pass = new Set([
  'Output',
  'Fill',
  'Stroke',
  'Linear Gradient Fill',
  'Radial Gradient Fill',
  'Convert to Path',
]);
// Resolve one unambiguous geometry route. Branching/generators/instances stay
// authored by their own controls; this query never creates or bakes a source.
export function directSelectionSource(doc, layerId, { fonts = {}, sourceId } = {}) {
  try {
    const layer = doc.layers.find((l) => l.id === layerId);
    if (!layer || !layer.visible || layer.guide) throw Error('Choose visible artwork');
    const byId = new Map(layer.nodes.map((n) => [n.id, n])),
      seen = new Set(),
      result = evaluate(layer, fonts, doc.imageAssets, doc);
    let routeLocked = layer.locked;
    let n = byId.get(layer.output),
      matrix = identityMatrix();
    while (n) {
      if (seen.has(n.id) || seen.size >= 256) throw Error('Path route is cyclic or too deep');
      seen.add(n.id);
      routeLocked ||= n.locked === true;
      if (n.bypassed) throw Error('Restore the path route before direct selection');
      if (['Curve', 'Multi Path', 'Path Remap'].includes(n.type)) break;
      if (n.type === 'Transform') {
        if (
          Object.entries(n.params).some(([key, q]) => key !== 'path' && (q.source || q.expression))
        )
          throw Error(
            'Direct selection through a driven Transform is unavailable; edit its source in the graph',
          );
        matrix = multiplyMatrices(matrix, result.transformValues(n.id).matrix);
      } else if (!pass.has(n.type))
        throw Error(
          'Select an editable source in the graph; generated or combined paths are protected',
        );
      const q = parameter(n, 'path');
      if (!q?.source) throw Error('Connect an editable source path');
      n = byId.get(q.source);
    }
    if (!n || (sourceId && n.id !== sourceId))
      throw Error('Selected source is outside this direct path route');
    try {
      inverseAffineDelta(matrix, [0, 0]);
    } catch {
      throw Error('Direct selection requires a nonsingular Transform');
    }
    const path = result.run(n.id);
    if (path.remapSuspended) throw Error('Rebind suspended path edits first');
    return {
      supported: true,
      layerId,
      nodeId: n.id,
      route: [...seen],
      matrix,
      path: transformDirectPath(path, matrix),
      locked: routeLocked,
    };
  } catch (error) {
    return { supported: false, reason: error.message };
  }
}
export function transformDirectPath(path, matrix) {
  const linear = [...matrix.slice(0, 4), 0, 0];
  return {
    ...path,
    contours: path.contours.map((c) => ({
      ...c,
      points: c.points.map((p) => ({
        ...p,
        position: transformCharacterPoint(matrix, p.position),
        handleA: transformCharacterPoint(linear, p.handleA),
        handleB: transformCharacterPoint(linear, p.handleB),
      })),
    })),
  };
}
