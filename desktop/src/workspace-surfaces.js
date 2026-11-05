export const WORKSPACE_SURFACES = Object.freeze(
  [
    { id: 'assets', side: 'left', label: 'Toggle Assets panel', kind: 'portal' },
    { id: 'colors', side: 'left', label: 'Toggle Shared Colors panel' },
    { id: 'layers', side: 'right', label: 'Focus Layers', selector: '.layers' },
    { id: 'align', side: 'right', label: 'Toggle Alignment panel', selector: '.alignment-panel' },
    { id: 'inspector', side: 'right', label: 'Focus Inspector', selector: '.inspector' },
  ].map(Object.freeze),
);

// The portal remains under body. Hiding it must not dispose its feature owner.
export function createAssetsPortalAdapter({ getAssetPanel, onAssets }) {
  if (typeof getAssetPanel !== 'function' || typeof onAssets !== 'function')
    throw Error('Invalid Assets portal binding');
  return Object.freeze({
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
export function createWorkspaceSurfaceAdapters({ getAssetPanel, onAssets, onAlign, onColors }) {
  if (typeof onAlign !== 'function' || (onColors !== undefined && typeof onColors !== 'function'))
    throw Error('Invalid legacy surface binding');
  const selector = (css, open = () => {}, restore = () => {}) =>
    Object.freeze({
      open,
      restore,
      focus({ root }) {
        const target = root.querySelector(css);
        if (target) target.tabIndex = -1;
        return target;
      },
    });
  return Object.freeze({
    assets: createAssetsPortalAdapter({ getAssetPanel, onAssets }),
    colors: selector(
      '.shared-colors-panel',
      () => onColors?.(),
      ({ root }) => {
        if (!root.querySelector('.shared-colors-panel')) onColors?.();
      },
    ),
    layers: selector('.layers'),
    align: selector(
      '.alignment-panel',
      () => onAlign(),
      () => onAlign(),
    ),
    inspector: selector('.inspector'),
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
        (typeof item.selector !== 'string' || !/^\.[a-z][a-z0-9-]*$/.test(item.selector)))
    )
      throw Error('Invalid workspace surface declaration');
    ids.add(item.id);
    const adapter = Object.hasOwn(adapters, item.id) && adapters[item.id];
    if (
      !adapter ||
      typeof adapter !== 'object' ||
      (adapter.canRetreat !== undefined && typeof adapter.canRetreat !== 'boolean') ||
      ['attach', 'setMotion', 'contains', 'open', 'restore', 'focus', 'refresh'].some(
        (key) => adapter[key] !== undefined && typeof adapter[key] !== 'function',
      ) ||
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
  return Object.freeze(resolved);
}
