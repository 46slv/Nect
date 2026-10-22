// Browser-evaluated observers only. Return every native CTM unchanged: the
// interaction, document, snapping settings and rendered DOM remain untouched.
export function beginCurvePlacementTrace() {
  window.curvePointerTrace = [];
  window.curveCtmTrace = [];
  window.curveRequestedInput = {};
  window.curveTraceEvent = null;
  if (window.curveTraceInstalled) return;
  window.curveTraceInstalled = true;
  const matrixValues = (matrix) =>
    matrix && [matrix.a, matrix.b, matrix.c, matrix.d, matrix.e, matrix.f];
  for (const prototype of [
    globalThis.SVGGraphicsElement?.prototype,
    globalThis.SVGSVGElement?.prototype,
  ]) {
    const descriptor = prototype && Object.getOwnPropertyDescriptor(prototype, 'getScreenCTM');
    if (typeof descriptor?.value !== 'function') continue;
    const native = descriptor.value;
    Object.defineProperty(prototype, 'getScreenCTM', {
      ...descriptor,
      value: function (...args) {
        const matrix = native.apply(this, args);
        if (this.matches('.artboard') && window.curveCtmTrace) {
          window.curveCtmTrace.push({
            lastEventBoundary: window.curveTraceEvent,
            matrix: matrixValues(matrix),
            stack: new Error().stack,
          });
          if (window.curveCtmTrace.length > 160) window.curveCtmTrace.shift();
        }
        return matrix;
      },
    });
  }
  for (const capture of [true, false])
    for (const type of ['pointerdown', 'pointermove', 'pointerup'])
      document.addEventListener(
        type,
        (e) => {
          if (!window.curvePointerTrace) return;
          window.curveTraceEvent = { type, capture, x: e.clientX, y: e.clientY };
          const matrix = document.querySelector('.artboard')?.getScreenCTM();
          const world =
            matrix && new DOMPoint(e.clientX, e.clientY).matrixTransform(matrix.inverse());
          window.curvePointerTrace.push({
            type,
            capture,
            x: e.clientX,
            y: e.clientY,
            buttons: e.buttons,
            pointerId: e.pointerId,
            altKey: e.altKey,
            shiftKey: e.shiftKey,
            ctrlKey: e.ctrlKey,
            metaKey: e.metaKey,
            world: world && [world.x, world.y],
            matrix: matrixValues(matrix),
          });
          if (window.curvePointerTrace.length > 80) window.curvePointerTrace.shift();
        },
        capture,
      );
}

export function curvePlacementTrace() {
  return {
    trace: window.curvePointerTrace || [],
    ctmReads: window.curveCtmTrace || [],
    requestedInput: window.curveRequestedInput || {},
  };
}
