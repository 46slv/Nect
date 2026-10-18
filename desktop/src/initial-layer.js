import { layerIsVisible } from './layer-visibility.js';

// Opening a document must not silently land on a hidden, locked source graph.
// This is editor selection only; authored order/visibility/locks stay intact.
export function initialLayerId(doc) {
  const editable = doc.layers.filter((l) => layerIsVisible(doc, l) && !l.locked && !l.guide);
  return (
    editable.find((l) => l.nodes.find((n) => n.id === l.output)?.params.path.source) ||
    editable[0] ||
    doc.layers.find((l) => layerIsVisible(doc, l)) ||
    doc.layers[0]
  ).id;
}
