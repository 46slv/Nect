import { toPaper } from './paper-runtime.js';
import { isVector2 } from './node-value-validation.js';

function assertValue(type, value) {
  if (type === 'Vector2' && !isVector2(value)) throw Error('Expected Vector2');
}
export function subdividePathGeometry(source, { segments, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isInteger(segments) || segments < 1 || segments > 32)
    throw Error('Subdivide segments must be an integer from 1 to 32');
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Subdivide Path needs a nonempty ID prefix');
  if (source.contours.length > 10000) throw Error('Subdivide Path exceeds geometry budget');

  let inputPoints = 0,
    outputPoints = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points) || typeof contour.closed !== 'boolean')
      throw Error('Subdivide Path needs valid contours');
    inputPoints += contour.points.length;
    if (inputPoints > 100000) throw Error('Subdivide Path exceeds geometry budget');
    const count = contour.points.length;
    outputPoints +=
      count === 0 ? 0 : contour.closed ? count * segments : 1 + (count - 1) * segments;
    if (outputPoints > 100000) throw Error('Subdivide Path exceeds geometry budget');
    for (const point of contour.points) {
      if (
        !point ||
        !Array.isArray(point.position) ||
        !Array.isArray(point.handleA) ||
        !Array.isArray(point.handleB)
      )
        throw Error('Subdivide Path needs valid points');
      assertValue('Vector2', point.position);
      assertValue('Vector2', point.handleA);
      assertValue('Vector2', point.handleB);
    }
  }

  const copy = () => ({
    ...source,
    contours: source.contours.map((contour, contourIndex) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      points: contour.points.map((point, pointIndex) => ({
        ...point,
        id: `${prefix}:c${contourIndex}:p${pointIndex}`,
        position: [...point.position],
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      })),
    })),
  });
  if (segments === 1) return copy();

  const item = toPaper(source);
  try {
    const contours = source.contours.map((contour, contourIndex) => {
      const sourceCount = contour.points.length,
        path = item.children[contourIndex],
        edgeCount = contour.closed ? sourceCount : Math.max(0, sourceCount - 1);
      if (edgeCount === 0) {
        const points = contour.points.map((point, pointIndex) => ({
          ...point,
          id: `${prefix}:c${contourIndex}:p${pointIndex}`,
          position: [...point.position],
          handleA: [...point.handleA],
          handleB: [...point.handleB],
        }));
        return { ...contour, id: `${prefix}:c${contourIndex}`, points };
      }
      if (path.curves.length !== edgeCount)
        throw Error('Subdivide Path could not resolve curve edges');
      const points = [];
      for (let edgeIndex = 0; edgeIndex < edgeCount; edgeIndex++) {
        const curve = path.curves[edgeIndex],
          sourceStart = contour.points[edgeIndex],
          sourceEnd = contour.points[(edgeIndex + 1) % sourceCount],
          keepLineHandles =
            sourceStart.handleB[0] === 0 &&
            sourceStart.handleB[1] === 0 &&
            sourceEnd.handleA[0] === 0 &&
            sourceEnd.handleA[1] === 0;
        for (let partIndex = 0; partIndex < segments; partIndex++) {
          const values = curve.getPart(partIndex / segments, (partIndex + 1) / segments).values;
          if (!Array.isArray(values) || values.length !== 8 || !values.every(Number.isFinite))
            throw Error('Subdivide Path exceeds finite coordinates');
          const start = [values[0], values[1]],
            control1 = [values[2], values[3]],
            control2 = [values[4], values[5]],
            end = [values[6], values[7]],
            startHandle = keepLineHandles
              ? [0, 0]
              : [control1[0] - start[0], control1[1] - start[1]],
            endHandle = keepLineHandles ? [0, 0] : [control2[0] - end[0], control2[1] - end[1]];
          let startPoint;
          if (points.length === 0) {
            startPoint = {
              ...sourceStart,
              id: `${prefix}:c${contourIndex}:p0`,
              position: start,
              handleA: [...sourceStart.handleA],
              handleB: startHandle,
            };
            points.push(startPoint);
          } else {
            startPoint = points[points.length - 1];
            startPoint.handleB = startHandle;
          }

          const isClosedSeam =
            contour.closed && edgeIndex === edgeCount - 1 && partIndex === segments - 1;
          if (isClosedSeam) {
            points[0].handleA = endHandle;
          } else {
            const pointIndex = points.length;
            points.push({
              ...(partIndex === segments - 1 ? sourceEnd : {}),
              id: `${prefix}:c${contourIndex}:p${pointIndex}`,
              position: end,
              handleA: endHandle,
              handleB:
                !contour.closed && edgeIndex === edgeCount - 1 && partIndex === segments - 1
                  ? [...sourceEnd.handleB]
                  : [0, 0],
            });
          }
        }
      }
      for (const point of points) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return { ...contour, id: `${prefix}:c${contourIndex}`, points };
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) !== outputPoints)
      throw Error('Subdivide Path exceeded its geometry estimate');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}
