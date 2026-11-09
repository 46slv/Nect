import fs from 'node:fs';
import { spawnSync } from 'node:child_process';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
const base = 'D:/Documents/Codex/worktrees/Nect-stage0-dispatch-20261108/desktop';
const candidate = 'D:/Documents/Codex/worktrees/Nect-reference-graph-reuse-20261108/desktop';
const out = 'test-results/graph-reuse';
const hashes = {};
for (const [label, p] of [
  ['before', base],
  ['after', candidate],
])
  hashes[label] = crypto
    .createHash('sha256')
    .update(fs.readFileSync(p + '/src/core.js'))
    .digest('hex');
const runs = [];
for (const [i, order] of [
  ['1', ['before', 'after']],
  ['2', ['after', 'before']],
  ['3', ['before', 'after']],
]) {
  for (const label of order) {
    const run = spawnSync(process.execPath, ['scripts/bench-cross-composition-edit.mjs'], {
      cwd: label === 'before' ? base : candidate,
      encoding: 'utf8',
      timeout: 120000,
    });
    fs.writeFileSync(out + '/' + label + '-' + i + '.json', run.stdout || '');
    fs.writeFileSync(out + '/' + label + '-' + i + '.stderr.log', run.stderr || '');
    assert.equal(
      run.status,
      0,
      JSON.stringify({
        label,
        i,
        status: run.status,
        signal: run.signal,
        error: run.error?.message,
        stderr: run.stderr,
      }),
    );
    const data = JSON.parse(run.stdout);
    assert.equal(data.fixtures.length, 3);
    for (const f of data.fixtures) assert.equal(f.sampleCount, 5);
    runs.push({ pair: Number(i), label, data });
    console.log(
      JSON.stringify({
        pair: Number(i),
        label,
        medians: data.fixtures.map((f) => [f.count, f.medianMs]),
      }),
    );
  }
}
const median = (xs) => {
  xs = [...xs].sort((a, b) => a - b);
  return xs[Math.floor(xs.length / 2)];
};
const summaries = [100, 1000, 10000].map((count) => {
  const sides = Object.fromEntries(
    ['before', 'after'].map((label) => {
      const samples = runs
        .filter((r) => r.label === label)
        .flatMap((r) => r.data.fixtures.find((f) => f.count === count).samples);
      return [
        label,
        {
          samples,
          medianMs: median(samples),
          minMs: Math.min(...samples),
          maxMs: Math.max(...samples),
        },
      ];
    }),
  );
  return {
    count,
    ...sides,
    improvementPercent:
      (100 * (sides.before.medianMs - sides.after.medianMs)) / sides.before.medianMs,
  };
});
fs.writeFileSync(
  out + '/comparison.json',
  JSON.stringify(
    {
      time: new Date().toISOString(),
      node: process.version,
      hashes,
      order:
        'AB BA AB; separate processes; 5 dispatch per size per process; original benchmark assertions enabled',
      runs,
      summaries,
    },
    null,
    2,
  ) + '\n',
);
console.log(JSON.stringify({ summaries }, null, 2));
