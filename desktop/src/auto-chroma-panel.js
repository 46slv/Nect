import { analyzePaletteContrast } from './palette-contrast.js';
import { proposeAutoChroma } from './auto-chroma.js';
import { wheelPoint } from './harmony-wheel.js';
import { createHarmonyWheelPanel } from './harmony-wheel-panel.js';
import { createColorDraftScrub } from './color-draft-scrub.js';
import { createLiveColorEdit } from './live-color-edit.js';
import { createColorRangeEdit } from './color-range-edit.js';
const esc = (v) =>
  String(v).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
function contrastHTML(doc, mode) {
  const report = analyzePaletteContrast(doc);
  const names = new Map((doc.colorSlots || []).map((slot) => [slot.id, slot.name]));
  const labels = {
    pass: 'Pairs meet 4.5:1',
    fail: 'Low contrast',
    unknown: 'Unknown',
    'not-applicable': 'No text/background pair',
  };
  const counts = ['pass', 'fail', 'unknown']
    .map((status) => `${report.pairs.filter((pair) => pair.status === status).length} ${status}`)
    .join(', ');
  const rows = [...report.pairs]
    .sort(
      (a, b) =>
        ({ unknown: 0, fail: 1, pass: 2 })[a.status] - { unknown: 0, fail: 1, pass: 2 }[b.status],
    )
    .slice(0, 16);
  return `<section class="font-note" data-palette-contrast="${mode}" data-contrast-status="${report.status}" aria-label="${mode === 'preview' ? 'Proposed' : 'Saved'} palette contrast"><strong>${mode === 'preview' ? 'Proposed' : 'Saved'} normal-text contrast: ${labels[report.status]}</strong><p>Text × Background candidates, 4.5:1. Not an artwork or whole-WCAG audit. No colors are changed.</p>${report.pairs.length ? `<p>${counts}</p>` : ''}${rows.map((pair) => `<p data-contrast-text-id="${esc(pair.textId)}" data-contrast-background-id="${esc(pair.backgroundId)}" data-contrast-status="${pair.status}">${esc(names.get(pair.textId) || pair.textId)} / ${esc(names.get(pair.backgroundId) || pair.backgroundId)}: ${pair.ratio === null ? 'unknown' : `${pair.ratio.toFixed(2)}:1`} · ${pair.status}${pair.reason ? ` (${esc(pair.reason)})` : ''}</p>`).join('')}${report.pairs.length > 16 ? `<p>Showing 16 of ${report.pairs.length} pairs; counts include all pairs.</p>` : ''}${report.reason ? `<p>${esc(report.reason)}</p>` : ''}</section>`;
}
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
  getCoverageSnapshot = () => null,
  dispatch,
  render,
  notify,
  envelope,
  live = false,
  renderPreview = () => {},
}) {
  let sessionId,
    savedDesignKey,
    preview = null,
    literalEdit = null,
    profile = 'balanced',
    strength = 0.72,
    spread = 0.62,
    advancedOpen = false,
    areaAware = true;
  const locked = new Set(),
    slots = () => getDocument().colorSlots || [];
  const harmony = createHarmonyWheelPanel({
    app,
    envelope,
    render,
    notify,
    live,
    isEditing: () => liveEdit.active,
    onCommit: () => {
      if (live) liveEdit.commit();
    },
    onCancel: () => {
      if (live) liveEdit.cancel();
    },
    getState: () => getSession().state,
    getSlotChroma: (id) => {
      const slot = slots().find((slot) => slot.id === id);
      if (!slot) throw Error('Palette color is no longer available');
      return colorChromaCoordinates(slot.value.hex).C;
    },
    onEdit: () => {
      preview = null;
      if (live && (draftChanged() || liveEdit.active)) liveEdit.schedule();
    },
  });
  const draftDesign = () => ({
    version: harmony.design ? 2 : 1,
    profile,
    colorfulness: strength,
    ...(spread !== 0.62 || Object.hasOwn(savedDesign(), 'spread') ? { spread } : {}),
    areaAware,
    lockedSlotIds: slots()
      .filter((slot) => locked.has(slot.id))
      .map((slot) => slot.id),
    ...(harmony.design || {}),
  });
  const savedDesign = () =>
    getDocument().colorDesign || {
      version: 1,
      profile: 'balanced',
      colorfulness: 0.72,
      areaAware: true,
      lockedSlotIds: [],
    };
  const savedDraft = () => {
    const saved = savedDesign();
    return {
      version: saved.version,
      profile: saved.profile,
      colorfulness: saved.colorfulness,
      ...(Object.hasOwn(saved, 'spread') ? { spread: saved.spread } : {}),
      areaAware: saved.areaAware,
      lockedSlotIds: slots()
        .filter((slot) => saved.lockedSlotIds.includes(slot.id))
        .map((slot) => slot.id),
      ...(saved.version === 2 ? { wheel: saved.wheel, palette: saved.palette } : {}),
    };
  };
  const draftChanged = () => JSON.stringify(draftDesign()) !== JSON.stringify(savedDraft());
  const requireSavedDraft = () => {
    synchronize();
    if (draftChanged())
      throw Error(
        'Preview and Apply your palette/settings changes before adding colors, or reload saved settings.',
      );
  };
  const key = () =>
    JSON.stringify([
      getSession().state.sessionId,
      getSession().state.expectedRevision,
      slots(),
      getDocument().colorDesign,
    ]);
  const previewCurrent = () =>
    preview &&
    preview.key === key() &&
    (preview.coverageKey === null || getCoverageSnapshot()?.key === preview.coverageKey);
  function synchronize() {
    const id = getSession().state.sessionId;
    const design = getDocument().colorDesign;
    const designKey = JSON.stringify(design ?? null);
    if (sessionId !== id || savedDesignKey !== designKey) {
      strengthScrub.stop(false);
      rangeEdit.stop(false);
      savedDesignKey = designKey;
      sessionId = id;
      locked.clear();
      preview = null;
      profile = design?.profile ?? 'balanced';
      strength = design?.colorfulness ?? 0.72;
      spread = design?.spread ?? 0.62;
      areaAware = design?.areaAware ?? true;
      for (const id of design?.lockedSlotIds || []) locked.add(id);
    }
    harmony.synchronize(design, id);
    if (preview && !previewCurrent()) preview = null;
  }
  function buildProposal() {
    const recipe = harmony.design;
    const entries = new Map((recipe?.palette || []).map((item) => [item.id, item]));
    const descriptors = slots().map((slot) => {
      const entry = entries.get(slot.id);
      if (entry) {
        const coordinates =
          entry.kind === 'neutral'
            ? { L: entry.L, h: 0, overrideC: 0 }
            : wheelPoint(recipe.wheel, entry.position, entry.ring);
        return {
          id: slot.id,
          L: coordinates.L,
          h: coordinates.h,
          role: entry.role,
          alpha: slot.value.alpha,
          ...(entry.kind === 'neutral'
            ? { overrideC: 0 }
            : entry.overrideC === undefined
              ? {}
              : { overrideC: entry.overrideC }),
          ...(locked.has(slot.id) ? { anchor: { ...slot.value } } : {}),
        };
      }
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
    const coverage = areaAware ? getCoverageSnapshot() : null;
    if (coverage && (typeof coverage.key !== 'string' || !coverage.key))
      throw Error('Invalid current coverage snapshot');
    return {
      key: key(),
      coverageKey: coverage?.key ?? null,
      areaNote: areaAware
        ? coverage
          ? 'Uses current visible area as a soft weight.'
          : 'No current area measurement; preview is unweighted.'
        : 'Visible-area weighting is off.',
      rows: proposeAutoChroma(
        descriptors,
        { profile, colorfulness: strength, spread, areaAware },
        coverage?.value ?? null,
      ),
    };
  }
  const liveEdit = createLiveColorEdit({
    app,
    getSession,
    getDocument,
    notify,
    proposal: () => {
      if (!literalEdit && !draftChanged())
        return {
          updates: slots().map((slot) => ({ slotId: slot.id, value: { ...slot.value } })),
        };
      if (literalEdit)
        return {
          updates: slots().map((slot) => ({
            slotId: slot.id,
            value: { ...(slot.id === literalEdit.id ? literalEdit.value : slot.value) },
          })),
        };
      preview = buildProposal();
      return {
        updates: preview.rows.map((row) => ({ slotId: row.id, value: { ...row.value } })),
        design: draftDesign(),
      };
    },
    onPreview: renderPreview,
    onSettled: () => {
      literalEdit = null;
      preview = null;
      savedDesignKey = undefined;
      harmony.synchronize(null, undefined);
      synchronize();
    },
  });
  const strengthScrub = createColorDraftScrub({
    app,
    selector:
      'input[data-color-scrub-step][data-chroma-strength],input[data-color-scrub-step][data-chroma-spread]',
    envelope,
    getState: () => getSession().state,
    render,
    snapshot: () => ({ strength, spread }),
    restore: (before) => {
      strength = before.strength;
      spread = before.spread;
    },
    update: (input, value) => {
      if (input.hasAttribute('data-chroma-spread')) spread = value;
      else strength = value;
    },
    onEdit: () => {
      preview = null;
      if (live) liveEdit.schedule();
    },
    onCommit: () => {
      if (live) liveEdit.commit();
    },
    onCancel: () => {
      if (live) liveEdit.cancel();
    },
  });
  const rangeEdit = createColorRangeEdit({
    app,
    selector: '[data-chroma-strength-range],[data-chroma-spread-range]',
    snapshot: () => ({ strength, spread }),
    restore: (before) => {
      strength = before.strength;
      spread = before.spread;
    },
    onCommit: () => {
      if (live) liveEdit.commit();
    },
    onCancel: () => {
      if (live) liveEdit.cancel();
    },
    render,
  });
  app.addEventListener(
    'toggle',
    (event) => {
      if (event.target.matches?.('[data-color-advanced]')) advancedOpen = event.target.open;
    },
    true,
  );
  const reset = () => {
    preview = null;
    harmony.cancelGesture();
    strengthScrub.stop();
    rangeEdit.stop();
    if (live) liveEdit.cancel();
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
      '[data-chroma-preview],[data-chroma-apply],[data-chroma-cancel],[data-harmony-create],[data-harmony-add],[data-chroma-reload],[data-chroma-clear-locks]',
    );
    if (!button) return;
    event.stopImmediatePropagation();
    guard(() => {
      const state = envelope(button);
      const focused = app.ownerDocument.activeElement === button;
      const returnFocusToPreview = () => {
        render();
        if (focused) app.querySelector('[data-chroma-preview]')?.focus({ preventScroll: true });
      };
      if (button.hasAttribute('data-chroma-clear-locks')) {
        locked.clear();
        liveEdit.schedule();
        liveEdit.commit();
        render();
        return;
      }
      if (button.hasAttribute('data-chroma-reload')) {
        savedDesignKey = undefined;
        harmony.cancelGesture();
        harmony.synchronize(null, undefined);
        synchronize();
        render();
        return;
      }
      if (button.hasAttribute('data-harmony-create') || button.hasAttribute('data-harmony-add')) {
        requireSavedDraft();
        if (button.hasAttribute('data-harmony-create'))
          dispatch('create-harmony-palette', {}, state);
        else {
          if (!harmony.design) throw Error('Create a harmony palette first');
          const kind = app.querySelector('[data-wheel-add-kind]')?.value;
          if (!['chromatic', 'neutral'].includes(kind)) throw Error('Choose a palette color kind');
          dispatch('add-harmony-color', { kind }, state);
        }
        reset();
        render();
        return;
      }
      if (button.hasAttribute('data-chroma-cancel')) {
        reset();
        returnFocusToPreview();
        return;
      }
      if (button.hasAttribute('data-chroma-apply')) {
        if (!previewCurrent()) throw Error('Color preview is stale; preview again');
        const updates = preview.rows.map((row) => ({ slotId: row.id, value: { ...row.value } }));
        const design = draftDesign();
        reset();
        dispatch('set-color-slots', { updates, design }, state);
        returnFocusToPreview();
        return;
      }
      synchronize();
      if (!slots().length) throw Error('Add a shared color first');
      preview = buildProposal();
      returnFocusToPreview();
    });
  });
  function strengthInput(event) {
    const el = event.target;
    if (
      !live ||
      event.isComposing ||
      !el.matches?.(
        '[data-chroma-strength],[data-chroma-spread],[data-chroma-strength-range],[data-chroma-spread-range]',
      )
    )
      return;
    event.stopImmediatePropagation();
    try {
      envelope(el);
      const value = Number(el.value);
      if (el.matches('[data-chroma-strength-range],[data-chroma-spread-range]'))
        rangeEdit.restoreBaseline();
      if (!el.value.trim() || !Number.isFinite(value) || value < 0 || value > 1) return;
      if (el.hasAttribute('data-chroma-spread') || el.hasAttribute('data-chroma-spread-range'))
        spread = value;
      else strength = value;
      liveEdit.schedule();
      render();
    } catch {
      /* Do not publish incomplete text or an IME composition. */
    }
  }
  app.addEventListener('input', strengthInput);
  app.addEventListener('compositionend', strengthInput);
  app.addEventListener('change', (event) => {
    const el = event.target;
    if (
      !el.matches?.(
        '[data-chroma-profile],[data-chroma-strength],[data-chroma-spread],[data-chroma-strength-range],[data-chroma-spread-range],[data-chroma-lock],[data-chroma-area]',
      )
    )
      return;
    event.stopImmediatePropagation();
    if (el.matches?.('[data-chroma-strength-range],[data-chroma-spread-range]') && rangeEdit.active)
      return;
    if (
      live &&
      el.tagName === 'INPUT' &&
      el.type === 'number' &&
      el.value === el.defaultValue &&
      !liveEdit.active
    )
      return;
    guard(() => {
      envelope(el);
      synchronize();
      if (el.hasAttribute('data-chroma-area')) {
        areaAware = el.checked;
      } else if (el.hasAttribute('data-chroma-profile')) {
        if (!['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'].includes(el.value))
          throw Error('Invalid color profile');
        profile = el.value;
      } else if (
        el.matches(
          '[data-chroma-strength],[data-chroma-spread],[data-chroma-strength-range],[data-chroma-spread-range]',
        )
      ) {
        const v = Number(el.value);
        if (!el.value.trim() || !Number.isFinite(v) || v < 0 || v > 1)
          throw Error('Colorfulness must be between zero and one');
        if (el.hasAttribute('data-chroma-spread') || el.hasAttribute('data-chroma-spread-range'))
          spread = v;
        else strength = v;
      } else {
        const id = el.dataset.chromaLock;
        if (!slots().some((slot) => slot.id === id))
          throw Error('Shared color is no longer available');
        if (el.checked) locked.add(id);
        else locked.delete(id);
      }
      if (live) {
        liveEdit.schedule();
        liveEdit.commit();
      } else reset();
      render();
    });
  });
  return {
    reset,
    finishEdit() {
      if (live) liveEdit.commit();
    },
    cancelEdit() {
      if (live) reset();
    },
    get editing() {
      return live && liveEdit.active;
    },
    get previewDocument() {
      return live ? liveEdit.document : null;
    },
    metrics: () => liveEdit.metrics(),
    stageLiteral(id, value) {
      if (!live) return;
      literalEdit = { id, value: { ...value } };
      liveEdit.schedule();
    },
    html({ savedColors = '' } = {}) {
      synchronize();
      if (!slots().length) return '';
      const names = new Map(slots().map((slot) => [slot.id, slot]));
      const displayedSlots = (liveEdit.document?.colorSlots || slots()).map((slot) => ({
        ...slot,
        value: preview?.rows.find((row) => row.id === slot.id)?.value || slot.value,
      }));
      const state = live
        ? liveEdit.active
          ? 'editing'
          : 'saved'
        : preview
          ? 'preview'
          : draftChanged()
            ? 'draft'
            : 'saved';
      const status = {
        saved: live ? 'Colors' : 'Saved palette settings',
        editing: 'Live colors',
        draft: 'Draft · not applied',
        preview: 'Preview · not applied',
      };
      const note = {
        saved: live ? 'Colors update while editing.' : 'Edit settings, then Preview and Apply.',
        editing: 'Escape cancels this edit.',
        draft: 'Artwork still uses saved colors. Preview before applying.',
        preview: 'Artwork still uses saved colors. Apply to save this proposal.',
      };
      const actions = live
        ? `<div class="chroma-draft-bar chroma-live-status" aria-label="Live palette"><strong role="status" data-chroma-state="${state}">${status[state]}</strong></div>`
        : `<div class="chroma-draft-bar" aria-label="Palette draft actions"><div role="status" data-chroma-state="${state}" title="${note[state]}"><strong>${status[state]}</strong><p class="visually-hidden">${note[state]}</p></div><div class="chroma-draft-actions"><button type="button" data-chroma-preview aria-label="${harmony.design ? 'Preview palette' : 'Preview saturation'}">Preview</button>${preview ? '<button type="button" data-chroma-apply aria-label="Apply colors">Apply</button><button type="button" data-chroma-cancel aria-label="Cancel preview">Cancel</button>' : ''}</div></div>`;
      const wheelOptions = {
        slots: displayedSlots,
        profile,
        colorfulness: strength,
        spread,
        lockedSlotIds: [...locked],
        preview: live ? liveEdit.active : !!preview,
      };
      const entries = harmony.html({ ...wheelOptions, section: 'entry' });
      const locks = `<div class="chroma-locks">${slots()
        .map(
          (slot) =>
            `<label><input type="checkbox" data-chroma-lock="${esc(slot.id)}" ${locked.has(slot.id) ? 'checked' : ''}>Keep ${esc(slot.name)}</label>`,
        )
        .join('')}</div>`;
      const reports = `${contrastHTML(getDocument(), 'current')}<button type="button" data-chroma-reload ${live ? 'hidden' : ''}>Reload saved settings</button>${preview && !live ? `<div class="chroma-preview" aria-label="Proposed colors"><strong>Proposed colors · not applied</strong><p class="font-note" data-chroma-area-status>${esc(preview.areaNote)}</p>${contrastHTML({ ...getDocument(), colorDesign: draftDesign(), colorSlots: slots().map((slot) => ({ ...slot, value: preview.rows.find((row) => row.id === slot.id)?.value || slot.value })) }, 'preview')}${preview.rows.map((row) => `<div><span>${esc(names.get(row.id).name)}</span><span style="background:${esc(names.get(row.id).value.hex)}" class="chroma-swatch" aria-label="Current ${esc(names.get(row.id).value.hex)}"></span>→<span style="background:${esc(row.value.hex)}" class="chroma-swatch" aria-label="Proposed ${esc(row.value.hex)}"></span><output>${esc(row.value.hex)}</output>${row.gamutLimited ? '<span class="font-note" data-chroma-gamut-limited>Gamut limited</span>' : ''}</div>`).join('')}</div>` : ''}${live && liveEdit.document ? contrastHTML(liveEdit.document, 'preview') : ''}`;
      const extra = live
        ? `<details data-color-advanced class="color-advanced" ${advancedOpen ? 'open' : ''}><summary>Advanced</summary>${entries}<strong>Legacy fixed colors</strong>${locks}${locked.size ? '<button type="button" data-chroma-clear-locks>Clear legacy fixed colors</button>' : ''}${savedColors}${reports}</details>`
        : `${entries}${locks}<p class="font-note" ${live ? 'hidden' : ''}>${harmony.design ? 'Palette colors use wheel coordinates and roles; unmanaged colors preserve their current lightness and hue. Alpha and Keep literals are preserved.' : 'Preserves lightness, hue and alpha. Neutral colors stay neutral.'} Apply saves colors and these settings; Cancel leaves the project unchanged. Contrast below checks only explicitly assigned Text/Background candidates.</p>${reports}${savedColors}`;
      const globalSettings = `<strong>${live ? 'Chroma' : 'Auto Chroma'}</strong><label>Profile <select data-chroma-profile aria-label="Auto Chroma profile">${['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'].map((name) => `<option value="${name}" ${name === profile ? 'selected' : ''}>${name}</option>`).join('')}</select></label><label>Colorfulness <span class="color-parameter-value">${live ? `<input type="range" data-chroma-strength-range min="0" max="1" step="0.01" value="${strength}" aria-label="Colorfulness slider">` : ''}<input type="number" min="0" max="1" step="0.01" data-color-scrub-step="0.01" title="Drag horizontally to adjust; click to type" data-chroma-strength value="${strength}" aria-label="Auto Chroma colorfulness"></span></label>${live ? `<label>Chroma Spread <span class="color-parameter-value"><input type="range" data-chroma-spread-range min="0" max="1" step="0.01" value="${spread}" aria-label="Chroma Spread slider"><input type="number" min="0" max="1" step="0.01" data-color-scrub-step="0.01" title="Drag horizontally to adjust; click to type" data-chroma-spread value="${spread}" aria-label="Chroma Spread"></span></label>` : ''}<label><span>Use visible area</span><input type="checkbox" data-chroma-area ${areaAware ? 'checked' : ''}></label>`;
      return `<section class="auto-chroma-controls" aria-label="Auto Chroma">${actions}${harmony.html({ ...wheelOptions, section: 'visual' })}<div class="chroma-settings-scroll" tabindex="0" aria-label="Palette parameters">${harmony.html({ ...wheelOptions, section: 'settings' })}${globalSettings}${extra}</div></section>`;
    },
  };
}
