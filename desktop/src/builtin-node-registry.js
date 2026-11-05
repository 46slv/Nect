import { createNodeRegistry } from './node-foundation.js';
import { builtinManifest } from './builtin-node-manifest.js';
import { manifestBindings } from './registered-node-projections.js';
import { nectNodeHost } from './nect-node-host.js';

const implementations = manifestBindings(builtinManifest);
export const builtinNodeRegistry = createNodeRegistry(
  builtinManifest.map((row) => row.declaration),
  implementations,
  nectNodeHost,
);
export const registeredNodes = Object.freeze(
  builtinManifest.map((row) =>
    Object.freeze({
      resolved: builtinNodeRegistry.get(row.declaration.typeId),
      placement: row.placement,
      exposure: row.exposure,
    }),
  ),
);
export const MATH_NODE = builtinNodeRegistry.get('Math');
export const SEPARATE_VECTOR = builtinNodeRegistry.get('Separate Vector');
export const VECTOR_MATH = builtinNodeRegistry.get('Vector Math');
export const REVERSE_PATH = builtinNodeRegistry.get('Reverse Path');
export const JOIN_GEOMETRY = builtinNodeRegistry.get('Join Geometry');
