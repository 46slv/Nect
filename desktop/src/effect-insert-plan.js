import { ARTWORK_TYPES } from './artwork-creation.js';
import { resolveArtworkMoveTarget } from './artwork-move-target.js';
import {
  hasExternalArtworkMoveConsumer,
  outputType,
  parameter,
  socketCompatibleFor,
} from './core.js';

// Entry identity is independent of both the visible name and the node type.
export const TRANSFORM_EFFECT_ID = 'builtin.transform.v1';

const unsupported = (code, reason) => ({ supported: false, code, reason });

// Read-only planning for the first Effects entry. A caller must capture and
// recheck its Session/project envelope and this exact edge before committing.
export function planTransformEffect(doc, layerId, options = {}) {
  const move = resolveArtworkMoveTarget(doc, layerId, options);
  if (!move.supported) return move;
  const route = move.target.route,
    layer = doc.layers.find((item) => item.id === layerId);
  if (
    !layer ||
    route.length !== 3 ||
    !ARTWORK_TYPES.includes(route[0].type) ||
    route[1].type !== 'Fill' ||
    route[2].type !== 'Output' ||
    move.target.nodeId !== route[0].nodeId ||
    move.target.outputNodeId !== layer.output
  )
    return unsupported('unsupported-route', 'Apply Transform to a simple filled artwork.');

  const byId = new Map(layer.nodes.map((item) => [item.id, item])),
    source = byId.get(route[0].nodeId),
    fill = byId.get(route[1].nodeId),
    output = byId.get(route[2].nodeId),
    fillPath = parameter(fill, 'path'),
    outputPath = parameter(output, 'path');
  if (
    !source ||
    !fillPath ||
    !outputPath ||
    fillPath.locked !== false ||
    fillPath.type !== 'PaintedPath' ||
    outputPath.type !== 'PaintedPath' ||
    fillPath.source !== source.id ||
    outputPath.source !== fill.id ||
    fillPath.sourcePort !== undefined ||
    outputPath.sourcePort !== undefined ||
    outputType(source) !== 'Path' ||
    !socketCompatibleFor(source, fill, 'path') ||
    !socketCompatibleFor(fill, output, 'path')
  )
    return unsupported('unavailable-edge', 'The artwork path connection cannot be changed.');

  const allowed = [
    { source: source.id, nodeId: fill.id, key: 'path' },
    { source: fill.id, nodeId: output.id, key: 'path' },
  ];
  if (
    hasExternalArtworkMoveConsumer(
      doc,
      layerId,
      route.map((item) => item.nodeId),
      allowed,
    )
  )
    return unsupported('shared-route', 'The artwork path is used elsewhere.');

  return {
    supported: true,
    target: {
      effectId: TRANSFORM_EFFECT_ID,
      layerId,
      outputNodeId: output.id,
      consumerNodeId: fill.id,
      inputKey: 'path',
      producerNodeId: source.id,
      producerPort: null,
    },
  };
}

export function sameTransformEffectTarget(a, b) {
  const keys = [
    'effectId',
    'layerId',
    'outputNodeId',
    'consumerNodeId',
    'inputKey',
    'producerNodeId',
    'producerPort',
  ];
  return (
    !!a &&
    !!b &&
    keys.every((key) => Object.hasOwn(a, key) && Object.hasOwn(b, key) && a[key] === b[key]) &&
    a.effectId === TRANSFORM_EFFECT_ID &&
    a.inputKey === 'path' &&
    a.producerPort === null
  );
}
