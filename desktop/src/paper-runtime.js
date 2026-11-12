// The core Paper build avoids PaperScript's unsafe-eval requirement.
import paper from 'paper/dist/paper-core.js';

let sharedScope;
export function getPaperScope() {
  if (!sharedScope) {
    const scope = new paper.PaperScope();
    scope.setup(new scope.Size(1200, 800));
    sharedScope = scope;
  }
  return sharedScope;
}

export function fromPaper(item, prefix) {
  let children = item.children || [item];
  return {
    contours: children.map((c, ci) => ({
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
}
