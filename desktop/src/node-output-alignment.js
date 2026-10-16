import { evaluate, outputType, pathBounds } from './core.js';
import { artworkAlignments } from './artwork-alignment.js';
import { resolveArtworkMoveTarget, CURVE_ARTWORK_MOVE_POINT_LIMIT } from './artwork-move-target.js';

const positions = {
  Vector: 'value',
  Null: 'position',
  Text: 'position',
  Image: 'position',
  Circle: 'center',
  Ellipse: 'center',
  Rectangle: 'center',
  Polygon: 'center',
  Star: 'center',
  Transform: 'offset',
  'Vector Transform': 'offset',
};
const union = (members) =>
  [0, 1].map((axis) => {
    const low = Math.min(...members.map((m) => m.bounds[axis][0]));
    const high = Math.max(...members.map((m) => m.bounds[axis][2]));
    return [low, low / 2 + high / 2, high];
  });

export function resolveNodeOutputTargets(doc, layerId, nodeIds, options = {}) {
  try {
    const layer = doc.layers.find((l) => l.id === layerId);
    if (!layer || layer.locked || !layer.visible || layer.guide)
      throw Error('Select an unlocked visible composition.');
    if (
      !Array.isArray(nodeIds) ||
      !nodeIds.length ||
      nodeIds.length > 100 ||
      new Set(nodeIds).size !== nodeIds.length
    )
      throw Error('Select 1–100 distinct node outputs.');
    const selected = new Set(nodeIds),
      byId = new Map(layer.nodes.map((n) => [n.id, n]));
    const evaluated = evaluate(layer, options.fonts || {}, doc.imageAssets);
    const final = resolveArtworkMoveTarget(doc, layerId, options);
    const finalRoute = final.supported
      ? new Set(final.target.route.map((n) => n.nodeId))
      : new Set();
    const logical = [];
    for (const id of nodeIds) {
      let n = byId.get(id);
      if (!n) throw Error('A selected node is missing.');
      const fail = (reason) => {
        throw Error(`${n.name}: ${reason}`);
      };
      if (finalRoute.has(id)) {
        logical.push({
          outputId: layer.output,
          route: [...finalRoute],
          bounds: final.bounds,
          key: final.target.nodeId + ':' + final.target.parameter,
          target: { kind: 'artwork-translate', layerIds: [layerId] },
        });
        continue;
      }
      const route = new Set([id]);
      const outputId = id;
      while (['Fill', 'Output', 'Image Output'].includes(n.type)) {
        if (n.bypassed || !n.params.path.source || route.has(n.params.path.source))
          fail('Connect a supported editable output.');
        n = byId.get(n.params.path.source);
        if (!n) throw Error('Missing output source.');
        route.add(n.id);
      }
      if (n.bypassed || n.pathAttachment) fail('This output cannot be translated safely.');
      const key = ['Transform', 'Vector Transform'].includes(n.type) && n.params.position ? 'position' : positions[n.type];
      if (!key && n.type !== 'Curve') fail('This output has no supported position control.');
      for (const [name, q] of Object.entries(n.params)) {
        if (name !== key && name !== 'path' && (q.source || q.expression))
          fail('A driven shape setting prevents a position-only alignment.');
      }
      let owner = n;
      if (key) {
        let q = n.params[key];
        const seen = new Set([n.id]);
        while (q?.source) {
          const source = byId.get(q.source);
          if (
            !source ||
            !['Vector', 'Null'].includes(source.type) ||
            outputType(source, q.sourcePort) !== 'Vector2' ||
            seen.has(source.id)
          )
            fail('Select an editable Vector/Null position source.');
          if (!selected.has(source.id))
            fail('Select the driving Vector/Null together with its output.');
          route.add(source.id);
          seen.add(source.id);
          owner = source;
          q = owner.params[positions[owner.type]];
        }
        if (!q || q.locked || q.expression || q.type !== 'Vector2')
          fail('The position is locked or driven.');
      } else {
        if (n.points?.length > CURVE_ARTWORK_MOVE_POINT_LIMIT)
          fail(
            `Output alignment supports up to ${CURVE_ARTWORK_MOVE_POINT_LIMIT} Curve anchors; use graph source editing for larger paths.`,
          );
        if (
          !n.points?.length ||
          n.points.some(
            (p) =>
              [p.position, p.handleA, p.handleB].some((q) => q.source || q.expression) ||
              p.position.locked,
          )
        )
          fail('Curve positions and relative handles must be editable literals.');
      }
      const value = evaluated.run(outputId);
      const bounds =
        Array.isArray(value) && value.length === 2 && value.every(Number.isFinite)
          ? value.map((v) => [v, v, v])
          : value?.kind === 'image'
            ? value.position.map((v, i) => [v, v + value.size[i] / 2, v + value.size[i]])
            : value?.contours?.some((c) => c.points.length)
              ? pathBounds(value)
              : null;
      if (!bounds || bounds.flat().some((v) => !Number.isFinite(v)))
        fail('The selected output has no finite geometry or Vector2 position.');
      logical.push({
        outputId,
        route: [...route],
        bounds,
        key: owner.id + ':' + ((['Transform', 'Vector Transform'].includes(owner.type) && owner.params.position ? 'position' : positions[owner.type]) || 'points'),
        target: { kind: 'nodes-translate', layerId, nodeIds: [owner.id] },
      });
    }
    const unique = [...new Map(logical.map((member) => [member.outputId, member])).values()];
    // A selected downstream output represents its selected upstream source once.
    const leaves = unique.filter(
      (m) => !unique.some((other) => other !== m && other.route.includes(m.outputId)),
    );
    const groups = new Map();
    for (const member of leaves) {
      if (!groups.has(member.key)) groups.set(member.key, []);
      groups.get(member.key).push(member);
    }
    const members = [...groups.values()].map((group) => ({ ...group[0], bounds: union(group) }));
    return { supported: true, members, bounds: union(members) };
  } catch (error) {
    return { supported: false, reason: `${error.message} No selected output moved.` };
  }
}

export function planNodeOutputAlignment(doc, layerId, nodeIds, action, options = {}) {
  const alignment = artworkAlignments.find(([id]) => id === action);
  if (!alignment) throw Error('Choose one of the six alignments.');
  if (!['artboard', 'selection'].includes(options.reference))
    throw Error('Choose Artboard or Selection.');
  const resolved = resolveNodeOutputTargets(doc, layerId, nodeIds, options);
  if (!resolved.supported) throw Error(resolved.reason);
  if (resolved.members.length < (options.reference === 'artboard' ? 1 : 2))
    throw Error('Select at least two independent outputs for Selection alignment.');
  const [, , axis, edge] = alignment;
  const reference =
    options.reference === 'artboard'
      ? [0, (axis ? doc.height : doc.width) / 2, axis ? doc.height : doc.width]
      : resolved.bounds[axis];
  return resolved.members.map((member) => {
    const delta = [0, 0],
      distance = reference[edge] - member.bounds[axis][edge];
    delta[axis] = Math.abs(distance) < 1.0001e-5 ? 0 : distance;
    return { target: member.target, delta };
  });
}
