import { proposeAutoChroma } from './auto-chroma.js';

export const DEFAULT_HARMONY_WHEEL = Object.freeze({
  harmony: 'triadic',
  hueSteps: 12,
  hueRotation: 18,
  lightnessSteps: 7,
  lightnessRange: 0.54,
});
const keys = Object.keys(DEFAULT_HARMONY_WHEEL);
const norm = (h) => ((h % 360) + 360) % 360;
export function normalizeHarmonyWheel(value) {
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.keys(value).length !== keys.length ||
    Object.keys(value).some((key) => !keys.includes(key)) ||
    !['triadic', 'complementary', 'analogous', 'split'].includes(value.harmony) ||
    !Number.isInteger(value.hueSteps) ||
    value.hueSteps < 3 ||
    value.hueSteps > 72 ||
    !Number.isInteger(value.lightnessSteps) ||
    value.lightnessSteps < 1 ||
    value.lightnessSteps > 12 ||
    !Number.isFinite(value.hueRotation) ||
    Math.abs(value.hueRotation) > 360000 ||
    !Number.isFinite(value.lightnessRange) ||
    value.lightnessRange < 0 ||
    value.lightnessRange > 1
  )
    throw Error('Invalid harmony wheel settings');
  const n = value.hueSteps;
  if (
    (value.harmony === 'triadic' && n % 3 !== 0) ||
    (['complementary', 'split'].includes(value.harmony) && n % 2 !== 0) ||
    (value.harmony === 'split' && n < 6)
  )
    throw Error('Harmony does not fit the selected hue cells');
  return { ...value, hueRotation: norm(value.hueRotation) };
}
const settings = normalizeHarmonyWheel;
function cellsFor(s) {
  const n = s.hueSteps;
  if (s.harmony === 'complementary') return [0, n / 2, n / 2];
  if (s.harmony === 'analogous') return [0, 1, n - 1];
  if (s.harmony === 'split') return [0, n / 2 - 1, n / 2 + 1];
  return [0, n / 3, (2 * n) / 3];
}
export function harmonyCells(value) {
  return cellsFor(settings(value));
}
function pointFor(s, cell, ring) {
  if (!Number.isInteger(ring) || ring < 0 || ring >= s.lightnessSteps)
    throw Error('Invalid lightness ring');
  const angle = -90 + (cell * 360) / s.hueSteps;
  const radius = (ring + 0.5) / s.lightnessSteps;
  const clean = (n) => (Math.abs(n) < 1e-15 ? 0 : n);
  return {
    cell,
    ring,
    angle,
    radius,
    x: clean(radius * Math.cos((angle * Math.PI) / 180)),
    y: clean(radius * Math.sin((angle * Math.PI) / 180)),
    L: 0.5 + s.lightnessRange / 2 - s.lightnessRange * radius,
    h: norm((cell * 360) / s.hueSteps + s.hueRotation),
  };
}
export function wheelPoint(value, position, ring) {
  const s = settings(value),
    cells = cellsFor(s);
  if (!Number.isInteger(position) || position < 0 || position >= cells.length)
    throw Error('Invalid harmony position');
  return pointFor(s, cells[position], ring);
}
// Visual cells are derived, not authored color slots. Their count can exceed
// the 256 authored-slot budget; each bounded proposal batch keeps that limit.
export function buildHarmonyWheel(value, options = {}) {
  const s = settings(value);
  proposeAutoChroma([], options);
  const cells = [];
  for (let ring = 0; ring < s.lightnessSteps; ring++) {
    for (let cell = 0; cell < s.hueSteps; cell++) {
      const p = pointFor(s, cell, ring),
        half = 180 / s.hueSteps;
      cells.push({
        ...p,
        id: `wheel-${cell}-${ring}`,
        startAngle: p.angle - half,
        endAngle: p.angle + half,
        innerRadius: ring / s.lightnessSteps,
        outerRadius: (ring + 1) / s.lightnessSteps,
      });
    }
  }
  const result = [];
  for (let start = 0; start < cells.length; start += 256) {
    const batch = cells.slice(start, start + 256);
    const colors = proposeAutoChroma(
      batch.map((p) => ({ id: p.id, L: p.L, h: p.h, role: 'Primary' })),
      { ...options, areaAware: false },
    );
    result.push(...batch.map((p, i) => ({ ...p, hex: colors[i].value.hex })));
  }
  return result;
}
export function resolveHarmonyPalette(value, slots, options = {}, coverage = null) {
  const s = settings(value),
    cells = cellsFor(s),
    markers = [];
  if (!Array.isArray(slots) || slots.length > 256) throw Error('Invalid harmony palette slots');
  const descriptors = Array.from(slots, (slot) => {
    if (!slot || typeof slot !== 'object' || Array.isArray(slot))
      throw Error('Invalid harmony palette slot');
    const neutral = slot.kind === 'neutral';
    if (slot.kind !== undefined && !['neutral', 'chromatic'].includes(slot.kind))
      throw Error('Invalid harmony slot kind');
    const allowed = neutral
      ? ['id', 'kind', 'L', 'role', 'alpha', 'anchor']
      : ['id', 'kind', 'position', 'ring', 'role', 'alpha', 'anchor', 'overrideC'];
    if (Object.keys(slot).some((key) => !allowed.includes(key)))
      throw Error('Unknown harmony slot field');
    const shared = {
      id: slot.id,
      role: slot.role,
      ...(slot.alpha === undefined ? {} : { alpha: slot.alpha }),
      ...(slot.anchor === undefined ? {} : { anchor: slot.anchor }),
    };
    if (neutral) return { ...shared, L: slot.L, h: 0, overrideC: 0 };
    if (!Number.isInteger(slot.position) || slot.position < 0 || slot.position >= cells.length)
      throw Error('Invalid harmony position');
    const p = pointFor(s, cells[slot.position], slot.ring);
    markers.push({ ...p, id: slot.id, position: slot.position });
    return {
      ...shared,
      L: p.L,
      h: p.h,
      ...(slot.overrideC === undefined ? {} : { overrideC: slot.overrideC }),
    };
  });
  return { colors: proposeAutoChroma(descriptors, options, coverage), markers };
}
