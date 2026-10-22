import { evaluate, parameter } from './core.js';
import { imageAssetFor } from './image-keyer.js';
import { copyImageRgba } from './image-luminance-field.js';

function resolveInput(doc, layerId, nodeId, options) {
  const layer = doc.layers.find((l) => l.id === layerId),
    node = layer?.nodes.find((n) => n.id === nodeId);
  if (node?.type !== 'Keyer') throw Error('Select one Keyer');
  const color = parameter(node, 'keyColor');
  if (layer.locked || node.locked || color.locked) throw Error('Key color is locked');
  if (!node.params.path.source) throw Error('Connect an Image to Keyer');
  const input = evaluate(layer, options.fonts || {}, doc.imageAssets, doc).run(
    node.params.path.source,
    node.params.path.sourcePort,
  );
  if (input?.kind !== 'image') throw Error('Keyer picking requires an Image input');
  const asset = imageAssetFor(input, doc.imageAssets),
    original = doc.imageAssets[input.asset];
  return {
    node,
    inputNode: layer.nodes.find((n) => n.id === node.params.path.source),
    color,
    input,
    asset,
    original,
    identity: JSON.stringify([
      layerId,
      nodeId,
      node.params.path.source,
      node.params.path.sourcePort || null,
      input.asset,
      input.imageEffects || [],
      input.position,
      input.size,
      color.value,
      color.source,
      color.expression,
    ]),
  };
}
export function keyerInputPickContext(doc, layerId, nodeId, options = {}) {
  const resolved = resolveInput(doc, layerId, nodeId, options);
  return {
    ...resolved,
    asset: { ...resolved.asset },
    original: { ...resolved.original },
    layerId,
    nodeId,
    rgba: copyImageRgba(resolved.asset),
  };
}
export function sampleKeyerInput(context, point) {
  const { width, height } = context.asset;
  if (!Array.isArray(point) || point.length !== 2 || !point.every(Number.isFinite))
    throw Error('Cannot map this preview position');
  if (point[0] < 0 || point[1] < 0 || point[0] >= width || point[1] >= height)
    return { state: 'outside' };
  const x = Math.floor(point[0]),
    y = Math.floor(point[1]),
    offset = (y * width + x) * 4;
  if (context.rgba[offset + 3] === 0) return { state: 'transparent', x, y };
  const hex =
    '#' + [0, 1, 2].map((i) => context.rgba[offset + i].toString(16).padStart(2, '0')).join('');
  return { state: 'color', x, y, hex };
}
export function planKeyerInputPick(doc, context, point, options = {}) {
  const current = resolveInput(doc, context.layerId, context.nodeId, options);
  if (
    current.identity !== context.identity ||
    current.original.data !== context.original.data ||
    current.original.width !== context.original.width ||
    current.original.height !== context.original.height ||
    current.original.mime !== context.original.mime
  )
    throw Error('Keyer input changed; pick again');
  const sample = sampleKeyerInput(context, point);
  if (sample.state !== 'color') return sample;
  return {
    ...sample,
    type: current.color.source || current.color.expression ? 'set-parameter-base' : 'set-parameter',
    payload: {
      layerId: context.layerId,
      nodeId: context.nodeId,
      key: 'keyColor',
      value: { ...current.color.value, hex: sample.hex },
    },
  };
}
