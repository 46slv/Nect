export function evaluateCurve(_operation, read, context) {
  const readPoint = context.pointReader(),
    closed = read('closed');
  return {
    contours: [
      {
        id: context.nodeId,
        closed,
        points: context.mapPointIds((id) => ({
          id,
          position: readPoint(id, 'position'),
          handleA: readPoint(id, 'handleA'),
          handleB: readPoint(id, 'handleB'),
        })),
      },
    ],
  };
}
