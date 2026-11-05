import {
  restoreKeyColorRecipe,
  editKeyColorRecipe,
  createKeyColorRecipe,
  isNeutralTextPolicySlot,
} from './key-color-recipe.js';
import { prepareKeyColorRecipeApplication } from './key-color-recipe-document.js';
import { createKeyColorC1Draft } from './key-color-c1-draft.js';
import { createKeyColorDeltaScrub } from './key-color-delta-scrub.js';
import { researchPlans } from './key-color-research-plans.js';

const esc = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
const literal = (value) =>
  `<span class="key-color-swatch" style="background:${esc(value.hex)};opacity:${value.alpha}" aria-hidden="true"></span><code>${esc(value.hex)}</code> · α ${value.alpha}`;

// An opt-in, detached view draft, not another authored palette owner. The
// historical receipt is read once; only one explicit Session Apply can write.
export function createKeyColorRecipePanel({ document, getSession, getDocument, dispatch, notify }) {
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

  function clearPreview() {
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
    return { hex, alpha, plan };
  }

  function researchInvalidate() {
    if (!dialog || !research) return;
    dialog.querySelector('[data-research-results]').replaceChildren();
    const selected = researchPlans.find(
      (item) => item.id === dialog.querySelector('[data-research-plan]').value,
    );
    dialog.querySelector('[data-research-details]').textContent = selected
      ? JSON.stringify(
          {
            palette: 'synthetic-only',
            key: { id: 'key', role: 'Identity' },
            preset: selected,
            slots: selected.slots.map(({ id, role }) => ({
              id,
              role,
              deltaL: 0,
              deltaRatioCmax: 0,
            })),
            settings: { areaAware: true },
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
      const { hex, alpha, plan } = researchInputs();
      const result = createKeyColorRecipe({
        paletteId: 'synthetic-only',
        keyColor: { id: 'key', role: 'Identity', value: { hex, alpha }, hueHint: 0 },
        preset: plan,
        slots: plan.slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
        settings: { areaAware: true },
      });
      dialog.querySelector('[data-research-results]').innerHTML =
        `<table><thead><tr><th>Role</th><th>Preview</th><th>Diagnostics</th></tr></thead><tbody>${result.resolved.map((row) => `<tr data-research-row="${esc(row.id)}"><th>${esc(row.role)}</th><td>${literal(row.value)}</td><td>${esc(row.diagnostics.join(', ') || '—')}</td></tr>`).join('')}</tbody></table><p>${esc(result.diagnostics.join(', '))}</p>`;
      status('Research preview ready. No shared colors were changed.');
    } catch (error) {
      status(error.message, true);
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

  function showPreview() {
    clearPreview();
    if (!validateDraft()) return;
    try {
      const inputs = readInputs(),
        prepared = prepare(candidate(inputs.values)),
        slots = new Map(getDocument().colorSlots.map((slot) => [slot.id, slot]));
      preview = { snapshot: prepared.snapshot, key: inputs.key };
      dialog.querySelector('[data-key-color-comparison]').innerHTML =
        `<h3>Shared literals · before → after</h3><p>Only active rows below will change. No artwork changes until Apply.</p><table><thead><tr><th>Stable slot</th><th>Current shared literal</th><th>Proposed shared literal</th></tr></thead><tbody>${prepared.updates.map(({ slotId, value }) => `<tr data-key-color-compare="${esc(slotId)}"><th scope="row">${esc(slots.get(slotId).name)}<br><code>${esc(slotId)}</code></th><td data-key-color-before>${literal(slots.get(slotId).value)}</td><td data-key-color-after>${literal(value)}</td></tr>`).join('')}</tbody></table>`;
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
          return `<p>Palette <code>${esc(base.paletteId)}</code> · retained revision ${base.revision}. This receipt is historical, not a live driver.</p><fieldset><legend>C1 · ${esc(key.role)} · ${esc(key.id)}</legend><div data-key-color-c1></div></fieldset><h3>Active derived intent</h3><p>ΔL and Δr are finite stored offsets, not clamped slider positions. Derived alpha must be 1.</p>${base.intent.slots.map((slot) => (active.has(slot.id) ? `<fieldset><legend>${esc(slot.role)} · ${esc(slot.id)}</legend><label>ΔL<input type="number" step="any" data-key-color-input="deltaL" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaL}" aria-label="${esc(slot.id)} ΔL"></label>${isNeutralTextPolicySlot(base.solverVersion, slot.role) ? `<p data-key-color-support-intent>Stored Δr ${slot.deltaRatioCmax} · not applied${slot.deltaRatioCmax !== 0 ? ' · support-chroma-intent-unapplied' : ''}</p>` : `<label>Δr<input type="number" step="any" data-key-color-input="deltaRatioCmax" data-key-color-slot="${esc(slot.id)}" value="${slot.deltaRatioCmax}" aria-label="${esc(slot.id)} Δr"></label>`}<p>Stored derived alpha · ${base.resolved.find((row) => row.id === slot.id).value.alpha} (read-only; Apply requires 1, never coerces).</p></fieldset>` : `<fieldset disabled data-key-color-dormant="${esc(slot.id)}"><legend>${esc(slot.role)} · ${esc(slot.id)} · dormant, unchanged</legend><p>ΔL ${slot.deltaL} · Δr ${slot.deltaRatioCmax}</p><p>${literal(base.resolved.find((row) => row.id === slot.id).value)}</p></fieldset>`)).join('')}<details><summary>Read-only preset, coefficients and versions</summary><p>Recipe v${base.version} · solver ${esc(base.solverVersion)} · ${esc(base.colorSemanticsVersion)}</p><pre>${esc(JSON.stringify({ preset: base.intent.preset, settings: base.intent.settings }, null, 2))}</pre></details><p data-key-color-status role="status" aria-live="polite">Draft only. Preview these inputs before Apply.</p><section class="key-color-comparison" data-key-color-comparison aria-label="Key color preview"></section>`;
        })()
      : research
        ? `<p data-key-color-empty>Research preview only. It does not create a preset or change shared colors. Creation plans are not yet adopted.</p><fieldset><legend>C1</legend><div data-key-color-c1></div></fieldset><label>Research candidate <select data-research-plan><option value="">Choose a candidate</option><option value="research-triadic-offset">A · chromatic comparison</option><option value="research-neutral-start">B · neutral start</option></select></label><details><summary>Candidate roles and coefficients</summary><pre data-research-details></pre></details><p data-key-color-status role="status" aria-live="polite">Enter C1 and choose a candidate.</p><section data-research-results aria-label="Research color preview"></section>`
        : `<p data-key-color-empty>${esc(error || 'The retained recipe is unavailable.')}</p>`;
    dialog.innerHTML = `<header><h2 id="key-color-recipe-title">Key color · Experimental</h2><button type="button" data-key-color-cancel aria-label="Close Key color">×</button></header>${base ? '<p>Opt-in one-shot Apply. The existing legacy wheel and product defaults are unchanged. Dark TonePlan coefficients remain unresolved.</p>' : ''}<div class="key-color-recipe-body">${controls}</div><footer><button type="button" data-key-color-cancel>Cancel</button>${base ? '<button type="button" data-key-color-preview>Preview</button><button type="button" data-key-color-apply disabled>Apply · 1 Undo</button>' : research ? '<button type="button" data-research-preview disabled>Preview</button>' : ''}</footer>${base ? '<p class="key-color-undo-note">A changed Apply is one Undo. Equal literals and receipt are a no-op. Cancel and Escape discard the draft.</p>' : ''}`;
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
    dialog.addEventListener('input', invalidate);
    dialog.addEventListener('change', invalidate);
    dialog.addEventListener('compositionstart', (event) => {
      composing.add(event.target);
      invalidate();
    });
    dialog.addEventListener('compositionend', (event) => {
      composing.delete(event.target);
      invalidate();
    });
    dialog.addEventListener('click', (event) => {
      if (event.target.closest('[data-key-color-cancel]')) close();
      else if (event.target.closest('[data-research-preview]')) showResearchPreview();
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
