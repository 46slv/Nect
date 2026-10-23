import test from 'node:test';
import assert from 'node:assert/strict';
import { beginCurvePlacementTrace, curvePlacementTrace } from './helpers/curve-placement-trace.js';

test('Curve placement trace preserves native CTMs and observes internal reads between event phases', () => {
  const listeners = [];
  const originalGlobals = Object.fromEntries(
    ['window', 'document', 'DOMPoint', 'SVGGraphicsElement', 'SVGSVGElement'].map((key) => [
      key,
      Object.getOwnPropertyDescriptor(globalThis, key),
    ]),
  );
  const matrix = {
    a: 5 / 6,
    b: 0,
    c: 0,
    d: 5 / 6,
    e: 337,
    f: 100.171875,
    inverse() {
      return { a: 1 / this.a, d: 1 / this.d, e: -this.e / this.a, f: -this.f / this.d };
    },
  };
  let nativeCalls = 0,
    expectedArgs = [];
  class Graphics {
    getScreenCTM(...args) {
      assert.deepEqual(args, expectedArgs);
      nativeCalls++;
      assert.equal(this, svg);
      return matrix;
    }
    matches(selector) {
      return selector === '.artboard';
    }
  }
  class SVG extends Graphics {}
  const svg = new SVG();
  const document = {
    querySelector: () => svg,
    addEventListener: (type, callback, capture) => listeners.push({ type, callback, capture }),
  };
  try {
    Object.assign(globalThis, {
      window: {},
      document,
      SVGGraphicsElement: Graphics,
      SVGSVGElement: SVG,
      DOMPoint: class {
        constructor(x, y) {
          this.x = x;
          this.y = y;
        }
        matrixTransform(m) {
          return { x: this.x * m.a + m.e, y: this.y * m.d + m.f };
        }
      },
    });
    beginCurvePlacementTrace();
    assert.equal(listeners.length, 6);
    const wrapper = Graphics.prototype.getScreenCTM;
    const event = {
      clientX: 687,
      clientY: 275.171875,
      buttons: 1,
      pointerId: 1,
      altKey: true,
      shiftKey: false,
      ctrlKey: false,
      metaKey: false,
    };
    const dispatch = (capture) =>
      listeners.find((v) => v.type === 'pointerdown' && v.capture === capture).callback(event);
    dispatch(true);
    // The exact internal read that entry/exit event samples cannot expose.
    matrix.f = 114.375;
    const internalRead = svg.getScreenCTM();
    assert.equal(internalRead, matrix, 'observation must return the same native object');
    matrix.f = 100.171875;
    dispatch(false);
    const evidence = curvePlacementTrace();
    assert.deepEqual(
      evidence.trace.map((v) => v.matrix[5]),
      [100.171875, 100.171875],
    );
    assert.deepEqual(
      evidence.ctmReads.map((v) => v.matrix[5]),
      [100.171875, 114.375, 100.171875],
    );
    assert.equal(evidence.ctmReads[1].lastEventBoundary.capture, true);
    assert.match(evidence.ctmReads[1].stack, /curve-placement-trace.test.js/);
    assert.equal(nativeCalls, 3, 'no extra native geometry reads');
    expectedArgs = ['forwarded'];
    assert.equal(svg.getScreenCTM('forwarded'), matrix);
    expectedArgs = [];
    assert.deepEqual(evidence.trace[0].world, [420, 210]);
    assert.equal(evidence.trace[0].altKey, true);
    for (let i = 0; i < 200; i++) svg.getScreenCTM();
    assert.equal(curvePlacementTrace().ctmReads.length, 160);
    window.curveRequestedInput.end = { artboard: [438.75, 198.75] };
    assert.deepEqual(curvePlacementTrace().requestedInput, window.curveRequestedInput);
    beginCurvePlacementTrace();
    assert.equal(Graphics.prototype.getScreenCTM, wrapper, 'restarts must not wrap twice');
    assert.equal(listeners.length, 6, 'restarts must not add duplicate event listeners');
    assert.deepEqual(curvePlacementTrace(), { trace: [], ctmReads: [], requestedInput: {} });
  } finally {
    for (const [key, descriptor] of Object.entries(originalGlobals)) {
      if (descriptor) Object.defineProperty(globalThis, key, descriptor);
      else delete globalThis[key];
    }
  }
});
