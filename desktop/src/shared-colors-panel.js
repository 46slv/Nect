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
  dispatch,
  render,
  notify,
}) {
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
  function close() {
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
        else if (kind === 'alpha') {
          if (!el.value.trim()) throw Error('Enter an alpha between zero and one');
          value.alpha = Number(el.value);
        } else throw Error('Unknown Color control');
        dispatch('set-color-slot', { slotId, value }, state);
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
      close();
    }
  });
  return {
    toggle() {
      open = !open;
      render();
    },
    get open() {
      return open;
    },
    html() {
      if (!open) return '';
      const slots = getDocument().colorSlots || [];
      return `<section class="shared-colors-panel" aria-label="Shared colors" ${stamp()}><header><strong>Shared colors</strong><button type="button" data-shared-color-add aria-label="Add shared color">+</button><button type="button" data-shared-color-close aria-label="Close Shared colors">×</button></header><div class="shared-color-list">${slots.map((slot) => `<div class="shared-color-row" data-shared-color-row="${esc(slot.id)}"><input type="color" data-shared-color-edit="hex" data-shared-color-id="${esc(slot.id)}" value="${esc(slot.value.hex)}" aria-label="${esc(slot.name)} color"><input type="text" data-shared-color-edit="name" data-shared-color-id="${esc(slot.id)}" maxlength="128" value="${esc(slot.name)}" aria-label="Shared color name"><input type="number" data-shared-color-edit="alpha" data-shared-color-id="${esc(slot.id)}" min="0" max="1" step="0.01" value="${slot.value.alpha}" aria-label="${esc(slot.name)} alpha"></div>`).join('') || '<p class="font-note">Add a color, then link a Color field.</p>'}</div></section>`;
    },
    bindingControl(node, key, q, layerId) {
      const slots = getDocument().colorSlots || [];
      if (!slots.length && q.colorSlotId === undefined) return '';
      return `<select class="shared-color-binding" data-shared-color-binding data-shared-color-layer="${esc(layerId)}" data-shared-color-node="${esc(node.id)}" data-shared-color-key="${esc(key)}" ${stamp()} aria-label="${esc(node.name)} ${esc(key)} shared color" ${q.locked || q.source || q.expression || getDocument().layers.find((l) => l.id === layerId)?.locked ? 'disabled' : ''}><option value="" ${q.colorSlotId === undefined ? 'selected' : ''}>Local color</option>${slots.map((slot) => `<option value="${esc(slot.id)}" ${slot.id === q.colorSlotId ? 'selected' : ''}>${esc(slot.name)}</option>`).join('')}</select>`;
    },
  };
}
