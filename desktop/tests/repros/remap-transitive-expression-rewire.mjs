// Known pre-existing limitation, deliberately outside the dual-era compatibility
// repair. Run from any directory with Node; this is not a passing acceptance test.
import * as core from '../../src/core.js';
import { remapFixture } from '../helpers/remap-fingerprint-fixture.js';

const f = remapFixture(core, 'Halftone', 'vector', 'expression'),
  controller = f.layer.nodes.find((n) => n.id === 'compat-expression-source'),
  a = core.node('Value'),
  b = core.node('Value');
a.id = 'expression-wire-a';
b.id = 'expression-wire-b';
a.params.value.value = b.params.value.value = 8;
f.layer.nodes.push(a, b);
controller.params.value.source = a.id;
const before = core.evaluate(f.layer).run(f.source.id),
  point = before.contours[0].points[0];
core.enableRemapPoint(f.remap, before, point.id).params.position.value = [3, 5];
controller.params.value.source = b.id;
const evaluation = core.evaluate(f.layer),
  after = evaluation.run(f.source.id),
  edited = evaluation.run(f.remap.id);
console.log(
  JSON.stringify(
    {
      desiredFutureBehavior: 'Retained edit suspends after source identity changes',
      geometryUnchanged: JSON.stringify(before.contours) === JSON.stringify(after.contours),
      fingerprintUnchanged: before.fingerprint === after.fingerprint,
      observedSuspended: !!edited.remapSuspended,
      warnings: evaluation.warnings,
    },
    null,
    2,
  ),
);
