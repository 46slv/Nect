import { signature } from './path-signature.js';

export function evaluateMultiPath(_operation, _read, context) {
  const readPoint = context.pointReader(),
    points = new Map(
      context.mapPointIds((id) => [
        id,
        {
          id,
          position: readPoint(id, 'position'),
          handleA: readPoint(id, 'handleA'),
          handleB: readPoint(id, 'handleB'),
        },
      ]),
    ),
    result = {
      contours: context.mapContours((contour) => ({
        ...contour,
        points: contour.points.map((id) => points.get(id)),
      })),
    };
  // Keep the existing fingerprint used by downstream Remap edits.
  result.unstable = true;
  result.fingerprint = signature(result);
  return result;
}
