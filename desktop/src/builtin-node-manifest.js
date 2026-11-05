import { snapshotNodeDeclaration } from './node-foundation.js';
import { mathDeclaration } from './math-node-declaration.js';
import { evaluateMath } from './math-node-binding.js';
import { separateVectorDeclaration } from './separate-vector-declaration.js';
import { evaluateSeparateVector, projectSeparateVector } from './separate-vector-binding.js';
import { vectorMathDeclaration } from './vector-math-declaration.js';
import { evaluateVectorMath } from './vector-math-binding.js';
import { reversePathDeclaration } from './reverse-path-declaration.js';
import { evaluateReversePath } from './reverse-path-binding.js';
import { joinGeometryDeclaration } from './join-geometry-declaration.js';
import { evaluateJoinGeometry } from './join-geometry-binding.js';

// One trusted row per built-in: behavior, placement and exposure are captured at startup.
const entry = (declaration, binding, definitionAfter, catalogAfter, exposure = 'public') =>
  Object.freeze({
    declaration: snapshotNodeDeclaration(declaration),
    binding: Object.freeze({ ...binding }),
    placement: Object.freeze({ definitionAfter, catalogAfter }),
    exposure,
  });
export const builtinManifest = Object.freeze([
  entry(mathDeclaration, { evaluate: evaluateMath }, 'Value', 'Value'),
  entry(
    separateVectorDeclaration,
    { evaluate: evaluateSeparateVector, project: projectSeparateVector },
    'Vector Math',
    'Vector Transform',
  ),
  entry(vectorMathDeclaration, { evaluate: evaluateVectorMath }, 'Math', 'Vector'),
  entry(reversePathDeclaration, { evaluate: evaluateReversePath }, 'Auto Handles', 'Set Closure'),
  entry(joinGeometryDeclaration, { evaluate: evaluateJoinGeometry }, 'Boolean', 'Boolean'),
]);
