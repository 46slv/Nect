// Bounded, in-memory instrumentation for issue #222 Stage 0. No product source writes.
import assert from 'node:assert/strict';
import { registerHooks } from 'node:module';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import os from 'node:os';

const root = new URL('../', import.meta.url);
const sources = {};
const samples = [];
let active = null;
const stack = [];
const probe = {
  enter(name) {
    if (!active) return;
    const frame = { name, start: performance.now(), children: 0 };
    stack.push(frame);
    return frame;
  },
  exit(frame) {
    if (!frame) return;
    const elapsed = performance.now() - frame.start;
    assert.equal(stack.pop(), frame, 'stage nesting');
    const row = (active.stages[frame.name] ||= { calls: 0, inclusiveMs: 0, exclusiveMs: 0 });
    row.calls++;
    row.inclusiveMs += elapsed;
    row.exclusiveMs += elapsed - frame.children;
    if (stack.length) stack.at(-1).children += elapsed;
  },
  begin(count, index) {
    assert.equal(stack.length, 0);
    assert.equal(active, null);
    active = { count, index, stages: {} };
  },
  end(totalMs) {
    assert.equal(stack.length, 0);
    active.totalMs = totalMs;
    samples.push(active);
    active = null;
  },
};
globalThis.__nectStage0 = probe;
const enter = (name, variable) =>
  '\nconst ' + variable + ' = globalThis.__nectStage0.enter(' + JSON.stringify(name) + ');\n';
const exit = (variable) => '\nglobalThis.__nectStage0.exit(' + variable + ');\n';
const once = (s, from, to) => {
  assert.equal(s.split(from).length, 2, 'unique source anchor: ' + from.slice(0, 100));
  return s.replace(from, to);
};
let serial = 0;
function wrap(s, start, name, indent = '') {
  const a = s.indexOf(start);
  assert.ok(a >= 0 && s.indexOf(start, a + 1) < 0, 'unique function: ' + start);
  const rest = s.slice(a);
  const open = /\) \{/.exec(rest);
  assert.ok(open, 'function opening: ' + start);
  const body = a + open.index + open[0].length;
  const close = new RegExp('^' + indent + '\\}', 'm').exec(s.slice(body));
  assert.ok(close, 'function closing: ' + start);
  const end = body + close.index;
  const variable = '__stage0Frame' + serial++;
  return (
    s.slice(0, body) +
    enter(name, variable) +
    'try {\n' +
    s.slice(body, end) +
    '\n} finally {' +
    exit(variable) +
    '}\n' +
    s.slice(end)
  );
}
function segment(s, start, end, name) {
  const variable = '__stage0Segment' + serial++;
  s = once(s, start, enter(name, variable) + start);
  return once(s, end, exit(variable) + end);
}
function transformCore(s) {
  // All segments are disjoint within foreignAffectedVertices. Calls nested in
  // these segments are subtracted for exclusive time, never added twice.
  s = segment(
    s,
    '  const nodeKey = (layerId, nodeId) =>',
    '  // Properties can be read directly even when their node output does not use',
    'foreign.changedNodeDetection',
  );
  s = segment(
    s,
    '  const reverse = new Map(),',
    '  // Changed/re-activated destinations must themselves be checked whenever',
    'foreign.graphEnumerationAndReverseBuild',
  );
  s = segment(
    s,
    '  const dependsOnForeign = new Set(directForeign),',
    '  const queue = [],\n    seen = new Set(),\n    affected = new Map(),',
    'foreign.reverseForeignClosure',
  );
  s = segment(
    s,
    '  const queue = [],\n    seen = new Set(),\n    affected = new Map(),',
    "  // A changed Boolean source can switch a downstream node's effective inputs",
    'foreign.changedVertexSeeding',
  );
  s = segment(
    s,
    '  const reachable = new Set(queue.map((item) => item.key)),',
    '  for (const key of reachableQueue) {',
    'foreign.reverseReachableClosure',
  );
  s = segment(
    s,
    '  for (const key of reachableQueue) {',
    '  for (let i = 0; i < queue.length; i++) {\n    const item = queue[i],',
    'foreign.effectiveInputActivation',
  );
  s = segment(
    s,
    '  for (let i = 0; i < queue.length; i++) {\n    const item = queue[i],',
    '  return {\n    affected: [...affected.values()]',
    'foreign.reverseAffectedClosure',
  );
  s = segment(
    s,
    '  const currentFonts = resolveFonts(candidate, fonts),',
    '  validateEffective(effective);',
    'transition.effectiveProjectionAndEvaluation',
  );
  s = once(
    s,
    '  validateEffective(effective);',
    '  const __effectiveValidation = globalThis.__nectStage0.enter("transition.effectiveDomainValidation");\n  try { validateEffective(effective); } finally { globalThis.__nectStage0.exit(__effectiveValidation); }',
  );
  for (const [start, name] of [
    ['function foreignAffectedVertices(', 'foreignAffectedVertices'],
    ['function documentDependencyGraph(', 'documentDependencyGraph'],
    ['function assertArtworkReferenceTransition(', 'assertArtworkReferenceTransition'],
    ['export function assertProjectReferenceTransition(', 'assertProjectReferenceTransition'],
    ['function projectAsReferenceDocument(', 'projectAsReferenceDocument'],
    ['export function validate(doc)', 'validate'],
    ['export function cloneDocument(', 'cloneDocument'],
    ['export function sameDocument(', 'sameDocument'],
  ])
    s = wrap(s, start, name);
  s = wrap(
    s,
    '  #commandCandidate(type, payload, before, deferVariable = false)',
    'session.commandCandidate',
    '  ',
  );
  s = wrap(s, '  dispatch(command) {', 'session.dispatch', '  ');
  return s;
}
registerHooks({
  load(url, context, nextLoad) {
    const result = nextLoad(url, context);
    const rel = url.startsWith(root.href) ? url.slice(root.href.length) : '';
    if (
      ![
        'src/core.js',
        'src/composition-session-owner.js',
        'scripts/bench-cross-composition-edit.mjs',
      ].includes(rel)
    )
      return result;
    let s =
      typeof result.source === 'string' ? result.source : Buffer.from(result.source).toString();
    sources[rel] = createHash('sha256').update(s).digest('hex');
    s = s.replaceAll('\r\n', '\n');
    if (rel === 'src/core.js') s = transformCore(s);
    if (rel === 'src/composition-session-owner.js') {
      s = wrap(s, '  #projectWithDocument(', 'owner.projectCandidate', '  ');
      s = wrap(s, '  #documentFor(', 'owner.documentProjection', '  ');
      s = wrap(s, '  dispatch(command) {', 'owner.dispatch', '  ');
    }
    if (rel === 'scripts/bench-cross-composition-edit.mjs') {
      s = once(
        s,
        '    const start = performance.now();',
        '    globalThis.__nectStage0.begin(count, index);\n    const start = performance.now();',
      );
      s = once(
        s,
        '    samples.push(performance.now() - start);',
        '    samples.push(performance.now() - start);\n    globalThis.__nectStage0.end(samples.at(-1));',
      );
    }
    return { ...result, source: s };
  },
});

export { samples, sources };
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const { measureReferenceEdits } = await import('./bench-cross-composition-edit.mjs');
  const fixtures = [100, 1000, 10000].map((count) => measureReferenceEdits(count, 5));
  const median = (xs) => [...xs].sort((a, b) => a - b)[Math.floor(xs.length / 2)];
  const summaries = fixtures.map((f) => {
    const rows = samples.filter((s) => s.count === f.count);
    const names = [...new Set(rows.flatMap((r) => Object.keys(r.stages)))];
    return {
      count: f.count,
      totalMedianMs: f.medianMs,
      stages: names
        .map((name) => ({
          name,
          calls: rows.map((r) => r.stages[name]?.calls || 0),
          inclusiveMedianMs: median(rows.map((r) => r.stages[name]?.inclusiveMs || 0)),
          exclusiveMedianMs: median(rows.map((r) => r.stages[name]?.exclusiveMs || 0)),
        }))
        .sort((a, b) => b.exclusiveMedianMs - a.exclusiveMedianMs),
    };
  });
  assert.equal(samples.length, 15);
  console.log(
    JSON.stringify(
      {
        timestamp: new Date().toISOString(),
        node: process.version,
        platform: process.platform,
        arch: process.arch,
        cpu: os.cpus()[0]?.model,
        base: '8281ae7ea992b8824e03eeb2b5abe2c66f30008e',
        measurement:
          'Instrumented Node synchronous dispatch, 5 per size; no GUI/native/frame timing.',
        instrumentation:
          'In-memory module load transforms; no product source changes. Inclusive includes nested stages; exclusive subtracts nested spans. Logging, setup, value/Undo/Redo/serialization/reopen checks are outside dispatch timing. Timer overhead remains in results.',
        sources,
        fixtures,
        summaries,
        samples,
      },
      null,
      2,
    ),
  );
}
