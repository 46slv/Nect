// Solo is an authored top-level preview/export filter. Hidden layers stay hidden;
// source Composition evaluation deliberately ignores this display-only filter.
export const hasLayerSolo = (doc) => doc.layers.some((l) => l.solo === true);
export function layerIsVisible(doc, layer) {
  return layer.visible && (!hasLayerSolo(doc) || layer.solo === true);
}
