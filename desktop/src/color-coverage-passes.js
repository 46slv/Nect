// Bounded bridge to the production SVG renderer, used by the shared-color panel.
// Provenance is an explicit caller contract; equal display colors never imply IDs.
import { renderElement } from './core.js';
import { renderLinearGradientFill } from './linear-gradient.js';
import { renderRadialGradientFill } from './radial-gradient.js';

const angularStops = ['startColor', 'color1', 'color2', 'endColor'];
const gradientRenderers = {
  'linear-gradient': renderLinearGradientFill,
  'radial-gradient': renderRadialGradientFill,
};

function slot(id) {
  if (id !== null && (typeof id !== 'string' || !/^[A-Za-z0-9_-]{1,128}$/.test(id)))
    throw Error('Invalid explicit Color provenance');
  return id;
}

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
      (value.fill?.kind &&
        value.fill.kind !== 'angular-gradient' &&
        !Object.hasOwn(gradientRenderers, value.fill.kind)) ||
      value.stroke?.kind ||
      ['filter', 'filters', 'blendMode', 'opacity', 'matrix'].some((key) =>
        Object.hasOwn(value, key),
      ) ||
      value.contours.some(
        (c) =>
          !c ||
          c.group !== undefined ||
          c.repeaterOpacity !== undefined ||
          c.repeaterFill !== undefined ||
          c.repeaterStroke !== undefined,
      )
    )
      throw Error(
        'Coverage attribution requires plain solid, two-stop linear/radial-gradient or four-stop angular-gradient paths',
      );
    const provenance = entry.paintProvenance ?? bindings.get(layer.id) ?? {};
    for (const paint of ['fill', 'stroke']) {
      if (!value[paint]) continue;
      const identity = provenance[paint];
      const angular = paint === 'fill' && value.fill.kind === 'angular-gradient';
      const split =
        paint === 'fill' && (angular || Object.hasOwn(gradientRenderers, value.fill.kind));
      if (split && identity != null && (typeof identity !== 'object' || Array.isArray(identity)))
        throw Error('Gradient coverage needs explicit stop provenance');
      let slotId = split ? null : slot(identity ?? null);
      let stopIds = split
        ? (angular ? angularStops : ['startColor', 'endColor']).map((key) =>
            slot(identity?.[key] ?? null),
          )
        : null;
      // Identical explicit IDs own the entire paint exactly, including Other.
      // Validate every stop above before collapsing; equal HEX is irrelevant.
      if (angular && stopIds.every((id) => id === stopIds[0])) {
        slotId = stopIds[0];
        stopIds = null;
      }
      paints.push({ layer, value, paint, slotId, stopIds, angular });
      if (paints.length > 256) throw Error('Coverage paint pass budget exceeded');
    }
  }
  let svgBytes = 0;
  return paints.map(({ layer, value, paint, slotId, stopIds, angular }) => {
    const isolated = { ...value, [paint === 'fill' ? 'stroke' : 'fill']: undefined };
    const svg = (path, attributes = '', definitions = '') => {
      const result = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" overflow="hidden">${definitions}${renderElement(path, layer, {}, attributes)}</svg>`;
      svgBytes += result.length;
      if (result.length > 4 * 1024 * 1024 || svgBytes > 16 * 1024 * 1024)
        throw Error('Coverage SVG budget exceeded');
      return result;
    };
    // Angular interpolation is premultiplied: isolate each alpha_i * weight_i
    // directly, not A(t) * weight_i as for SVG linear/radial gradients. Keep
    // ORIGINAL per-wedge alpha so quantized overlapping wedges retain identical
    // intra-pattern source-over occlusion. A white/black stop basis encodes the
    // fraction in RGB; the decoder recovers premultiplied red, not alpha. Reusing
    // the production renderer preserves its 720 midpoint wedges, pattern bounds,
    // direction, seam and crispEdges. No authored geometry or paint is changed.
    const contributions =
      angular && stopIds
        ? angularStops.map((key, index) => ({
            slotId: stopIds[index],
            channel: 'premultiplied-red',
            svg: svg({
              ...isolated,
              fill: {
                ...isolated.fill,
                ...Object.fromEntries(
                  angularStops.map((stop) => [
                    stop,
                    {
                      hex: stop === key ? '#ffffff' : '#000000',
                      alpha: isolated.fill[stop].alpha,
                    },
                  ]),
                ),
              },
            }),
          }))
        : undefined;
    let split;
    if (stopIds && !angular) {
      // SVG interpolates sRGB colors without premultiplication. Allocate the
      // actual paint alpha A(t) by color interpolation weights: A(t)*(1-t),
      // A(t)*t, NOT startAlpha*(1-t) and endAlpha*t. A transparent stop can
      // still supply color to mixed pixels. This private scalar mask has the
      // same artboard ramp/pad semantics (linear projection or concentric
      // circular distance); authored scene masks remain refused.
      const weight = gradientRenderers[isolated.fill.kind](
        {
          ...isolated.fill,
          startColor: { hex: '#ffffff', alpha: 1 },
          endColor: { hex: '#ffffff', alpha: 0 },
        },
        'coverage-start-weight',
      );
      const definitions = `<defs>${weight.definition}<mask id="coverage-start-mask" maskUnits="userSpaceOnUse" maskContentUnits="userSpaceOnUse" x="0" y="0" width="${width}" height="${height}" color-interpolation="sRGB"><rect width="${width}" height="${height}" fill="${weight.paint}"/></mask></defs>`;
      split = {
        startSlotId: stopIds[0],
        endSlotId: stopIds[1],
        startSvg: svg(isolated, 'mask="url(#coverage-start-mask)"', definitions),
      };
    }
    return {
      layerId: layer.id,
      paint,
      slotId,
      svg: svg(isolated),
      ...(split ? { split } : {}),
      ...(contributions ? { contributions } : {}),
    };
  });
}
