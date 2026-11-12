// View state only. The apply owner must validate the captured target before editing.
export function createEffectsPanel({
  document,
  getTarget,
  getTargetVersion,
  getEntries,
  onApply,
  onError = () => {},
}) {
  let query = '',
    selectedId = null,
    composing = false,
    currentView = null;

  const sameVersion = (a, b) =>
    Array.isArray(a) &&
    Array.isArray(b) &&
    a.length === b.length &&
    a.every((v, i) => Object.is(v, b[i]));

  const normalize = (value) => String(value).normalize('NFKC').toLowerCase();
  const matches = (entry) => {
    const text = normalize([entry.label, ...(entry.aliases || [])].join(' '));
    return normalize(query)
      .trim()
      .split(/\s+/)
      .filter(Boolean)
      .every((word) => text.includes(word));
  };

  function createView() {
    let target = getTarget(),
      snapshot = target?.snapshot,
      reason = target?.reason,
      targetVersion = getTargetVersion?.(),
      visibleCount = 0;
    const entries = getEntries().map((entry) => ({
      id: entry.id,
      label: entry.label,
      aliases: [...(entry.aliases || [])],
    }));
    const panel = document.createElement('section');
    panel.className = 'effects-panel';
    panel.setAttribute('aria-label', 'Effects');
    const heading = document.createElement('h2');
    heading.textContent = 'Effects';
    const targetLabel = document.createElement('div');
    targetLabel.className = 'effects-target';
    targetLabel.dataset.effectsTarget = '';
    targetLabel.textContent = target?.label || '';
    const search = document.createElement('input');
    search.type = 'search';
    search.dataset.effectsSearch = '';
    search.setAttribute('aria-label', 'Search effects');
    search.placeholder = 'Search effects';
    search.value = query;
    const list = document.createElement('div');
    list.className = 'effects-list';
    list.setAttribute('aria-label', 'Effects');
    const status = document.createElement('output');
    status.dataset.effectsStatus = '';
    status.setAttribute('aria-live', 'polite');
    const apply = document.createElement('button');
    apply.type = 'button';
    apply.dataset.effectsApply = '';
    apply.textContent = 'Apply';
    panel.append(heading, targetLabel, search, list, status, apply);

    function updateState() {
      targetLabel.textContent = target?.label || '';
      status.textContent =
        reason || (!target ? 'Select artwork' : !visibleCount ? 'No matching effects' : '');
      apply.disabled = !snapshot || !!reason || !selectedId || composing;
    }
    function refreshTarget() {
      if (!panel.isConnected || composing) return;
      const version = getTargetVersion?.();
      if (getTargetVersion && sameVersion(version, targetVersion)) return;
      target = getTarget();
      snapshot = target?.snapshot;
      reason = target?.reason;
      targetVersion = getTargetVersion?.();
      updateState();
    }
    function refresh() {
      const visible = entries.filter(matches);
      visibleCount = visible.length;
      if (!visible.some((entry) => entry.id === selectedId)) selectedId = visible[0]?.id || null;
      const existing = new Map([...list.children].map((row) => [row.dataset.effectId, row]));
      for (const [index, entry] of visible.entries()) {
        let row = existing.get(entry.id);
        if (!row) {
          row = document.createElement('button');
          row.type = 'button';
          row.dataset.effectId = entry.id;
          row.addEventListener('click', () => {
            selectedId = entry.id;
            for (const button of list.querySelectorAll('[data-effect-id]'))
              button.setAttribute('aria-pressed', String(button.dataset.effectId === selectedId));
            updateState();
          });
        }
        row.textContent = entry.label;
        row.setAttribute('aria-pressed', String(entry.id === selectedId));
        if (list.children[index] !== row) list.insertBefore(row, list.children[index] || null);
        existing.delete(entry.id);
      }
      for (const row of existing.values()) row.remove();
      updateState();
    }
    search.addEventListener('compositionstart', () => {
      composing = true;
      apply.disabled = true;
    });
    search.addEventListener('compositionend', () => {
      composing = false;
      query = search.value;
      refreshTarget();
      refresh();
    });
    search.addEventListener('input', (event) => {
      query = search.value;
      if (!composing && !event.isComposing) refresh();
    });
    // Search input never commits an effect; IME confirmation remains text input.
    search.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' && !event.isComposing && !composing) event.preventDefault();
    });
    // A pointer press on disabled Apply must not commit IME, enable the button,
    // and then become an authoring click in the same gesture.
    panel.addEventListener(
      'pointerdown',
      (event) => {
        if (!composing || event.target === search) return;
        event.preventDefault();
        event.stopImmediatePropagation();
      },
      true,
    );
    apply.addEventListener('click', () => {
      if (!panel.isConnected || panel.closest('[hidden], [inert]') || apply.disabled || composing)
        return;
      try {
        onApply(selectedId, snapshot);
      } catch (error) {
        onError(error);
      }
    });
    currentView = { panel, refreshTarget };
    refresh();
    return panel;
  }

  return {
    createView,
    refreshTarget() {
      currentView?.refreshTarget();
    },
    get canDeactivate() {
      return !composing;
    },
  };
}
