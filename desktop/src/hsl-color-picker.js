// HSL is the default editing surface; the document still owns sRGB hex + alpha.
const esc = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );

export function hexToHsl(hex) {
  if (typeof hex !== 'string' || !/^#[0-9a-f]{6}$/i.test(hex))
    throw Error('Expected a six-digit sRGB color');
  const [r, g, b] = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
  const hi = Math.max(r, g, b),
    lo = Math.min(r, g, b),
    delta = hi - lo,
    l = (hi + lo) / 2;
  let h = 0;
  if (delta !== 0) {
    if (hi === r) h = ((g - b) / delta) % 6;
    else if (hi === g) h = (b - r) / delta + 2;
    else h = (r - g) / delta + 4;
    h = ((h * 60) % 360 + 360) % 360;
  }
  const s = delta === 0 ? 0 : delta / (1 - Math.abs(2 * l - 1));
  return { h, s: s * 100, l: l * 100 };
}

export function hslToHex(h, s, l) {
  if (
    ![h, s, l].every(Number.isFinite) ||
    h < 0 ||
    h > 360 ||
    s < 0 ||
    s > 100 ||
    l < 0 ||
    l > 100
  )
    throw Error('HSL must be H 0–360, S 0–100, L 0–100');
  const hue = h % 360,
    saturation = s / 100,
    lightness = l / 100,
    a = saturation * Math.min(lightness, 1 - lightness);
  const channel = (n) => {
    const k = (n + hue / 30) % 12;
    return Math.round(255 * (lightness - a * Math.max(-1, Math.min(k - 3, 9 - k, 1))));
  };
  return `#${[channel(0), channel(8), channel(4)]
    .map((n) => n.toString(16).padStart(2, '0'))
    .join('')}`;
}

export function updateHexHsl(hex, channel, raw) {
  if (!['h', 's', 'l'].includes(channel)) throw Error('Invalid HSL channel');
  if (typeof raw !== 'string' || !raw.trim()) throw Error('Enter an HSL value');
  const value = Number(raw);
  if (!Number.isFinite(value)) throw Error('Enter a finite HSL value');
  const hsl = hexToHsl(hex);
  hsl[channel] = value;
  return hslToHex(hsl.h, hsl.s, hsl.l);
}

export function renderHslFields(
  hex,
  { mode = 'parameter', name = 'Color', id = '', disabled = false, base = false } = {},
) {
  if (!['parameter', 'shared'].includes(mode)) throw Error('Invalid HSL input mode');
  const values = hexToHsl(hex);
  const label = esc(name);
  const fields = [
    ['h', 'H', 360, '°'],
    ['s', 'S', 100, '%'],
    ['l', 'L', 100, '%'],
  ].map(([channel, short, max, unit]) => {
    const data =
      mode === 'shared'
        ? `data-shared-color-edit="hsl-${channel}" data-shared-color-id="${esc(id)}"`
        : `data-color="hsl-${channel}"${base ? ' data-color-base' : ''}`;
    const rounded = Number(values[channel].toFixed(2));
    return `<label class="hsl-color-channel">${short}<input type="number" min="0" max="${max}" step="any" value="${rounded}" ${data} aria-label="${label} ${short} (${unit})" ${disabled ? 'disabled' : ''}><span aria-hidden="true">${unit}</span></label>`;
  });
  return `<div class="hsl-color-fields" role="group" aria-label="${label} HSL">${fields.join('')}</div>`;
}
