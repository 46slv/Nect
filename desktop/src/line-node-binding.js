export function evaluateLine(_operation, read, { nodeId }) {
  const [x, y] = read('position'),
    length = read('length'),
    endX = x + length;
  if (!Number.isFinite(length) || length <= 0 || !Number.isFinite(endX) || endX === x)
    throw Error('Line length must produce a positive, finite segment');
  return {
    contours: [
      {
        id: nodeId,
        closed: false,
        points: [x, endX].map((px, i) => ({
          id: `${nodeId}:${i}`,
          position: [px, y],
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  };
}
