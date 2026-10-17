import opentype from 'opentype.js';
import paper from 'paper';
import { LIMITS, requireThat as check } from './contract.js';
/** Same OpenType.Path -> Paper -> relative cubic handles seam as current core.
 * Input is already Y-down in font units. Scaling belongs to the future caller. */
export function variableRunToCubicPath(run, prefix = 'variable') {
  check(Array.isArray(run.commands) && run.commands.length <= LIMITS.commands, 'OUTPUT_LIMIT');
  const path = new opentype.Path();
  path.commands = run.commands;
  const scope = new paper.PaperScope();
  scope.setup(new scope.Size(1, 1));
  try {
    const item = new scope.CompoundPath({ pathData: path.toPathData(5), insert: false });
    return {
      contours: item.children.map((c, ci) => ({
        id: `${prefix}:c${ci}`,
        closed: c.closed,
        points: c.segments.map((s, si) => ({
          id: `${prefix}:c${ci}:p${si}`,
          position: [s.point.x, s.point.y],
          handleA: [s.handleIn.x, s.handleIn.y],
          handleB: [s.handleOut.x, s.handleOut.y],
        })),
      })),
    };
  } finally {
    scope.project.remove();
  }
}
