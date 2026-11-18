// Bounded evaluator benchmark, not a UI frame-rate assertion or CI time limit.
import { performance } from 'node:perf_hooks';
import { evaluateBlend } from '../src/blend-node-binding.js';
const polygon = (count, dx) => ({
  contours: [
    {
      closed: true,
      points: Array.from({ length: count }, (_, i) => ({
        id: `p${i}`,
        position: [
          dx + 100 * Math.cos((i * 2 * Math.PI) / count),
          100 * Math.sin((i * 2 * Math.PI) / count),
        ],
        handleA: [0, 0],
        handleB: [0, 0],
      })),
    },
  ],
  fill: { hex: dx ? '#ffffff' : '#000000', alpha: 1 },
});
const measurements = [];
for (const points of [4, 128])
  for (const steps of [0, 1, 5, 100]) {
    const values = { a: polygon(points, 0), b: polygon(points, 300), steps };
    const run = () =>
      evaluateBlend(null, (key) => values[key], {
        nodeId: 'benchmark',
        groupAppearance: (children) => ({ children }),
      });
    run(); // One warm-up; five bounded measured calls.
    const samples = [];
    let output;
    for (let i = 0; i < 5; i++) {
      const start = performance.now();
      output = run();
      samples.push(performance.now() - start);
    }
    samples.sort((a, b) => a - b);
    measurements.push({
      points,
      steps,
      outputShapes: output.children.length,
      outputPoints: output.children.reduce((n, c) => n + c.contours[0].points.length, 0),
      medianMs: +samples[2].toFixed(3),
      maxMs: +samples[4].toFixed(3),
    });
  }
console.log(
  JSON.stringify(
    {
      node: process.version,
      scope: 'synthetic geometry/paint evaluator only; excludes Session/render/UI/IPC',
      measurements,
    },
    null,
    2,
  ),
);
