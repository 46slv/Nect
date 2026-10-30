import { createAutoChromaPanel } from './auto-chroma-panel.js';
import { createColorDraftScrub } from './color-draft-scrub.js';
import { renderHslFields, updateHexHsl } from './hsl-color-picker.js';
// View state only; all authored edits go through the current typed Session.
const esc = (v) =>
  String(v).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
export function createSharedColorsPanel({
  app,
  getSession,
  getDocument,
  getCoverageSnapshot,
  dispatch,
  render,
  notify,
  live = false,
  onPreview = () => {},
}) {
  let activeColorPointer = null,
    rendering = false;
  const composingInputs = new WeakSet();
  app.addEventListener(
    'compositionstart',
    (event) => {
      if (event.target.closest?.('.shared-colors-panel')) composingInputs.add(event.target);
    },
    true,
  );
  app.addEventListener(
    'compositionend',
    (event) => {
      composingInputs.delete(event.target);
    },
    true,
  );
  for (const type of ['input', 'change'])
    app.addEventListener(
      type,
      (event) => {
        // Removing a changed, focused number can emit native change. A render is
        // reconciliation, not a new authoring action after Escape/rollback.
        if (
          live &&
          event.target.closest?.('.shared-colors-panel') &&
          (rendering || event.isComposing || composingInputs.has(event.target))
        )
          event.stopImmediatePropagation();
      },
      true,
    );
  // Keep both bounded scrollports stable across draft renders and selection.
  const renderApp = render;
  render = () => {
    if (live && chroma.editing) return; // A queued frame patches only the color controls/canvas.
    const selectors = ['.chroma-settings-scroll', '.harmony-palette-swatches'];
    const positions = selectors.map((selector) => {
      const el = app.querySelector(selector);
      return [el?.scrollTop || 0, el?.scrollLeft || 0];
    });
    rendering = true;
    try {
      renderApp();
    } finally {
      rendering = false;
    }
    if (live)
      for (const input of app.querySelectorAll(
        '.shared-colors-panel input:is([type="number"],[type="range"])',
      ))
        input.value = input.defaultValue;
    selectors.forEach((selector, index) => {
      const el = app.querySelector(selector);
      if (el) {
        el.scrollTop = positions[index][0];
        el.scrollLeft = positions[index][1];
      }
    });
  };
  let open = false;
  const stamp = () => {
    const s = getSession().state;
    return `data-shared-color-session="${esc(s.sessionId)}" data-shared-color-revision="${s.expectedRevision}"`;
  };
  function envelope(el) {
    const host = el.closest('[data-shared-color-session]'),
      state = getSession().state;
    if (
      !host?.isConnected ||
      !app.contains(host) ||
      host.dataset.sharedColorSession !== state.sessionId ||
      Number(host.dataset.sharedColorRevision) !== state.expectedRevision
    )
      throw Error('Color control is stale; use the current document');
    return { ...state };
  }
  const chroma = createAutoChromaPanel({
    app,
    getSession,
    getDocument,
    getCoverageSnapshot,
    dispatch,
    render,
    notify,
    envelope,
    live,
    renderPreview: patchPreview,
  });
  function patchPreview(rebaseFrom = null) {
    const host = app.querySelector('.shared-colors-panel');
    if (!host) return;
    const template = app.ownerDocument.createElement('template');
    template.innerHTML = controller.html();
    const next = template.content.firstElementChild;
    const state = getSession().state;
    if (
      rebaseFrom &&
      rebaseFrom.sessionId === state.sessionId &&
      state.expectedRevision === rebaseFrom.expectedRevision + 1
    ) {
      for (const control of app.querySelectorAll('[data-shared-color-session]')) {
        if (
          control.dataset.sharedColorSession === rebaseFrom.sessionId &&
          Number(control.dataset.sharedColorRevision) === rebaseFrom.expectedRevision
        )
          control.dataset.sharedColorRevision = String(state.expectedRevision);
      }
    }
    host.dataset.sharedColorSession = next.dataset.sharedColorSession;
    host.dataset.sharedColorRevision = next.dataset.sharedColorRevision;
    const replaceContents = (selector) => {
      const old = host.querySelector(selector),
        fresh = next.querySelector(selector);
      if (old && fresh && old.innerHTML !== fresh.innerHTML) old.innerHTML = fresh.innerHTML;
    };
    replaceContents('.harmony-wheel-grid');
    replaceContents('.harmony-wheel-spokes');
    const markers = new Map(
      [...next.querySelectorAll('[data-wheel-marker]')].map((el) => [el.dataset.wheelMarker, el]),
    );
    for (const old of host.querySelectorAll('[data-wheel-marker]')) {
      const fresh = markers.get(old.dataset.wheelMarker);
      if (!fresh) continue;
      for (const key of ['transform', 'aria-pressed'])
        old.setAttribute(key, fresh.getAttribute(key));
      old.querySelector('title').textContent = fresh.querySelector('title').textContent;
      const markerColor = old.querySelector('[data-wheel-marker-color]'),
        freshColor = fresh.querySelector('[data-wheel-marker-color]'),
        text = old.querySelector('text');
      markerColor.setAttribute('style', freshColor.getAttribute('style'));
      text.setAttribute('style', fresh.querySelector('text').getAttribute('style'));
    }
    const swatches = new Map(
      [...next.querySelectorAll('[data-wheel-select]')].map((el) => [el.dataset.wheelSelect, el]),
    );
    for (const old of host.querySelectorAll('[data-wheel-select]')) {
      const fresh = swatches.get(old.dataset.wheelSelect);
      if (!fresh) continue;
      old.setAttribute('aria-pressed', fresh.getAttribute('aria-pressed'));
      old.title = fresh.title;
      old
        .querySelector('.harmony-palette-chip')
        .setAttribute('style', fresh.querySelector('.harmony-palette-chip').getAttribute('style'));
    }
    const neutrals = new Map(
      [...next.querySelectorAll('[data-wheel-neutral]')].map((el) => [el.dataset.wheelNeutral, el]),
    );
    for (const old of host.querySelectorAll('[data-wheel-neutral]')) {
      const fresh = neutrals.get(old.dataset.wheelNeutral);
      if (!fresh) continue;
      for (const key of ['style', 'aria-valuenow', 'data-selected'])
        old.setAttribute(key, fresh.getAttribute(key));
    }
    replaceContents('[data-wheel-selection-info]');
    const group = host.querySelector('.harmony-palette-swatches');
    if (group)
      group.setAttribute(
        'aria-label',
        next.querySelector('.harmony-palette-swatches').getAttribute('aria-label'),
      );
    const identity = (el) =>
      JSON.stringify([
        el.tagName,
        el.getAttribute('aria-label'),
        el.dataset.wheelId,
        el.dataset.sharedColorId,
        el.dataset.chromaLock,
      ]);
    const controls = new Map(
      [...next.querySelectorAll('input, select')].map((el) => [identity(el), el]),
    );
    for (const old of host.querySelectorAll('input, select')) {
      const fresh = controls.get(identity(old));
      if (!fresh) continue;
      for (const key of ['min', 'max'])
        if (fresh.hasAttribute(key)) old.setAttribute(key, fresh.getAttribute(key));
      if (old !== app.ownerDocument.activeElement || !chroma.editing) {
        if (old.tagName === 'SELECT' && old.innerHTML !== fresh.innerHTML)
          old.innerHTML = fresh.innerHTML;
        old.value = fresh.value;
        if (old.type === 'checkbox') old.checked = fresh.checked;
        if (!chroma.editing && old.tagName === 'INPUT') old.defaultValue = fresh.defaultValue;
      }
    }
    const status = host.querySelector('[data-chroma-state]'),
      nextStatus = next.querySelector('[data-chroma-state]');
    if (status && nextStatus) {
      status.dataset.chromaState = nextStatus.dataset.chromaState;
      status.textContent = nextStatus.textContent;
    }
    const report = host.querySelector('[data-palette-contrast="preview"]'),
      freshReport = next.querySelector('[data-palette-contrast="preview"]');
    if (report) {
      if (freshReport) report.replaceWith(freshReport);
      else report.remove();
    } else if (freshReport)
      host.querySelector('[data-palette-contrast="current"]')?.after(freshReport);
    const heading = host.querySelector('.shared-colors-heading');
    if (heading) heading.textContent = next.querySelector('.shared-colors-heading').textContent;
    onPreview();
  }
  const alphaScrub = createColorDraftScrub({
    app,
    selector: live ? 'input[data-shared-color-edit="alpha"]' : 'input[data-unused-live-alpha]',
    envelope,
    getState: () => getSession().state,
    render,
    snapshot: (el) => {
      const id = el.dataset.sharedColorId;
      return { id, value: { ...getDocument().colorSlots.find((slot) => slot.id === id).value } };
    },
    restore: (before) => chroma.stageLiteral(before.id, before.value),
    update: (el, alpha) => {
      const id = el.dataset.sharedColorId,
        slot = getDocument().colorSlots.find((slot) => slot.id === id);
      chroma.stageLiteral(id, { ...slot.value, alpha });
    },
    onEdit: () => {},
    onCommit: () => chroma.finishEdit(),
    onCancel: () => chroma.cancelEdit(),
  });
  app.addEventListener(
    'pointerdown',
    (event) => {
      if (activeColorPointer !== null) {
        if (event.pointerId !== activeColorPointer) {
          event.preventDefault();
          event.stopImmediatePropagation();
        }
        return;
      }
      if (live && chroma.editing && event.isPrimary !== false) {
        const before = getSession().state,
          focused = app.ownerDocument.activeElement;
        if (
          focused?.matches('input[type="number"]') &&
          (!focused.value.trim() || !focused.validity.valid || composingInputs.has(focused))
        )
          chroma.cancelEdit();
        else chroma.finishEdit();
        patchPreview(before);
      }
      if (
        live &&
        event.button === 0 &&
        event.isPrimary !== false &&
        event.target.closest?.(
          '.shared-colors-panel input[data-color-scrub-step],.shared-colors-panel input[type="range"],[data-wheel-marker],[data-wheel-neutral]',
        )
      )
        activeColorPointer = event.pointerId;
    },
    true,
  );
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
    app.addEventListener(
      type,
      (event) => {
        if (event.pointerId === activeColorPointer) activeColorPointer = null;
      },
      true,
    );
  app.addEventListener('focusout', (event) => {
    const input = event.target;
    if (
      !live ||
      rendering ||
      !chroma.editing ||
      !input.isConnected ||
      !input.closest?.('.shared-colors-panel')
    )
      return;
    queueMicrotask(() => {
      if (
        rendering ||
        !chroma.editing ||
        !input.isConnected ||
        app.ownerDocument.activeElement === input
      )
        return;
      if (
        input.matches('input[type="number"]') &&
        (!input.value.trim() || !input.validity.valid || composingInputs.has(input))
      )
        chroma.cancelEdit();
      else chroma.finishEdit();
      render();
    });
  });
  app.ownerDocument.defaultView?.addEventListener('blur', () => {
    activeColorPointer = null;
    if (!live || !chroma.editing) return;
    chroma.cancelEdit();
    render();
  });
  function close() {
    alphaScrub.stop();
    chroma.reset();
    open = false;
    render();
    app.querySelector('[data-workspace-panel="colors"]')?.focus({ preventScroll: true });
  }
  function perform(action) {
    try {
      action();
    } catch (error) {
      notify(error.message, true);
      render();
    }
  }
  app.addEventListener('click', (event) => {
    const button = event.target.closest?.('[data-shared-color-add],[data-shared-color-close]');
    if (!button) return;
    event.stopImmediatePropagation();
    if (button.hasAttribute('data-shared-color-close')) return close();
    perform(() =>
      dispatch(
        'create-color-slot',
        {
          name: `Color ${(getDocument().colorSlots || []).length + 1}`,
          value: { hex: '#808080', alpha: 1 },
        },
        envelope(button),
      ),
    );
  });
  function literalInput(event) {
    const el = event.target;
    if (
      !live ||
      event.isComposing ||
      !el.matches?.('[data-shared-color-edit="hex"],[data-shared-color-edit="alpha"],[data-shared-color-edit^="hsl-"]')
    )
      return;
    event.stopImmediatePropagation();
    try {
      envelope(el);
      const id = el.dataset.sharedColorId,
        slot = getDocument().colorSlots.find((slot) => slot.id === id);
      const value = { ...slot.value };
      if (el.dataset.sharedColorEdit === 'hex') value.hex = el.value;
      else if (el.dataset.sharedColorEdit.startsWith('hsl-')) {
        if (!el.value.trim()) return;
        try {
          value.hex = updateHexHsl(value.hex, el.dataset.sharedColorEdit.slice(4), el.value);
        } catch {
          return;
        }
      } else {
        if (
          !el.value.trim() ||
          !Number.isFinite(Number(el.value)) ||
          Number(el.value) < 0 ||
          Number(el.value) > 1
        )
          return;
        value.alpha = Number(el.value);
      }
      chroma.stageLiteral(id, value);
    } catch {
      /* Incomplete input stays local. */
    }
  }
  app.addEventListener('input', literalInput);
  app.addEventListener('compositionend', literalInput);
  app.addEventListener('change', (event) => {
    const el = event.target;
    if (el.matches?.('[data-shared-color-binding]')) {
      event.stopImmediatePropagation();
      perform(() => {
        const state = envelope(el),
          payload = {
            layerId: el.dataset.sharedColorLayer,
            nodeId: el.dataset.sharedColorNode,
            key: el.dataset.sharedColorKey,
          };
        if (el.value) {
          payload.slotId = el.value;
          dispatch('bind-color-slot', payload, state);
        } else dispatch('unbind-color-slot', payload, state);
      });
      return;
    }
    const kind = el.dataset?.sharedColorEdit;
    if (!kind) return;
    event.stopImmediatePropagation();
    perform(() => {
      const state = envelope(el),
        slotId = el.closest('[data-shared-color-row]')?.dataset.sharedColorRow;
      const slot = getDocument().colorSlots?.find((s) => s.id === slotId);
      if (!slot) throw Error('Shared color is no longer available');
      if (kind === 'name') dispatch('rename-color-slot', { slotId, name: el.value }, state);
      else {
        const value = { ...slot.value };
        if (kind === 'hex') value.hex = el.value;
        else if (kind.startsWith('hsl-'))
          value.hex = updateHexHsl(value.hex, kind.slice(4), el.value);
        else if (kind === 'alpha') {
          if (!el.value.trim()) throw Error('Enter an alpha between zero and one');
          value.alpha = Number(el.value);
        } else throw Error('Unknown Color control');
        if (live) {
          if (!chroma.editing && value.hex === slot.value.hex && value.alpha === slot.value.alpha)
            return;
          chroma.stageLiteral(slotId, value);
          chroma.finishEdit();
          render();
        } else dispatch('set-color-slot', { slotId, value }, state);
      }
    });
  });
  app.addEventListener('keydown', (event) => {
    if (
      event.key === 'Escape' &&
      !event.isComposing &&
      event.keyCode !== 229 &&
      !event.defaultPrevented &&
      !event.target.closest?.('dialog[open]') &&
      event.target.closest?.('.shared-colors-panel')
    ) {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (
        live &&
        (chroma.editing ||
          (event.target.matches?.('input[type="number"]') &&
            (event.target.value !== event.target.defaultValue || event.target.validity.badInput)))
      ) {
        chroma.cancelEdit();
        render();
      } else close();
      return;
    }
    // Sidebar navigation/editing keys must not target the selected graph node.
    // Native button activation, text editing and scrollport navigation stay intact.
    if (event.target.closest?.('.shared-colors-panel')) event.stopImmediatePropagation();
  });
  const controller = {
    get previewDocument() {
      return chroma.previewDocument;
    },
    metrics: () => chroma.metrics(),
    cancelEdit() {
      activeColorPointer = null;
      alphaScrub.stop();
      chroma.cancelEdit();
    },
    toggle() {
      chroma.reset();
      open = !open;
      render();
    },
    get open() {
      return open;
    },
    html() {
      if (!open) return '';
      const slots = (chroma.previewDocument || getDocument()).colorSlots || [];
      const rows = slots
        .map(
          (slot) =>
            `<div class="shared-color-row" data-shared-color-row="${esc(slot.id)}"><input type="color" data-shared-color-edit="hex" data-shared-color-id="${esc(slot.id)}" value="${esc(slot.value.hex)}" aria-label="${esc(slot.name)} color"><input type="text" data-shared-color-edit="name" data-shared-color-id="${esc(slot.id)}" maxlength="128" value="${esc(slot.name)}" aria-label="Shared color name"><input type="number" data-color-scrub-step="0.01" title="Drag horizontally to adjust; click to type" data-shared-color-edit="alpha" data-shared-color-id="${esc(slot.id)}" min="0" max="1" step="0.01" value="${slot.value.alpha}" aria-label="${esc(slot.name)} alpha">${renderHslFields(slot.value.hex, { mode: "shared", name: slot.name, id: slot.id })}<output data-color-coverage-id="${esc(slot.id)}" aria-label="${esc(slot.name)} visible area">—</output></div>`,
        )
        .join('');
      const savedColors = `<div class="shared-color-list" aria-label="Saved shared colors"><h3 class="shared-colors-heading">${live && chroma.editing ? 'Live colors' : 'Saved colors'}</h3>${rows || '<p class="font-note">Add a color, then link a Color field.</p>'}</div>${getDocument().colorDesign?.version === 2 ? '' : '<p class="font-note" data-color-coverage-status role="status">Measuring visible area…</p>'}`;
      return `<section class="shared-colors-panel" aria-label="Shared colors" ${stamp()}><header><strong>Shared colors</strong>${!live || getDocument().colorDesign?.version !== 2 ? '<button type="button" data-harmony-create>Create palette</button>' : ''}<button type="button" data-shared-color-add aria-label="Add shared color">+</button><button type="button" data-shared-color-close aria-label="Close Shared colors">×</button></header>${live || getDocument().colorDesign?.version === 2 ? chroma.html({ savedColors }) : savedColors + chroma.html()}</section>`;
    },
    bindingControl(node, key, q, layerId) {
      const slots = getDocument().colorSlots || [];
      if (!slots.length && q.colorSlotId === undefined) return '';
      return `<select class="shared-color-binding" data-shared-color-binding data-shared-color-layer="${esc(layerId)}" data-shared-color-node="${esc(node.id)}" data-shared-color-key="${esc(key)}" ${stamp()} aria-label="${esc(node.name)} ${esc(key)} shared color" ${q.locked || q.source || q.expression || getDocument().layers.find((l) => l.id === layerId)?.locked ? 'disabled' : ''}><option value="" ${q.colorSlotId === undefined ? 'selected' : ''}>Local color</option>${slots.map((slot) => `<option value="${esc(slot.id)}" ${slot.id === q.colorSlotId ? 'selected' : ''}>${esc(slot.name)}</option>`).join('')}</select>`;
    },
  };
  return controller;
}
