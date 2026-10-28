import { proposeAutoChroma } from './auto-chroma.js';
const esc = (v) =>
  String(v).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
export function colorChromaCoordinates(hex) {
  if (typeof hex !== 'string' || !/^#[0-9a-f]{6}$/i.test(hex)) throw Error('Invalid shared color');
  const [r, g, b] = [1, 3, 5]
    .map((i) => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map((v) => (v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4));
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b),
    m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b),
    s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  const L = 0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
    a = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
    bb = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s,
    C = Math.hypot(a, bb);
  return {
    L: Math.max(0, Math.min(1, L)),
    C,
    h: C < 1e-6 ? 0 : ((Math.atan2(bb, a) * 180) / Math.PI + 360) % 360,
  };
}
export function createAutoChromaPanel({
  app,
  getSession,
  getDocument,
  dispatch,
  render,
  notify,
  envelope,
}) {
  let sessionId,
    preview = null,
    profile = 'balanced',
    strength = 0.72;
  const locked = new Set(),
    slots = () => getDocument().colorSlots || [];
  const key = () =>
    JSON.stringify([getSession().state.sessionId, getSession().state.expectedRevision, slots()]);
  function synchronize() {
    const id = getSession().state.sessionId;
    if (sessionId !== id) {
      sessionId = id;
      locked.clear();
      preview = null;
      profile = 'balanced';
      strength = 0.72;
    }
    if (preview && preview.key !== key()) preview = null;
  }
  const reset = () => {
    preview = null;
  };
  function guard(action) {
    try {
      action();
    } catch (error) {
      reset();
      notify(error.message, true);
      render();
    }
  }
  app.addEventListener('click', (event) => {
    const button = event.target.closest?.(
      '[data-chroma-preview],[data-chroma-apply],[data-chroma-cancel]',
    );
    if (!button) return;
    event.stopImmediatePropagation();
    guard(() => {
      const state = envelope(button);
      if (button.hasAttribute('data-chroma-cancel')) {
        reset();
        render();
        return;
      }
      if (button.hasAttribute('data-chroma-apply')) {
        if (!preview || preview.key !== key()) throw Error('Color preview is stale; preview again');
        const updates = preview.rows.map((row) => ({ slotId: row.id, value: { ...row.value } }));
        reset();
        dispatch('set-color-slots', { updates }, state);
        render();
        return;
      }
      synchronize();
      if (!slots().length) throw Error('Add a shared color first');
      const descriptors = slots().map((slot) => {
        const { L, C, h } = colorChromaCoordinates(slot.value.hex);
        return {
          id: slot.id,
          L,
          h,
          alpha: slot.value.alpha,
          role: L < 0.25 ? 'Text' : L > 0.9 ? 'Background' : 'Primary',
          ...(locked.has(slot.id) || C < 1e-6 ? { anchor: { ...slot.value } } : {}),
        };
      });
      preview = {
        key: key(),
        rows: proposeAutoChroma(descriptors, { profile, colorfulness: strength, areaAware: false }),
      };
      render();
    });
  });
  app.addEventListener('change', (event) => {
    const el = event.target;
    if (!el.matches?.('[data-chroma-profile],[data-chroma-strength],[data-chroma-lock]')) return;
    event.stopImmediatePropagation();
    guard(() => {
      envelope(el);
      synchronize();
      if (el.hasAttribute('data-chroma-profile')) {
        if (!['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'].includes(el.value))
          throw Error('Invalid color profile');
        profile = el.value;
      } else if (el.hasAttribute('data-chroma-strength')) {
        const v = Number(el.value);
        if (!el.value.trim() || !Number.isFinite(v) || v < 0 || v > 1)
          throw Error('Colorfulness must be between zero and one');
        strength = v;
      } else {
        const id = el.dataset.chromaLock;
        if (!slots().some((slot) => slot.id === id))
          throw Error('Shared color is no longer available');
        if (el.checked) locked.add(id);
        else locked.delete(id);
      }
      reset();
      render();
    });
  });
  return {
    reset,
    html() {
      synchronize();
      if (!slots().length) return '';
      const names = new Map(slots().map((slot) => [slot.id, slot]));
      return `<section class="auto-chroma-controls" aria-label="Auto Chroma"><strong>Auto Chroma</strong><label>Profile <select data-chroma-profile aria-label="Auto Chroma profile">${['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'].map((name) => `<option value="${name}" ${name === profile ? 'selected' : ''}>${name}</option>`).join('')}</select></label><label>Colorfulness <input type="number" min="0" max="1" step="0.01" data-chroma-strength value="${strength}" aria-label="Auto Chroma colorfulness"></label><div class="chroma-locks">${slots()
        .map(
          (slot) =>
            `<label><input type="checkbox" data-chroma-lock="${esc(slot.id)}" ${locked.has(slot.id) ? 'checked' : ''}>Keep ${esc(slot.name)}</label>`,
        )
        .join(
          '',
        )}</div><p class="font-note">Preserves lightness, hue and alpha. Neutral colors stay neutral. Preview settings are temporary; applied colors are saved. Contrast is not checked.</p><button type="button" data-chroma-preview>Preview saturation</button>${preview ? `<div class="chroma-preview" aria-label="Proposed colors">${preview.rows.map((row) => `<div><span>${esc(names.get(row.id).name)}</span><span style="background:${esc(names.get(row.id).value.hex)}" class="chroma-swatch" aria-label="Current ${esc(names.get(row.id).value.hex)}"></span>→<span style="background:${esc(row.value.hex)}" class="chroma-swatch" aria-label="Proposed ${esc(row.value.hex)}"></span><output>${esc(row.value.hex)}</output></div>`).join('')}<button type="button" data-chroma-apply>Apply colors</button><button type="button" data-chroma-cancel>Cancel preview</button></div>` : ''}</section>`;
    },
  };
}
