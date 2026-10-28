// Experimental bridge to the production SVG renderer. Not imported by the UI.
// Provenance is an explicit caller contract; equal display colors never imply IDs.
import { renderElement } from './core.js';

export function colorCoveragePasses(scene, { width, height }, bindings = new Map()) {
  if (!Array.isArray(scene) || scene.length > 256 || !(bindings instanceof Map))
    throw Error('Expected a bounded evaluated scene and explicit provenance map');
  if (![width, height].every((n) => Number.isFinite(n) && n > 0 && n <= 100000))
    throw Error('Invalid coverage artboard bounds');
  const paints = [];
  // Preflight the whole scene before emitting anything. Unsupported attribution
  // must not silently become a percentage for only the supported objects.
  for (const entry of scene) {
    const { layer, value } = entry ?? {};
    if (
      !layer ||
      !value ||
      !Array.isArray(value.contours) ||
      value.children ||
      value.mask ||
      value.kind === 'image' ||
      value.fill?.kind ||
      value.contours.some(
        (c) =>
          !c ||
          c.group !== undefined ||
          c.repeaterOpacity !== undefined ||
          c.repeaterFill !== undefined ||
          c.repeaterStroke !== undefined,
      )
    )
      throw Error('Coverage attribution currently requires plain solid paths');
    const provenance = entry.paintProvenance ?? bindings.get(layer.id) ?? {};
    for (const paint of ['fill', 'stroke']) {
      if (!value[paint]) continue;
      const slotId = provenance[paint] ?? null;
      if (slotId !== null && (typeof slotId !== 'string' || !/^[A-Za-z0-9_-]{1,128}$/.test(slotId)))
        throw Error('Invalid explicit Color provenance');
      paints.push({ layer, value, paint, slotId });
      if (paints.length > 256) throw Error('Coverage paint pass budget exceeded');
    }
  }
  return paints.map(({ layer, value, paint, slotId }) => {
    const isolated = { ...value, [paint === 'fill' ? 'stroke' : 'fill']: undefined };
    const body = renderElement(isolated, layer);
    return {
      layerId: layer.id,
      paint,
      slotId,
      svg: `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" overflow="hidden">${body}</svg>`,
    };
  });
}
