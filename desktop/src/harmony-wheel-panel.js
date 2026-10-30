import { buildHarmonyWheel, harmonyCells, wheelPoint } from './harmony-wheel.js';
import { MAX_CHROMA } from './auto-chroma.js';
import { createColorDraftScrub } from './color-draft-scrub.js';
import { createColorRangeEdit } from './color-range-edit.js';

const esc = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
const roles = ['Primary', 'Secondary', 'Accent', 'Background', 'Text'];
const presets = ['triadic', 'complementary', 'analogous', 'split'];
const clamp = (n, min, max) => Math.max(min, Math.min(max, n));
const polar = (r, angle) =>
  [r * Math.cos((angle * Math.PI) / 180), r * Math.sin((angle * Math.PI) / 180)].join(' ');
function markerInk(hex) {
  const [r, g, b] = [1, 3, 5].map((i) => {
    const c = parseInt(hex.slice(i, i + 2), 16) / 255;
    return c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
  });
  return 0.2126 * r + 0.7152 * g + 0.0722 * b > 0.179 ? '#000000' : '#ffffff';
}
function cellPath(cell) {
  const { innerRadius: inner, outerRadius: outer, startAngle: start, endAngle: end } = cell;
  return `M ${polar(outer * 100, start)} A ${outer * 100} ${outer * 100} 0 0 1 ${polar(outer * 100, end)} ${inner ? `L ${polar(inner * 100, end)} A ${inner * 100} ${inner * 100} 0 0 0 ${polar(inner * 100, start)}` : 'L 0 0'} Z`;
}
function compatible(wheel, change) {
  try {
    harmonyCells({ ...wheel, ...change });
    return true;
  } catch {
    return false;
  }
}

// View drafts project live through the owning Session; completion authors one validated edit.
export function createHarmonyWheelPanel({
  app,
  envelope,
  render,
  notify,
  onEdit,
  getState,
  getSlotChroma,
  buildGrid = buildHarmonyWheel,
  live = false,
  onCommit = () => {},
  onCancel = () => {},
  isEditing = () => false,
}) {
  let wheel = null,
    palette = [],
    savedWheel = null,
    savedPalette = [],
    savedKey,
    selected,
    gesture = null;
  const gridCache = new Map(); // At most four derived grids; pointer edits reuse the same entry.
  const fail = (error) => {
    onEdit();
    if (live) onCancel();
    notify(error.message, true);
    render();
  };
  const entry = () => palette.find((item) => item.id === selected);
  function stopGesture(rollback = false) {
    const old = gesture;
    gesture = null;
    if (!old) return;
    if (rollback) {
      const item = palette.find((item) => item.id === old.id);
      if (item) {
        if (old.kind === 'neutral') item.L = old.L;
        else item.ring = old.ring;
      }
      onEdit();
    }
    try {
      if (app.hasPointerCapture?.(old.pointerId)) app.releasePointerCapture(old.pointerId);
    } catch {}
  }
  function edit(action) {
    try {
      action();
      onEdit();
      onCommit();
      render();
      const picker = app.querySelector('[data-wheel-entry]');
      if (picker) picker.value = selected;
    } catch (error) {
      fail(error);
    }
  }
  app.addEventListener('change', (event) => {
    let el = event.target;
    const range = el.matches?.('[data-wheel-range]') ? el : null;
    if (range && rangeEdit.active) return;
    if (range) el = rangeControl(range);
    if (!el) return;
    if (el.matches?.('[data-wheel-add-kind]')) {
      event.stopImmediatePropagation();
      try {
        envelope(el);
        if (!['chromatic', 'neutral'].includes(el.value))
          throw Error('Choose a palette color kind');
      } catch (error) {
        fail(error);
      }
      return;
    }
    if (
      !el.matches?.(
        '[data-wheel-setting],[data-wheel-entry],[data-wheel-role],[data-wheel-ring],[data-wheel-lightness],[data-wheel-position],[data-wheel-chroma-mode],[data-wheel-chroma]',
      )
    )
      return;
    event.stopImmediatePropagation();
    if (live && !range && el.tagName === 'INPUT' && el.value === el.defaultValue && !isEditing())
      return;
    scrub.stop();
    edit(() => {
      envelope(range || el);
      stopGesture(true);
      update(el, range ? range.value : el.value);
    });
  });
  function update(el, raw) {
    raw = String(raw);
    if (!wheel) throw Error('Harmony palette is no longer available');
    if (el.hasAttribute('data-wheel-entry')) {
      if (!palette.some((item) => item.id === raw))
        throw Error('Palette color is no longer available');
      selected = raw;
    } else if (el.hasAttribute('data-wheel-setting')) {
      const key = el.dataset.wheelSetting;
      if (
        !['harmony', 'hueSteps', 'hueRotation', 'lightnessSteps', 'lightnessRange'].includes(key)
      )
        throw Error('Invalid wheel setting');
      if (!raw.trim()) throw Error('Enter a wheel setting');
      const value = key === 'harmony' ? raw : Number(raw);
      const next = { ...wheel, [key]: value };
      harmonyCells(next); // Validate before altering either settings or entries.
      if (key === 'lightnessSteps') {
        const originalWheel = live ? savedWheel : wheel;
        const originals = new Map((live ? savedPalette : palette).map((item) => [item.id, item]));
        for (const item of palette)
          if (item.kind === 'chromatic')
            item.ring = clamp(
              Math.round(
                ((originals.get(item.id).ring + 0.5) / originalWheel.lightnessSteps) * value - 0.5,
              ),
              0,
              value - 1,
            );
      }
      wheel = { ...next, hueRotation: ((next.hueRotation % 360) + 360) % 360 };
    } else {
      const item = entry();
      if (!item) throw Error('Palette color is no longer available');
      if (el.dataset.wheelId !== item.id)
        throw Error('Palette control is stale; select its color again');
      if (el.hasAttribute('data-wheel-role')) {
        if (!roles.includes(raw)) throw Error('Invalid palette role');
        item.role = raw;
      } else if (el.hasAttribute('data-wheel-chroma-mode')) {
        if (item.kind !== 'chromatic' || !['auto', 'override'].includes(raw))
          throw Error('Invalid palette chroma mode');
        if (raw === 'auto') delete item.overrideC;
        else if (item.overrideC === undefined) {
          const C = getSlotChroma(item.id);
          if (!Number.isFinite(C) || C < 0 || C > MAX_CHROMA)
            throw Error('Invalid palette chroma override');
          item.overrideC = C;
        }
      } else {
        const value = Number(raw);
        if (!raw.trim() || !Number.isFinite(value))
          throw Error('Enter a palette coordinate');
        if (el.hasAttribute('data-wheel-chroma')) {
          if (item.kind !== 'chromatic' || item.overrideC === undefined)
            throw Error('Choose Chroma override first');
          if (value < 0 || value > MAX_CHROMA)
            throw Error(`Chroma must be between zero and ${MAX_CHROMA}`);
          item.overrideC = value;
        } else if (el.hasAttribute('data-wheel-ring')) {
          if (
            item.kind !== 'chromatic' ||
            !Number.isInteger(value) ||
            value < 1 ||
            value > wheel.lightnessSteps
          )
            throw Error('Invalid lightness ring');
          item.ring = value - 1;
        } else if (el.hasAttribute('data-wheel-position')) {
          if (item.kind !== 'chromatic' || !Number.isInteger(value) || value < 0 || value > 2)
            throw Error('Invalid harmony position');
          item.position = value;
        } else {
          if (item.kind !== 'neutral' || value < 0 || value > 1)
            throw Error('Neutral lightness must be between zero and one');
          item.L = value;
        }
      }
    }
  }
  const scrub = createColorDraftScrub({
    app,
    selector:
      'input[data-color-scrub-step][data-wheel-setting],input[data-color-scrub-step][data-wheel-id]',
    envelope,
    getState,
    render,
    onEdit,
    update,
    onCommit,
    onCancel,
    snapshot: () => ({ wheel: { ...wheel }, palette: palette.map((item) => ({ ...item })) }),
    restore: (before) => {
      wheel = { ...before.wheel };
      palette = before.palette.map((item) => ({ ...item }));
    },
  });
  const rangeEdit = createColorRangeEdit({
    app,
    selector: '[data-wheel-range]',
    snapshot: () => ({ wheel: { ...wheel }, palette: palette.map((item) => ({ ...item })) }),
    restore: (before) => {
      wheel = { ...before.wheel };
      palette = before.palette.map((item) => ({ ...item }));
    },
    onCommit,
    onCancel,
    render,
  });
  function rangeControl(range) {
    const key = range.dataset.wheelRange;
    if (!wheel || !['hueSteps', 'hueRotation', 'lightnessSteps', 'lightnessRange'].includes(key))
      return null;
    if (key === 'hueSteps') {
      const allowed = Array.from({ length: 70 }, (_, index) => index + 3).filter((value) =>
        compatible(wheel, { hueSteps: value }),
      );
      const requested = Number(range.value);
      range.value = String(
        allowed.reduce((best, value) =>
          Math.abs(value - requested) < Math.abs(best - requested) ? value : best,
        ),
      );
    }
    return [...app.querySelectorAll('[data-wheel-setting]')].find(
      (el) => el.dataset.wheelSetting === key,
    );
  }
  function liveInput(event) {
    const input = event.target;
    const range = input.matches?.('[data-wheel-range]') ? input : null;
    const el = range ? rangeControl(range) : input;
    if (!el) return;
    if (
      !live ||
      event.isComposing ||
      (!range &&
        !el.matches?.(
          'input[data-color-scrub-step][data-wheel-setting],input[data-color-scrub-step][data-wheel-id]',
        ))
    )
      return;
    event.stopImmediatePropagation();
    try {
      envelope(input);
      if (range) rangeEdit.restoreBaseline();
      update(el, range ? range.value : el.value);
      onEdit();
      render();
    } catch {
      /* Keep incomplete/invalid text local until it is valid or cancelled. */
    }
  }
  app.addEventListener('input', liveInput);
  app.addEventListener('compositionend', liveInput);
  app.addEventListener('click', (event) => {
    const marker = event.target.closest?.(
      '[data-wheel-marker],[data-wheel-select],[data-wheel-neutral]',
    );
    if (!marker) return;
    event.stopImmediatePropagation();
    edit(() => {
      envelope(marker);
      scrub.stop();
      selected =
        marker.dataset.wheelMarker || marker.dataset.wheelSelect || marker.dataset.wheelNeutral;
    });
  });
  app.addEventListener('keydown', (event) => {
    if (event.isComposing || event.keyCode === 229 || event.defaultPrevented) return;
    if (event.target.closest?.('[data-wheel-select]') && event.key !== 'Escape') {
      // Swatch buttons retain native activation/Tab, not graph deletion/tool shortcuts.
      event.stopImmediatePropagation();
      return;
    }
    if (event.key === 'Escape' && gesture) {
      event.preventDefault();
      event.stopImmediatePropagation();
      stopGesture(true);
      onCancel();
      render();
      return;
    }
    const marker = event.target.closest?.('[data-wheel-marker],[data-wheel-neutral]');
    if (!marker || event.key === 'Escape') return;
    // A focused marker must never forward Delete/tool shortcuts into the graph.
    // Leave Tab's native focus movement intact while isolating its propagation.
    event.stopImmediatePropagation();
    if (
      !['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Home', 'End', 'Enter', ' '].includes(
        event.key,
      )
    )
      return;
    event.preventDefault();
    edit(() => {
      envelope(marker);
      selected = marker.dataset.wheelMarker || marker.dataset.wheelNeutral;
      const item = entry();
      if (!item) throw Error('Palette color is no longer available');
      if (item.kind === 'neutral') {
        if (event.key === 'Home') item.L = 1;
        else if (event.key === 'End') item.L = 0;
        else if (event.key.startsWith('Arrow'))
          item.L = clamp(
            item.L + (['ArrowLeft', 'ArrowUp'].includes(event.key) ? 0.01 : -0.01),
            0,
            1,
          );
        return;
      }
      if (event.key === 'Home') item.ring = 0;
      else if (event.key === 'End') item.ring = wheel.lightnessSteps - 1;
      else if (event.key.startsWith('Arrow'))
        item.ring = clamp(
          item.ring + (['ArrowUp', 'ArrowLeft'].includes(event.key) ? -1 : 1),
          0,
          wheel.lightnessSteps - 1,
        );
    });
  });
  app.addEventListener('pointerdown', (event) => {
    const marker = event.target.closest?.('[data-wheel-marker],[data-wheel-neutral]');
    if (!marker || event.button !== 0 || event.isPrimary === false) return;
    if (gesture && gesture.pointerId !== event.pointerId) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    try {
      const state = envelope(marker),
        neutral = marker.hasAttribute('data-wheel-neutral'),
        surface = neutral ? marker.closest('.harmony-neutral-track') : marker.closest('svg'),
        rect = surface.getBoundingClientRect();
      stopGesture(true);
      selected = marker.dataset.wheelMarker || marker.dataset.wheelNeutral;
      const item = entry();
      if (
        !item ||
        item.kind !== (neutral ? 'neutral' : 'chromatic') ||
        rect.width <= 0 ||
        rect.height <= 0
      )
        throw Error('Wheel is not available for dragging');
      gesture = {
        id: item.id,
        kind: item.kind,
        ring: item.ring,
        L: item.L,
        pointerId: event.pointerId,
        state,
        rect,
      };
      app.setPointerCapture?.(event.pointerId);
      onEdit();
      render();
    } catch (error) {
      stopGesture(true);
      fail(error);
    }
  });
  app.addEventListener('pointermove', (event) => {
    if (!gesture || gesture.pointerId !== event.pointerId) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    const state = getState();
    if (
      state.sessionId !== gesture.state.sessionId ||
      state.expectedRevision !== gesture.state.expectedRevision
    ) {
      stopGesture(true);
      onCancel();
      render();
      return;
    }
    const item = entry(),
      r = gesture.rect;
    if (!item || item.id !== gesture.id || !wheel) {
      stopGesture(true);
      onCancel();
      render();
      return;
    }
    if (gesture.kind === 'neutral') {
      const value = Number(clamp(1 - (event.clientX - r.left) / r.width, 0, 1).toFixed(3));
      if (item.L !== value) {
        item.L = value;
        onEdit();
        render();
      }
      return;
    }
    // SVG uses x/y ±115; preserve its square aspect even in a wide layout.
    const size = Math.min(r.width, r.height),
      radius =
        (Math.hypot(event.clientX - r.left - r.width / 2, event.clientY - r.top - r.height / 2) *
          230) /
        size /
        100;
    const ring = clamp(Math.floor(radius * wheel.lightnessSteps), 0, wheel.lightnessSteps - 1);
    if (item.ring !== ring) {
      item.ring = ring;
      onEdit();
      render();
    }
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
    app.addEventListener(type, (event) => {
      if (!gesture || event.pointerId !== gesture.pointerId) return;
      event.stopImmediatePropagation();
      stopGesture(type !== 'pointerup');
      if (type === 'pointerup') onCommit();
      else onCancel();
      render();
    });
  app.ownerDocument.defaultView?.addEventListener('blur', () => {
    if (gesture) {
      stopGesture(true);
      onCancel();
      render();
    }
  });
  return {
    synchronize(design, sessionId) {
      const key = JSON.stringify([sessionId, design?.version === 2 ? design : null]);
      if (key === savedKey) return;
      stopGesture();
      scrub.stop(false);
      rangeEdit.stop(false);
      savedKey = key;
      wheel = design?.version === 2 ? { ...design.wheel } : null;
      palette = design?.version === 2 ? design.palette.map((item) => ({ ...item })) : [];
      savedWheel = wheel ? { ...wheel } : null;
      savedPalette = palette.map((item) => ({ ...item }));
      if (!palette.some((item) => item.id === selected)) selected = palette[0]?.id;
    },
    cancelGesture() {
      rangeEdit.stop();
      scrub.stop();
      stopGesture(true);
    },
    get design() {
      return wheel ? { wheel: { ...wheel }, palette: palette.map((item) => ({ ...item })) } : null;
    },
    html({
      slots,
      profile,
      colorfulness,
      spread = 0.62,
      lockedSlotIds = [],
      section = 'all',
      preview = false,
    }) {
      if (!wheel) return '';
      const key = JSON.stringify([wheel, profile, colorfulness, spread]);
      let grid = gridCache.get(key);
      if (grid === undefined) {
        grid = buildGrid(wheel, { profile, colorfulness, spread })
          .map((cell) => `<path d="${cellPath(cell)}" fill="${cell.hex}"/>`)
          .join('');
        gridCache.set(key, grid);
        if (gridCache.size > 4) gridCache.delete(gridCache.keys().next().value);
      }
      const names = new Map(slots.map((slot) => [slot.id, slot.name]));
      // Session validation requires every palette ID to reference a literal Color slot.
      // Empty/v1 designs return above; neutral-only palettes produce no markers.
      const values = new Map(slots.map((slot) => [slot.id, slot.value.hex]));
      const chromatic = palette.filter((item) => item.kind === 'chromatic');
      const points = chromatic.map((item) => ({
        ...item,
        ...wheelPoint(wheel, item.position, item.ring),
        number: palette.indexOf(item) + 1,
      }));
      const item = entry();
      const hueValues = Array.from({ length: 70 }, (_, index) => index + 3).filter((value) =>
        compatible(wheel, { hueSteps: value }),
      );
      const chromaControl =
        item?.kind === 'chromatic'
          ? `<label>Chroma <select data-wheel-chroma-mode data-wheel-id="${esc(item.id)}" aria-label="Selected color chroma mode"><option value="auto" ${item.overrideC === undefined ? 'selected' : ''}>Auto (global)</option><option value="override" ${item.overrideC === undefined ? '' : 'selected'}>Override</option></select></label>${item.overrideC === undefined ? '' : `<label>Chroma override <input type="number" data-color-scrub-step="0.001" title="Drag horizontally to adjust; click to type" data-wheel-chroma data-wheel-id="${esc(item.id)}" min="0" max="${MAX_CHROMA}" step="0.001" value="${item.overrideC}" aria-label="Selected color chroma override"></label><p class="font-note">sRGB gamut may limit output.</p>`}${lockedSlotIds.includes(item.id) ? '<p class="font-note">Keep preserves this color.</p>' : ''}`
          : '<output data-wheel-chroma-neutral>Chroma 0</output>';
      const selectOptions = (values, value) =>
        values
          .map(
            (v) => `<option value="${esc(v)}" ${v === value ? 'selected' : ''}>${esc(v)}</option>`,
          )
          .join('');
      const chips = palette
        .map(
          (p, i) =>
            `<button type="button" data-wheel-select="${esc(p.id)}" aria-pressed="${p.id === selected}" aria-label="Select palette color ${i + 1}, ${esc(names.get(p.id) || p.id)}" title="${esc(names.get(p.id) || p.id)} ${esc(values.get(p.id))}"><span class="harmony-palette-chip" style="background:${esc(values.get(p.id))}" aria-hidden="true"></span><span>${i + 1}. ${esc(names.get(p.id) || p.id)}</span><small class="harmony-palette-role">${esc(p.role)}</small><output data-color-coverage-id="${esc(p.id)}" aria-label="${esc(names.get(p.id) || p.id)} saved visible area" title="Approximate saved artwork area">—</output></button>`,
        )
        .join('');
      const neutralEntries = palette.filter((p) => p.kind === 'neutral');
      const neutralMarkers = neutralEntries
        .map(
          (p) =>
            `<button type="button" data-wheel-neutral="${esc(p.id)}" role="slider" aria-label="${esc(names.get(p.id) || p.id)} neutral lightness" aria-valuemin="0" aria-valuemax="1" aria-valuenow="${p.L}" data-selected="${p.id === selected}" title="${esc(names.get(p.id) || p.id)}" style="left:${(1 - p.L) * 100}%">${palette.indexOf(p) + 1}</button>`,
        )
        .join('');
      const neutralLane = neutralEntries.length
        ? `<div class="harmony-neutral-lane"><span>Neutral</span><div class="harmony-neutral-track">${neutralMarkers}</div></div>`
        : '';
      const visual = `<section class="harmony-wheel-panel" aria-label="Harmony palette"><svg class="harmony-wheel" viewBox="-115 -115 230 230" aria-label="Discrete harmony wheel"><g class="harmony-wheel-grid" aria-hidden="true">${grid}</g><g class="harmony-wheel-spokes" aria-hidden="true">${points.map((p) => `<line x1="0" y1="0" x2="${p.x * 100}" y2="${p.y * 100}"/>`).join('')}<circle cx="0" cy="0" r="2"/></g>${points.map((p) => `<g tabindex="0" role="button" data-wheel-marker="${esc(p.id)}" aria-pressed="${selected === p.id}" aria-label="${esc(names.get(p.id) || p.id)}, palette color ${p.number}. Arrow keys change ring." transform="translate(${p.x * 100} ${p.y * 100})"><title>Ring ${p.ring + 1} of ${wheel.lightnessSteps}</title><circle r="7"/><circle r="5.7" data-wheel-marker-color style="fill:${esc(values.get(p.id))};stroke:none"/><text text-anchor="middle" dominant-baseline="central" style="fill:${markerInk(values.get(p.id))}">${p.number}</text></g>`).join('')}</svg><div class="harmony-palette-swatches" role="group" aria-label="${preview ? 'Preview palette colors' : 'Saved palette colors'}">${chips}</div>${neutralLane}<p class="harmony-coverage-status font-note" data-color-coverage-status role="status">Measuring visible area…</p></section>`;
      const settings = `<section class="harmony-wheel-parameters" aria-label="Harmony settings"><div class="harmony-wheel-settings"><label>Harmony <select data-wheel-setting="harmony" aria-label="Harmony preset">${presets.map((v) => `<option value="${v}" ${v === wheel.harmony ? 'selected' : ''} ${compatible(wheel, { harmony: v }) ? '' : 'disabled'}>${v === 'split' ? 'Split complementary' : v}</option>`).join('')}</select></label><label>Hue cells <span class="color-parameter-value">${live ? `<input type="range" data-wheel-range="hueSteps" min="${hueValues[0]}" max="${hueValues.at(-1)}" step="${hueValues.length > 1 ? hueValues[1] - hueValues[0] : 1}" value="${wheel.hueSteps}" aria-label="Hue cells slider">` : ''}<select data-wheel-setting="hueSteps" aria-label="Hue cells">${Array.from(
        { length: 70 },
        (_, i) => i + 3,
      )
        .map(
          (v) =>
            `<option value="${v}" ${v === wheel.hueSteps ? 'selected' : ''} ${compatible(wheel, { hueSteps: v }) ? '' : 'disabled'}>${v}</option>`,
        )
        .join(
          '',
        )}</select></span></label><label>Hue rotation <span class="color-parameter-value">${live ? `<input type="range" data-wheel-range="hueRotation" min="0" max="359" step="1" value="${wheel.hueRotation}" aria-label="Hue rotation slider">` : ''}<input type="number" data-color-scrub-step="1" title="Drag horizontally to adjust; click to type" min="0" max="360" step="1" data-wheel-setting="hueRotation" aria-label="Hue rotation" value="${wheel.hueRotation}"></span></label><label>Lightness rings <span class="color-parameter-value">${live ? `<input type="range" data-wheel-range="lightnessSteps" min="1" max="12" step="1" value="${wheel.lightnessSteps}" aria-label="Lightness rings slider">` : ''}<input type="number" data-color-scrub-step="1" title="Drag horizontally to adjust; click to type" min="1" max="12" step="1" data-wheel-setting="lightnessSteps" aria-label="Lightness rings" value="${wheel.lightnessSteps}"></span></label><label>Lightness range <span class="color-parameter-value">${live ? `<input type="range" data-wheel-range="lightnessRange" min="0" max="1" step="0.01" value="${wheel.lightnessRange}" aria-label="Lightness range slider">` : ''}<input type="number" data-color-scrub-step="0.01" title="Drag horizontally to adjust; click to type" min="0" max="1" step="0.01" data-wheel-setting="lightnessRange" aria-label="Lightness range" value="${wheel.lightnessRange}"></span></label></div></section>`;
      const entries = `<section class="harmony-entry-panel"><div class="harmony-selected-color" data-wheel-selection-info>${item ? `<strong>${esc(names.get(item.id) || item.id)} · ${esc(item.role)}</strong><output>${esc(values.get(item.id))}</output>` : ''}</div><div class="harmony-entry-controls"><label>Palette color <select data-wheel-entry aria-label="Selected palette color">${palette.map((p, i) => `<option value="${esc(p.id)}" ${p.id === selected ? 'selected' : ''}>${i + 1}. ${esc(names.get(p.id) || p.id)}</option>`).join('')}</select></label>${item ? `<label>Role <select data-wheel-role data-wheel-id="${esc(item.id)}" aria-label="Selected color role">${selectOptions(roles, item.role)}</select></label>${item.kind === 'chromatic' ? `<label>Spoke <select data-wheel-position data-wheel-id="${esc(item.id)}" aria-label="Selected color spoke">${[0, 1, 2].map((v) => `<option value="${v}" ${v === item.position ? 'selected' : ''}>${v + 1}</option>`).join('')}</select></label><label>Ring <input type="number" data-color-scrub-step="1" title="Drag horizontally to adjust; click to type" data-wheel-ring data-wheel-id="${esc(item.id)}" min="1" max="${wheel.lightnessSteps}" step="1" value="${item.ring + 1}" aria-label="Selected color ring"></label>` : `<label>Neutral lightness <input type="number" data-color-scrub-step="0.01" title="Drag horizontally to adjust; click to type" data-wheel-lightness data-wheel-id="${esc(item.id)}" min="0" max="1" step="0.01" value="${item.L}" aria-label="Selected neutral lightness"></label>`}${chromaControl}` : ''}</div><div class="harmony-add-controls">${live ? '<button type="button" data-harmony-create>New harmony palette</button>' : ''}<select data-wheel-add-kind aria-label="New palette color kind"><option value="chromatic">Chromatic</option><option value="neutral">Neutral</option></select><button type="button" data-harmony-add>Add palette color</button></div><p class="font-note">Drag numbered markers radially, or select any palette color and edit its ring. Overlapping markers remain selectable here. The grid shows Primary weighting; role/area-weighted results may differ. Grid colors are not contrast-optimized. Text/Background contrast is reported below.</p></section>`;
      return section === 'visual'
        ? visual
        : section === 'settings'
          ? settings
          : section === 'entry'
            ? entries
            : visual + settings + entries;
    },
  };
}
