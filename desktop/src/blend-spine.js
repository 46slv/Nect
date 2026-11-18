import { toPaper } from './paper-runtime.js';

const vector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);
export function blendSpineOffsets(spine, a, b, count) {
  const contour = spine?.contours?.[0];
  if (
    !spine ||
    spine.kind ||
    spine.mask ||
    spine.contours?.length !== 1 ||
    contour?.closed !== false ||
    contour.group !== undefined ||
    !Array.isArray(contour.points) ||
    contour.points.length < 2 ||
    contour.points.length > 512 ||
    contour.points.some((p) => !['position', 'handleA', 'handleB'].every((key) => vector(p[key])))
  )
    throw Error('Spine Steps requires one open finite path with 2 to 512 anchors');
  const items = [];
  try {
    const item = toPaper(spine);
    items.push(item);
    const pa = toPaper({ contours: [a] });
    items.push(pa);
    const pb = toPaper({ contours: [b] });
    items.push(pb);
    const path = item.children[0],
      length = path.length;
    const ca = [pa.bounds.center.x, pa.bounds.center.y],
      cb = [pb.bounds.center.x, pb.bounds.center.y];
    if (!Number.isFinite(length) || length <= 0 || !vector(ca) || !vector(cb))
      throw Error('Spine Steps requires positive finite length and source bounds');
    return Array.from({ length: count }, (_, i) => {
      const t = i / (count - 1),
        sample =
          i === 0
            ? contour.points[0].position
            : i === count - 1
              ? contour.points.at(-1).position
              : (() => {
                  const p = path.getPointAt(length * t);
                  return p ? [p.x, p.y] : null;
                })();
      if (!vector(sample)) throw Error('Spine Steps cannot sample a finite position');
      const offset = sample.map((value, j) => value - ((1 - t) * ca[j] + t * cb[j]));
      if (!vector(offset)) throw Error('Spine Steps exceeds finite coordinates');
      return offset;
    });
  } finally {
    for (const item of items) item.remove();
  }
}

export function translateBlendContour(contour, offset) {
  const points = contour.points.map((point) => {
    const position = point.position.map((value, j) => value + offset[j]);
    if (!vector(position)) throw Error('Spine Steps exceeds finite coordinates');
    return { ...point, position };
  });
  return { ...contour, points };
}
