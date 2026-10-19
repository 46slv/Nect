import { spawn } from 'node:child_process';
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';

const suites = {
  smoke: [
    'project-composition-import.test.js',
    'command-session.test.js',
    'node-clipboard.test.js',
    'artwork-ordering.test.js',
    'numeric-scrub.test.js',
  ],
  composition: [
    'project-composition-import.test.js',
    'import-action-contract.test.js',
    'composition-reference.test.js',
    'svg-composition.test.js',
  ],
  graph: [
    'command-session.test.js',
    'node-clipboard.test.js',
    'node-duplication.test.js',
    'wiring.e2e.js',
  ],
  layers: [
    'artwork-ordering.test.js',
    'artwork-duplication.test.js',
    'artwork-multi-duplication.test.js',
  ],
  numeric: ['numeric-scrub.test.js', 'math.test.js', 'authored-value-expressions.test.js'],
};
const requested = process.argv.slice(2).filter((arg) => !arg.startsWith('--'));
const names = requested.length ? requested : ['smoke'];
const files = [...new Set(names.flatMap((name) => suites[name] ?? [name]))];
const unknown = names.filter(
  (name) => !suites[name] && !name.endsWith('.test.js') && !name.endsWith('.e2e.js'),
);
if (unknown.length) {
  console.error(`Unknown suite(s): ${unknown.join(', ')}`);
  process.exitCode = 2;
} else {
  const started = Date.now();
  const result = {
    status: 'RUNNING',
    suites: names,
    files,
    startedAt: new Date().toISOString(),
    cases: [],
  };
  const child = spawn(
    process.execPath,
    ['--test', ...files.map((file) => resolve('tests', file))],
    { cwd: resolve('.'), stdio: ['ignore', 'pipe', 'pipe'] },
  );
  let stdout = '',
    stderr = '';
  child.stdout.on('data', (chunk) => {
    stdout += chunk;
    process.stdout.write(chunk);
  });
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
    process.stderr.write(chunk);
  });
  child.on('close', async (code) => {
    result.status = code === 0 ? 'PASS' : 'FAIL';
    result.exitCode = code;
    result.durationMs = Date.now() - started;
    result.stdout = stdout;
    result.stderr = stderr;
    const out = resolve(process.env.NECT_RESULT ?? 'output/regression-result.json');
    await mkdir(dirname(out), { recursive: true });
    await writeFile(out, JSON.stringify(result, null, 2) + '\n');
    console.log(`Regression result: ${result.status} (${result.durationMs} ms) -> ${out}`);
    process.exitCode = code;
  });
}
