export const WORKSPACE_SURFACES = Object.freeze(
  [
    { id: 'assets', side: 'left', label: 'Toggle Assets panel', kind: 'portal' },
    { id: 'colors', side: 'left', label: 'Toggle Shared Colors panel' },
    {
      id: 'layers',
      side: 'right',
      label: 'Focus Layers',
      selector: '.layers',
      coVisible: 'editing',
      split: Object.freeze({ defaultPercent: 38, minPixels: 80, label: 'Layers' }),
    },
    { id: 'align', side: 'right', label: 'Toggle Alignment panel', selector: '.alignment-panel' },
    {
      id: 'inspector',
      side: 'right',
      label: 'Focus Inspector',
      selector: '.inspector',
      coVisible: 'editing',
    },
  ].map(Object.freeze),
);

// The portal remains under body. Hiding it must not dispose its feature owner.
export function createAssetsPortalAdapter({ getAssetPanel, onAssets }) {
  if (typeof getAssetPanel !== 'function' || typeof onAssets !== 'function')
    throw Error('Invalid Assets portal binding');
  return Object.freeze({
    fixedSide: 'left',
    canRetreat: true,
    attach(host) {
      const slot = host.ownerDocument.createElement('div');
      slot.dataset.workspaceAssetSlot = '';
      slot.className = 'workspace-asset-slot';
      host.append(slot);
    },
    setMotion(enabled) {
      const panel = getAssetPanel();
      if (panel) panel.dataset.motionEnabled = String(enabled);
    },
    contains(element) {
      return !!getAssetPanel()?.contains(element);
    },
    open() {
      if (!getAssetPanel()) onAssets();
    },
    restore() {
      if (!getAssetPanel()) onAssets(false);
    },
    focus() {
      return getAssetPanel()?.querySelector('[data-asset-search]');
    },
    refresh({ root, wrap, active, displayed, viewerMode, retreated, id }) {
      const slot = wrap.querySelector('[data-workspace-asset-slot]');
      const panel = getAssetPanel();
      if (slot) slot.hidden = displayed !== id;
      if (!panel) return;
      const visible = active === id && !viewerMode && slot && !slot.hidden;
      if (viewerMode && panel.contains(root.ownerDocument.activeElement))
        wrap
          .querySelector('.viewer-tabs [data-viewer-mode="viewers"]')
          ?.focus({ preventScroll: true });
      panel.dataset.motionSuppressed = String(retreated || viewerMode);
      panel.inert = !visible;
      panel.style.opacity = retreated ? '0' : '';
      panel.style.pointerEvents = retreated ? 'none' : '';
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
      panel.hidden = !visible;
    },
  });
}

// Temporary legacy bindings preserve the current owners and placement semantics.
export function createWorkspaceSurfaceAdapters({
  getAssetPanel,
  onAssets,
  onAlign,
  onColors,
  createColorsView,
  canDeactivateColors,
}) {
  if (
    typeof onAlign !== 'function' ||
    (onColors !== undefined && typeof onColors !== 'function') ||
    (createColorsView !== undefined && typeof createColorsView !== 'function') ||
    (canDeactivateColors !== undefined && typeof canDeactivateColors !== 'function')
  )
    throw Error('Invalid legacy surface binding');
  const colorsSelector = '.shared-colors-panel';
  const selector = (css, fixedSide, open = () => {}, restore = () => {}, hooks = {}) =>
    Object.freeze({
      fixedSide,
      open,
      restore,
      ...hooks,
      focus({ root }) {
        const target = root.querySelector(css);
        if (target) target.tabIndex = -1;
        return target;
      },
    });
  return Object.freeze({
    assets: createAssetsPortalAdapter({ getAssetPanel, onAssets }),
    colors: selector(
      colorsSelector,
      'left',
      () => onColors?.(),
      ({ root }) => {
        if (!root.querySelector(colorsSelector)) onColors?.();
      },
      {
        canDeactivate: () => canDeactivateColors?.() !== false,
        attach(host, { right }) {
          const colors = createColorsView
            ? createColorsView()
            : right.querySelector(colorsSelector);
          if (colors) host.append(colors);
        },
        refresh({ wrap, displayed, id }) {
          const colors = wrap.querySelector(colorsSelector);
          if (colors) colors.hidden = displayed !== id;
        },
      },
    ),
    layers: selector('.layers', 'right'),
    align: selector(
      '.alignment-panel',
      'right',
      () => onAlign(),
      () => onAlign(),
      {
        refresh({ root, active, id }) {
          root
            .querySelector('.canvas-toolbar [data-action="align-panel"]')
            ?.setAttribute('aria-pressed', String(active === id));
        },
      },
    ),
    inspector: selector('.inspector', 'right'),
  });
}

export function resolveWorkspaceSurfaces(declarations, adapters) {
  if (!Array.isArray(declarations) || !adapters || typeof adapters !== 'object')
    throw Error('Invalid workspace surfaces');
  const ids = new Set();
  const resolved = declarations.map((item) => {
    if (
      !item ||
      typeof item.id !== 'string' ||
      !/^[a-z][a-z0-9-]*$/.test(item.id) ||
      ids.has(item.id) ||
      !['left', 'right'].includes(item.side) ||
      (item.kind !== undefined && item.kind !== 'portal') ||
      typeof item.label !== 'string' ||
      !item.label ||
      (item.selector !== undefined &&
        (typeof item.selector !== 'string' || !/^\.[a-z][a-z0-9-]*$/.test(item.selector))) ||
      (item.coVisible !== undefined &&
        (typeof item.coVisible !== 'string' ||
          !/^[a-z][a-z0-9-]*$/.test(item.coVisible) ||
          !item.selector)) ||
      (item.split !== undefined &&
        (!item.coVisible ||
          !item.split ||
          typeof item.split !== 'object' ||
          Array.isArray(item.split) ||
          !Number.isFinite(item.split.defaultPercent) ||
          item.split.defaultPercent <= 0 ||
          item.split.defaultPercent >= 100 ||
          !Number.isFinite(item.split.minPixels) ||
          item.split.minPixels <= 0 ||
          typeof item.split.label !== 'string' ||
          !item.split.label))
    )
      throw Error('Invalid workspace surface declaration');
    ids.add(item.id);
    const adapter = Object.hasOwn(adapters, item.id) && adapters[item.id];
    if (
      !adapter ||
      typeof adapter !== 'object' ||
      !['left', 'right'].includes(adapter.fixedSide) ||
      adapter.fixedSide !== item.side ||
      (adapter.canRetreat !== undefined && typeof adapter.canRetreat !== 'boolean') ||
      [
        'attach',
        'setMotion',
        'contains',
        'open',
        'restore',
        'focus',
        'refresh',
        'canDeactivate',
      ].some((key) => adapter[key] !== undefined && typeof adapter[key] !== 'function') ||
      typeof adapter.open !== 'function' ||
      typeof adapter.restore !== 'function' ||
      typeof adapter.focus !== 'function'
    )
      throw Error(`Invalid workspace surface adapter: ${item.id}`);
    if (
      item.kind === 'portal' &&
      (typeof adapter.canRetreat !== 'boolean' ||
        ['attach', 'setMotion', 'contains', 'refresh'].some(
          (key) => typeof adapter[key] !== 'function',
        ))
    )
      throw Error(`Invalid workspace portal adapter: ${item.id}`);
    return Object.freeze({ ...item, adapter });
  });
  if (Object.keys(adapters).some((id) => !ids.has(id)))
    throw Error('Unknown workspace surface adapter');
  for (const entry of resolved.filter((item) => item.coVisible)) {
    const group = resolved.filter((item) => item.coVisible === entry.coVisible);
    if (
      group.length < 2 ||
      group.filter((item) => item.split).length > 1 ||
      (entry.split && group.length !== 2) ||
      new Set(group.map((item) => item.selector)).size !== group.length ||
      group.some((item) => item.side !== entry.side)
    )
      throw Error('Invalid co-visible workspace group');
  }
  return Object.freeze(resolved);
}
