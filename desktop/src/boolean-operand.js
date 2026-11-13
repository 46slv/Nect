import { toPaper } from './paper-runtime.js';
import { contourGroups } from './path-groups.js';

export function booleanOperand(path) {
  if (!path.contours.some((c) => c.group !== undefined)) return toPaper(path);
  let result = null;
  for (const contours of contourGroups(path)) {
    const item = toPaper({ contours });
    if (!result) {
      result = item;
      continue;
    }
    const union = result.unite(item, { insert: false });
    result.remove();
    item.remove();
    result = union;
  }
  return result || toPaper({ contours: [] });
}
