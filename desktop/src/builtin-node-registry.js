import { createNodeRegistry } from './node-foundation.js';
import { mathDeclaration } from './math-node-declaration.js';
import { evaluateMath } from './math-node-binding.js';

// Builtins are explicitly registered by trusted host code, never loaded from a declaration.
export const builtinNodeRegistry = createNodeRegistry(
  [mathDeclaration],
  new Map([['builtin.math.v1', Object.freeze({ evaluate: evaluateMath })]]),
);
export const MATH_NODE = builtinNodeRegistry.get('Math');
