import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, writeFile } from 'node:fs/promises';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { performance } from 'node:perf_hooks';
import { fileURLToPath } from 'node:url';

// Pin the pre-canonicalization implementation, not a duplicated formula.
// Recharacterize the actual algorithm before changing its derived-output bound.
const current = process.argv.includes('--current');
const revision = current
  ? execFileSync('git', ['rev-parse', 'HEAD'], { encoding: 'utf8' }).trim()
  : 'd8420660215674816a5b3492ffe455fa2b753eb7';
const path = 'desktop/src/hsl-color-picker.js';
const spec = `${revision}:${path}`;
const sourceFile = new URL('../src/hsl-color-picker.js', import.meta.url);
const source = current
  ? readFileSync(sourceFile, 'utf8')
  : execFileSync('git', ['show', spec], { encoding: 'utf8' });
const sourceBlob = execFileSync(
  'git',
  current ? ['hash-object', fileURLToPath(sourceFile)] : ['rev-parse', spec],
  { encoding: 'utf8' },
).trim();
const { hexToHsl, hslToHex } = await import(
  `data:text/javascript;base64,${Buffer.from(source).toString('base64')}`
);
const ulpAt100 = 2 ** -46;
let minS = Infinity,
  maxS = -Infinity,
  above100 = 0,
  below0 = 0;
let nonfinite = 0,
  badHue = 0,
  badLightness = 0,
  worstCount = 0;
let worstInputs = [];
let rejected = 0,
  roundtripMismatches = 0;
const failures = [];
const excessHistogram = new Map();
const started = performance.now();
for (let rgb = 0; rgb < 0x1000000; rgb++) {
  const hex = `#${rgb.toString(16).padStart(6, '0')}`;
  let coordinates;
  try {
    coordinates = hexToHsl(hex);
  } catch (error) {
    rejected++;
    if (failures.length < 8) failures.push({ hex, error: error.message });
    continue;
  }
  const { h, s, l } = coordinates;
  if (![h, s, l].every(Number.isFinite)) {
    nonfinite++;
    continue;
  }
  if (h < 0 || h >= 360) badHue++;
  if (l < 0 || l > 100) badLightness++;
  minS = Math.min(minS, s);
  if (s > maxS) {
    maxS = s;
    worstCount = 1;
    worstInputs = [hex];
  } else if (s === maxS) {
    worstCount++;
    if (worstInputs.length < 32) worstInputs.push(hex);
  }
  if (s < 0) below0++;
  if (s > 100) {
    above100++;
    const ulps = (s - 100) / ulpAt100;
    excessHistogram.set(ulps, (excessHistogram.get(ulps) || 0) + 1);
  }
  if (current) {
    try {
      const restored = hslToHex(h, s, l);
      if (restored !== hex) {
        roundtripMismatches++;
        if (failures.length < 8) failures.push({ hex, restored });
      }
    } catch (error) {
      rejected++;
      if (failures.length < 8) failures.push({ hex, error: error.message });
    }
  }
}
const result = {
  sourceRevision: revision,
  sourceMode: current ? 'current-worktree' : 'pinned-pre-canonicalization',
  sourcePath: path,
  sourceBlob,
  sourceSha256: createHash('sha256').update(source).digest('hex'),
  node: process.version,
  v8: process.versions.v8,
  inputDomain:
    'Every numeric six-digit HEX RGB triplet; letter-case variants decode to the same triplet',
  inputs: 0x1000000,
  minS,
  maxS,
  ulpAt100,
  maxExcess: maxS - 100,
  maxExcessUlpsAt100: (maxS - 100) / ulpAt100,
  above100,
  below0,
  nonfinite,
  badHue,
  badLightness,
  rejected,
  roundtripChecks: current ? 0x1000000 : 0,
  roundtripMismatches,
  failures,
  worstCount,
  worstInputs,
  excessHistogramUlpsAt100: Object.fromEntries([...excessHistogram].sort((a, b) => a[0] - b[0])),
  elapsedMs: performance.now() - started,
};
const output = resolve(
  process.argv.slice(2).find((arg) => !arg.startsWith('--')) ||
    (current ? 'test-results/hex-hsl-current.json' : 'test-results/hex-hsl-characterization.json'),
);
await mkdir(dirname(output), { recursive: true });
await writeFile(output, JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result, null, 2));
if (
  below0 ||
  nonfinite ||
  badHue ||
  badLightness ||
  rejected ||
  roundtripMismatches ||
  (current && above100)
)
  process.exitCode = 1;
