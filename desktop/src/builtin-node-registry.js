import { createNodeRegistry } from './node-foundation.js';
import { mathDeclaration } from './math-node-declaration.js';
import { evaluateMath } from './math-node-binding.js';
import { separateVectorDeclaration } from './separate-vector-declaration.js';
import { evaluateSeparateVector, projectSeparateVector } from './separate-vector-binding.js';
import { vectorMathDeclaration } from './vector-math-declaration.js';
import { evaluateVectorMath } from './vector-math-binding.js';
import { reversePathDeclaration } from './reverse-path-declaration.js';
import { evaluateReversePath } from './reverse-path-binding.js';
import { nectNodeHost } from './nect-node-host.js';

// Builtins are explicitly registered by trusted host code, never loaded from a declaration.
export const builtinNodeRegistry = createNodeRegistry(
  [mathDeclaration, separateVectorDeclaration, vectorMathDeclaration, reversePathDeclaration],
  new Map([
    ['builtin.math.v1', Object.freeze({ evaluate: evaluateMath })],
    ['builtin.reverse-path.v1', Object.freeze({ evaluate: evaluateReversePath })],
    ['builtin.vector-math.v1', Object.freeze({ evaluate: evaluateVectorMath })],
    [
      'builtin.separate-vector.v1',
      Object.freeze({ evaluate: evaluateSeparateVector, project: projectSeparateVector }),
    ],
  ]),
  nectNodeHost,
);
export const MATH_NODE = builtinNodeRegistry.get('Math');

export const SEPARATE_VECTOR = builtinNodeRegistry.get('Separate Vector');

export const VECTOR_MATH = builtinNodeRegistry.get('Vector Math');

export const REVERSE_PATH = builtinNodeRegistry.get('Reverse Path');
