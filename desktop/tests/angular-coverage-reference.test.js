import test from 'node:test';
import assert from 'node:assert/strict';
import {
  orderedAngularReferenceSVG,
  isAngularReferenceInterior,
} from './helpers/angular-coverage-reference.js';

test('fixed independent reference preserves the known ordered geometry and RGB mass basis', () => {
  const svg = orderedAngularReferenceSVG();
  const colors = [...svg.matchAll(/<path d="M32.5 24.5L[^\"]+" fill="(#[0-9a-f]+)"\/>/g)].map(
    (match) => match[1],
  );
  assert.equal(colors.length, 720);
  assert.equal(colors[0], '#fe0100');
  assert.equal(colors.at(-1), '#000001');
  assert.match(svg, /x="4" y="4" width="56" height="40" viewBox="4 4 56 40"/);
  assert.match(svg, /shape-rendering="crispEdges"/);
  assert.doesNotMatch(svg, /fill-opacity|mask/);
  assert.equal(svg, orderedAngularReferenceSVG());
});

test('only the shared positive ray is separated from strict ideal-angle interior checks', () => {
  const excluded = [];
  for (let y = 4; y < 44; y++)
    for (let x = 4; x < 60; x++) if (!isAngularReferenceInterior(x, y)) excluded.push([x, y]);
  assert.equal(excluded.length, 28);
  assert.deepEqual(excluded[0], [32, 24]);
  assert.deepEqual(excluded.at(-1), [59, 24]);
  assert.equal(isAngularReferenceInterior(31, 24), true);
  assert.equal(isAngularReferenceInterior(32, 25), true);
  assert.equal(isAngularReferenceInterior(60, 24), true);
});
