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
} from './key-color-recipe-document.js';
import { uid } from './core.js';
import { createKeyColorC1Draft } from './key-color-c1-draft.js';
import { createKeyColorDeltaScrub } from './key-color-delta-scrub.js';
import { researchPlans } from './key-color-research-plans.js';
import { analyzeSelectedContrast } from './palette-contrast.js';

const esc = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
const literal = (value) =>
  `<span class="key-color-swatch" style="background:${esc(value.hex)};opacity:${value.alpha}" aria-hidden="true"></span><code>${esc(value.hex)}</code> · α ${value.alpha}`;
const trialB = (preset, version) => preset === 'research-neutral-start' && version === 1;
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

// An opt-in, detached view draft, not another authored palette owner. The
// historical receipt is read once; only one explicit Session Apply can write.
export function createKeyColorRecipePanel({
  document,
  getSession,
  getDocument,
  getCoverageSnapshot = () => null,
  dispatch,
  notify,
}) {
  let dialog = null,
    trigger = null,
    owner = null,
    envelope = null,
    base = null,
    preview = null,
    c1Draft = null,
    deltaScrub = null,
    research = false,
    researchComposition = null,
    researchIds = null,
    stale = false,
    applying = false;
  const composing = new Set();

  function close(restoreFocus = true) {
    const oldTrigger = trigger;
    const old = dialog;
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
    composing.clear();
    if (old?.open) old.close();
    old?.remove();
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
      (research && session.activeCompositionId !== researchComposition)
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
      key: JSON.stringify([
        ...values.map(({ kind, slotId, raw }) => [kind, slotId, raw]),
        ['hue-slider', dialog.querySelector('[data-key-color-hue]')?.value],
      ]),
    };
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
    return prepareKeyColorRecipeApplication(
      doc,
      { snapshot },
      new Map((doc.colorSlots || []).map((slot) => [slot.id, slot])),
    );
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
    const options = rows
      .map((row) => `<option value="${esc(row.id)}">${esc(row.role)} · ${esc(row.id)}</option>`)
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
          .map((other) => other.role);
        const details = [
          row.active ? 'Active' : 'Dormant',
          intent
            ? `ΔL ${intent.deltaL}; base OKLCH L ${row.baseL}; effective L ${row.L}`
            : `C1 OKLCH L ${row.L}; no ΔL`,
          row.diagnostics.includes('lightness-clamped') ? 'L range limited' : 'L range not limited',
          row.diagnostics.includes('support-chroma-intent-unapplied')
            ? 'Stored Δr not applied'
            : '',
          row.diagnostics.length ? `Diagnostics ${row.diagnostics.join(', ')}` : '',
          equal.length ? `Same literal and alpha as ${equal.join(', ')}` : '',
        ]
          .filter(Boolean)
          .join(' · ');
        const usage = created
          ? `<span data-trial-usage="${esc(row.id)}">Usage unavailable</span>`
          : 'Not created; usage not measured';
        return `<tr data-trial-row="${esc(row.id)}" ${created ? '' : `data-research-row="${esc(row.id)}" data-research-role="${esc(row.role)}"`}><th>${esc(row.role)}</th><td>${literal(row.value)}</td><td>${usage}</td><td>${esc(details)}</td><td>Readability not evaluated · procedure not evaluated</td></tr>`;
      })
      .join('');
    return `<section data-trial-status="${esc(title)}"><h3>${esc(title)}</h3><table><thead><tr><th>Role</th><th>Color</th><th>Current visible paint share</th><th>OKLCH adjustment</th><th>Checks</th></tr></thead><tbody>${rows}</tbody></table><p>${esc(snapshot.diagnostics.join(', '))}</p></section>`;
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
          ? `Current visible paint share ≈ ${row.percent}%`
          : 'Usage unavailable';
    }
  }

  function clearPreview() {
    clearPairResult();
    preview = null;
    dialog.querySelector('[data-key-color-comparison]').innerHTML = '';
    dialog.querySelector('[data-key-color-apply]').disabled = true;
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
      const result = dispatch('initialize-key-color-recipe', payload, { ...envelope });
      if (result == null || result === false) throw Error('Trial colors were not created.');
      close();
      notify('Five trial colors added');
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
      dialog.querySelector('[data-key-color-comparison]').innerHTML =
        `<h3>Shared literals · before → after</h3><p>Only active rows below will change. No artwork changes until Apply.</p><table><thead><tr><th>Stable slot</th><th>Current shared literal</th><th>Proposed shared literal</th></tr></thead><tbody>${prepared.updates.map(({ slotId, value }) => `<tr data-key-color-compare="${esc(slotId)}"><th scope="row">${esc(slots.get(slotId).name)}<br><code>${esc(slotId)}</code></th><td data-key-color-before>${literal(slots.get(slotId).value)}</td><td data-key-color-after>${literal(value)}</td></tr>`).join('')}</tbody></table>`;
      if (
        trialB(prepared.snapshot.intent.preset.id, prepared.snapshot.intent.preset.version) &&
        prepared.snapshot.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION
      ) {
        dialog
          .querySelector('[data-key-color-comparison]')
          .insertAdjacentHTML(
            'beforeend',
            trialStatus(prepared.snapshot, 'Proposed colors · not applied', true),
          );
        refreshTrialUsage();
      }
      dialog.querySelector('[data-key-color-apply]').disabled = false;
      status('Preview ready. Apply writes these shared literals and the receipt together.');
      // Preview is an explicit review step: bring its actual before/after
      // literals into the bounded scrollport, not only a below-the-fold heading.
      dialog.querySelector('[data-key-color-comparison]').scrollIntoView?.({ block: 'start' });
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
      const result = dispatch('apply-key-color-recipe-snapshot', { snapshot }, { ...envelope });
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
  }

  function open(anchor = null) {
    close(false);
    owner = getSession();
    envelope = { ...owner.state }; // Captured once, never refreshed at Preview/Apply.
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
      ? (() => {
          const key = base.intent.keyColor,
            active = new Set(base.intent.preset.slots.map((slot) => slot.id));
          return `<p>Palette <code>${esc(base.paletteId)}</code> · retained revision ${base.revision}. This receipt is historical, not a live driver.</p><fieldset><legend>C1 · ${esc(key.role)} · ${esc(key.id)}</legend><div data-key-color-c1></div></fieldset><h3>Active derived intent</h3><p>ΔL and Δr are finite stored offsets, not clamped slider positions. Derived alpha must be 1.</p>${base.intent.slots.map((slot) => (active.has(slot.id) ? `<fieldset><legend>${esc(slot.role)} · ${esc(slot.id)}</legend><label>ΔL<input type="number" step="any" data-key-color-input="deltaL" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaL}" aria-label="${esc(slot.id)} ΔL"></label>${trialB(base.intent.preset.id, base.intent.preset.version) && base.solverVersion === KEY_COLOR_NEUTRAL_SOLVER_VERSION ? `<button type="button" data-key-color-reset-delta-l="${esc(slot.id)}" aria-label="Reset ${esc(slot.role)} ΔL to zero">Reset ΔL</button>` : ''}${hideDelta(base, slot) ? `<p data-key-color-support-intent>Stored Δr ${slot.deltaRatioCmax} · ${isNeutralTextPolicySlot(base.solverVersion, slot.role) ? 'not applied' : 'read-only'}${isNeutralTextPolicySlot(base.solverVersion, slot.role) && slot.deltaRatioCmax !== 0 ? ' · support-chroma-intent-unapplied' : ''}</p>` : `<label>Δr<input type="number" step="any" data-key-color-input="deltaRatioCmax" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaRatioCmax}" aria-label="${esc(slot.id)} Δr"></label>`}<p>Stored derived alpha · ${base.resolved.find((row) => row.id === slot.id).value.alpha} (read-only; Apply requires 1, never coerces).</p></fieldset>` : `<fieldset disabled data-key-color-dormant="${esc(slot.id)}"><legend>${esc(slot.role)} · ${esc(slot.id)} · dormant, unchanged</legend><p>ΔL ${slot.deltaL} · Δr ${slot.deltaRatioCmax}</p><p>${literal(base.resolved.find((row) => row.id === slot.id).value)}</p></fieldset>`)).join('')}<details><summary>Read-only preset, coefficients and versions</summary><p>Recipe v${base.version} · solver ${esc(base.solverVersion)} · ${esc(base.colorSemanticsVersion)}</p><pre>${esc(JSON.stringify({ preset: base.intent.preset, settings: base.intent.settings }, null, 2))}</pre></details><p data-key-color-status role="status" aria-live="polite">Draft only. Preview these inputs before Apply.</p><section class="key-color-comparison" data-key-color-comparison aria-label="Key color preview"></section>`;
        })()
      : research
        ? `<p data-key-color-empty>Research candidates. B adds five trial colors without changing existing colors, in one Undo. Coverage and readability are not evaluated; colors may coincide.</p><fieldset><legend>C1</legend><div data-key-color-c1></div></fieldset><label>Research candidate <select data-research-plan><option value="">Choose a candidate</option><option value="research-triadic-offset">A · chromatic comparison</option><option value="research-neutral-start">B · neutral start</option></select></label><details><summary>Candidate roles and coefficients</summary><pre data-research-details></pre></details><p data-key-color-status role="status" aria-live="polite">Enter C1 and choose a candidate.</p><section data-research-results aria-label="Research color preview"></section>`
        : `<p data-key-color-empty>${esc(error || 'The retained recipe is unavailable.')}</p>`;
    dialog.innerHTML = `<header><h2 id="key-color-recipe-title">Key color · Experimental</h2><button type="button" data-key-color-cancel aria-label="Close Key color">×</button></header>${base ? '<p>Opt-in one-shot Apply. The existing legacy wheel and product defaults are unchanged. Dark TonePlan coefficients remain unresolved.</p>' : ''}<div class="key-color-recipe-body">${controls}</div><footer><button type="button" data-key-color-cancel>Cancel</button>${base ? '<button type="button" data-key-color-preview>Preview</button><button type="button" data-key-color-apply disabled>Apply · 1 Undo</button>' : research ? '<button type="button" data-research-preview disabled>Preview</button><button type="button" data-research-create disabled>Add 5 trial colors · 1 Undo</button>' : ''}</footer>${base ? '<p class="key-color-undo-note">A changed Apply is one Undo. Equal literals and receipt are a no-op. Cancel and Escape discard the draft.</p>' : ''}`;
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
      else if (event.target.closest('[data-pair-run]')) checkPair();
      else if (event.target.closest('[data-trial-refresh]')) refreshTrialUsage();
      else if (event.target.closest('[data-research-preview]')) showResearchPreview();
      else if (event.target.closest('[data-research-create]')) createTrial();
      else if (event.target.closest('[data-key-color-preview]')) showPreview();
      else if (event.target.closest('[data-key-color-apply]')) apply();
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
    get editing() {
      return !!dialog && (!!base || research);
    },
  };
}
