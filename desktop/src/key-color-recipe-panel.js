import {
  restoreKeyColorRecipe,
  editKeyColorRecipe,
  createKeyColorRecipe,
  isNeutralTextPolicySlot,
  KEY_COLOR_NEUTRAL_SOLVER_VERSION,
} from './key-color-recipe.js';
import {
  prepareKeyColorRecipeApplication,
  prepareKeyColorRecipeInitialization,
  prepareLiveKeyColorRecipeApplication,
  isLiveKeyColorRecipe,
} from './key-color-recipe-document.js';
import { uid } from './core.js';
import { createKeyColorC1Draft } from './key-color-c1-draft.js';
import { createKeyColorDeltaScrub } from './key-color-delta-scrub.js';
import { researchPlans } from './key-color-research-plans.js';
import { analyzeSelectedContrast } from './palette-contrast.js';
import { compareModelLightnessOrder } from './key-color-lightness-order.js';
import { displayColorLabels } from './color-display-labels.js';

const esc = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
const literal = (value) =>
  `<span class="key-color-swatch" style="background:${esc(value.hex)};opacity:${value.alpha}" aria-hidden="true"></span><code>${esc(value.hex)}</code> · α ${value.alpha}`;
const trialB = (preset, version) => preset === 'research-neutral-start' && version === 1;
const roleLabels = (snapshot) =>
  displayColorLabels(snapshot.resolved.map(({ id, role }) => ({ id, label: role })));
const approximateL = (value) => `≈ ${Number(value.toFixed(3))}`;
const hideDelta = (snapshot, slot) =>
  isNeutralTextPolicySlot(snapshot.solverVersion, slot.role) ||
  (snapshot.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION &&
    trialB(snapshot.intent.preset.id, snapshot.intent.preset.version) &&
    !['Assort', 'Accent'].includes(slot.role));

// Exact encoded gray follows the existing solver's achromatic branch. No
// threshold or perceptual claim is implied for colors close to gray.
const isExactGray = (hex) => {
  const rgb = hex.slice(1).toLowerCase();
  return rgb.slice(0, 2) === rgb.slice(2, 4) && rgb.slice(2, 4) === rgb.slice(4, 6);
};

function lightnessOrderSummary(snapshot) {
  const pairs = compareModelLightnessOrder(snapshot);
  if (!pairs) return '<p data-model-l-order>Model L order unavailable: invalid metadata.</p>';
  if (!pairs.length) return '<p data-model-l-order>No active derived pair to compare.</p>';
  const labels = roleLabels(snapshot);
  const reversed = pairs.filter((pair) => pair.state === 'reversed').length;
  const shown = pairs
    .slice(0, 30)
    .map(
      ({ a, b, state }) =>
        `<li>${esc(labels.get(a.id))} / ${esc(labels.get(b.id))}: ${esc(state)}</li>`,
    )
    .join('');
  return `<details data-model-l-order><summary>Derived model L order${reversed ? ` · ${reversed} reversed` : ''}</summary><p>Exact comparison with the clamped zero-offset baseline; appearance and contrast are not evaluated.</p><ul>${shown}</ul>${pairs.length > 30 ? `<p>${pairs.length - 30} more pairs omitted.</p>` : ''}</details>`;
}

// An opt-in, detached view draft, not another authored palette owner. The
// historical receipt is read once; only one explicit Session Apply can write.
export function createKeyColorRecipePanel({
  document,
  getSession,
  getDocument,
  getCoverageSnapshot = () => null,
  onArtworkPreview = () => {},
  dispatch,
  notify,
}) {
  let dialog = null,
    trigger = null,
    owner = null,
    envelope = null,
    base = null,
    preview = null,
    artworkProjection = null,
    artworkComposition = null,
    c1Draft = null,
    deltaScrub = null,
    research = false,
    researchComposition = null,
    researchIds = null,
    initialInputKey = null,
    stale = false,
    applying = false;
  const composing = new Set();

  function close(restoreFocus = true) {
    const oldTrigger = trigger;
    const old = dialog;
    const hadArtwork = !!artworkProjection;
    artworkProjection = null;
    artworkComposition = null;
    deltaScrub?.dispose();
    deltaScrub = null;
    c1Draft?.dispose();
    c1Draft = null;
    dialog = null;
    trigger = null;
    owner = null;
    envelope = null;
    base = null;
    preview = null;
    research = false;
    researchComposition = null;
    researchIds = null;
    initialInputKey = null;
    composing.clear();
    if (old?.open) old.close();
    old?.remove();
    if (hadArtwork) onArtworkPreview();
    if (restoreFocus)
      (oldTrigger?.isConnected
        ? oldTrigger
        : document.querySelector('[data-key-color-recipe-open]')
      )?.focus({ preventScroll: true });
  }

  function current() {
    const session = getSession(),
      state = session.state;
    if (
      owner !== session ||
      state.sessionId !== envelope.sessionId ||
      state.expectedRevision !== envelope.expectedRevision ||
      (research && session.activeCompositionId !== researchComposition) ||
      (base &&
        trialB(base.intent.preset.id, base.intent.preset.version) &&
        base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION &&
        session.activeCompositionId !== artworkComposition)
    )
      stale = true; // Never rebase, even if the user later undoes the intervening edit.
    if (stale)
      throw Error('This Key color draft is stale. Cancel and reopen it from the current document.');
  }

  function readInputs() {
    if (composing.size) throw Error('Finish composing the input before Preview.');
    const inputs = [...dialog.querySelectorAll('[data-key-color-input]')];
    let error;
    const values = inputs.map((input) => {
      const raw = input.value,
        kind = input.dataset.keyColorInput,
        value = kind === 'hex' ? raw : Number(raw),
        valid =
          kind === 'hex'
            ? /^#[0-9a-f]{6}$/i.test(raw)
            : !!raw.trim() &&
              Number.isFinite(value) &&
              (kind !== 'alpha' || (value >= 0 && value <= 1)) &&
              (!kind.startsWith('hsl-') || (value >= 0 && value <= (kind === 'hsl-h' ? 360 : 100)));
      input.setAttribute('aria-invalid', String(!valid));
      if (!valid && !error)
        error =
          kind === 'hex'
            ? 'Enter C1 as a six-digit HEX literal, including #.'
            : kind === 'alpha'
              ? 'Enter C1 alpha between zero and one (zero is preserved).'
              : kind.startsWith('hsl-')
                ? 'Enter H between 0 and 360, and S/L between 0 and 100.'
                : 'Enter a finite value in every active ΔL and Δr field.';
      return { kind, slotId: input.dataset.keyColorSlot, raw, value };
    });
    if (error) throw Error(error);
    if (
      base &&
      base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION &&
      trialB(base.intent.preset.id, base.intent.preset.version) &&
      isExactGray(values.find((input) => input.kind === 'hex').value)
    ) {
      const hasChromaIntent = base.intent.preset.slots.some((plan) => {
        if (isNeutralTextPolicySlot(base.solverVersion, plan.role)) return false;
        const stored = base.intent.slots.find((slot) => slot.id === plan.id);
        const input = values.find(
          (item) => item.kind === 'deltaRatioCmax' && item.slotId === plan.id,
        );
        return (input?.value ?? stored.deltaRatioCmax) !== 0 || plan.chromaOffset !== 0;
      });
      if (hasChromaIntent)
        throw Error(
          'C1 is gray and has no defined hue. Restore a colored C1 or clear active chroma intent before Preview.',
        );
    }
    return {
      values,
      key: inputKey(),
    };
  }

  function inputKey() {
    return JSON.stringify([
      ...[...dialog.querySelectorAll('[data-key-color-input]')].map((input) => [
        input.dataset.keyColorInput,
        input.dataset.keyColorSlot,
        input.value,
      ]),
      ['hue-slider', dialog.querySelector('[data-key-color-hue]')?.value],
    ]);
  }

  function canUnlink() {
    current();
    return (
      isLiveKeyColorRecipe(getDocument()) &&
      initialInputKey !== null &&
      !composing.size &&
      !c1Draft?.active &&
      !deltaScrub?.active &&
      !preview &&
      !artworkProjection &&
      inputKey() === initialInputKey
    );
  }

  function updateUnlink() {
    const button = dialog?.querySelector('[data-key-color-unlink]');
    if (!button) return;
    let enabled = false;
    try {
      enabled = canUnlink();
    } catch {
      // A stale Session must be reopened, never silently rebased.
    }
    button.disabled = !enabled;
    dialog.querySelector('[data-key-color-unlink-hint]').hidden = enabled;
  }

  function unlink() {
    try {
      if (!canUnlink()) throw Error('Close and reopen Key color to unlink the saved colors.');
      applying = true;
      const result = dispatch(
        'detach-live-key-color-recipe',
        { expectedPaletteRevision: base.revision },
        { ...envelope },
      );
      if (result == null || result === false) throw Error('Unlink was not accepted.');
      close();
      notify('Trial colors unlinked');
    } catch (error) {
      if (dialog) {
        updateUnlink();
        status(error.message, true);
      }
      notify(error.message, true);
    } finally {
      applying = false;
      reconcile();
    }
  }

  function candidate(values) {
    let next = restoreKeyColorRecipe(base);
    const value = {
      hex: values.find((input) => input.kind === 'hex').value,
      alpha: values.find((input) => input.kind === 'alpha').value,
    };
    if (
      value.hex !== base.intent.keyColor.value.hex ||
      value.alpha !== base.intent.keyColor.value.alpha
    )
      next = editKeyColorRecipe(next, {
        type: 'key-color',
        expectedRevision: next.revision,
        value,
      });
    for (const slot of base.intent.slots) {
      const delta = values.filter((input) => input.slotId === slot.id);
      if (!delta.length) continue; // Dormant intent/results are read-only, not regenerated.
      const deltaL = delta.find((input) => input.kind === 'deltaL').value,
        deltaRatioCmax =
          delta.find((input) => input.kind === 'deltaRatioCmax')?.value ?? slot.deltaRatioCmax;
      if (deltaL !== slot.deltaL || deltaRatioCmax !== slot.deltaRatioCmax)
        next = editKeyColorRecipe(next, {
          type: 'slot-delta',
          expectedRevision: next.revision,
          slotId: slot.id,
          deltaL,
          deltaRatioCmax,
        });
    }
    return next;
  }

  function prepare(snapshot) {
    const doc = getDocument();
    const slots = new Map((doc.colorSlots || []).map((slot) => [slot.id, slot]));
    return isLiveKeyColorRecipe(doc)
      ? prepareLiveKeyColorRecipeApplication(
          doc,
          { snapshot, expectedPaletteRevision: base.revision },
          slots,
        )
      : prepareKeyColorRecipeApplication(doc, { snapshot }, slots);
  }

  function status(message, error = false) {
    const output = dialog.querySelector('[data-key-color-status]');
    output.textContent = message;
    output.dataset.error = String(error);
  }

  function clearPairResult() {
    const result = dialog?.querySelector('[data-pair-result]');
    if (result) result.textContent = 'Not evaluated.';
  }

  function pairControls() {
    const active = new Set(base.intent.preset.slots.map((slot) => slot.id));
    const rows = [base.intent.keyColor, ...base.intent.slots.filter((slot) => active.has(slot.id))];
    const labels = roleLabels(base);
    const options = rows
      .map((row) => `<option value="${esc(row.id)}">${esc(labels.get(row.id))}</option>`)
      .join('');
    return `<section data-pair-check><h3>Selected pair contrast</h3><p>Normal-text 4.5:1 reference for these sRGB literals; opaque background required. Artwork is not evaluated.</p><label>Foreground <select data-pair-foreground><option value="">Choose a color</option>${options}</select></label><label>Background <select data-pair-background><option value="">Choose a color</option>${options}</select></label><button type="button" data-pair-run>Check pair</button><p data-pair-result role="status" aria-live="polite">Not evaluated.</p></section>`;
  }

  function checkPair() {
    if (!dialog || !base || !dialog.isConnected) return;
    clearPairResult();
    try {
      current();
      if (composing.size) throw Error('Finish composing before checking the pair.');
      const foreground = dialog.querySelector('[data-pair-foreground]').value;
      const background = dialog.querySelector('[data-pair-background]').value;
      const slots = getDocument().colorSlots;
      const currentResult = analyzeSelectedContrast(slots, foreground, background);
      const reasons = {
        'select-two-colors': 'Choose both colors',
        'select-distinct-colors': 'Choose two different colors',
        'invalid-color-slots': 'Colors are unavailable',
        'duplicate-color-slot-id': 'Color ID is ambiguous',
        'missing-background-color': 'Background color is missing',
        'missing-text-color': 'Foreground color is missing',
        'invalid-background-color': 'Background color is invalid',
        'invalid-text-color': 'Foreground color is invalid',
        'non-opaque-background': 'Background must be opaque',
      };
      const describe = (name, result) =>
        result.ratio === null
          ? `${name}: unknown (${reasons[result.reason] || 'Unavailable'}).`
          : `${name}: ${result.ratio.toFixed(3)}:1 · ${result.status === 'pass' ? 'at least' : 'below'} 4.5:1 (unrounded comparison).`;
      let proposed = null;
      if (preview) {
        try {
          if (readInputs().key !== preview.key) throw Error('Draft changed');
          const invalidIds = [
            'select-two-colors',
            'select-distinct-colors',
            'invalid-color-slots',
            'duplicate-color-slot-id',
            'missing-background-color',
            'missing-text-color',
          ];
          if (!invalidIds.includes(currentResult.reason))
            proposed = analyzeSelectedContrast(
              preview.updates.map(({ slotId, value }) => ({ id: slotId, value })),
              foreground,
              background,
            );
        } catch {
          invalidate();
        }
      }
      dialog.querySelector('[data-pair-result]').textContent =
        `${describe('Current shared colors', currentResult)} ${proposed ? describe('Proposed colors', proposed) : 'Proposed colors: not evaluated.'}`;
    } catch (error) {
      if (stale) {
        clearPreview();
        dialog.querySelector('[data-key-color-preview]').disabled = true;
        status(error.message, true);
      }
      dialog.querySelector('[data-pair-result]').textContent = error.message;
    }
  }

  function trialStatus(snapshot, title, created) {
    const slots = new Map(snapshot.intent.slots.map((slot) => [slot.id, slot]));
    const labels = roleLabels(snapshot);
    const diagnosticLabels = {
      'achromatic-hue-hint': 'Achromatic hue hint',
      'support-chroma-intent-unapplied': 'Stored Δr not applied',
      'lightness-clamped': 'L range limited',
      'ratio-clamped': 'Chroma range limited',
      'zero-cmax': 'Zero maximum chroma',
      'gamut-limited': 'Gamut limited',
    };
    const rows = snapshot.resolved
      .map((row) => {
        const intent = slots.get(row.id);
        const equal = snapshot.resolved
          .filter(
            (other) =>
              other.id !== row.id &&
              other.value.hex.toLowerCase() === row.value.hex.toLowerCase() &&
              other.value.alpha === row.value.alpha,
          )
          .map((other) => labels.get(other.id));
        const notices = [
          row.active ? '' : 'Dormant',
          ...row.diagnostics.map((item) => diagnosticLabels[item] || item),
          equal.length ? `Same literal and alpha as ${equal.join(', ')}` : '',
        ]
          .filter(Boolean)
          .join(' · ');
        const exact = intent
          ? `ΔL ${intent.deltaL}; base OKLCH L ${row.baseL}; effective L ${row.L}`
          : `C1 OKLCH L ${row.L}; no ΔL`;
        const detail = `<details data-trial-details><summary aria-label="${esc(labels.get(row.id))} details">Details</summary><div>Slot ID <code>${esc(row.id)}</code></div><div>${esc(exact)}</div>${row.diagnostics.length ? `<div>Diagnostics ${esc(row.diagnostics.join(', '))}</div>` : ''}</details>`;
        const usage = created
          ? `<span data-trial-usage="${esc(row.id)}">Unavailable</span>`
          : 'Not measured';
        return `<tr data-trial-row="${esc(row.id)}" ${created ? '' : `data-research-row="${esc(row.id)}" data-research-role="${esc(row.role)}"`}><th scope="row">${esc(labels.get(row.id))}${notices ? `<div data-trial-notice>${esc(notices)}</div>` : ''}</th><td>${literal(row.value)}</td><td>${usage}</td><td><span data-trial-l>${esc(approximateL(row.L))}</span>${detail}</td></tr>`;
      })
      .join('');
    const order =
      created &&
      snapshot.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION &&
      trialB(snapshot.intent.preset.id, snapshot.intent.preset.version)
        ? lightnessOrderSummary(snapshot)
        : '';
    const diagnostics = snapshot.diagnostics.length
      ? `<p>${esc(snapshot.diagnostics.map((item) => (item === 'coverage-unavailable-unweighted' ? 'Visible-area weighting was not applied.' : item)).join(' '))}</p><details><summary>Diagnostics</summary><code>${esc(snapshot.diagnostics.join(', '))}</code></details>`
      : '';
    return `<section data-trial-status="${esc(title)}"><h3>${esc(title)}</h3><div class="key-color-table-scroll"><table><thead><tr><th scope="col">Role</th><th scope="col">Color</th><th scope="col">Saved visible share</th><th scope="col">OKLCH L</th></tr></thead><tbody>${rows}</tbody></table></div><p>Readability and procedure not evaluated.${created ? '' : ' Usage is not measured before creation.'}</p>${diagnostics}${order}</section>`;
  }

  function refreshTrialUsage() {
    if (!dialog) return;
    let snapshot = null;
    try {
      current();
      snapshot = getCoverageSnapshot();
    } catch {
      /* unavailable or stale */
    }
    if (stale && base) {
      clearPreview();
      dialog.querySelector('[data-key-color-preview]').disabled = true;
    }
    for (const output of dialog.querySelectorAll('[data-trial-usage]')) {
      const rows = snapshot?.value?.slots;
      const row = Array.isArray(rows)
        ? rows.find((slot) => slot.id === output.dataset.trialUsage)
        : null;
      output.textContent =
        Number.isFinite(row?.percent) && row.percent >= 0 && row.percent <= 100
          ? `≈ ${row.percent}%`
          : 'Unavailable';
    }
  }

  function clearArtworkProjection() {
    if (!artworkProjection) return;
    artworkProjection = null;
    dialog?.querySelector('[data-artwork-show]')?.setAttribute('aria-pressed', 'false');
    onArtworkPreview();
  }

  function restoreArtwork() {
    if (!artworkProjection) return;
    clearArtworkProjection();
    if (preview) status('Artwork preview off.');
  }

  function showArtworkProjection() {
    try {
      current();
      if (
        !preview ||
        !base ||
        owner.activeCompositionId !== artworkComposition ||
        !trialB(base.intent.preset.id, base.intent.preset.version) ||
        base.solverVersion !== KEY_COLOR_NEUTRAL_SOLVER_VERSION ||
        c1Draft?.active ||
        deltaScrub?.active ||
        readInputs().key !== preview.key
      )
        throw Error('Preview the current inputs before showing artwork.');
      const prepared = prepare(preview.snapshot);
      const projected = structuredClone(getDocument());
      const updates = new Map(prepared.updates.map(({ slotId, value }) => [slotId, value]));
      projected.colorSlots = projected.colorSlots.map((slot) =>
        updates.has(slot.id) ? { ...slot, value: structuredClone(updates.get(slot.id)) } : slot,
      );
      if (isLiveKeyColorRecipe(projected))
        projected.keyColorRecipe.committedSnapshot = structuredClone(prepared.snapshot);
      artworkProjection = projected;
      dialog.querySelector('[data-artwork-show]').setAttribute('aria-pressed', 'true');
      onArtworkPreview();
      status('Artwork preview. Apply to save.');
    } catch (error) {
      clearPreview();
      if (validateDraft()) status(error.message, true);
    }
  }

  function clearPreview() {
    clearArtworkProjection();
    clearPairResult();
    preview = null;
    dialog.querySelector('[data-key-color-comparison]').innerHTML = '';
    dialog.querySelector('[data-key-color-apply]').disabled = true;
    updateUnlink();
  }

  function researchInputs() {
    current();
    if (composing.size) throw Error('Finish composing before Preview.');
    const hex = dialog.querySelector('[data-research-hex]').value;
    const rawAlpha = dialog.querySelector('[data-research-alpha]').value;
    const alpha = Number(rawAlpha);
    const id = dialog.querySelector('[data-research-plan]').value;
    const plan = researchPlans.find((item) => item.id === id);
    if (!/^#[0-9a-f]{6}$/i.test(hex)) throw Error('Enter C1 as six-digit HEX.');
    if (!rawAlpha.trim() || !Number.isFinite(alpha) || alpha < 0 || alpha > 1)
      throw Error('Enter alpha between 0 and 1.');
    for (const channel of ['h', 's', 'l']) {
      const raw = dialog.querySelector(`[data-key-color-input="hsl-${channel}"]`).value;
      const number = Number(raw);
      if (
        !raw.trim() ||
        !Number.isFinite(number) ||
        number < 0 ||
        number > (channel === 'h' ? 360 : 100)
      )
        throw Error('Enter valid C1 HSL values.');
    }
    if (!plan) throw Error('Choose a research candidate.');
    const key = JSON.stringify([
      hex,
      rawAlpha,
      id,
      ...['h', 's', 'l'].map(
        (channel) => dialog.querySelector(`[data-key-color-input="hsl-${channel}"]`).value,
      ),
      dialog.querySelector('[data-key-color-hue]').value,
    ]);
    return { hex, alpha, plan, key };
  }

  function trialRequest({ hex, alpha, plan }, allocate = false) {
    if (!trialB(plan.id, plan.version)) throw Error('Only B is available for trial creation.');
    if (!researchIds && allocate)
      researchIds = Object.fromEntries(
        ['palette', 'key', 'assort', 'accent', 'neutral', 'text'].map((id) => [
          id,
          uid('key-color'),
        ]),
      );
    if (!researchIds) throw Error('Preview the current candidate first.');
    const slots = plan.slots.map((slot) => ({ ...slot, id: researchIds[slot.id] }));
    return {
      version: 1,
      modelVersion: 2,
      solverVersion: KEY_COLOR_NEUTRAL_SOLVER_VERSION,
      colorSemanticsVersion: 'linear-srgb-straight-v1',
      paletteId: researchIds.palette,
      intent: {
        keyColor: { id: researchIds.key, role: 'Identity', value: { hex, alpha }, hueHint: 0 },
        preset: { ...plan, slots },
        slots: slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
        settings: { areaAware: true },
      },
      allocations: [
        { slotId: researchIds.key, name: 'C1' },
        ...slots.map(({ id, role }) => ({ slotId: id, name: role })),
      ],
    };
  }
  function prepareTrial(payload) {
    const doc = getDocument();
    return prepareKeyColorRecipeInitialization(
      doc,
      payload,
      new Map((doc.colorSlots || []).map((slot) => [slot.id, slot])),
    );
  }

  function researchInvalidate() {
    if (!dialog || !research) return;
    preview = null;
    dialog.querySelector('[data-research-create]').disabled = true;
    dialog.querySelector('[data-research-results]').replaceChildren();
    const selected = researchPlans.find(
      (item) => item.id === dialog.querySelector('[data-research-plan]').value,
    );
    const linkOption = dialog.querySelector('[data-research-live-option]');
    linkOption.hidden = !selected || !trialB(selected.id, selected.version);
    if (linkOption.hidden) dialog.querySelector('[data-research-live]').checked = false;
    dialog.querySelector('[data-research-live-warning]').hidden =
      linkOption.hidden || !dialog.querySelector('[data-research-live]').checked;
    dialog.querySelector('[data-research-details]').textContent = selected
      ? JSON.stringify(
          {
            note: 'Candidate labels; IDs are assigned only to a B preview draft.',
            key: { id: 'key', role: 'Identity' },
            preset: selected,
            slots: selected.slots.map(({ id, role }) => ({
              id,
              role,
              deltaL: 0,
              deltaRatioCmax: 0,
            })),
            settings: { areaAware: true },
            creationPolicy: trialB(selected.id, selected.version)
              ? KEY_COLOR_NEUTRAL_SOLVER_VERSION
              : 'preview-only',
          },
          null,
          2,
        )
      : '';
    try {
      researchInputs();
      dialog.querySelector('[data-research-preview]').disabled = false;
      status('Ready to preview.');
    } catch (error) {
      dialog.querySelector('[data-research-preview]').disabled = true;
      status(error.message, true);
    }
  }

  function showResearchPreview() {
    researchInvalidate();
    try {
      const inputs = researchInputs();
      const { hex, alpha, plan } = inputs;
      const payload = trialB(plan.id, plan.version) ? trialRequest(inputs, true) : null;
      const result = payload
        ? prepareTrial(payload).snapshot
        : createKeyColorRecipe({
            paletteId: 'synthetic-only',
            keyColor: { id: 'key', role: 'Identity', value: { hex, alpha }, hueHint: 0 },
            preset: plan,
            slots: plan.slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
            settings: { areaAware: true },
          });
      dialog.querySelector('[data-research-results]').innerHTML = payload
        ? trialStatus(result, 'Trial preview · not created', false)
        : `<table><thead><tr><th>Role</th><th>Preview</th><th>Diagnostics</th></tr></thead><tbody>${result.resolved.map((row) => `<tr data-research-row="${esc(row.id)}" data-research-role="${esc(row.role)}"><th>${esc(row.role)}</th><td>${literal(row.value)}</td><td>${esc(row.diagnostics.join(', ') || '—')}</td></tr>`).join('')}</tbody></table><p>${esc(result.diagnostics.join(', '))}</p>`;
      if (payload) {
        preview = {
          key: inputs.key,
          payload: structuredClone(payload),
          serialized: JSON.stringify(payload),
        };
        dialog.querySelector('[data-research-details]').textContent = JSON.stringify(
          payload,
          null,
          2,
        );
        dialog.querySelector('[data-research-create]').disabled = false;
      }
      status('Research preview ready. No shared colors were changed.');
    } catch (error) {
      status(error.message, true);
    }
  }

  function createTrial() {
    try {
      const inputs = researchInputs();
      if (!preview?.payload || inputs.key !== preview.key)
        throw Error('Preview the current B inputs first.');
      const payload = trialRequest(inputs);
      if (JSON.stringify(payload) !== preview.serialized)
        throw Error('The trial plan changed. Preview again.');
      prepareTrial(payload);
      applying = true;
      const linked = dialog.querySelector('[data-research-live]').checked;
      const result = dispatch(
        linked ? 'initialize-live-key-color-recipe' : 'initialize-key-color-recipe',
        payload,
        { ...envelope },
      );
      if (result == null || result === false) throw Error('Trial colors were not created.');
      close();
      notify(linked ? 'Linked trial colors added' : 'Five trial colors added');
    } catch (error) {
      if (dialog) {
        researchInvalidate();
        status(error.message, true);
      }
      notify(error.message, true);
    } finally {
      applying = false;
      reconcile();
    }
  }

  function validateDraft() {
    if (!dialog || !base) return false;
    const button = dialog.querySelector('[data-key-color-preview]');
    try {
      current();
      readInputs();
      button.disabled = false;
      return true;
    } catch (error) {
      button.disabled = true;
      clearPreview();
      status(error.message, true);
      return false;
    }
  }

  function invalidate() {
    if (research) return researchInvalidate();
    if (!dialog || !base) return;
    clearPreview();
    if (validateDraft()) status('Draft only. Preview these inputs before Apply.');
  }

  function resetDeltaL(button) {
    if (!dialog || !base || !button.isConnected || !dialog.contains(button)) return;
    try {
      current();
      if (composing.size || c1Draft?.active || deltaScrub?.active)
        throw Error('Finish the active input before resetting ΔL.');
      const id = button.dataset.keyColorResetDeltaL;
      const input = button.closest('fieldset')?.querySelector('[data-key-color-input="deltaL"]');
      if (
        !base ||
        !trialB(base.intent.preset.id, base.intent.preset.version) ||
        base.solverVersion !== KEY_COLOR_NEUTRAL_SOLVER_VERSION ||
        !base.intent.preset.slots.some((slot) => slot.id === id) ||
        !dialog.contains(button) ||
        !input ||
        input.dataset.keyColorSlot !== id ||
        input.disabled ||
        input.readOnly
      )
        throw Error('This color is unavailable. Cancel and reopen the draft.');
      input.value = '0';
      invalidate();
    } catch (error) {
      if (stale) {
        clearPreview();
        dialog.querySelector('[data-key-color-preview]').disabled = true;
        refreshTrialUsage();
      }
      status(error.message, true);
    }
  }

  function showPreview() {
    clearPreview();
    if (!validateDraft()) return;
    try {
      const inputs = readInputs(),
        prepared = prepare(candidate(inputs.values)),
        slots = new Map(getDocument().colorSlots.map((slot) => [slot.id, slot]));
      preview = {
        snapshot: prepared.snapshot,
        updates: structuredClone(prepared.updates),
        key: inputs.key,
      };
      const isB =
        trialB(prepared.snapshot.intent.preset.id, prepared.snapshot.intent.preset.version) &&
        prepared.snapshot.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION;
      const labels = displayColorLabels(
        prepared.updates.map(({ slotId }) => ({ id: slotId, label: slots.get(slotId).name })),
      );
      const comparisonRows = prepared.updates
        .map(({ slotId, value }) => {
          const name = labels.get(slotId);
          const identity = isB
            ? `<details><summary aria-label="${esc(name)} slot ID">Details</summary><code>${esc(slotId)}</code></details>`
            : `<br><code>${esc(slotId)}</code>`;
          return `<tr data-key-color-compare="${esc(slotId)}"><th scope="row">${esc(isB ? name : slots.get(slotId).name)}${identity}</th><td data-key-color-before>${literal(slots.get(slotId).value)}</td><td data-key-color-after>${literal(value)}</td></tr>`;
        })
        .join('');
      dialog.querySelector('[data-key-color-comparison]').innerHTML =
        `<h3>${isB ? 'Shared colors' : 'Shared literals · before → after'}</h3><p>${isB ? 'Shared colors change on Apply.' : 'Only active rows below will change. Shared colors change on Apply.'}</p><table><thead><tr><th>${isB ? 'Color' : 'Stable slot'}</th><th>${isB ? 'Current' : 'Current shared literal'}</th><th>${isB ? 'Proposed' : 'Proposed shared literal'}</th></tr></thead><tbody>${comparisonRows}</tbody></table>`;
      if (isB) {
        dialog
          .querySelector('[data-key-color-comparison]')
          .insertAdjacentHTML(
            'beforeend',
            trialStatus(prepared.snapshot, 'Proposed colors · not applied', true),
          );
        refreshTrialUsage();
      }
      dialog.querySelector('[data-key-color-apply]').disabled = false;
      status(
        isLiveKeyColorRecipe(getDocument())
          ? 'Preview ready. Apply updates the linked trial colors.'
          : 'Preview ready. Apply writes these shared literals and the receipt together.',
      );
      // Preview is an explicit review step: bring its actual before/after
      // literals into the bounded scrollport, not only a below-the-fold heading.
      dialog.querySelector('[data-key-color-comparison]').scrollIntoView?.({ block: 'start' });
      updateUnlink();
    } catch (error) {
      clearPreview();
      status(error.message, true);
    }
  }

  function apply() {
    if (!validateDraft()) return;
    try {
      const inputs = readInputs();
      if (!preview || inputs.key !== preview.key) {
        invalidate();
        throw Error('Preview the current inputs before Apply.');
      }
      // Same preflight as the Session; no unlock, skipped row or alpha coercion.
      const { snapshot } = prepare(preview.snapshot);
      applying = true;
      const linked = isLiveKeyColorRecipe(getDocument());
      const result = dispatch(
        linked ? 'apply-live-key-color-recipe-snapshot' : 'apply-key-color-recipe-snapshot',
        linked ? { snapshot, expectedPaletteRevision: base.revision } : { snapshot },
        { ...envelope },
      );
      if (result == null || result === false)
        throw Error('Key color Apply was not accepted. No success was recorded.');
      close();
      notify('Key color Apply accepted');
    } catch (error) {
      if (dialog) {
        clearPreview();
        status(error.message, true);
      }
      notify(error.message, true);
    } finally {
      applying = false;
      reconcile();
    }
  }

  function reconcile() {
    if (applying) return;
    if (artworkProjection && owner.activeCompositionId !== artworkComposition) clearPreview();
    if (dialog && research) {
      try {
        current();
      } catch {
        researchInvalidate();
      }
      return;
    }
    if (!dialog || !base || applying) return;
    validateDraft();
    refreshTrialUsage();
    updateUnlink();
  }

  function retainedControls() {
    const key = base.intent.keyColor;
    const active = new Set(base.intent.preset.slots.map((slot) => slot.id));
    const isB =
      trialB(base.intent.preset.id, base.intent.preset.version) &&
      base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION;
    const labels = roleLabels(base);
    const label = (row) => (isB ? labels.get(row.id) : `${row.role} · ${row.id}`);
    const fields = base.intent.slots
      .map((slot) => {
        const resolved = base.resolved.find((row) => row.id === slot.id);
        if (!active.has(slot.id))
          return `<fieldset disabled data-key-color-dormant="${esc(slot.id)}"><legend>${esc(label(slot))} · dormant, unchanged</legend><p>ΔL ${slot.deltaL} · Δr ${slot.deltaRatioCmax}</p><p>${literal(resolved.value)}</p></fieldset>`;
        const reset = isB
          ? `<button type="button" data-key-color-reset-delta-l="${esc(slot.id)}" aria-label="Reset ${esc(label(slot))} ΔL to zero">Reset ΔL</button>`
          : '';
        const hiddenDelta =
          !isB || slot.deltaRatioCmax !== 0
            ? `<p data-key-color-support-intent>Stored Δr ${slot.deltaRatioCmax} · ${isNeutralTextPolicySlot(base.solverVersion, slot.role) ? 'not applied' : 'read-only'}${isNeutralTextPolicySlot(base.solverVersion, slot.role) && slot.deltaRatioCmax !== 0 ? ' · support-chroma-intent-unapplied' : ''}</p>`
            : '';
        const deltaR = hideDelta(base, slot)
          ? hiddenDelta
          : `<label>Δr<input type="number" step="any" data-key-color-input="deltaRatioCmax" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaRatioCmax}" aria-label="${esc(isB ? label(slot) : slot.id)} Δr"></label>`;
        const alpha =
          !isB || resolved.value.alpha !== 1
            ? `<p>Stored derived alpha · ${resolved.value.alpha} (read-only; Apply requires 1, never coerces).</p>`
            : '';
        return `<fieldset><legend>${esc(label(slot))}</legend><label>ΔL<input type="number" step="any" data-key-color-input="deltaL" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaL}" aria-label="${esc(isB ? label(slot) : slot.id)} ΔL"></label>${reset}${deltaR}${alpha}</fieldset>`;
      })
      .join('');
    const saved = isLiveKeyColorRecipe(getDocument())
      ? '<p>Linked trial palette.</p><button type="button" data-key-color-unlink disabled aria-describedby="key-color-unlink-hint">Unlink · keep colors</button><p id="key-color-unlink-hint" data-key-color-unlink-hint hidden>Close and reopen Key color to unlink the saved colors.</p>'
      : isB
        ? `<p>Saved recipe · revision ${base.revision}. Later shared-color edits do not update this snapshot.</p>`
        : `<p>Palette <code>${esc(base.paletteId)}</code> · retained revision ${base.revision}. This receipt is historical, not a live driver.</p>`;
    const details = isB
      ? {
          paletteId: base.paletteId,
          keyColor: key,
          preset: base.intent.preset,
          slots: base.intent.slots,
          settings: base.intent.settings,
        }
      : { preset: base.intent.preset, settings: base.intent.settings };
    return `${saved}<fieldset><legend>C1 · ${esc(isB ? label(key) : `${key.role} · ${key.id}`)}</legend><div data-key-color-c1></div></fieldset><h3>Active derived intent</h3><p>ΔL and Δr are finite stored offsets, not clamped slider positions. Derived alpha must be 1.</p>${fields}<details><summary>Read-only preset, coefficients and versions</summary><p>Recipe v${base.version} · solver ${esc(base.solverVersion)} · ${esc(base.colorSemanticsVersion)}</p><pre>${esc(JSON.stringify(details, null, 2))}</pre></details><p data-key-color-status role="status" aria-live="polite">Draft only. Preview these inputs before Apply.</p><section class="key-color-comparison" data-key-color-comparison aria-label="Key color preview"></section>`;
  }

  function open(anchor = null) {
    close(false);
    owner = getSession();
    envelope = { ...owner.state }; // Captured once, never refreshed at Preview/Apply.
    artworkComposition = owner.activeCompositionId;
    trigger = anchor;
    stale = false;
    let error = null;
    const saved = getDocument().keyColorRecipe?.committedSnapshot;
    research = !getDocument().keyColorRecipe;
    researchComposition = owner.activeCompositionId;
    if (saved)
      try {
        base = restoreKeyColorRecipe(saved);
      } catch (reason) {
        error = reason.message;
      }
    dialog = document.createElement('dialog');
    dialog.className = 'key-color-recipe-dialog';
    dialog.dataset.keyColorRecipePanel = '';
    dialog.setAttribute('aria-labelledby', 'key-color-recipe-title');
    const controls = base
      ? retainedControls()
      : research
        ? `<p data-key-color-empty>Research candidates. B adds five trial colors without changing existing colors, in one Undo. Coverage and readability are not evaluated; colors may coincide.</p><fieldset><legend>C1</legend><div data-key-color-c1></div></fieldset><label>Research candidate <select data-research-plan><option value="">Choose a candidate</option><option value="research-triadic-offset">A · chromatic comparison</option><option value="research-neutral-start">B · neutral start</option></select></label><label data-research-live-option hidden><input type="checkbox" data-research-live>Link trial colors to C1</label><p data-research-live-warning role="status" aria-live="polite" hidden>Older Nect versions cannot open this project after linking.</p><details><summary>Candidate roles and coefficients</summary><pre data-research-details></pre></details><p data-key-color-status role="status" aria-live="polite">Enter C1 and choose a candidate.</p><section data-research-results aria-label="Research color preview"></section>`
        : `<p data-key-color-empty>${esc(error || 'The retained recipe is unavailable.')}</p>`;
    const isB =
      base &&
      trialB(base.intent.preset.id, base.intent.preset.version) &&
      base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION;
    const linked = !!base && isLiveKeyColorRecipe(getDocument());
    dialog.innerHTML = `<header><h2 id="key-color-recipe-title">Key color · Experimental</h2><button type="button" data-key-color-cancel aria-label="Close Key color">×</button></header>${base && !linked ? (isB ? '<p>Trial palette.</p>' : '<p>Opt-in one-shot Apply. The existing legacy wheel and product defaults are unchanged. Dark TonePlan coefficients remain unresolved.</p>') : ''}<div class="key-color-recipe-body">${controls}</div><footer><button type="button" data-key-color-cancel>Cancel</button>${base ? '<button type="button" data-key-color-preview>Preview</button><button type="button" data-key-color-apply disabled>Apply · 1 Undo</button>' : research ? '<button type="button" data-research-preview disabled>Preview</button><button type="button" data-research-create disabled>Add 5 trial colors · 1 Undo</button>' : ''}</footer>${base && !isB ? '<p class="key-color-undo-note">A changed Apply is one Undo. Equal literals and receipt are a no-op. Cancel and Escape discard the draft.</p>' : ''}`;
    if (
      base &&
      trialB(base.intent.preset.id, base.intent.preset.version) &&
      base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION
    ) {
      const hueNote = document.createElement('p');
      hueNote.textContent =
        'Exact gray requires zero active chroma intent. Near-gray hue is not evaluated.';
      dialog.querySelector('.key-color-recipe-body').prepend(hueNote);
      dialog
        .querySelector('.key-color-recipe-body')
        .insertAdjacentHTML(
          'beforeend',
          trialStatus(base, 'Saved recipe', true) +
            '<button type="button" data-trial-refresh>Refresh usage</button>' +
            '<p>Existing bindings only.</p>' +
            '<button type="button" data-artwork-show aria-pressed="false">Show proposal on artwork</button><button type="button" data-artwork-restore>Show saved artwork</button>' +
            pairControls(),
        );
      refreshTrialUsage();
    }
    if (base || research) {
      const compositionId = owner.activeCompositionId;
      c1Draft = createKeyColorC1Draft(
        dialog.querySelector('[data-key-color-c1]'),
        base ? base.intent.keyColor.value : null,
        {
          allowEmpty: research,
          blocked: () => composing.size > 0 || !!deltaScrub?.active,
          valid: () => {
            try {
              current();
              return owner.activeCompositionId === compositionId && !composing.size;
            } catch {
              return false;
            }
          },
        },
      );
      if (research) {
        dialog.querySelector('[data-key-color-input="hex"]').dataset.researchHex = '';
        dialog.querySelector('[data-key-color-input="alpha"]').dataset.researchAlpha = '';
      }
      if (base)
        deltaScrub = createKeyColorDeltaScrub(dialog, {
          blocked: () => composing.size > 0 || !!c1Draft?.active,
          valid: () => {
            try {
              current();
              return owner.activeCompositionId === compositionId && !composing.size;
            } catch {
              return false;
            }
          },
        });
    }
    document.body.append(dialog);
    if (base) initialInputKey = inputKey();
    updateUnlink();
    const onInput = (event) => {
      if (event.target.matches?.('[data-pair-foreground], [data-pair-background]'))
        clearPairResult();
      else invalidate();
    };
    dialog.addEventListener('input', onInput);
    dialog.addEventListener('change', onInput);
    dialog.addEventListener('compositionstart', (event) => {
      composing.add(event.target);
      invalidate();
    });
    dialog.addEventListener('compositionend', (event) => {
      composing.delete(event.target);
      if (research) c1Draft?.settleComposition();
      invalidate();
    });
    dialog.addEventListener('click', (event) => {
      if (event.target.closest('[data-key-color-cancel]')) close();
      else if (event.target.closest('[data-key-color-reset-delta-l]'))
        resetDeltaL(event.target.closest('[data-key-color-reset-delta-l]'));
      else if (event.target.closest('[data-artwork-show]')) showArtworkProjection();
      else if (event.target.closest('[data-artwork-restore]')) restoreArtwork();
      else if (event.target.closest('[data-pair-run]')) checkPair();
      else if (event.target.closest('[data-trial-refresh]')) refreshTrialUsage();
      else if (event.target.closest('[data-research-preview]')) showResearchPreview();
      else if (event.target.closest('[data-research-create]')) createTrial();
      else if (event.target.closest('[data-key-color-preview]')) showPreview();
      else if (event.target.closest('[data-key-color-apply]')) apply();
      else if (event.target.closest('[data-key-color-unlink]')) unlink();
    });
    dialog.addEventListener('cancel', (event) => {
      event.preventDefault();
      if (!composing.size) close();
    });
    dialog.addEventListener('close', () => {
      if (dialog && !dialog.open) close();
    });
    dialog.showModal();
    (
      dialog.querySelector('[data-research-hex]') ||
      dialog.querySelector('[data-key-color-input]') ||
      dialog.querySelector('[data-key-color-cancel]')
    )?.focus({ preventScroll: true });
    return dialog;
  }

  // Pointer scrubs can start or finish without an input event. Observe the
  // bubbling event after their handlers; the command also rechecks on click.
  for (const type of ['pointerdown', 'pointerup', 'pointercancel', 'lostpointercapture'])
    document.addEventListener(type, () => {
      if (dialog) updateUnlink();
    });

  // Register before the app's document shortcut handlers. Native text editing
  // and Tab stay native; graph/Undo/save shortcuts cannot author through a modal.
  document.addEventListener(
    'keydown',
    (event) => {
      if (!dialog?.contains(event.target)) return;
      event.stopImmediatePropagation();
      if (
        event.key === 'Escape' &&
        !event.isComposing &&
        event.keyCode !== 229 &&
        !composing.size
      ) {
        event.preventDefault();
        if (deltaScrub?.cancel()) return;
        close();
      }
    },
    true,
  );

  return {
    open,
    close,
    reconcile,
    get previewDocument() {
      if (!preview || !dialog || research) return null;
      try {
        current();
        if (
          (artworkProjection &&
            (owner.activeCompositionId !== artworkComposition ||
              c1Draft?.active ||
              deltaScrub?.active)) ||
          readInputs().key !== preview.key
        )
          throw Error('Artwork preview expired');
        return artworkProjection;
      } catch {
        const hadArtwork = !!artworkProjection;
        // This getter is also read outside drawing; expire everything first,
        // then queue a redraw if a projection had been displayed.
        artworkProjection = null;
        dialog.querySelector('[data-artwork-show]')?.setAttribute('aria-pressed', 'false');
        invalidate();
        if (hadArtwork) onArtworkPreview(false);
        return null;
      }
    },
    get editing() {
      return !!dialog && (!!base || research);
    },
  };
}
