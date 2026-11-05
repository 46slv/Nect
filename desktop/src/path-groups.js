export function contourGroups(path) {
  const groups = new Map();
  for (const c of path.contours) {
    const key = c.group ?? '';
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(c);
  }
  return [...groups.values()];
}
