import { roundCorners } from './corner-round-geometry.js';
import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';

export function evaluateCornerRound(_operation, read, context) {
  const source = read('path');
  const radius = read('radius');
  if (radius < 0 || radius > 100000) throw Error('Corner Round radius must be from 0 to 100000 px');
  const result = roundCorners(source, { radius, prefix: fingerprint(context.nodeId) });
  for (const [reason, count] of Object.entries(result.cornerDiagnostics?.skipped || {}))
    context.warn(
      `${count} corner(s) left unchanged (${reason}). Curved corners use conservative local G1 blends.`,
    );
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Corner Round; edit the source Text',
  );
  result.unstable = true;
  result.fingerprint = fingerprint(signature(result));
  return result;
}
