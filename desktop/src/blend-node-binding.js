import {
  blendSpineOffsets,
  blendSpineDistanceOffsets,
  translateBlendContour,
  blendSpineOrientations,
  orientBlendContour,
} from './blend-spine.js';
import {
  blendPointCount,
  translatedContourDistance,
  prepareClosedContourBlend,
  copyClosedContourEndpoint,
} from './blend-geometry.js';
import { colorOperation, premultipliedColorDifference, publicColor } from './color-operations.js';

// Mixing a fully transparent color erases its RGB. Endpoints retain authored paint.
const blendColor = (a, b, t) =>
  t === 0 ? { ...a } : t === 1 ? { ...b } : publicColor(colorOperation('mix', [a, b, t]));

// First vertical: one closed contour with matching solid fill/stroke presence. Complex painted
// groups and incompatible strokes must not silently lose their appearance.
function evaluateThreeSourceSteps(read, context) {
  const steps = read('steps');
  if (!Number.isInteger(steps) || steps < 0 || steps > 126)
    throw Error('Three-source Steps requires an integer from 0 to 126 (256-shape limit)');
  const inputs = ['a', 'b', 'c'].map((key) => read(key));
  if (inputs.some((value) => !value)) throw Error('Connect all three Blend inputs A, B and C');
  const counts = inputs.map((value) => value.contours?.[0]?.points?.length);
  if (counts.some((count) => !Number.isInteger(count) || count < 2 || count > 512))
    throw Error('Three-source Steps requires bounded contour point counts');
  const total =
    counts.reduce((sum, count) => sum + count, 0) +
    (steps
      ? steps * (blendPointCount(counts[0], counts[1]) + blendPointCount(counts[1], counts[2]))
      : 0);
  if (total > 100000) throw Error('Blend exceeds combined geometry budget');
  // Evaluate both spans privately. The shared B endpoint is retained from A/B once.
  const spans = [0, 1].map((i) =>
    evaluateBlend('steps', (key) => ({ a: inputs[i], b: inputs[i + 1], steps })[key], {
      ...context,
      nodeId: `${context.nodeId}:span:${i}`,
      groupAppearance: (values) => values,
    }),
  );
  const values = [...spans[0], ...spans[1].slice(1)];
  const blockers = [...new Set(inputs.flatMap((value) => value.exportBlockers || []))];
  if (blockers.length) for (const value of values) value.exportBlockers = [...blockers];
  return context.groupAppearance(values);
}

export function evaluateBlend(_operation, read, context) {
  if (_operation === 'three-source-steps') return evaluateThreeSourceSteps(read, context);
  const a = read('a'),
    b = read('b');
  const spineDistanceMode = _operation === 'spine-distance';
  const smoothColorMode = _operation === 'smooth-color';
  let steps = spineDistanceMode || smoothColorMode ? null : read('steps');
  if (
    ![
      '',
      'subtract',
      'union',
      'intersect',
      'difference',
      'steps',
      'smooth-color',
      'distance',
      'spine-steps',
      'spine-distance',
      'spine-tangent-steps',
      undefined,
    ].includes(_operation)
  )
    throw Error('Unknown Blend spacing');
  if (!a || !b) throw Error('Connect both Blend inputs');
  if (_operation === 'distance') {
    const distance = read('distance');
    if (!Number.isFinite(distance) || distance <= 0)
      throw Error('Blend Distance must be positive and finite');
    const length = translatedContourDistance(a.contours?.[0], b.contours?.[0]);
    const intervals = Math.max(1, Math.ceil(length / distance));
    if (intervals > 255) throw Error('Blend Distance exceeds 255 intervals');
    steps = intervals - 1;
  }
  if (
    !spineDistanceMode &&
    !smoothColorMode &&
    (!Number.isInteger(steps) || steps < 0 || steps > 254)
  )
    throw Error('Blend steps must be an integer from 0 to 254');
  for (const value of [a, b]) {
    if (value.kind || value.mask || value.contours?.length !== 1)
      throw Error('Blend currently supports single closed contours without mask');
    if (
      [value, value.contours[0]].some((item) =>
        Object.keys(item).some((key) => key.startsWith('repeater')),
      ) ||
      value.characterOccurrences?.length
    )
      throw Error('Blend does not support per-copy appearance');
    if (value.fill != null) publicColor(colorOperation('mix', [value.fill, value.fill, 0]));
    if (value.fill == null && !value.stroke) throw Error('Blend requires visible paint slots');
  }
  if ((a.fill != null) !== (b.fill != null)) throw Error('Blend requires matching fill presence');
  if (!!a.stroke !== !!b.stroke) throw Error('Blend requires matching stroke presence');
  let strokeA, strokeB, strokeStyle;
  if (a.stroke) {
    const { hex: ah, alpha: aa, width: aw, ...as } = a.stroke;
    const { hex: bh, alpha: ba, width: bw, ...bs } = b.stroke;
    if (
      !Number.isFinite(aw) ||
      aw < 0 ||
      aw > 100000 ||
      !Number.isFinite(bw) ||
      bw < 0 ||
      bw > 100000 ||
      [as, bs].some(
        (style) =>
          (style.kind !== undefined && style.kind !== 'solid') ||
          (style.dash && (!Array.isArray(style.dash) || style.dash.length)),
      ) ||
      JSON.stringify(
        Object.keys(as)
          .sort()
          .map((key) => [key, as[key]]),
      ) !==
        JSON.stringify(
          Object.keys(bs)
            .sort()
            .map((key) => [key, bs[key]]),
        )
    )
      throw Error('Blend requires compatible solid strokes');
    strokeA = { hex: ah, alpha: aa };
    strokeB = { hex: bh, alpha: ba };
    publicColor(colorOperation('mix', [strokeA, strokeB, 0]));
    strokeStyle = as;
  }
  if (smoothColorMode) {
    const difference = Math.max(
      a.fill == null ? 0 : premultipliedColorDifference(a.fill, b.fill),
      a.stroke ? premultipliedColorDifference(strokeA, strokeB) : 0,
    );
    steps = Math.max(1, Math.ceil(32 * difference)) - 1;
  }
  const checkBudget = (intermediates) => {
    if (
      intermediates > 0 &&
      (intermediates + 2) *
        blendPointCount(a.contours[0].points.length, b.contours[0].points.length) >
        100000
    )
      throw Error('Blend exceeds geometry budget');
  };
  const tangentMode = _operation === 'spine-tangent-steps';
  const spine =
    _operation === 'spine-steps' || tangentMode || spineDistanceMode ? read('spine') : null;
  const distanceOffsets = spineDistanceMode
    ? blendSpineDistanceOffsets(spine, a.contours[0], b.contours[0], read('distance'), checkBudget)
    : null;
  if (distanceOffsets) steps = distanceOffsets.length - 2;
  else checkBudget(steps);
  const blockers = [...new Set([...(a.exportBlockers || []), ...(b.exportBlockers || [])])];
  // No intermediate objects means no correspondence or LCM subdivision is needed.
  const interpolate =
    steps === 0
      ? (t, id) => copyClosedContourEndpoint((t === 0 ? a : b).contours[0], id)
      : prepareClosedContourBlend(a.contours[0], b.contours[0]);
  const orientations = tangentMode
    ? blendSpineOrientations(spine, a.contours[0], b.contours[0], steps + 2)
    : null;
  const offsets =
    distanceOffsets ??
    (_operation === 'spine-steps'
      ? blendSpineOffsets(spine, a.contours[0], b.contours[0], steps + 2)
      : null);
  if (spine?.exportBlockers)
    blockers.push(...spine.exportBlockers.filter((x) => !blockers.includes(x)));
  const values = [];
  for (let i = 0; i < steps + 2; i++) {
    const t = i / (steps + 1);
    const contour = interpolate(t, `${context.nodeId}:blend:${i}`);
    values.push({
      contours: [
        orientations
          ? orientBlendContour(contour, orientations[i])
          : offsets
            ? translateBlendContour(
                contour,
                offsets[i],
                spineDistanceMode ? 'Spine Distance' : undefined,
              )
            : contour,
      ],
      fill: a.fill == null ? null : blendColor(a.fill, b.fill, t),
      ...(a.stroke
        ? {
            stroke: {
              ...blendColor(strokeA, strokeB, t),
              width:
                t === 0
                  ? a.stroke.width
                  : t === 1
                    ? b.stroke.width
                    : a.stroke.width + (b.stroke.width - a.stroke.width) * t,
              ...strokeStyle,
            },
          }
        : {}),
      ...(blockers.length ? { exportBlockers: blockers } : {}),
    });
  }
  return context.groupAppearance(values);
}
