import { hexToHsl, hslToHex } from './hsl-color-picker.js';
import { createNumericControlBehavior } from './numeric-control-behavior.js';

// Local input only. The recipe panel owns Preview/Apply and the Session envelope.
export function createKeyColorC1Draft(
  host,
  value,
  { valid = () => true, blocked = () => false, allowEmpty = false } = {},
) {
  const document = host.ownerDocument;
  const field = (kind) => host.querySelector(`[data-key-color-input="${kind}"]`);
  let hsl = value ? hexToHsl(value.hex) : null,
    paintedHue = null,
    pointer = null,
    scrub = null,
    internalInput = false;
  const composing = new Set();
  const pendingComposition = new Set();
  host.innerHTML = `<div class="key-color-c1-picker"><div class="key-color-c1-plane"><canvas width="180" height="132" aria-label="Saturation left to right; lightness bottom to top"></canvas><span data-key-color-marker aria-hidden="true"></span></div><div class="key-color-c1-fields"><label class="hsl-picker-hue">Hue<input type="range" data-key-color-hue min="0" max="360" step="any" aria-label="C1 hue slider"></label><div>${['h', 's', 'l'].map((channel) => `<label>${channel.toUpperCase()}<input type="number" data-key-color-input="hsl-${channel}" min="0" max="${channel === 'h' ? 360 : 100}" step="any" aria-label="C1 ${channel === 'h' ? 'hue degrees' : channel === 's' ? 'saturation percent' : 'lightness percent'}"><span>${channel === 'h' ? '°' : '%'}</span></label>`).join('')}</div></div></div><div class="key-color-c1-literal"><span data-key-color-chip class="key-color-swatch" aria-hidden="true"></span><label>HEX<input type="text" data-key-color-input="hex" spellcheck="false" autocomplete="off" aria-label="C1 HEX literal"></label><label>Alpha<input type="number" data-key-color-input="alpha" min="0" max="1" step="any" aria-label="C1 alpha"></label></div>`;
  field('hex').value = value?.hex ?? '';
  field('alpha').value = value ? String(value.alpha) : '';
  const canvas = host.querySelector('canvas'),
    hue = host.querySelector('[data-key-color-hue]');

  function paint(skip = null) {
    const marker = host.querySelector('[data-key-color-marker]');
    hue.disabled = !hsl;
    hue.hidden = !hsl;
    canvas.setAttribute('aria-disabled', String(!hsl));
    marker.hidden = !hsl;
    for (const channel of ['h', 's', 'l']) field(`hsl-${channel}`).disabled = !hsl;
    if (!hsl) {
      for (const channel of ['h', 's', 'l']) field(`hsl-${channel}`).value = '';
      canvas.getContext('2d')?.clearRect?.(0, 0, canvas.width, canvas.height);
      paintedHue = null;
      paintChip();
      return;
    }
    for (const channel of ['h', 's', 'l']) {
      const input = field(`hsl-${channel}`);
      if (input !== skip) input.value = String(Number(hsl[channel].toFixed(2)));
    }
    hue.value = String(hsl.h);
    marker.style.left = `${hsl.s}%`;
    marker.style.top = `${100 - hsl.l}%`;
    marker.style.borderColor = hsl.l > 65 ? '#252525' : '#ffffff';
    paintChip();
    if (paintedHue === hsl.h) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const { width, height } = canvas,
      pixels = ctx.createImageData(width, height);
    for (let y = 0; y < height; y++)
      for (let x = 0; x < width; x++) {
        const hex = hslToHex(hsl.h, (x / (width - 1)) * 100, (1 - y / (height - 1)) * 100),
          at = (y * width + x) * 4;
        for (let c = 0; c < 3; c++)
          pixels.data[at + c] = parseInt(hex.slice(1 + c * 2, 3 + c * 2), 16);
        pixels.data[at + 3] = 255;
      }
    ctx.putImageData(pixels, 0, 0);
    paintedHue = hsl.h;
  }
  function paintChip() {
    const chip = host.querySelector('[data-key-color-chip]');
    chip.style.background = /^#[0-9a-f]{6}$/i.test(field('hex').value) ? field('hex').value : '';
    chip.style.opacity = field('alpha').value;
  }
  function validHsl() {
    return ['h', 's', 'l'].every((channel) => {
      const raw = field(`hsl-${channel}`).value,
        n = Number(raw);
      return !!raw.trim() && Number.isFinite(n) && n >= 0 && n <= (channel === 'h' ? 360 : 100);
    });
  }
  function writeHsl(skip = null) {
    if (!validHsl()) return;
    const hex = hslToHex(hsl.h, hsl.s, hsl.l);
    // A hue edit on gray, or an equal HSL input, must not rewrite the C1 literal.
    if (hex !== field('hex').value.toLowerCase()) field('hex').value = hex;
    paint(skip);
  }
  function edit(input) {
    if (composing.size) return;
    const kind = input.dataset?.keyColorInput;
    if (kind === 'hex' && /^#[0-9a-f]{6}$/i.test(input.value)) {
      const next = hexToHsl(input.value);
      // HSL hue is display/draft state, not the recipe's OKLCH hueHint.
      hsl = { ...next, h: next.s === 0 && hsl ? hsl.h : next.h };
      paint();
    } else if (allowEmpty && kind === 'hex') {
      hsl = null;
      paint();
    } else if (kind?.startsWith('hsl-')) {
      if (allowEmpty && (!hsl || !valid() || blocked())) return;
      const channel = kind.slice(4),
        n = Number(input.value);
      if (
        input.value.trim() &&
        Number.isFinite(n) &&
        n >= 0 &&
        n <= (channel === 'h' ? 360 : 100)
      ) {
        hsl[channel] = n;
        writeHsl(input);
      }
    } else if (input === hue) {
      if (allowEmpty && (!hsl || !valid() || blocked())) return;
      field('hsl-h').value = input.value;
      hsl.h = Number(input.value);
      writeHsl();
    } else if (kind === 'alpha') paintChip();
  }
  host.addEventListener('input', (event) => {
    if (scrub && !internalInput) {
      const raw = event.target.value;
      finishScrub();
      event.target.value = raw;
    }
    edit(event.target);
  });
  host.addEventListener('change', (event) => edit(event.target));
  host.addEventListener('compositionstart', (event) => {
    if (scrub) {
      finishScrub();
    }
    composing.add(event.target);
  });
  host.addEventListener('compositionend', (event) => {
    composing.delete(event.target);
    if (allowEmpty) pendingComposition.add(event.target);
    else edit(event.target);
  });
  const alpha = field('alpha');
  const setAlpha = (raw) => {
    alpha.value = raw;
    internalInput = true;
    try {
      alpha.dispatchEvent(new document.defaultView.Event('input', { bubbles: true }));
    } finally {
      internalInput = false;
    }
  };
  const notifyDraft = () => {
    internalInput = true;
    try {
      canvas.dispatchEvent(new document.defaultView.Event('input', { bubbles: true }));
    } finally {
      internalInput = false;
    }
  };
  function finishScrub(commit = false) {
    if (!scrub) return;
    const previous = scrub;
    scrub = null;
    document.removeEventListener('pointermove', onScrubMove, true);
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
      document.removeEventListener(type, onScrubEnd, true);
    document.defaultView?.removeEventListener('blur', onWindowBlur);
    if (commit) previous.behavior.finish();
    else previous.behavior.cancel();
  }
  function onWindowBlur() {
    finishScrub();
  }
  function onScrubMove(event) {
    if (!scrub || event.pointerId !== scrub.id) return;
    const current = scrub,
      result = current.behavior.move(event.clientX - current.x);
    if (result.cancelled) finishScrub();
    else if (current.behavior.active) {
      event.preventDefault();
      try {
        current.input.setPointerCapture(event.pointerId);
      } catch {}
    }
  }
  function onScrubEnd(event) {
    if (!scrub || event.pointerId !== scrub.id) return;
    finishScrub(event.type === 'pointerup');
  }
  host.addEventListener('pointerdown', (event) => {
    const input = event.target.closest?.('[data-key-color-input]'),
      kind = input?.dataset.keyColorInput,
      channel = kind?.startsWith('hsl-') ? kind.slice(4) : null;
    if (input !== alpha && !['h', 's', 'l'].includes(channel)) return;
    if (scrub || event.button !== 0 || event.isPrimary === false) return;
    if (allowEmpty && !hsl) return;
    if (composing.size || blocked()) {
      event.preventDefault();
      return;
    }
    const rect = input.getBoundingClientRect(),
      number = Number(input.value),
      max = channel ? (channel === 'h' ? 360 : 100) : 1;
    if (
      (rect.width && event.clientX >= rect.right - 18) ||
      !input.value.trim() ||
      !Number.isFinite(number) ||
      number < 0 ||
      number > max ||
      (channel && (!validHsl() || !/^#[0-9a-f]{6}$/i.test(field('hex').value))) ||
      !valid()
    )
      return;
    let original,
      originalHsl,
      originalHex,
      originalChannels,
      previewChanged = false;
    const restore = () => {
      if (!previewChanged) return;
      previewChanged = false;
      if (!channel) return setAlpha(original);
      hsl = { ...originalHsl };
      field('hex').value = originalHex;
      paint();
      for (const [name, raw] of Object.entries(originalChannels)) field(`hsl-${name}`).value = raw;
      notifyDraft();
    };
    const behavior = createNumericControlBehavior({
      valid: () =>
        input.isConnected && !input.disabled && !input.readOnly && !composing.size && valid(),
      read: () => {
        original = input.value;
        originalHsl = { ...hsl };
        originalHex = field('hex').value;
        originalChannels = Object.fromEntries(
          ['h', 's', 'l'].map((name) => [name, field(`hsl-${name}`).value]),
        );
        return [channel ? hsl[channel] : Number(original)];
      },
      step: () => (channel ? 1 : 0.01),
      bounds: () => [0, max],
      normalizeValues(values, { baseline: [baseline], requestedDelta }) {
        if (requestedDelta === 0) return [baseline];
        if (requestedDelta <= -baseline) return [0];
        if (requestedDelta >= max - baseline) return [max];
        return values;
      },
      begin() {},
      preview: ([next]) => {
        if (next === (channel ? originalHsl[channel] : Number(original))) return restore();
        previewChanged = true;
        if (!channel) return setAlpha(String(next));
        hsl = { ...originalHsl, [channel]: next };
        input.value = String(next);
        writeHsl(input);
        for (const [name, raw] of Object.entries(originalChannels))
          if (name !== channel) field(`hsl-${name}`).value = raw;
        notifyDraft();
      },
      commit() {},
      cancel: restore,
    });
    scrub = { behavior, input, id: event.pointerId, x: event.clientX };
    document.addEventListener('pointermove', onScrubMove, true);
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
      document.addEventListener(type, onScrubEnd, true);
    document.defaultView?.addEventListener('blur', onWindowBlur);
  });
  host.addEventListener('dragstart', (event) => {
    if (scrub?.input === event.target) event.preventDefault();
  });
  host.addEventListener(
    'blur',
    (event) => {
      if (scrub?.input === event.target) finishScrub();
    },
    true,
  );
  function fromPlane(event) {
    if (
      (allowEmpty && (!hsl || !valid() || blocked())) ||
      composing.size ||
      !field('hsl-h').value.trim() ||
      !Number.isFinite(Number(field('hsl-h').value))
    )
      return;
    const rect = canvas.getBoundingClientRect();
    if (!(rect.width > 0 && rect.height > 0)) return;
    hsl.s = Math.max(0, Math.min(100, ((event.clientX - rect.left) / rect.width) * 100));
    hsl.l = Math.max(0, Math.min(100, (1 - (event.clientY - rect.top) / rect.height) * 100));
    field('hsl-s').value = String(hsl.s);
    field('hsl-l').value = String(hsl.l);
    writeHsl();
    canvas.dispatchEvent(new document.defaultView.Event('input', { bubbles: true }));
  }
  canvas.addEventListener('pointerdown', (event) => {
    if (event.button !== 0) return;
    if (allowEmpty && (!hsl || !valid() || blocked() || composing.size)) return;
    event.preventDefault();
    field('hsl-s').focus();
    pointer = event.pointerId;
    canvas.setPointerCapture(pointer);
    fromPlane(event);
  });
  canvas.addEventListener('pointermove', (event) => {
    if (pointer === event.pointerId) fromPlane(event);
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
    canvas.addEventListener(type, () => {
      pointer = null;
    });
  paint();
  return {
    get active() {
      return !!scrub || pointer !== null;
    },
    settleComposition() {
      if (!allowEmpty || composing.size || blocked()) return;
      if (!valid()) {
        pendingComposition.clear();
        return;
      }
      for (const input of pendingComposition)
        if (input.isConnected && host.contains(input) && !input.disabled && !input.readOnly)
          edit(input);
      pendingComposition.clear();
    },
    dispose: () => {
      finishScrub();
      pendingComposition.clear();
      if (allowEmpty && pointer !== null) {
        try {
          canvas.releasePointerCapture(pointer);
        } catch {}
        pointer = null;
      }
    },
  };
}
