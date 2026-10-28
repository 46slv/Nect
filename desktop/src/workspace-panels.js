// Workspace geometry is view state, never an authored Session mutation.
export const WORKSPACE_PREFERENCES_KEY = 'nect.workspace-panels.v1';

export function layerHeightBounds(sidebarHeight, toolHeight = 0) {
  const height = Number.isFinite(sidebarHeight) ? sidebarHeight : 500;
  const maximum = Math.max(96, height - Math.max(0, toolHeight) - 134);
  return { minimum: 96, maximum };
}

export function clampLayerHeight(value, sidebarHeight, toolHeight = 0) {
  const { minimum, maximum } = layerHeightBounds(sidebarHeight, toolHeight);
  return Math.max(minimum, Math.min(maximum, Number.isFinite(value) ? value : 160));
}

export function createWorkspacePanels({
  getAssetPanel,
  onAssets,
  onAlign,
  onGeometry = () => {},
  storage,
}) {
  let root,
    observer,
    view,
    preferredHeight = 160,
    resizing = null;
  try {
    const saved = JSON.parse(storage?.getItem(WORKSPACE_PREFERENCES_KEY) || '{}');
    if (Number.isFinite(saved.layersHeight))
      preferredHeight = Math.max(96, Math.min(1200, saved.layersHeight));
  } catch {
    /* Unavailable or stale preferences must not prevent editing. */
  }

  function save() {
    try {
      storage?.setItem(
        WORKSPACE_PREFERENCES_KEY,
        JSON.stringify({ layersHeight: preferredHeight }),
      );
    } catch {
      /* Workspace preferences are optional, unlike document saves. */
    }
  }

  function refresh() {
    if (!root?.isConnected) return;
    const workspace = root.querySelector('.workspace');
    const right = root.querySelector('.right');
    const layers = right?.querySelector('.layers');
    const slot = right?.querySelector('[data-workspace-asset-slot]');
    const separator = right?.querySelector('[data-layers-resize]');
    if (!workspace || !right || !layers || !slot || !separator) return;
    const panel = getAssetPanel();
    workspace.classList.toggle('has-workspace-assets', !!panel);
    slot.hidden = !panel;
    const available = right.clientHeight || 500;
    const toolHeight = panel ? Math.min(280, Math.max(144, available * 0.36)) : 0;
    slot.style.height = `${toolHeight}px`;
    const alignmentHeight =
      right.querySelector('.alignment-panel')?.getBoundingClientRect().height || 0;
    const height = clampLayerHeight(preferredHeight, available, toolHeight + alignmentHeight);
    layers.style.height = `${height}px`;
    const limits = layerHeightBounds(available, toolHeight + alignmentHeight);
    separator.setAttribute('aria-valuemin', String(Math.round(limits.minimum)));
    separator.setAttribute('aria-valuemax', String(Math.round(limits.maximum)));
    separator.setAttribute('aria-valuenow', String(Math.round(height)));
    separator.setAttribute('aria-valuetext', `Layers height ${Math.round(height)} pixels`);
    root
      .querySelector('[data-workspace-panel="assets"]')
      ?.setAttribute('aria-pressed', String(!!panel));
    root
      .querySelector('[data-workspace-panel="align"]')
      ?.setAttribute('aria-pressed', String(!!right.querySelector('.alignment-panel')));
    // Keep the library's live DOM outside render()'s replaceable #app. Its async
    // reads, selection, focus and native file chooser retain their identities.
    if (panel) {
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
    onGeometry();
  }

  function attach(nextRoot) {
    observer?.disconnect();
    view?.removeEventListener('resize', refresh);
    if (resizing) {
      preferredHeight = resizing.previous;
      resizing = null;
    }
    root = nextRoot;
    view = root.ownerDocument.defaultView;
    // A fixed-width sidebar can move without resizing (e.g. 1000 → 900 px).
    // ResizeObserver alone cannot keep the body-mounted portal aligned.
    view?.addEventListener('resize', refresh);
    const workspace = root.querySelector('.workspace');
    const right = root.querySelector('.right');
    const layers = right?.querySelector('.layers');
    const inspector = right?.querySelector('.inspector');
    if (!workspace || !right || !layers || !inspector) return;
    workspace.classList.add('has-workspace-panels');
    let slot = right.querySelector('[data-workspace-asset-slot]');
    if (!slot) {
      slot = root.ownerDocument.createElement('div');
      slot.dataset.workspaceAssetSlot = '';
      slot.className = 'workspace-asset-slot';
      slot.hidden = true;
      right.insertBefore(slot, inspector);
    }
    let separator = right.querySelector('[data-layers-resize]');
    if (!separator) {
      separator = root.ownerDocument.createElement('div');
      separator.dataset.layersResize = '';
      separator.className = 'layers-resize';
      separator.tabIndex = 0;
      separator.setAttribute('role', 'separator');
      separator.setAttribute('aria-label', 'Resize Layers');
      separator.setAttribute('aria-orientation', 'horizontal');
      layers.after(separator);
      separator.addEventListener('pointerdown', (event) => {
        if (event.button !== 0) return;
        event.preventDefault();
        event.stopPropagation();
        resizing = {
          id: event.pointerId,
          y: event.clientY,
          height: layers.getBoundingClientRect().height,
          previous: preferredHeight,
        };
        separator.setPointerCapture?.(event.pointerId);
      });
      separator.addEventListener('pointermove', (event) => {
        if (!resizing || resizing.id !== event.pointerId) return;
        event.preventDefault();
        preferredHeight = resizing.height + event.clientY - resizing.y;
        refresh();
      });
      const finish = (event) => {
        if (!resizing || resizing.id !== event.pointerId) return;
        if (event.type === 'pointercancel') preferredHeight = resizing.previous;
        else preferredHeight = Number(separator.getAttribute('aria-valuenow'));
        resizing = null;
        if (separator.hasPointerCapture?.(event.pointerId))
          separator.releasePointerCapture(event.pointerId);
        refresh();
        if (event.type !== 'pointercancel') save();
      };
      separator.addEventListener('pointerup', finish);
      separator.addEventListener('pointercancel', finish);
      separator.addEventListener('keydown', (event) => {
        const current = Number(separator.getAttribute('aria-valuenow'));
        const next = {
          ArrowUp: current - 16,
          ArrowDown: current + 16,
          Home: Number(separator.getAttribute('aria-valuemin')),
          End: Number(separator.getAttribute('aria-valuemax')),
        }[event.key];
        if (!Number.isFinite(next) || event.altKey || event.ctrlKey || event.metaKey) return;
        event.preventDefault();
        event.stopPropagation();
        preferredHeight = next;
        refresh();
        preferredHeight = Number(separator.getAttribute('aria-valuenow'));
        save();
      });
    }
    let bar = workspace.querySelector('.workspace-panel-bar');
    if (!bar) {
      bar = root.ownerDocument.createElement('nav');
      bar.className = 'workspace-panel-bar';
      bar.setAttribute('aria-label', 'Workspace panels');
      for (const [id, label, glyph, action] of [
        ['layers', 'Focus Layers', '▤', () => focus('.layers')],
        ['assets', 'Toggle Assets panel', '▧', onAssets],
        ['align', 'Toggle Alignment panel', '↔', onAlign],
        ['inspector', 'Focus Inspector', '≡', () => focus('.inspector')],
      ]) {
        const button = root.ownerDocument.createElement('button');
        button.type = 'button';
        button.dataset.workspacePanel = id;
        button.textContent = glyph;
        button.title = label;
        button.setAttribute('aria-label', label);
        if (id === 'assets' || id === 'align') button.setAttribute('aria-pressed', 'false');
        button.addEventListener('click', (event) => {
          event.stopPropagation();
          action();
          refresh();
        });
        bar.append(button);
      }
      workspace.append(bar);
    }
    refresh();
    const ResizeObserver = root.ownerDocument.defaultView?.ResizeObserver;
    if (ResizeObserver) {
      observer = new ResizeObserver(refresh);
      observer.observe(right);
      observer.observe(workspace);
    }
  }

  function focus(selector) {
    const element = root?.querySelector(selector);
    if (!element) return;
    element.tabIndex = -1;
    element.focus({ preventScroll: true });
  }
  return {
    attach,
    refresh,
    get layersHeight() {
      return preferredHeight;
    },
  };
}
