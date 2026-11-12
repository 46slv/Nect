import { waveGeometry } from './wave-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateWavePoints(operation, read, context) {
  const source = read('path');
  const result = waveGeometry(source, {
    amplitude: read('amplitude'),
    wavelength: read('wavelength'),
    phase: read('phase'),
    direction: operation,
    keepEndpoints: read('keepEndpoints'),
    prefix: fingerprint(context.nodeId),
  });
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Wave Points; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
