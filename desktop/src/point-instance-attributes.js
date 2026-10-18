export function pointInstanceTransforms(targets) {
  const points = targets?.contours?.flatMap((contour) => contour.points) || [];
  if (!points.some((point) => point && Object.hasOwn(point, 'instanceTransform'))) return undefined;
  return points.map((point) => {
    if (!Object.hasOwn(point, 'instanceTransform')) return { scale: 1, rotation: 0 };
    const transform = point.instanceTransform;
    if (
      !transform ||
      typeof transform !== 'object' ||
      Array.isArray(transform) ||
      Object.keys(transform).length !== 2 ||
      !Number.isFinite(transform.scale) ||
      transform.scale < 0 ||
      transform.scale > 100 ||
      !Number.isFinite(transform.rotation) ||
      Math.abs(transform.rotation) > 360
    )
      throw Error(
        'Instance on Points needs valid per-point scale (0–100) and rotation (±360 degrees)',
      );
    return { scale: transform.scale, rotation: transform.rotation };
  });
}
