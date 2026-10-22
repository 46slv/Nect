import { paintedGroup, isPaintedGroup, appearanceExportBlockers } from './composition-reference.js';
import { formatSvgNumber, formatSvgExtent } from './svg-number-format.js';

export const MASK_FEATHER_SCHEMA_VERSION = 50;
export const MAX_MASK_FEATHER = 256;
export function maskFeather(value) {
  if (!Number.isFinite(value) || value < 0 || value > MAX_MASK_FEATHER)
    throw Error(`Mask Feather sigma must be from 0 to ${MAX_MASK_FEATHER} document units`);
  return value;
}
export function featherMaskSVG(mask, id, shapeData, precision) {
  const esc = (value) =>
      String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('"', '&quot;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;'),
    filterId = esc(id + '-feather'),
    region = `x="${formatSvgNumber(mask.region[0], precision)}" y="${formatSvgNumber(mask.region[1], precision)}" width="${formatSvgExtent(mask.region[2], precision)}" height="${formatSvgExtent(mask.region[3], precision)}"`,
    inverse = mask.invert
      ? `<feComponentTransfer in="soft" result="coverage" ${region}><feFuncA type="linear" slope="-1" intercept="1"/></feComponentTransfer>`
      : '';
  return `<defs><filter id="${filterId}" filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse" ${region} color-interpolation-filters="sRGB"><feGaussianBlur in="SourceAlpha" stdDeviation="${formatSvgNumber(maskFeather(mask.feather))}" result="soft" ${region}/>${inverse}<feFlood flood-color="white" flood-opacity="1" result="white" ${region}/><feComposite in="white" in2="${mask.invert ? 'coverage' : 'soft'}" operator="in" ${region}/></filter><mask id="${esc(id)}" maskUnits="userSpaceOnUse" maskContentUnits="userSpaceOnUse" ${region} style="mask-type:luminance" color-interpolation="sRGB"><path d="${esc(shapeData)}" fill="white" fill-rule="nonzero" filter="url(#${filterId})"/></mask></defs>`;
}

function strokePadding(value, scale = 1) {
  if (isPaintedGroup(value)) {
    const [a, b, c, d] = value.matrix,
      next = scale * Math.hypot(a, b, c, d);
    return Math.max(0, ...value.children.map((child) => strokePadding(child, next)));
  }
  const stroke = value.stroke;
  return stroke
    ? ((Math.abs(stroke.width) * scale) / 2) *
        (stroke.join === 'miter' ? Math.max(1, stroke.miterLimit || 4) : 1)
    : 0;
}
export function maskAppearance(source, shape, { invert, prefix, bounds, feather = 0 }) {
  maskFeather(feather);
  if (!source || (!Array.isArray(source.contours) && source.kind !== 'image'))
    throw Error('Connect Image or Composition appearance to Mask');
  if (!Array.isArray(shape?.contours)) throw Error('Connect a closed Path shape to Mask');
  if (typeof invert !== 'boolean') throw Error('Mask invert must be Boolean');
  if (
    shape.contours.length > 256 ||
    shape.contours.reduce((n, c) => n + c.points.length, 0) > 10000
  )
    throw Error('Mask shape exceeds 256 contours or 10000 points');
  if (shape.contours.some((c) => !c.closed)) throw Error('Mask shape requires closed contours');
  const group = paintedGroup([source], { prefix }),
    pad = 2 + strokePadding(source) + 4 * feather,
    region = [
      bounds[0][0] - pad,
      bounds[1][0] - pad,
      Math.max(1, bounds[0][2] - bounds[0][0] + 2 * pad),
      Math.max(1, bounds[1][2] - bounds[1][0] + 2 * pad),
    ];
  if (!region.every((v) => Number.isFinite(v) && Math.abs(v) <= 1e9))
    throw Error('Mask paint bounds exceed the supported range');
  if (feather > 0 && (region[2] > 8192 || region[3] > 8192 || region[2] * region[3] > 16_000_000))
    throw Error(
      'Feathered mask region exceeds 8192 units per side or 16 million square document units',
    );
  group.mask = {
    shape: { contours: structuredClone(shape.contours) },
    invert,
    region,
    ...(feather > 0 ? { feather } : {}),
  };
  const blockers = appearanceExportBlockers([source, shape]);
  if (blockers.length) group.exportBlockers = blockers;
  return group;
}
