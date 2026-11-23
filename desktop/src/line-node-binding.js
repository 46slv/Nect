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

export function evaluateLine(_operation, read, { nodeId }) {
  return {
    contours: [
      {
        id: nodeId,
        closed: false,
        points: lineEndpoints(read).map((position, i) => ({
          id: `${nodeId}:${i}`,
          position,
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  };
}
