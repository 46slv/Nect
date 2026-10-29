import { buildHarmonyWheel, harmonyCells, wheelPoint } from './harmony-wheel.js';
import { MAX_CHROMA } from './auto-chroma.js';

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

// This controller contains view drafts only. Its owner authors one validated design on Apply.
export function createHarmonyWheelPanel({
  app,
  envelope,
  render,
  notify,
  onEdit,
  getState,
  getSlotChroma,
  buildGrid = buildHarmonyWheel,
}) {
  let wheel = null,
    palette = [],
    savedKey,
    selected,
    gesture = null;
  const gridCache = new Map(); // At most four derived grids; pointer edits reuse the same entry.
  const fail = (error) => {
    onEdit();
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
      if (item) item.ring = old.ring;
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
      render();
    } catch (error) {
      fail(error);
    }
  }
  app.addEventListener('change', (event) => {
    const el = event.target;
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
    edit(() => {
      envelope(el);
      if (!wheel) throw Error('Harmony palette is no longer available');
      stopGesture(true);
      if (el.hasAttribute('data-wheel-entry')) {
        if (!palette.some((item) => item.id === el.value))
          throw Error('Palette color is no longer available');
        selected = el.value;
      } else if (el.hasAttribute('data-wheel-setting')) {
        const key = el.dataset.wheelSetting;
        if (
          !['harmony', 'hueSteps', 'hueRotation', 'lightnessSteps', 'lightnessRange'].includes(key)
        )
          throw Error('Invalid wheel setting');
        if (!el.value.trim()) throw Error('Enter a wheel setting');
        const value = key === 'harmony' ? el.value : Number(el.value);
        const next = { ...wheel, [key]: value };
        harmonyCells(next); // Validate before altering either settings or entries.
        if (key === 'lightnessSteps') {
          for (const item of palette)
            if (item.kind === 'chromatic')
              item.ring = clamp(
                Math.round(((item.ring + 0.5) / wheel.lightnessSteps) * value - 0.5),
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
          if (!roles.includes(el.value)) throw Error('Invalid palette role');
          item.role = el.value;
        } else if (el.hasAttribute('data-wheel-chroma-mode')) {
          if (item.kind !== 'chromatic' || !['auto', 'override'].includes(el.value))
            throw Error('Invalid palette chroma mode');
          if (el.value === 'auto') delete item.overrideC;
          else if (item.overrideC === undefined) {
            const C = getSlotChroma(item.id);
            if (!Number.isFinite(C) || C < 0 || C > MAX_CHROMA)
              throw Error('Invalid palette chroma override');
            item.overrideC = C;
          }
        } else {
          const value = Number(el.value);
          if (!el.value.trim() || !Number.isFinite(value))
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
    });
  });
  app.addEventListener('click', (event) => {
    const marker = event.target.closest?.('[data-wheel-marker]');
    if (!marker) return;
    event.stopImmediatePropagation();
    edit(() => {
      envelope(marker);
      selected = marker.dataset.wheelMarker;
    });
  });
  app.addEventListener('keydown', (event) => {
    if (event.isComposing || event.keyCode === 229 || event.defaultPrevented) return;
    if (event.key === 'Escape' && gesture) {
      event.preventDefault();
      event.stopImmediatePropagation();
      stopGesture(true);
      render();
      return;
    }
    const marker = event.target.closest?.('[data-wheel-marker]');
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
      selected = marker.dataset.wheelMarker;
      const item = entry();
      if (!item || item.kind !== 'chromatic') throw Error('Palette color is no longer available');
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
    const marker = event.target.closest?.('[data-wheel-marker]');
    if (!marker || event.button !== 0 || event.isPrimary === false) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    try {
      const state = envelope(marker),
        svg = marker.closest('svg'),
        rect = svg.getBoundingClientRect();
      stopGesture(true);
      selected = marker.dataset.wheelMarker;
      const item = entry();
      if (!item || item.kind !== 'chromatic' || rect.width <= 0 || rect.height <= 0)
        throw Error('Wheel is not available for dragging');
      gesture = { id: item.id, ring: item.ring, pointerId: event.pointerId, state, rect };
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
      render();
      return;
    }
    const item = entry(),
      r = gesture.rect;
    if (!item || item.id !== gesture.id || !wheel) {
      stopGesture(true);
      render();
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
      render();
    });
  app.ownerDocument.defaultView?.addEventListener('blur', () => {
    if (gesture) {
      stopGesture(true);
      render();
    }
  });
  return {
    synchronize(design, sessionId) {
      const key = JSON.stringify([sessionId, design?.version === 2 ? design : null]);
      if (key === savedKey) return;
      stopGesture();
      savedKey = key;
      wheel = design?.version === 2 ? { ...design.wheel } : null;
      palette = design?.version === 2 ? design.palette.map((item) => ({ ...item })) : [];
      if (!palette.some((item) => item.id === selected)) selected = palette[0]?.id;
    },
    cancelGesture() {
      stopGesture(true);
    },
    get design() {
      return wheel ? { wheel: { ...wheel }, palette: palette.map((item) => ({ ...item })) } : null;
    },
    html({ slots, profile, colorfulness, lockedSlotIds = [] }) {
      if (!wheel) return '';
      const key = JSON.stringify([wheel, profile, colorfulness]);
      let grid = gridCache.get(key);
      if (grid === undefined) {
        grid = buildGrid(wheel, { profile, colorfulness })
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
      const chromaControl =
        item?.kind === 'chromatic'
          ? `<label>Chroma <select data-wheel-chroma-mode data-wheel-id="${esc(item.id)}" aria-label="Selected color chroma mode"><option value="auto" ${item.overrideC === undefined ? 'selected' : ''}>Auto (global)</option><option value="override" ${item.overrideC === undefined ? '' : 'selected'}>Override</option></select></label>${item.overrideC === undefined ? '' : `<label>Chroma override <input type="number" data-wheel-chroma data-wheel-id="${esc(item.id)}" min="0" max="${MAX_CHROMA}" step="0.001" value="${item.overrideC}" aria-label="Selected color chroma override"></label><p class="font-note">sRGB gamut may limit output.</p>`}${lockedSlotIds.includes(item.id) ? '<p class="font-note">Keep preserves this color.</p>' : ''}`
          : '<output data-wheel-chroma-neutral>Chroma 0</output>';
      const selectOptions = (values, value) =>
        values
          .map(
            (v) => `<option value="${esc(v)}" ${v === value ? 'selected' : ''}>${esc(v)}</option>`,
          )
          .join('');
      return `<section class="harmony-wheel-panel" aria-label="Harmony palette"><strong>Harmony wheel</strong><p class="font-note">Edit a draft. Preview then Apply to update artwork colors.</p><svg class="harmony-wheel" viewBox="-115 -115 230 230" aria-label="Discrete harmony wheel"><g class="harmony-wheel-grid" aria-hidden="true">${grid}</g><g class="harmony-wheel-spokes" aria-hidden="true">${points.map((p) => `<line x1="0" y1="0" x2="${p.x * 100}" y2="${p.y * 100}"/>`).join('')}<circle cx="0" cy="0" r="2"/></g>${points.map((p) => `<g tabindex="0" role="button" data-wheel-marker="${esc(p.id)}" aria-pressed="${selected === p.id}" aria-label="${esc(names.get(p.id) || p.id)}, palette color ${p.number}. Arrow keys change ring." transform="translate(${p.x * 100} ${p.y * 100})"><title>Ring ${p.ring + 1} of ${wheel.lightnessSteps}</title><circle r="7"/><circle r="5.7" data-wheel-marker-color style="fill:${esc(values.get(p.id))};stroke:none"/><text text-anchor="middle" dominant-baseline="central" style="fill:${markerInk(values.get(p.id))}">${p.number}</text></g>`).join('')}</svg><div class="harmony-wheel-settings"><label>Harmony <select data-wheel-setting="harmony" aria-label="Harmony preset">${presets.map((v) => `<option value="${v}" ${v === wheel.harmony ? 'selected' : ''} ${compatible(wheel, { harmony: v }) ? '' : 'disabled'}>${v === 'split' ? 'Split complementary' : v}</option>`).join('')}</select></label><label>Hue cells <select data-wheel-setting="hueSteps" aria-label="Hue cells">${Array.from(
        { length: 70 },
        (_, i) => i + 3,
      )
        .map(
          (v) =>
            `<option value="${v}" ${v === wheel.hueSteps ? 'selected' : ''} ${compatible(wheel, { hueSteps: v }) ? '' : 'disabled'}>${v}</option>`,
        )
        .join(
          '',
        )}</select></label><label>Hue rotation <input type="number" min="0" max="360" step="1" data-wheel-setting="hueRotation" aria-label="Hue rotation" value="${wheel.hueRotation}"></label><label>Lightness rings <input type="number" min="1" max="12" step="1" data-wheel-setting="lightnessSteps" aria-label="Lightness rings" value="${wheel.lightnessSteps}"></label><label>Lightness range <input type="number" min="0" max="1" step="0.01" data-wheel-setting="lightnessRange" aria-label="Lightness range" value="${wheel.lightnessRange}"></label></div><div class="harmony-entry-controls"><label>Palette color <select data-wheel-entry aria-label="Selected palette color">${palette.map((p, i) => `<option value="${esc(p.id)}" ${p.id === selected ? 'selected' : ''}>${i + 1}. ${esc(names.get(p.id) || p.id)}</option>`).join('')}</select></label>${item ? `<label>Role <select data-wheel-role data-wheel-id="${esc(item.id)}" aria-label="Selected color role">${selectOptions(roles, item.role)}</select></label>${item.kind === 'chromatic' ? `<label>Spoke <select data-wheel-position data-wheel-id="${esc(item.id)}" aria-label="Selected color spoke">${[0, 1, 2].map((v) => `<option value="${v}" ${v === item.position ? 'selected' : ''}>${v + 1}</option>`).join('')}</select></label><label>Ring <input type="number" data-wheel-ring data-wheel-id="${esc(item.id)}" min="1" max="${wheel.lightnessSteps}" step="1" value="${item.ring + 1}" aria-label="Selected color ring"></label>` : `<label>Neutral lightness <input type="number" data-wheel-lightness data-wheel-id="${esc(item.id)}" min="0" max="1" step="0.01" value="${item.L}" aria-label="Selected neutral lightness"></label>`}${chromaControl}` : ''}</div><div class="harmony-add-controls"><select data-wheel-add-kind aria-label="New palette color kind"><option value="chromatic">Chromatic</option><option value="neutral">Neutral</option></select><button type="button" data-harmony-add>Add palette color</button></div><p class="font-note">Drag numbered markers radially, or select any palette color and edit its ring. Overlapping markers remain selectable here. The grid shows Primary weighting; role/area-weighted results may differ. Grid colors are not contrast-optimized. Text/Background contrast is reported below.</p></section>`;
    },
  };
}
