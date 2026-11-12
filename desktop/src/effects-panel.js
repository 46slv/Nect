// View state only. The apply owner must validate the captured target before editing.
export const EFFECT_FAVORITES_KEY = 'nect.effect-favorites.v1';

function validFavoriteIds(ids) {
  return (
    Array.isArray(ids) &&
    ids.length <= 256 &&
    ids.every(
      (id) =>
        typeof id === 'string' && id.length > 0 && id.length <= 256 && !/[\x00-\x1f]/.test(id),
    ) &&
    new Set(ids).size === ids.length
  );
}

export function createEffectsPanel({
  document,
  getTarget,
  getTargetVersion,
  getEntries,
  onApply,
  onError = () => {},
  storage,
}) {
  let query = '',
    selectedId = null,
    composing = false,
    currentView = null,
    favoriteIds = [],
    favoriteStorage = storage,
    favoriteError = '';

  function readFavorites() {
    try {
      favoriteStorage ||= document.defaultView.localStorage;
      const raw = favoriteStorage.getItem(EFFECT_FAVORITES_KEY);
      const ids = raw === null ? [] : JSON.parse(raw);
      if (!validFavoriteIds(ids)) throw Error('Invalid favorites');
      favoriteIds = ids;
      favoriteError = '';
      return true;
    } catch {
      // Never replace unreadable saved preferences with a new empty list.
      favoriteError = 'Effect favorites could not be read';
      return false;
    }
  }

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
    readFavorites();
    const readEntries = () =>
      getEntries().map((entry) => ({
        id: entry.id,
        label: entry.label,
        aliases: [...(entry.aliases || [])],
      }));
    let entries = readEntries(),
      target = null,
      snapshot = null,
      reason = null,
      targetVersion,
      targetSelectedId,
      targetInitialized = false,
      visibleCount = 0;
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
    const favoriteStatus = document.createElement('output');
    favoriteStatus.dataset.effectsFavoriteStatus = '';
    favoriteStatus.setAttribute('aria-live', 'polite');
    const actions = document.createElement('div');
    actions.className = 'effects-actions';
    const favorite = document.createElement('button');
    favorite.type = 'button';
    favorite.dataset.effectsFavorite = '';
    const apply = document.createElement('button');
    apply.type = 'button';
    apply.dataset.effectsApply = '';
    apply.textContent = 'Apply';
    actions.append(favorite, apply);
    panel.append(heading, targetLabel, search, list, status, favoriteStatus, actions);

    function updateState() {
      targetLabel.textContent = target?.label || '';
      status.textContent =
        reason || (!target ? 'Select artwork' : !visibleCount ? 'No matching effects' : '');
      apply.disabled = !snapshot || !!reason || !selectedId || composing;
      const entry = entries.find((item) => item.id === selectedId);
      const pinned = !!entry && favoriteIds.includes(entry.id);
      favorite.textContent = pinned ? '★' : '☆';
      favorite.setAttribute('aria-pressed', String(pinned));
      const favoriteLabel = entry
        ? `${pinned ? 'Remove' : 'Add'} ${entry.label} ${pinned ? 'from' : 'to'} favorites`
        : 'Add to favorites';
      favorite.setAttribute('aria-label', favoriteLabel);
      favorite.title = favoriteLabel;
      favorite.disabled = !entry || composing;
      favoriteStatus.textContent = favoriteError;
    }
    function captureTarget() {
      target = getTarget(selectedId);
      snapshot = target?.snapshot;
      reason = target?.reason;
      targetVersion = getTargetVersion?.();
      targetSelectedId = selectedId;
      targetInitialized = true;
    }
    function refreshTarget() {
      if (!panel.isConnected || composing) return;
      const version = getTargetVersion?.();
      if (
        getTargetVersion &&
        sameVersion(version, targetVersion) &&
        targetSelectedId === selectedId
      )
        return;
      entries = readEntries();
      refresh(true);
    }
    function refresh(forceTarget = false) {
      const visible = entries
        .filter(matches)
        .sort((a, b) => Number(favoriteIds.includes(b.id)) - Number(favoriteIds.includes(a.id)));
      visibleCount = visible.length;
      if (!visible.some((entry) => entry.id === selectedId)) selectedId = visible[0]?.id || null;
      if (forceTarget || !targetInitialized || selectedId !== targetSelectedId) captureTarget();
      const existing = new Map([...list.children].map((row) => [row.dataset.effectId, row]));
      for (const [index, entry] of visible.entries()) {
        let row = existing.get(entry.id);
        if (!row) {
          row = document.createElement('button');
          row.type = 'button';
          row.dataset.effectId = entry.id;
          row.addEventListener('click', () => {
            selectedId = entry.id;
            captureTarget();
            for (const button of list.querySelectorAll('[data-effect-id]'))
              button.setAttribute('aria-pressed', String(button.dataset.effectId === selectedId));
            updateState();
          });
        }
        row.textContent = entry.label;
        row.setAttribute('aria-label', entry.label);
        row.dataset.favorite = String(favoriteIds.includes(entry.id));
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
      favorite.disabled = true;
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
    favorite.addEventListener('click', () => {
      if (
        currentView?.panel !== panel ||
        !panel.isConnected ||
        panel.closest('[hidden], [inert]') ||
        favorite.disabled ||
        composing
      )
        return;
      const entry = entries.find((item) => item.id === selectedId);
      const wasPinned = entry && favoriteIds.includes(entry.id);
      if (!entry || !readFavorites()) {
        updateState();
        return;
      }
      // Another window may already have performed the requested change.
      // Refresh the shown state instead of reversing that change.
      if (favoriteIds.includes(entry.id) !== wasPinned) {
        refresh();
        return;
      }
      const next = favoriteIds.includes(entry.id)
        ? favoriteIds.filter((id) => id !== entry.id)
        : [...favoriteIds, entry.id];
      try {
        if (!validFavoriteIds(next)) throw Error('Invalid favorites');
        favoriteStorage.setItem(EFFECT_FAVORITES_KEY, JSON.stringify(next));
        favoriteIds = next;
      } catch {
        favoriteError = 'Effect favorites could not be saved';
      }
      refresh();
    });
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
