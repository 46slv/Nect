// Pure proposal stage from the accepted Color Guard v1.1 prototype.
// Role/area weights are design heuristics, not a guarantee of good composition.
const weights = {
  balanced: { Primary: 1, Secondary: 0.76, Accent: 1.16, Background: 0.28, Text: 0.04 },
  fidelity: { Primary: 1, Secondary: 0.94, Accent: 1.03, Background: 0.45, Text: 0.04 },
  vibrant: { Primary: 1.08, Secondary: 0.92, Accent: 1.28, Background: 0.48, Text: 0.05 },
  soft: { Primary: 0.58, Secondary: 0.5, Accent: 0.72, Background: 0.18, Text: 0.03 },
  uniform: { Primary: 1, Secondary: 1, Accent: 1, Background: 1, Text: 0.03 },
};
const target = { Primary: 0.28, Secondary: 0.16, Accent: 0.07, Background: 0.47, Text: 0.02 };
const clamp = (n, a, b) => Math.max(a, Math.min(b, n));
const unit = (n) => Number.isFinite(n) && n >= 0 && n <= 1;
const idPattern = /^[A-Za-z0-9_-]{1,128}$/;

function linearRGB(L, C, hue) {
  const h = (hue * Math.PI) / 180,
    a = C * Math.cos(h),
    b = C * Math.sin(h);
  const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3;
  const m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3;
  const s = (L - 0.0894841775 * a - 1.291485548 * b) ** 3;
  return [
    4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
    -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
  ];
}
const inGamut = (rgb) => rgb.every((n) => n >= -1e-7 && n <= 1 + 1e-7);
// Each linear RGB channel is cubic in chroma. Split at all cube-boundary
// crossings, not the first crossing: the blue cusp can contain a second interval.
function gamutIntervals(L, hue) {
  if (L === 0 || L === 1) return [[0, 0]];
  const h = (hue * Math.PI) / 180,
    a = Math.cos(h),
    b = Math.sin(h);
  const k = [
    0.3963377774 * a + 0.2158037573 * b,
    -0.1055613458 * a - 0.0638541728 * b,
    -0.0894841775 * a - 1.291485548 * b,
  ];
  const matrix = [
    [4.0767416621, -3.3077115913, 0.2309699292],
    [-1.2684380046, 2.6097574011, -0.3413193965],
    [-0.0041960863, -0.7034186147, 1.707614701],
  ];
  const points = [0, 0.45];
  for (const row of matrix) {
    const sum = (power) => row.reduce((v, m, i) => v + m * k[i] ** power, 0);
    const c = [L ** 3 * sum(0), 3 * L * L * sum(1), 3 * L * sum(2), sum(3)];
    const A = 3 * c[3],
      B = 2 * c[2],
      C = c[1];
    const cuts = [0, 0.45];
    if (Math.abs(A) < 1e-14) {
      if (Math.abs(B) > 1e-14) cuts.push(-C / B);
    } else {
      const d = B * B - 4 * A * C;
      if (d >= 0) cuts.push((-B - Math.sqrt(d)) / (2 * A), (-B + Math.sqrt(d)) / (2 * A));
    }
    const ordered = cuts.filter((x) => x >= 0 && x <= 0.45).sort((x, y) => x - y);
    for (const boundary of [-1e-7, 1 + 1e-7]) {
      const f = (x) => ((c[3] * x + c[2]) * x + c[1]) * x + c[0] - boundary;
      for (let i = 1; i < ordered.length; i++) {
        let lo = ordered[i - 1],
          hi = ordered[i];
        const flo = f(lo),
          fhi = f(hi);
        if (Math.abs(flo) < 1e-13) points.push(lo);
        if (Math.abs(fhi) < 1e-13) points.push(hi);
        if (flo < 0 === fhi < 0) continue;
        for (let j = 0; j < 52; j++) {
          const mid = (lo + hi) / 2;
          if (f(mid) < 0 === flo < 0) lo = mid;
          else hi = mid;
        }
        points.push((lo + hi) / 2);
      }
    }
  }
  points.sort((x, y) => x - y);
  const intervals = [];
  for (let i = 1; i < points.length; i++) {
    const lo = points[i - 1],
      hi = points[i];
    if (hi > lo && inGamut(linearRGB(L, (lo + hi) / 2, hue))) intervals.push([lo, hi]);
  }
  return intervals.length ? intervals : [[0, 0]];
}
function boundedChroma(intervals, desired) {
  for (let i = intervals.length - 1; i >= 0; i--) {
    const [lo, hi] = intervals[i];
    if (desired >= lo) return Math.min(desired, hi);
  }
  return 0;
}
function toHex(rgb) {
  return (
    '#' +
    rgb
      .map((v) => {
        const s = v <= 0.0031308 ? 12.92 * v : 1.055 * v ** (1 / 2.4) - 0.055;
        return Math.round(clamp(s, 0, 1) * 255)
          .toString(16)
          .padStart(2, '0');
      })
      .join('')
  );
}

export function proposeAutoChroma(slots, settings = {}, coverage = null) {
  if (
    !settings ||
    typeof settings !== 'object' ||
    Array.isArray(settings) ||
    Object.keys(settings).some(
      (key) => !['profile', 'colorfulness', 'spread', 'areaAware'].includes(key),
    )
  )
    throw Error('Invalid Auto Chroma settings');
  const { profile = 'balanced', colorfulness = 0.72, spread = 0.62, areaAware = true } = settings;
  if (
    !Object.hasOwn(weights, profile) ||
    !unit(colorfulness) ||
    !unit(spread) ||
    typeof areaAware !== 'boolean'
  )
    throw Error('Invalid Auto Chroma settings');
  if (!Array.isArray(slots) || slots.length > 256) throw Error('Invalid Auto Chroma slots');
  const ids = new Set();
  const prepared = Array.from(slots, (slot) => {
    if (
      !slot ||
      typeof slot !== 'object' ||
      Array.isArray(slot) ||
      typeof slot.id !== 'string' ||
      !idPattern.test(slot.id) ||
      ids.has(slot.id) ||
      !unit(slot.L) ||
      !Number.isFinite(slot.h) ||
      Math.abs(slot.h) > 360000 ||
      !Object.hasOwn(target, slot.role)
    )
      throw Error('Invalid Auto Chroma slot');
    ids.add(slot.id);
    if (slot.alpha !== undefined && !unit(slot.alpha)) throw Error('Invalid slot alpha');
    if (
      slot.overrideC !== undefined &&
      (!Number.isFinite(slot.overrideC) || slot.overrideC < 0 || slot.overrideC > 0.45)
    )
      throw Error('Invalid chroma override');
    if (
      slot.anchor !== undefined &&
      (!slot.anchor ||
        typeof slot.anchor !== 'object' ||
        Array.isArray(slot.anchor) ||
        typeof slot.anchor.hex !== 'string' ||
        !/^#[a-fA-F0-9]{6}$/.test(slot.anchor.hex) ||
        !unit(slot.anchor.alpha))
    )
      throw Error('Invalid protected anchor');
    return { ...slot, h: ((slot.h % 360) + 360) % 360 };
  });
  const shares = new Map();
  if (coverage !== null) {
    if (typeof coverage !== 'object' || Array.isArray(coverage))
      throw Error('Invalid coverage input');
    if (
      !Array.isArray(coverage.slots) ||
      coverage.slots.length > 256 ||
      !Number.isFinite(coverage.other?.percent) ||
      !unit(coverage.other?.percent / 100)
    )
      throw Error('Invalid coverage input');
    let total = coverage.other.percent;
    for (const row of coverage.slots) {
      if (
        !row ||
        !ids.has(row.id) ||
        shares.has(row.id) ||
        !Number.isFinite(row.percent) ||
        !unit(row.percent / 100)
      )
        throw Error('Invalid coverage row');
      shares.set(row.id, row.percent / 100);
      total += row.percent;
    }
    if (total !== 0 && Math.abs(total - 100) > 1e-6)
      throw Error('Coverage percentages must total zero or 100');
  }
  return prepared.map((slot) => {
    if (slot.anchor !== undefined)
      return {
        id: slot.id,
        value: { hex: slot.anchor.hex, alpha: slot.anchor.alpha },
        source: 'protected-anchor',
      };
    const intervals = gamutIntervals(slot.L, slot.h);
    const maximum = intervals.at(-1)[1];
    const share = shares.get(slot.id) || 0;
    const area =
      areaAware && share > 0
        ? clamp((target[slot.role] / Math.max(share, 0.008)) ** 0.18, 0.66, 1.34)
        : 1;
    const ratio = clamp(
      colorfulness * (1 + spread * (weights[profile][slot.role] * area - 1)),
      0,
      0.98,
    );
    const desired = slot.overrideC === undefined ? maximum * ratio : slot.overrideC;
    const chroma = boundedChroma(intervals, desired);
    return {
      id: slot.id,
      value: { hex: toHex(linearRGB(slot.L, chroma, slot.h)), alpha: slot.alpha ?? 1 },
      L: slot.L,
      C: chroma,
      h: slot.h,
      cmax: maximum,
      source: slot.overrideC === undefined ? 'auto-chroma' : 'manual-chroma',
      gamutLimited: desired > chroma + 1e-9,
    };
  });
}
