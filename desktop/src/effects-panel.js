// View state only. The apply owner must validate the captured target before editing.
export function createEffectsPanel({
  document,
  getTarget,
  getEntries,
  onApply,
  onError = () => {},
}) {
  let query = '',
    selectedId = null;

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
    const target = getTarget();
    const entries = getEntries();
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

    let composing = false;
    function refresh() {
      const visible = entries.filter(matches);
      if (!visible.some((entry) => entry.id === selectedId)) selectedId = visible[0]?.id || null;
      list.replaceChildren();
      for (const entry of visible) {
        const row = document.createElement('button');
        row.type = 'button';
        row.dataset.effectId = entry.id;
        row.textContent = entry.label;
        row.setAttribute('aria-pressed', String(entry.id === selectedId));
        row.addEventListener('click', () => {
          selectedId = entry.id;
          for (const button of list.querySelectorAll('[data-effect-id]'))
            button.setAttribute('aria-pressed', String(button.dataset.effectId === selectedId));
          apply.disabled = !target?.snapshot || !!target.reason || composing;
        });
        list.append(row);
      }
      status.textContent =
        target?.reason ||
        (!target ? 'Select artwork' : !visible.length ? 'No matching effects' : '');
      apply.disabled = !target?.snapshot || !!target.reason || !selectedId || composing;
    }
    search.addEventListener('compositionstart', () => {
      composing = true;
      apply.disabled = true;
    });
    search.addEventListener('compositionend', () => {
      composing = false;
      query = search.value;
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
    apply.addEventListener('click', () => {
      if (!panel.isConnected || apply.disabled || composing) return;
      try {
        onApply(selectedId, target.snapshot);
      } catch (error) {
        onError(error);
      }
    });
    refresh();
    return panel;
  }

  return { createView };
}
