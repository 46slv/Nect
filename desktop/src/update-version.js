// SemVer precedence uses decimal strings, never floating-point numbers.
export function parseVersion(value) {
  if (typeof value !== 'string' || value.length > 128 || !/^[\x00-\x7f]+$/.test(value)) return null;
  const match =
    /^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/.exec(
      value,
    );
  if (!match || match[0] !== value) return null;
  const pre = match[4]?.split('.') ?? [];
  if (pre.some((part) => /^[0-9]+$/.test(part) && part.length > 1 && part[0] === '0')) return null;
  return { core: match.slice(1, 4), pre };
}

const decimalOrder = (a, b) =>
  a.length === b.length ? (a === b ? 0 : a < b ? -1 : 1) : a.length < b.length ? -1 : 1;
export function compareVersions(a, b) {
  const left = parseVersion(a),
    right = parseVersion(b);
  if (!left || !right) return null;
  for (let i = 0; i < 3; i++) {
    const order = decimalOrder(left.core[i], right.core[i]);
    if (order) return order;
  }
  if (!left.pre.length || !right.pre.length)
    return left.pre.length === right.pre.length ? 0 : left.pre.length ? -1 : 1;
  for (let i = 0; i < Math.max(left.pre.length, right.pre.length); i++) {
    const x = left.pre[i],
      y = right.pre[i];
    if (x === undefined || y === undefined) return x === undefined ? -1 : 1;
    if (x === y) continue;
    const xn = /^[0-9]+$/.test(x),
      yn = /^[0-9]+$/.test(y);
    return xn && yn ? decimalOrder(x, y) : xn !== yn ? (xn ? -1 : 1) : x < y ? -1 : 1;
  }
  return 0;
}

export function validateVersionSet(input) {
  try {
    const versions = ['cargo', 'npm', 'tauriBase', 'tauriEffective'].map((key) => input?.[key]);
    if (versions.some((version) => !parseVersion(version)))
      return { ok: false, reason: 'INVALID_VERSION' };
    if (versions.some((version) => version !== versions[0]))
      return { ok: false, reason: 'VERSION_DRIFT' };
    return { ok: true, reason: 'CONSISTENT' };
  } catch {
    return { ok: false, reason: 'INVALID_VERSION' };
  }
}
