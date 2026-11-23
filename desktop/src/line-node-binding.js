export function lineEndpoints(read) {
  const [x, y] = read('position'),
    length = read('length'),
    endX = x + length;
  if (!Number.isFinite(length) || length <= 0 || !Number.isFinite(endX) || endX === x)
    throw Error('Line length must produce a positive, finite segment');
  return [
    [x, y],
    [endX, y],
  ];
}

export function assertLineWidth(width) {
  if (!Number.isFinite(width) || width < 0)
    throw Error('Line width must be finite and nonnegative');
}

export function lineGeometry(read) {
  const [start, end] = lineEndpoints(read);
  const width = read('useWidth') ? read('width') : 0;
  assertLineWidth(width);
  if (width === 0) return { closed: false, positions: [start, end] };
  const half = width / 2,
    top = start[1] - half,
    bottom = start[1] + half;
  if (!Number.isFinite(top) || !Number.isFinite(bottom) || top === bottom)
    throw Error('Line width must produce a finite area');
  return {
    closed: true,
    positions: [
      // Match Rectangle's path origin and direction, including Stroke dash phase.
      [start[0], bottom],
      [start[0], top],
      [end[0], top],
      [end[0], bottom],
    ],
  };
}

export function evaluateLine(_operation, read, { nodeId }) {
  const { closed, positions } = lineGeometry(read),
    id = closed ? nodeId + ':area' : nodeId;
  return {
    contours: [
      {
        id,
        closed,
        points: positions.map((position, i) => ({
          id: `${id}:${i}`,
          position,
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  };
}
