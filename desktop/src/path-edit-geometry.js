import { getPaperScope, toPaper } from './paper-runtime.js';
import { isVector2 } from './node-value-validation.js';
import { dashIntervals } from './dash-intervals.js';

const clone = (value) => structuredClone(value);
function assertValue(type, value) {
  if (type === 'Vector2' && !isVector2(value)) throw Error('Expected Vector2');
}
// Normalize tiny curves before finding their arc-length parameter, then use it
// to take the exact subcurve from the original geometry.
function curveTimeAtDistance(curve, distance, length) {
  const scope = getPaperScope();
  if (distance <= 0) return 0;
  if (distance >= length) return 1;
  if (length >= 1e-6) return curve.getTimeAt(distance);
  const values = curve.values,
    scale = 1 / length,
    originX = values[0],
    originY = values[1],
    normalized = values.map((value, index) => (value - (index % 2 ? originY : originX)) * scale),
    scaledCurve = new scope.Curve(normalized),
    time = scaledCurve.getTimeAt(distance * scale);
  return Number.isFinite(time) ? Math.min(1, Math.max(0, time)) : time;
}

export function trimPathGeometry(source, { start, end, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isFinite(start) || start < 0 || start > 1)
    throw Error('Trim Path start must be from 0 to 1');
  if (!Number.isFinite(end) || end < 0 || end > 1) throw Error('Trim Path end must be from 0 to 1');
  if (start > end) throw Error('Trim Path start must not exceed end');
  if (typeof prefix !== 'string' || !prefix) throw Error('Trim Path needs a nonempty ID prefix');

  let inputPoints = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points)) throw Error('Trim Path needs valid contours');
    inputPoints += contour.points.length;
    if (inputPoints > 100000) throw Error('Trim Path exceeds geometry budget');
  }
  if (source.contours.length > 10000) throw Error('Trim Path exceeds geometry budget');

  const copyPoint = (point, contourIndex, pointIndex) => {
    const position = [...point.position],
      handleA = [...point.handleA],
      handleB = [...point.handleB];
    assertValue('Vector2', position);
    assertValue('Vector2', handleA);
    assertValue('Vector2', handleB);
    return {
      ...point,
      id: `${prefix}:c${contourIndex}:p${pointIndex}`,
      position,
      handleA,
      handleB,
    };
  };
  const copyContour = (contour, contourIndex, closed = contour.closed) => ({
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    closed,
    points: contour.points.map((point, pointIndex) => copyPoint(point, contourIndex, pointIndex)),
  });

  if (start === end) return { ...source, contours: [] };
  if (start === 0 && end === 1)
    return {
      ...source,
      contours: source.contours.map((contour, index) => copyContour(contour, index)),
    };

  const item = toPaper(source);
  try {
    let outputPointCount = 0;
    const contours = source.contours.map((contour, contourIndex) => {
      const path = item.children[contourIndex],
        length = path.length;
      if (!Number.isFinite(length) || length < 0)
        throw Error('Trim Path exceeds finite path length');

      // Arc length cannot distinguish positions on a degenerate contour. Keep
      // its finite points as an open contour for any non-empty partial range.
      if (length === 0) {
        outputPointCount += contour.points.length;
        if (outputPointCount > 100000) throw Error('Trim Path exceeds geometry budget');
        return copyContour(contour, contourIndex, false);
      }

      const from = length * start,
        to = length * end,
        output = [];
      if (![from, to].every(Number.isFinite)) throw Error('Trim Path exceeds finite path length');
      const append = (values) => {
        if (!Array.isArray(values) || values.length !== 8 || !values.every(Number.isFinite))
          throw Error('Trim Path exceeds finite coordinates');
        const addedPoints = output.length ? 1 : 2;
        if (outputPointCount + addedPoints > 100000)
          throw Error('Trim Path exceeds geometry budget');
        const startPoint = [values[0], values[1]],
          control1 = [values[2], values[3]],
          control2 = [values[4], values[5]],
          endPoint = [values[6], values[7]];
        if (!output.length) {
          output.push({
            id: `${prefix}:c${contourIndex}:p0`,
            position: startPoint,
            handleA: [0, 0],
            handleB: [control1[0] - startPoint[0], control1[1] - startPoint[1]],
          });
        } else {
          const previous = output[output.length - 1];
          previous.handleB = [
            control1[0] - previous.position[0],
            control1[1] - previous.position[1],
          ];
        }
        const index = output.length;
        output.push({
          id: `${prefix}:c${contourIndex}:p${index}`,
          position: endPoint,
          handleA: [control2[0] - endPoint[0], control2[1] - endPoint[1]],
          handleB: [0, 0],
        });
        outputPointCount += addedPoints;
      };
      let cursor = 0;
      for (const curve of path.curves) {
        const curveLength = curve.length,
          next = cursor + curveLength;
        if (!Number.isFinite(curveLength) || !Number.isFinite(next))
          throw Error('Trim Path exceeds finite path length');
        if (curveLength > 0) {
          const localStart = Math.max(0, from - cursor),
            localEnd = Math.min(curveLength, to - cursor);
          if (localEnd > localStart) {
            const t0 = curveTimeAtDistance(curve, localStart, curveLength),
              t1 = curveTimeAtDistance(curve, localEnd, curveLength);
            if (!Number.isFinite(t0) || !Number.isFinite(t1) || t0 < 0 || t1 > 1 || t0 > t1)
              throw Error('Trim Path could not locate a finite curve segment');
            if (t1 > t0) append(curve.getPart(t0, t1).values);
          }
        }
        cursor = next;
      }
      if (output.length > 100000) throw Error('Trim Path exceeds geometry budget');
      for (const point of output) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return {
        ...contour,
        id: `${prefix}:c${contourIndex}`,
        closed: false,
        points: output,
      };
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) > 100000)
      throw Error('Trim Path exceeds geometry budget');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}

export function setPathStartGeometry(source, { start, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isFinite(start) || start < 0 || start > 1)
    throw Error('Set Path Start start must be from 0 to 1');
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Set Path Start needs a nonempty ID prefix');
  if (source.contours.length > 10000) throw Error('Set Path Start exceeds geometry budget');

  let inputPointCount = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points) || typeof contour.closed !== 'boolean')
      throw Error('Set Path Start needs valid contours');
    inputPointCount += contour.points.length;
    if (inputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
    for (const point of contour.points) {
      if (!point) throw Error('Set Path Start needs valid points');
      assertValue('Vector2', point.position);
      assertValue('Vector2', point.handleA);
      assertValue('Vector2', point.handleB);
    }
  }

  const copyPoint = (point, contourIndex, pointIndex) => ({
      ...point,
      id: `${prefix}:c${contourIndex}:p${pointIndex}`,
      position: [...point.position],
      handleA: [...point.handleA],
      handleB: [...point.handleB],
    }),
    copyContour = (contour, contourIndex, points = contour.points) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      // Open paths have no seam to move. They intentionally pass through with
      // their point order, curve shape, groups and arbitrary source metadata intact.
      points: points.map((point, pointIndex) => copyPoint(point, contourIndex, pointIndex)),
    });

  // Both ends are geometry identities. Still give the modifier's result its
  // own deterministic IDs, like the other derived-path modifiers.
  if (start === 0 || start === 1)
    return {
      ...source,
      contours: source.contours.map((contour, contourIndex) => copyContour(contour, contourIndex)),
    };

  if (!source.contours.some((contour) => contour.closed && contour.points.length))
    return {
      ...source,
      contours: source.contours.map((contour, contourIndex) => copyContour(contour, contourIndex)),
    };

  const item = toPaper(source);
  try {
    if (!Array.isArray(item.children) || item.children.length !== source.contours.length)
      throw Error('Set Path Start could not resolve contour geometry');
    let outputPointCount = 0;
    const contours = source.contours.map((contour, contourIndex) => {
      const count = contour.points.length,
        path = item.children[contourIndex];
      if (!contour.closed || count === 0) {
        outputPointCount += count;
        if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
        return copyContour(contour, contourIndex);
      }

      const length = path.length;
      if (!Number.isFinite(length) || length < 0)
        throw Error('Set Path Start exceeds finite path length');
      // No arc-length position can distinguish a zero-length closed contour.
      // Keep its original closure and finite points for every seam value.
      if (length === 0) {
        outputPointCount += count;
        if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
        return copyContour(contour, contourIndex);
      }
      if (!path.curves || path.curves.length !== count)
        throw Error('Set Path Start could not resolve closed contour edges');

      const distance = start * length,
        tolerance = length * Number.EPSILON * 16;
      if (!Number.isFinite(distance)) throw Error('Set Path Start exceeds finite path length');

      // An exact source-vertex seam needs no split and must not acquire a
      // duplicate knot. Resolve a boundary before looking for an interior edge.
      let cursor = 0,
        vertexIndex = null,
        split = null;
      for (let edgeIndex = 0; edgeIndex < path.curves.length; edgeIndex++) {
        const curve = path.curves[edgeIndex],
          curveLength = curve.length,
          next = cursor + curveLength;
        if (!Number.isFinite(curveLength) || curveLength < 0 || !Number.isFinite(next))
          throw Error('Set Path Start exceeds finite path length');
        if (Math.abs(distance - cursor) <= tolerance) {
          vertexIndex = edgeIndex;
          break;
        }
        if (Math.abs(distance - next) <= tolerance) {
          vertexIndex = (edgeIndex + 1) % count;
          break;
        }
        if (curveLength > 0 && distance > cursor && distance < next) {
          const time = curveTimeAtDistance(curve, distance - cursor, curveLength);
          if (!Number.isFinite(time) || time <= 0 || time >= 1)
            throw Error('Set Path Start could not represent an interior seam');
          const tail = curve.getPart(time, 1)?.values,
            head = curve.getPart(0, time)?.values;
          if (
            !Array.isArray(tail) ||
            tail.length !== 8 ||
            !tail.every(Number.isFinite) ||
            !Array.isArray(head) ||
            head.length !== 8 ||
            !head.every(Number.isFinite)
          )
            throw Error('Set Path Start could not represent an interior seam');
          const position = tail.slice(0, 2),
            handleB = [tail[2] - position[0], tail[3] - position[1]],
            handleA = [head[4] - position[0], head[5] - position[1]];
          if (![...position, ...handleA, ...handleB].every(Number.isFinite))
            throw Error('Set Path Start could not represent an interior seam');
          split = { position, handleA, handleB, edgeIndex, tail, head };
          break;
        }
        cursor = next;
      }

      let points;
      if (vertexIndex !== null) {
        points = Array.from(
          { length: count },
          (_, offset) => contour.points[(vertexIndex + offset) % count],
        );
      } else if (split) {
        const seamSource = contour.points[split.edgeIndex];
        const tailEndIndex = (split.edgeIndex + 1) % count,
          tailEnd = contour.points[tailEndIndex],
          headStart = seamSource,
          tailEndHandleA = [split.tail[4] - split.tail[6], split.tail[5] - split.tail[7]],
          headStartHandleB = [split.head[2] - split.head[0], split.head[3] - split.head[1]];
        points =
          count === 1
            ? [
                {
                  ...seamSource,
                  position: split.position,
                  handleA: split.handleA,
                  handleB: split.handleB,
                },
                {
                  ...tailEnd,
                  handleA: tailEndHandleA,
                  handleB: headStartHandleB,
                },
              ]
            : [
                {
                  ...seamSource,
                  position: split.position,
                  handleA: split.handleA,
                  handleB: split.handleB,
                },
                {
                  ...tailEnd,
                  handleA: tailEndHandleA,
                },
                ...Array.from(
                  { length: count - 2 },
                  (_, offset) => contour.points[(split.edgeIndex + 2 + offset) % count],
                ),
                {
                  ...headStart,
                  handleB: headStartHandleB,
                },
              ];
      } else {
        throw Error('Set Path Start could not locate a finite seam');
      }

      outputPointCount += points.length;
      if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
      const remapped = copyContour(contour, contourIndex, points);
      for (const point of remapped.points) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return remapped;
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) > 100000)
      throw Error('Set Path Start exceeds geometry budget');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}

export function dashPathGeometry(source, options) {
  dashIntervals(0, options);
  if (!source?.contours) throw Error('Connect a Path input');
  if (
    source.contours.length > 128 ||
    source.contours.reduce((sum, c) => sum + c.points.length, 0) > 2048
  )
    throw Error('Dash Path exceeds input geometry budget');
  if (options.gapLength === 0) return clone(source);
  const item = toPaper(source),
    contours = [];
  let segments = 0,
    work = 0,
    points = 0;
  try {
    for (const [ci, c] of source.contours.entries()) {
      const length = item.children[ci].length;
      const intervals = dashIntervals(length, options);
      segments += intervals.length;
      work += intervals.length * c.points.length;
      if (segments > 512 || work > 200000) throw Error('Dash Path exceeds segment/work budget');
      let pieces = intervals.map(([a, b], i) => {
        const piece = trimPathGeometry(
          { contours: [c] },
          {
            start: a / length,
            end: b / length,
            prefix: `${options.prefix}:c${ci}:d${i}`,
          },
        ).contours[0];
        if (!piece?.points.length)
          throw Error('Dash Path interval is too small at this coordinate scale');
        return piece;
      });
      // A dash crossing a closed contour's stored seam remains one open segment.
      if (
        c.closed &&
        pieces.length > 1 &&
        intervals[0][0] === 0 &&
        intervals.at(-1)[1] === length
      ) {
        const first = pieces[0],
          last = pieces.at(-1);
        last.points.at(-1).handleB = [...first.points[0].handleB];
        last.points.push(...first.points.slice(1));
        pieces = [last, ...pieces.slice(1, -1)];
      }
      for (const [di, piece] of pieces.entries()) {
        points += piece.points.length;
        if (points > 100000) throw Error('Dash Path exceeds output geometry budget');
        piece.id = `${options.prefix}:c${ci}:d${di}`;
        piece.points.forEach((p, pi) => {
          p.id = `${piece.id}:p${pi}`;
        });
        contours.push(piece);
      }
    }
    return { contours };
  } finally {
    item.remove();
  }
}
