import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';

const validIndex = (value) => Number.isSafeInteger(value) && value >= 0;
const validId = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;
export const LIVE_OBJECT_PAGE_SIZE = 32;

// A target-bound view of authored objects, not evaluated geometry or the project file.
// Layer indexes only have meaning for the captured target and are not durable IDs.
export function readLiveEditorObjects(
  { readTarget, readDocument },
  { target, layerIndex, offset },
) {
  if ((layerIndex !== null && !validIndex(layerIndex)) || !validIndex(offset))
    return { status: 'invalid-arguments' };
  const current = captureEditorTarget(readTarget());
  if (!editorTargetMatches(current, target)) return { status: 'stale-target' };

  const doc = readDocument();
  if (!Array.isArray(doc?.layers)) return { status: 'unavailable' };
  const layers = doc.layers;
  const layer = layerIndex === null ? null : layers[layerIndex];
  if (layerIndex !== null && !layer) return { status: 'unsupported-target' };
  const source = layer ? layer.nodes : layers;
  if (!Array.isArray(source) || (layer && !validId(layer.id))) return { status: 'unavailable' };
  if (offset > source.length) return { status: 'invalid-arguments' };

  const end = Math.min(source.length, offset + LIVE_OBJECT_PAGE_SIZE);
  const items = [];
  for (let index = offset; index < end; index++) {
    const entry = source[index];
    if (!validId(entry?.id) || (layer && !validId(entry.type))) return { status: 'unavailable' };
    items.push(
      layer
        ? { nodeId: entry.id, type: entry.type }
        : { layerId: entry.id, layerIndex: index, locked: entry.locked === true },
    );
  }
  if (!editorTargetMatches(readTarget(), current)) return { status: 'stale-target' };
  return {
    status: 'ok',
    target: current,
    layerId: layer?.id ?? null,
    items,
    nextOffset: end < source.length ? end : null,
  };
}
