// Workspace controls are view state only. The real panels and commands keep their owners.
export const DRAWER_PREFERENCES_KEY = 'nect.workspace-drawers.v1';

// Definitions are deliberately small: each item binds an existing panel owner, not document state.
const ITEMS = [
  { id: 'assets', side: 'left', label: 'Toggle Assets panel', glyph: '▧' },
  { id: 'colors', side: 'left', label: 'Toggle Shared Colors panel', glyph: '●' },
  { id: 'layers', side: 'right', label: 'Focus Layers', glyph: '▤', selector: '.layers' },
  {
    id: 'align',
    side: 'right',
    label: 'Toggle Alignment panel',
    glyph: '↔',
    selector: '.alignment-panel',
  },
  { id: 'inspector', side: 'right', label: 'Focus Inspector', glyph: '≡', selector: '.inspector' },
];
const SHAPE = 'M0 0 H10 C10 30 12 49 34 68 C51 83 51 145 34 160 C12 179 10 198 10 228 H0 Z';

export function createWorkspacePanels({ getAssetPanel, onAssets, onAlign, onColors, storage }) {
  let root,
    view,
    observer,
    active = { left: null, right: 'inspector' },
    retreated = false;
  let attachVersion = 0,
    restorePending = true;
  try {
    const saved = JSON.parse(storage?.getItem(DRAWER_PREFERENCES_KEY) || 'null');
    if (saved && typeof saved === 'object') {
      for (const side of ['left', 'right']) {
        if (
          saved[side] === null ||
          ITEMS.some((entry) => entry.side === side && entry.id === saved[side])
        )
          active[side] = saved[side];
      }
    }
  } catch {
    /* Bad or obsolete preferences never prevent editing. */
  }
  function save() {
    try {
      storage?.setItem(DRAWER_PREFERENCES_KEY, JSON.stringify(active));
    } catch {
      /* View preferences are optional. */
    }
  }
  const item = (id) => ITEMS.find((entry) => entry.id === id);
  const trigger = (id) => root?.querySelector(`[data-workspace-panel="${id}"]`);
  function refresh() {
    if (!root?.isConnected) return;
    const wrap = root.querySelector('.canvas-wrap');
    if (!wrap) return;
    const viewerMode = wrap.dataset.viewerMode === 'viewers';
    const stage = wrap.querySelector(viewerMode ? '.node-viewers' : '.canvas-stage');
    if (stage) {
      wrap.style.setProperty('--stage-left', `${stage.offsetLeft}px`);
      wrap.style.setProperty(
        '--stage-right',
        `${Math.max(0, wrap.clientWidth - stage.offsetLeft - stage.clientWidth)}px`,
      );
      wrap.style.setProperty('--stage-width', `${stage.clientWidth}px`);
    }
    if (stage) {
      const toolbar = viewerMode ? null : wrap.querySelector('.canvas-toolbar');
      const top = Math.max(
        stage.offsetTop,
        toolbar
          ? toolbar.getBoundingClientRect().bottom - wrap.getBoundingClientRect().top + 8
          : stage.offsetTop,
      );
      const height = Math.max(0, stage.offsetTop + stage.clientHeight - top);
      wrap.style.setProperty('--drawer-top', `${top}px`);
      wrap.style.setProperty('--drawer-height', `${height}px`);
      for (const overlay of wrap.querySelectorAll('.workspace-drawer, .workspace-panel-bar')) {
        overlay.style.top = `${top}px`;
        overlay.style.height = `${height}px`;
      }
    }
    for (const side of ['left', 'right']) {
      const host = wrap.querySelector(`[data-drawer-side="${side}"]`);
      const selected = item(active[side]);
      const shown = !!selected && !(side === 'left' && (retreated || viewerMode));
      const bar = wrap.querySelector(`.workspace-panel-bar-${side}`);
      if (
        viewerMode &&
        side === 'left' &&
        (host.contains(root.ownerDocument.activeElement) ||
          bar?.contains(root.ownerDocument.activeElement))
      )
        wrap
          .querySelector('.viewer-tabs [data-viewer-mode="viewers"]')
          ?.focus({ preventScroll: true });
      if (bar) {
        bar.hidden = viewerMode && side === 'left';
        bar.inert = bar.hidden;
      }
      host.dataset.activePanel = selected?.id || '';
      host.hidden = !shown;
      host.inert = !shown;
    }
    for (const entry of ITEMS) {
      const button = trigger(entry.id);
      const shown = active[entry.side] === entry.id && !(viewerMode && entry.side === 'left');
      button?.setAttribute('aria-expanded', String(shown));
      button?.setAttribute('aria-pressed', String(shown));
      if (entry.selector) {
        const panel = wrap.querySelector(entry.selector);
        if (panel) panel.hidden = active[entry.side] !== entry.id;
      }
    }
    root
      .querySelector('.canvas-toolbar [data-action="align-panel"]')
      ?.setAttribute('aria-pressed', String(active.right === 'align'));
    const colors = wrap.querySelector('.shared-colors-panel');
    if (colors) colors.hidden = active.left !== 'colors';
    const slot = wrap.querySelector('[data-workspace-asset-slot]');
    const panel = getAssetPanel();
    if (slot) slot.hidden = active.left !== 'assets';
    if (panel) {
      const visible = active.left === 'assets' && !viewerMode && slot && !slot.hidden;
      if (viewerMode && panel.contains(root.ownerDocument.activeElement))
        wrap
          .querySelector('.viewer-tabs [data-viewer-mode="viewers"]')
          ?.focus({ preventScroll: true });
      panel.hidden = !visible;
      panel.inert = !visible;
      panel.style.opacity = retreated ? '0' : '';
      panel.style.pointerEvents = retreated ? 'none' : '';
      // Keep the last valid portal rectangle while the ancestor is hidden during native drag.
      if (visible && !retreated) {
        const rect = slot.getBoundingClientRect();
        panel.classList.add('asset-library-docked');
        Object.assign(panel.style, {
          left: `${rect.left}px`,
          top: `${rect.top}px`,
          width: `${rect.width}px`,
          height: `${rect.height}px`,
          right: 'auto',
          bottom: 'auto',
        });
      }
    }
  }
  function select(id, { focus = true } = {}) {
    const entry = item(id);
    if (!entry) return;
    if (active[entry.side] === id) return close(id, focus);
    active[entry.side] = id;
    save();
    if (entry.side === 'left') retreated = false;
    // These callbacks may synchronously render; set active before invoking them.
    if (id === 'assets' && !getAssetPanel()) onAssets();
    if (id === 'align') onAlign();
    if (id === 'colors') onColors?.();
    refresh();
    if (focus) {
      const target =
        id === 'assets'
          ? getAssetPanel()?.querySelector('[data-asset-search]')
          : root.querySelector(entry.selector || '.shared-colors-panel');
      if (target) {
        if (id !== 'assets') target.tabIndex = -1;
        target.focus({ preventScroll: true });
      }
    }
  }
  function close(id, focus = true) {
    const entry = item(id);
    if (!entry || active[entry.side] !== id) return;
    active[entry.side] = null;
    save();
    if (entry.side === 'left') retreated = false;
    // Preserve the Assets DOM and in-flight work across panel switches/close.
    refresh();
    if (focus) trigger(id)?.focus({ preventScroll: true });
  }
  function attach(nextRoot) {
    observer?.disconnect();
    view?.removeEventListener('resize', refresh);
    root?.ownerDocument.removeEventListener('keydown', onKeydown);
    root = nextRoot;
    view = root.ownerDocument.defaultView;
    const wrap = root.querySelector('.canvas-wrap');
    const right = root.querySelector('.right');
    const tools = root.querySelector('.tools');
    if (!wrap || !right || !tools) return;
    if (wrap.querySelector('.workspace-panel-bar')) {
      view?.addEventListener('resize', refresh);
      root.ownerDocument.addEventListener('keydown', onKeydown);
      const ResizeObserver = view?.ResizeObserver;
      if (ResizeObserver) {
        observer = new ResizeObserver(refresh);
        observer.observe(wrap);
      }
      refresh();
      return;
    }
    wrap.append(tools, right);
    right.classList.add('workspace-drawer');
    right.dataset.drawerSide = 'right';
    right.id = 'workspace-drawer-right';
    right.setAttribute('aria-label', 'Editing panels');
    const left = root.ownerDocument.createElement('aside');
    left.className = 'workspace-drawer workspace-drawer-left';
    left.dataset.drawerSide = 'left';
    left.id = 'workspace-drawer-left';
    left.setAttribute('aria-label', 'Assets and colors');
    const slot = root.ownerDocument.createElement('div');
    slot.dataset.workspaceAssetSlot = '';
    slot.className = 'workspace-asset-slot';
    left.append(slot);
    const colors = right.querySelector('.shared-colors-panel');
    if (colors) left.append(colors);
    wrap.append(left);
    for (const side of ['left', 'right']) {
      const bar = root.ownerDocument.createElement('nav');
      bar.className = `workspace-panel-bar workspace-panel-bar-${side}`;
      bar.setAttribute('aria-label', `${side === 'left' ? 'Assets and colors' : 'Editing'} panels`);
      bar.innerHTML = `<svg viewBox="0 0 58 228" preserveAspectRatio="none" aria-hidden="true"><path d="${SHAPE}"></path></svg>`;
      for (const entry of ITEMS.filter((entry) => entry.side === side)) {
        const button = root.ownerDocument.createElement('button');
        button.type = 'button';
        button.dataset.workspacePanel = entry.id;
        button.textContent = entry.glyph;
        button.title = entry.label;
        button.setAttribute('aria-label', entry.label);
        button.setAttribute('aria-controls', `workspace-drawer-${side}`);
        button.addEventListener('click', (event) => {
          event.stopPropagation();
          select(entry.id);
        });
        bar.append(button);
      }
      wrap.append(bar);
    }
    view?.addEventListener('resize', refresh);
    root.ownerDocument.addEventListener('keydown', onKeydown);
    const ResizeObserver = view?.ResizeObserver;
    if (ResizeObserver) {
      observer = new ResizeObserver(refresh);
      observer.observe(wrap);
    }
    refresh();
    // Restore only real owners, after render has completed. A newer close or
    // attach wins; transient drag/focus state is never persisted.
    const version = ++attachVersion;
    if (restorePending)
      queueMicrotask(() => {
        if (version !== attachVersion || !restorePending) return;
        restorePending = false;
        if (active.left === 'assets' && !getAssetPanel()) onAssets(false);
        if (active.left === 'colors' && !root.querySelector('.shared-colors-panel')) onColors?.();
        if (active.right === 'align') onAlign();
        refresh();
      });
  }
  function onKeydown(event) {
    if (
      event.key !== 'Escape' ||
      event.defaultPrevented ||
      event.isComposing ||
      event.target.closest?.('dialog[open], input, textarea, select, [contenteditable="true"]')
    )
      return;
    const drawer = event.target.closest?.('[data-drawer-side]');
    const rail = event.target.closest?.('.workspace-panel-bar');
    const side =
      drawer?.dataset.drawerSide ||
      (rail?.classList.contains('workspace-panel-bar-left') ? 'left' : rail ? 'right' : null);
    const id = side ? active[side] : null;
    if (id) {
      event.preventDefault();
      close(id);
    }
  }
  function setRetreat(value) {
    retreated = !!value && active.left === 'assets';
    refresh();
  }
  return {
    attach,
    refresh,
    select,
    close,
    setRetreat,
    get active() {
      return active;
    },
  };
}
