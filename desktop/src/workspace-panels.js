// Workspace controls are view state only. The real panels and commands keep their owners.
export const WORKSPACE_PREFERENCES_KEY = 'nect.workspace-panels.v1';
export function layerHeightBounds(sidebarHeight, toolHeight = 0) {
  const height = Number.isFinite(sidebarHeight) ? sidebarHeight : 500;
  return { minimum: 96, maximum: Math.max(96, height - Math.max(0, toolHeight) - 134) };
}
export function clampLayerHeight(value, sidebarHeight, toolHeight = 0) {
  const { minimum, maximum } = layerHeightBounds(sidebarHeight, toolHeight);
  return Math.max(minimum, Math.min(maximum, Number.isFinite(value) ? value : 160));
}

// Definitions are deliberately small: each item binds an existing panel owner, not document state.
const ITEMS = [
  { id: 'assets', side: 'left', label: 'Toggle Assets panel', glyph: '▧' },
  { id: 'colors', side: 'left', label: 'Toggle Shared Colors panel', glyph: '●' },
  { id: 'layers', side: 'right', label: 'Focus Layers', glyph: '▤', selector: '.layers' },
  { id: 'align', side: 'right', label: 'Toggle Alignment panel', glyph: '↔', selector: '.alignment-panel' },
  { id: 'inspector', side: 'right', label: 'Focus Inspector', glyph: '≡', selector: '.inspector' },
];
const SHAPE = 'M0 0 H10 C10 30 12 49 34 68 C51 83 51 145 34 160 C12 179 10 198 10 228 H0 Z';

export function createWorkspacePanels({ getAssetPanel, onAssets, onAlign, onColors }) {
  let root, view, observer, active = { left: null, right: 'inspector' }, retreated = false;
  const item = (id) => ITEMS.find((entry) => entry.id === id);
  const trigger = (id) => root?.querySelector(`[data-workspace-panel="${id}"]`);
  function refresh() {
    if (!root?.isConnected) return;
    const wrap = root.querySelector('.canvas-wrap');
    if (!wrap) return;
    const stage = wrap.querySelector('.canvas-stage');
    if (stage) {
      wrap.style.setProperty('--stage-left', `${stage.offsetLeft}px`);
      wrap.style.setProperty('--stage-right', `${Math.max(0, wrap.clientWidth - stage.offsetLeft - stage.clientWidth)}px`);
      wrap.style.setProperty('--stage-width', `${stage.clientWidth}px`);
    }
    if (stage) for (const overlay of wrap.querySelectorAll('.workspace-drawer, .workspace-panel-bar')) {
      overlay.style.top = `${stage.offsetTop}px`;
      overlay.style.height = `${stage.clientHeight}px`;
    }
    for (const side of ['left', 'right']) {
      const host = wrap.querySelector(`[data-drawer-side="${side}"]`);
      const selected = item(active[side]);
      const shown = !!selected && !(side === 'left' && retreated);
      host.dataset.activePanel = selected?.id || '';
      host.hidden = !shown;
      host.inert = !shown;
    }
    for (const entry of ITEMS) {
      const button = trigger(entry.id);
      button?.setAttribute('aria-expanded', String(active[entry.side] === entry.id));
      button?.setAttribute('aria-pressed', String(active[entry.side] === entry.id));
      if (entry.selector) {
        const panel = wrap.querySelector(entry.selector);
        if (panel) panel.hidden = active[entry.side] !== entry.id;
      }
    }
    const colors = wrap.querySelector('.shared-colors-panel');
    if (colors) colors.hidden = active.left !== 'colors';
    const slot = wrap.querySelector('[data-workspace-asset-slot]');
    const panel = getAssetPanel();
    if (slot) slot.hidden = active.left !== 'assets';
    if (panel) {
      const visible = active.left === 'assets' && slot && !slot.hidden;
      panel.hidden = !visible;
      panel.inert = !visible;
      panel.style.opacity = retreated ? '0' : '';
      panel.style.pointerEvents = retreated ? 'none' : '';
      if (visible) {
        const rect = slot.getBoundingClientRect();
        panel.classList.add('asset-library-docked');
        Object.assign(panel.style, {
          left: `${rect.left}px`, top: `${rect.top}px`, width: `${rect.width}px`,
          height: `${rect.height}px`, right: 'auto', bottom: 'auto',
        });
      }
    }
  }
  function select(id, { focus = true } = {}) {
    const entry = item(id);
    if (!entry) return;
    if (active[entry.side] === id) return close(id, focus);
    active[entry.side] = id;
    retreated = false;
    // These callbacks may synchronously render; set active before invoking them.
    if (id === 'assets' && !getAssetPanel()) onAssets();
    if (id === 'align') onAlign();
    if (id === 'colors' && !root.querySelector('.shared-colors-panel')) onColors?.();
    refresh();
    if (focus && id !== 'assets') {
      const target = root.querySelector(entry.selector || '.shared-colors-panel');
      if (target) { target.tabIndex = -1; target.focus({ preventScroll: true }); }
    }
  }
  function close(id, focus = true) {
    const entry = item(id);
    if (!entry || active[entry.side] !== id) return;
    active[entry.side] = null;
    retreated = false;
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
    wrap.append(tools, right);
    right.classList.add('workspace-drawer');
    right.dataset.drawerSide = 'right';
    right.setAttribute('aria-label', 'Editing panels');
    const left = root.ownerDocument.createElement('aside');
    left.className = 'workspace-drawer workspace-drawer-left';
    left.dataset.drawerSide = 'left';
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
        button.addEventListener('click', (event) => { event.stopPropagation(); select(entry.id); });
        bar.append(button);
      }
      wrap.append(bar);
    }
    view?.addEventListener('resize', refresh);
    root.ownerDocument.addEventListener('keydown', onKeydown);
    const ResizeObserver = view?.ResizeObserver;
    if (ResizeObserver) { observer = new ResizeObserver(refresh); observer.observe(wrap); }
    refresh();
  }
  function onKeydown(event) {
    if (event.key !== 'Escape' || event.defaultPrevented || event.isComposing ||
        event.target.closest?.('dialog[open], input, textarea, select, [contenteditable="true"]')) return;
    const drawer = event.target.closest?.('[data-drawer-side]');
    const rail = event.target.closest?.('.workspace-panel-bar');
    const side = drawer?.dataset.drawerSide || (rail?.classList.contains('workspace-panel-bar-left') ? 'left' : rail ? 'right' : null);
    const id = side ? active[side] : null;
    if (id) { event.preventDefault(); close(id); }
  }
  function setRetreat(value) { retreated = !!value && active.left === 'assets'; refresh(); }
  return { attach, refresh, select, close, setRetreat, get active() { return active; } };
}
