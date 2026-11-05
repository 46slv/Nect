// Read-only semantic candidates, not a rendered-artwork or whole-WCAG audit.
// WCAG sRGB luminance/ratio and unrounded normal-text threshold:
// https://www.w3.org/TR/WCAG22/#dfn-relative-luminance
// https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html
// Source-over in encoded sRGB, only onto an explicitly opaque backdrop:
// https://www.w3.org/TR/compositing-1/#simplealphacompositing
export const NORMAL_TEXT_CONTRAST_THRESHOLD = 4.5;
const LIMIT = 256;
const idPattern = /^[A-Za-z0-9_-]{1,128}$/;
const roles = new Set(['Primary', 'Secondary', 'Accent', 'Background', 'Text']);
const record = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);
const validId = (id) => typeof id === 'string' && idPattern.test(id);

function rgb(value) {
  if (
    !record(value) ||
    Object.keys(value).length !== 2 ||
    typeof value.hex !== 'string' ||
    !/^#[0-9a-fA-F]{6}$/.test(value.hex) ||
    !Number.isFinite(value.alpha) ||
    value.alpha < 0 ||
    value.alpha > 1
  )
    return null;
  return [1, 3, 5].map((offset) => parseInt(value.hex.slice(offset, offset + 2), 16) / 255);
}

function luminance(channels) {
  const linear = channels.map((c) => (c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4));
  return linear[0] * 0.2126 + linear[1] * 0.7152 + linear[2] * 0.0722;
}

function compareChannels(foreground, alpha, background) {
  const mixed = foreground.map(
    (channel, index) => channel * alpha + background[index] * (1 - alpha),
  );
  const a = luminance(mixed),
    b = luminance(background);
  const ratio = (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
  return { ratio, status: ratio >= NORMAL_TEXT_CONTRAST_THRESHOLD ? 'pass' : 'fail', reason: null };
}

// Advisory for two explicit sRGB literals, not a rendered-artwork audit.
export function analyzeExplicitContrast(foreground, background) {
  const fg = rgb(foreground),
    bg = rgb(background);
  if (!bg) return { ratio: null, status: 'unknown', reason: 'invalid-background-color' };
  if (background.alpha !== 1)
    return { ratio: null, status: 'unknown', reason: 'non-opaque-background' };
  if (!fg) return { ratio: null, status: 'unknown', reason: 'invalid-text-color' };
  return compareChannels(fg, foreground.alpha, bg);
}

export function analyzeSelectedContrast(slots, foregroundId, backgroundId) {
  const unknown = (reason) => ({ ratio: null, status: 'unknown', reason });
  if (!validId(foregroundId) || !validId(backgroundId)) return unknown('select-two-colors');
  if (foregroundId === backgroundId) return unknown('select-distinct-colors');
  if (!Array.isArray(slots) || slots.length > LIMIT) return unknown('invalid-color-slots');
  const ids = new Set();
  for (const slot of slots) {
    if (!record(slot) || !validId(slot.id)) return unknown('invalid-color-slots');
    if (ids.has(slot.id)) return unknown('duplicate-color-slot-id');
    ids.add(slot.id);
  }
  const fg = slots.filter((slot) => record(slot) && slot.id === foregroundId);
  const bg = slots.filter((slot) => record(slot) && slot.id === backgroundId);
  if (fg.length > 1 || bg.length > 1) return unknown('duplicate-color-slot-id');
  if (!bg.length) return unknown('missing-background-color');
  if (!fg.length) return unknown('missing-text-color');
  return analyzeExplicitContrast(fg[0].value, bg[0].value);
}

/**
 * Analyze explicitly authored Text × Background slot candidates.
 * Slot names, equal hex values and palette order never infer semantic roles.
 * This validates the bounded report inputs, not the full document schema.
 * No canvas substrate, stacking order, text size, blends or effects are assumed.
 */
export function analyzePaletteContrast(doc) {
  const report = {
    mode: 'semantic-normal-text-contrast-v1',
    threshold: NORMAL_TEXT_CONTRAST_THRESHOLD,
    status: 'unknown',
    reason: null,
    pairs: [],
    issues: [],
  };
  const issue = (code, details = {}) => report.issues.push({ code, ...details });
  const invalid = () => ({ ...report, status: 'unknown', reason: 'invalid-input' });
  if (!record(doc)) {
    issue('invalid-document');
    return invalid();
  }
  const slots = doc.colorSlots === undefined ? [] : doc.colorSlots;
  if (!Array.isArray(slots) || slots.length > LIMIT) {
    issue('invalid-color-slots');
    return invalid();
  }
  const byId = new Map();
  let ambiguous = false;
  for (const [index, slot] of slots.entries()) {
    if (!record(slot) || !validId(slot.id)) {
      issue('invalid-color-slot', { index });
      ambiguous = true;
      continue;
    }
    if (byId.has(slot.id)) {
      issue('duplicate-color-slot-id', { index, id: slot.id });
      ambiguous = true;
      continue;
    }
    const channels = rgb(slot.value);
    if (!channels) issue('invalid-color-value', { index, id: slot.id });
    byId.set(slot.id, { channels, alpha: channels ? slot.value.alpha : null });
  }
  const design = doc.colorDesign;
  if (design === undefined || (record(design) && design.version === 1)) {
    if (report.issues.length) return invalid();
    return { ...report, status: 'not-applicable', reason: 'no-semantic-palette' };
  }
  if (!record(design) || design.version !== 2) {
    issue('invalid-color-design');
    return invalid();
  }
  if (!Array.isArray(design.palette) || design.palette.length > LIMIT) {
    issue('invalid-palette');
    return invalid();
  }
  const seen = new Set();
  for (const [index, entry] of design.palette.entries()) {
    if (
      !record(entry) ||
      !validId(entry.id) ||
      !['chromatic', 'neutral'].includes(entry.kind) ||
      !roles.has(entry.role)
    ) {
      issue('invalid-palette-entry', { index });
      ambiguous = true;
      continue;
    }
    if (seen.has(entry.id)) {
      issue('duplicate-palette-id', { index, id: entry.id });
      ambiguous = true;
    }
    seen.add(entry.id);
    if (!byId.has(entry.id)) issue('dangling-color-slot', { index, id: entry.id });
  }
  if (ambiguous) return invalid();
  const texts = design.palette.filter((entry) => entry.role === 'Text');
  const backgrounds = design.palette.filter((entry) => entry.role === 'Background');
  for (const text of texts) {
    for (const background of backgrounds) {
      const foreground = byId.get(text.id);
      const backdrop = byId.get(background.id);
      const pair = {
        textId: text.id,
        backgroundId: background.id,
        backgroundRole: background.role,
        ratio: null,
        status: 'unknown',
        reason: null,
      };
      if (!backdrop) pair.reason = 'missing-background-color';
      else if (!backdrop.channels) pair.reason = 'invalid-background-color';
      else if (backdrop.alpha !== 1) pair.reason = 'non-opaque-background';
      else if (!foreground) pair.reason = 'missing-text-color';
      else if (!foreground.channels) pair.reason = 'invalid-text-color';
      else {
        Object.assign(
          pair,
          compareChannels(foreground.channels, foreground.alpha, backdrop.channels),
        );
      }
      report.pairs.push(pair);
    }
  }
  if (report.issues.length) return invalid();
  if (!report.pairs.length)
    return { ...report, status: 'not-applicable', reason: 'no-text-background-pair' };
  if (report.pairs.some((pair) => pair.status === 'unknown'))
    return { ...report, reason: 'unresolved-pairs' };
  report.status = report.pairs.some((pair) => pair.status === 'fail') ? 'fail' : 'pass';
  return report;
}
