/** Private prerequisite. No application parser or layout route imports this module. */
export const ENGINE = 'fontkit@2.0.4/simple-latin-v1';
export const LIMITS = Object.freeze({
  bytes: 20_000_000,
  totalBytes: 32_000_000,
  assets: 8,
  axes: 16,
  glyphs: 4096,
  text: 1024,
  commands: 50000,
  instances: 8,
  runs: 8,
  cachedCommands: 100000,
  outputBytes: 4_000_000,
  timeout: 5000,
});
export function reject(code) {
  const error = new Error(code);
  error.code = code;
  throw error;
}
export function requireThat(test, code) {
  if (!test) reject(code);
}
export function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}
export function canonicalAxes(axes, authored = {}) {
  requireThat(authored && Object.getPrototypeOf(authored) === Object.prototype, 'INVALID_AXES');
  const effective = {},
    sparse = {};
  requireThat(
    Object.keys(authored).every((tag) => Object.hasOwn(axes, tag)),
    'INVALID_AXES',
  );
  for (const tag of Object.keys(axes).sort()) {
    const a = axes[tag],
      v = Object.hasOwn(authored, tag) ? authored[tag] : a.default;
    requireThat(Number.isFinite(v) && v >= a.min && v <= a.max, 'INVALID_AXES');
    effective[tag] = Object.is(v, -0) ? 0 : v;
    if (v !== a.default) sparse[tag] = effective[tag];
  }
  return { effective, sparse };
}
export function validateRequest(request) {
  requireThat(
    request &&
      typeof request.text === 'string' &&
      request.text.length > 0 &&
      request.text.length <= LIMITS.text,
    'TEXT_LIMIT',
  );
  // Intentionally narrower than general Latin: no marks, controls, surrogate pairs or complex scripts.
  requireThat(
    /^[\u0020-\u007e\u00c0-\u00d6\u00d8-\u00f6\u00f8-\u024f]+$/u.test(request.text),
    'UNSUPPORTED_MAPPING',
  );
  requireThat(
    Object.keys(request).every((k) => ['text', 'axes'].includes(k)),
    'UNSUPPORTED_OPTIONS',
  );
}
export function runKey(asset, request) {
  validateRequest(request);
  if (Object.hasOwn(request, 'axes')) {
    requireThat(
      request.axes &&
        Object.getPrototypeOf(request.axes) === Object.prototype &&
        Object.keys(request.axes).length <= LIMITS.axes &&
        Object.entries(request.axes).every(
          ([tag, value]) => /^[\x20-\x7e]{4}$/.test(tag) && Number.isFinite(value),
        ),
      'INVALID_AXES',
    );
  }
  return JSON.stringify([
    ENGINE,
    asset,
    request.text,
    Object.entries(request.axes || {}).sort(([a], [b]) => a.localeCompare(b)),
  ]);
}
