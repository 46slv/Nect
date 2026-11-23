import {
  planLayerFolderMove,
  planLayerFolderOrder,
  planLayerFolderRemoval,
} from './layer-folders.js';
import { editorTargetMatches } from './live-editor-target.js';

// Share the GUI's planner, not its capability to commit after a confirmation.
export function readLiveLayerFolderPlan({ readTarget, readDocument }, { target, request }) {
  const matches = () => {
    const current = readTarget();
    return current && editorTargetMatches(current, target);
  };
  if (!matches()) return { status: 'stale-target' };
  const doc = readDocument();
  if (!doc) return { status: 'unavailable' };
  let prepared;
  try {
    const plan =
      request.mode === 'order'
        ? planLayerFolderOrder
        : request.mode === 'move'
          ? planLayerFolderMove
          : planLayerFolderRemoval;
    prepared = plan(doc, request);
  } catch {
    return { status: matches() ? 'rejected' : 'stale-target' };
  }
  if (!matches()) return { status: 'stale-target' };
  const projected = {
    status: 'ok',
    target,
    before: doc.layers.map(({ id }) => id),
    after: prepared.paintOrder,
    orderChanged: prepared.orderChanged,
    membership: prepared.members,
  };
  if (new TextEncoder().encode(JSON.stringify(projected)).byteLength > 3500)
    return { status: 'plan-too-large' };
  return projected;
}
