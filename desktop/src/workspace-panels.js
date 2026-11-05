import { workspaceIcon } from './ui-icons.js';
import { WORKSPACE_SURFACES, resolveWorkspaceSurfaces } from './workspace-surfaces.js';

// Workspace controls are view state only. The real panels and commands keep their owners.
export const DRAWER_PREFERENCES_KEY = 'nect.workspace-drawers.v1';

// Rounded-tab E reference: concave corner, short tangent, convex corner.
// Optical proportions keep a 26px face and focus ring inside the surface.
// The filled strip joins the bar to its panel; the curved edge stays one path.
export function workspaceRailGeometry(height, count) {
  const h = height > 0 ? height : 228;
  const n = count === 3 ? 3 : 2;
  const first = h / 2 - (n - 1) * 16;
  const last = h / 2 + (n - 1) * 16;
  const shoulder = Math.min(35.56, Math.max(0, first));
  const factor = shoulder / 35.56;
  const x = (value) => Number((6 + value / 0.82).toFixed(3));
  const y = (value) => Number(value.toFixed(3));
  const edge = `M6 0 V${y(first - shoulder)} C6 ${y(first - 27.0 * factor)} ${x(3.95)} ${y(first - 23.06 * factor)} ${x(10.87)} ${y(first - 20.1 * factor)} L${x(15.48)} ${y(first - 18.13 * factor)} C${x(24.7)} ${y(first - 14.18 * factor)} ${x(33.6)} ${y(first - 10.3 * factor)} ${x(33.6)} ${y(first)} V${y(last)} C${x(33.6)} ${y(last + 10.3 * factor)} ${x(24.7)} ${y(last + 14.18 * factor)} ${x(15.48)} ${y(last + 18.13 * factor)} L${x(10.87)} ${y(last + 20.1 * factor)} C${x(3.95)} ${y(last + 23.06 * factor)} 6 ${y(last + 27.0 * factor)} 6 ${y(last + shoulder)} V${y(h)}`;
  return { height: h, fill: `${edge} H6 Z`, edge };
}

export function createWorkspacePanels({ storage, declarations = WORKSPACE_SURFACES, adapters }) {
  const ITEMS = resolveWorkspaceSurfaces(declarations, adapters);
  const composing = new WeakSet();
  let root,
    view,
    observer,
    active = { left: null, right: null },
    lastSide = null,
    retreated = false;
  let attachVersion = 0,
    restorePending = true,
    peekFrame = 0,
    pointerDown = false,
    pointerDragging = false,
    pointerStart = null,
    lastPointer = null,
    lastViewerMode = null;
  // Keep the last child rendered under an inert, hidden host during its visual exit.
  const displayed = { left: null, right: null };
  const motionIntent = { left: false, right: false };
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
      lastSide = saved.lastSide === 'left' || saved.lastSide === 'right' ? saved.lastSide : null;
      if (active.left && active.right) {
        const keep = lastSide || 'right';
        active[keep === 'left' ? 'right' : 'left'] = null;
        lastSide = keep;
      }
    }
  } catch {
    /* Bad or obsolete preferences never prevent editing. */
  }
  function save() {
    try {
      storage?.setItem(DRAWER_PREFERENCES_KEY, JSON.stringify({ ...active, lastSide }));
    } catch {
      /* View preferences are optional. */
    }
  }
  const item = (id) => ITEMS.find((entry) => entry.id === id);
  const shareVisibleGroup = (first, second) => {
    const a = item(first),
      b = item(second);
    return !!a?.coVisible && a.coVisible === b?.coVisible && a.side === b.side;
  };
  const trigger = (id) => root?.querySelector(`[data-workspace-panel="${id}"]`);
  const composingInRightDrawer = () => {
    const focused = root?.ownerDocument.activeElement;
    return !!focused && composing.has(focused) && !!focused.closest?.('[data-drawer-side="right"]');
  };
  const canDeactivate = (id) =>
    !composingInRightDrawer() && item(id)?.adapter.canDeactivate?.({ root }) !== false;
  function onCompositionStart(event) {
    if (event.target.closest?.('[data-drawer-side="right"]') && root?.contains(event.target))
      composing.add(event.target);
  }
  function onCompositionEnd(event) {
    composing.delete(event.target);
  }
  function blocksComposingTransition(event) {
    if (!composingInRightDrawer()) return false;
    if (
      event.target.closest?.(
        '[data-workspace-panel], [data-action="align-panel"], [data-viewer-mode]',
      )
    )
      return true;
    const focused = root.ownerDocument.activeElement;
    const focusedEntry = ITEMS.find(
      (entry) => entry.selector && root.querySelector(entry.selector)?.contains(focused),
    );
    const targetEntry = ITEMS.find(
      (entry) => entry.selector && root.querySelector(entry.selector)?.contains(event.target),
    );
    return (
      !!focusedEntry &&
      !!targetEntry &&
      focusedEntry !== targetEntry &&
      shareVisibleGroup(focusedEntry.id, targetEntry.id)
    );
  }
  function onGuardedClick(event) {
    if (!blocksComposingTransition(event)) return;
    event.preventDefault();
    event.stopImmediatePropagation();
  }
  function onFocusIn(event) {
    const entry = ITEMS.find(
      (candidate) =>
        candidate.selector &&
        shareVisibleGroup(active[candidate.side], candidate.id) &&
        root.querySelector(candidate.selector)?.contains(event.target),
    );
    if (!entry || entry.id === active[entry.side]) return;
    active[entry.side] = entry.id;
    lastSide = entry.side;
    save();
    refresh();
  }
  function setMotion(side, enabled) {
    const host = root?.querySelector(`[data-drawer-side="${side}"]`);
    if (host) host.dataset.motionEnabled = String(enabled);
    for (const entry of ITEMS.filter((entry) => entry.side === side))
      entry.adapter.setMotion?.(enabled);
  }
  function setRailFlag(name, value) {
    root?.querySelectorAll('.workspace-panel-bar').forEach((bar) => {
      const next = String(value);
      if (bar.dataset[name] !== next) bar.dataset[name] = next;
    });
  }
  function schedulePeek() {
    if (!lastPointer || !view || pointerDown) return;
    if (peekFrame) view.cancelAnimationFrame(peekFrame);
    peekFrame = view.requestAnimationFrame(() => {
      peekFrame = 0;
      if (!lastPointer) return;
      const { x, y } = lastPointer;
      root?.querySelectorAll('.workspace-panel-bar').forEach((bar) => {
        const rect = bar.getBoundingClientRect();
        const center = (rect.top + rect.bottom) / 2;
        const near =
          x >= rect.left - 24 &&
          x <= rect.right + 24 &&
          Math.abs(y - center) <= Math.min(rect.height / 2, 114) + 24;
        if (bar.dataset.near !== String(near)) bar.dataset.near = String(near);
      });
    });
  }
  function onPointerMove(event) {
    if (event.buttons && pointerDown) {
      if (
        !pointerDragging &&
        pointerStart &&
        Math.hypot(event.clientX - pointerStart.x, event.clientY - pointerStart.y) >= 4
      ) {
        pointerDragging = true;
        setRailFlag('dragging', true);
      }
      return;
    }
    if (event.pointerType !== 'mouse' && event.pointerType !== 'pen') return;
    lastPointer = { x: event.clientX, y: event.clientY };
    if (pointerDown) {
      pointerDown = false;
      pointerDragging = false;
      pointerStart = null;
      setRailFlag('dragging', retreated);
    }
    schedulePeek();
  }
  function onPointerDown(event) {
    if (blocksComposingTransition(event)) {
      event.preventDefault();
      event.stopImmediatePropagation();
      return;
    }
    if (event.isPrimary === false || !root?.contains(event.target)) return;
    if (event.pointerType === 'touch') {
      lastPointer = null;
      setRailFlag('near', false);
    }
    pointerDown = true;
    pointerDragging = false;
    pointerStart = { x: event.clientX, y: event.clientY };
    if (peekFrame) view?.cancelAnimationFrame(peekFrame);
    peekFrame = 0;
    setRailFlag('dragging', retreated);
  }
  function onPointerEnd(event) {
    pointerDown = false;
    pointerDragging = false;
    pointerStart = null;
    setRailFlag('dragging', retreated);
    if (event?.pointerType === 'mouse' || event?.pointerType === 'pen') {
      lastPointer = { x: event.clientX, y: event.clientY };
      schedulePeek();
    }
  }
  function clearPeek() {
    if (peekFrame) view?.cancelAnimationFrame(peekFrame);
    peekFrame = 0;
    lastPointer = null;
    setRailFlag('near', false);
    onPointerEnd();
  }
  function onPointerOut(event) {
    if (event.relatedTarget) return;
    // Replacing a DOM node under the pointer can emit a null relatedTarget;
    // only a viewport exit should clear an in-progress drag/peek.
    if (
      event.clientX > 0 &&
      event.clientX < view.innerWidth &&
      event.clientY > 0 &&
      event.clientY < view.innerHeight
    )
      return;
    clearPeek();
  }
  function onWindowBlur() {
    clearPeek();
  }
  function bindPointerEvents(doc, bind) {
    const method = bind ? 'addEventListener' : 'removeEventListener';
    doc?.[method]('pointermove', onPointerMove, true);
    doc?.[method]('pointerdown', onPointerDown, true);
    doc?.[method]('pointerup', onPointerEnd, true);
    doc?.[method]('pointercancel', onPointerEnd, true);
    doc?.[method]('pointerout', onPointerOut, true);
    doc?.[method]('compositionstart', onCompositionStart, true);
    doc?.[method]('compositionend', onCompositionEnd, true);
    doc?.[method]('click', onGuardedClick, true);
    view?.[method]('blur', onWindowBlur);
  }
  function refresh() {
    if (!root?.isConnected) return;
    const wrap = root.querySelector('.canvas-wrap');
    if (!wrap) return;
    const viewerMode = wrap.dataset.viewerMode === 'viewers';
    if (lastViewerMode !== null && lastViewerMode !== viewerMode && !motionIntent.left)
      setMotion('left', false);
    lastViewerMode = viewerMode;
    for (const side of ['left', 'right']) {
      if (motionIntent[side]) setMotion(side, true);
    }
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
      for (const bar of wrap.querySelectorAll('.workspace-panel-bar')) {
        const count = bar.querySelectorAll('button').length;
        const geometry = workspaceRailGeometry(height, count);
        const svg = bar.querySelector(':scope > svg');
        svg.setAttribute('viewBox', `0 0 78 ${geometry.height}`);
        svg.querySelector('rect').setAttribute('height', String(geometry.height));
        svg.querySelector('[data-rail-fill]').setAttribute('d', geometry.fill);
        svg.querySelector('[data-rail-edge]').setAttribute('d', geometry.edge);
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
        bar.dataset.open = String(!!selected && !(viewerMode && side === 'left'));
      }
      if (selected) displayed[side] = selected.id;
      host.dataset.activePanel = selected?.id || displayed[side] || '';
      host.dataset.visibleGroup = item(displayed[side])?.coVisible || '';
      host.dataset.motionSuppressed = String(side === 'left' && (retreated || viewerMode));
      host.hidden = !shown;
      host.inert = !shown;
    }
    for (const entry of ITEMS) {
      const button = trigger(entry.id);
      const selected = active[entry.side] === entry.id;
      const shown =
        (selected || shareVisibleGroup(entry.id, active[entry.side])) &&
        !(viewerMode && entry.side === 'left');
      button?.setAttribute('aria-expanded', String(shown));
      button?.setAttribute('aria-pressed', String(selected && shown));
      if (entry.selector) {
        const panel = wrap.querySelector(entry.selector);
        if (panel)
          panel.hidden =
            displayed[entry.side] !== entry.id &&
            !shareVisibleGroup(entry.id, displayed[entry.side]);
      }
    }
    // Synchronize panel visibility before the fixed portal measures its slot.
    for (const entry of [...ITEMS].reverse())
      entry.adapter.refresh?.({
        root,
        wrap,
        active: active[entry.side],
        displayed: displayed[entry.side],
        viewerMode,
        retreated,
        id: entry.id,
      });
    schedulePeek();
  }
  function select(id, { focus = true } = {}) {
    const entry = item(id);
    if (!entry) return;
    if (active[entry.side] === id) return close(id, focus);
    const other = entry.side === 'left' ? 'right' : 'left';
    if (
      [active[entry.side], active[other]].some((activeId) => activeId && !canDeactivate(activeId))
    )
      return;
    if (shareVisibleGroup(active[entry.side], id)) {
      active[entry.side] = id;
      lastSide = entry.side;
      save();
      refresh();
      if (focus) entry.adapter.focus({ root })?.focus({ preventScroll: true });
      return;
    }
    if (active[other]) {
      const oldHost = root?.querySelector(`[data-drawer-side="${other}"]`);
      const focused = root?.ownerDocument.activeElement;
      if (oldHost?.contains(focused) || item(active[other])?.adapter.contains?.(focused))
        trigger(id)?.focus({ preventScroll: true });
      active[other] = null;
      setMotion(other, true);
      if (other === 'left') retreated = false;
    }
    active[entry.side] = id;
    lastSide = entry.side;
    save();
    if (entry.side === 'left') retreated = false;
    // Callbacks may synchronously attach or refresh a newly mounted portal.
    // Carry the motion request through that work before any layout is read.
    motionIntent[entry.side] = true;
    setMotion(entry.side, true);
    try {
      entry.adapter.open({ root });
      setMotion(entry.side, true);
      refresh();
    } finally {
      motionIntent[entry.side] = false;
    }
    if (focus) {
      const target = entry.adapter.focus({ root });
      target?.focus({ preventScroll: true });
    }
  }
  function close(id, focus = true, { animate = true } = {}) {
    const entry = item(id);
    if (!entry || active[entry.side] !== id || !canDeactivate(id)) return;
    active[entry.side] = null;
    save();
    if (entry.side === 'left') retreated = false;
    // A hidden surface retains its owner and in-flight work.
    setMotion(entry.side, animate);
    refresh();
    if (focus) trigger(id)?.focus({ preventScroll: true });
  }
  function attach(nextRoot) {
    if (
      !nextRoot ||
      typeof nextRoot.querySelector !== 'function' ||
      !nextRoot.ownerDocument?.defaultView
    )
      return;
    const wrap = nextRoot.querySelector('.canvas-wrap');
    const right = nextRoot.querySelector('.right');
    const tools = nextRoot.querySelector('.tools');
    if (!wrap || !right || !tools) return;
    observer?.disconnect();
    view?.removeEventListener('resize', refresh);
    if (peekFrame) view?.cancelAnimationFrame(peekFrame);
    peekFrame = 0;
    bindPointerEvents(root?.ownerDocument, false);
    root?.ownerDocument.removeEventListener('keydown', onKeydown);
    root?.removeEventListener('focusin', onFocusIn);
    root = nextRoot;
    view = root.ownerDocument.defaultView;
    root.addEventListener('focusin', onFocusIn);
    if (wrap.querySelector('.workspace-panel-bar')) {
      view?.addEventListener('resize', refresh);
      root.ownerDocument.addEventListener('keydown', onKeydown);
      bindPointerEvents(root.ownerDocument, true);
      setMotion('left', false);
      setMotion('right', false);
      setRailFlag('dragging', pointerDragging || retreated);
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
    for (const entry of ITEMS.filter((entry) => entry.side === 'left'))
      entry.adapter.attach?.(left, { right });
    wrap.append(left);
    for (const side of ['left', 'right']) {
      const bar = root.ownerDocument.createElement('nav');
      bar.className = `workspace-panel-bar workspace-panel-bar-${side}`;
      bar.setAttribute('aria-label', `${side === 'left' ? 'Assets and colors' : 'Editing'} panels`);
      const geometry = workspaceRailGeometry(
        228,
        ITEMS.filter((entry) => entry.side === side).length,
      );
      bar.innerHTML = `<svg viewBox="0 0 78 ${geometry.height}" preserveAspectRatio="none" aria-hidden="true"><rect x="0" y="0" width="6" height="${geometry.height}" stroke="none"></rect><path data-rail-fill d="${geometry.fill}" stroke="none"></path><path data-rail-edge d="${geometry.edge}" fill="none"></path></svg>`;
      for (const entry of ITEMS.filter((entry) => entry.side === side)) {
        const button = root.ownerDocument.createElement('button');
        button.type = 'button';
        button.dataset.workspacePanel = entry.id;
        button.innerHTML = workspaceIcon(entry.id);
        button.title = entry.label;
        button.setAttribute('aria-label', entry.label);
        const panel = entry.coVisible && root.querySelector(entry.selector);
        if (panel && !panel.id) panel.id = `workspace-panel-${entry.id}`;
        button.setAttribute('aria-controls', panel?.id || `workspace-drawer-${side}`);
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
    bindPointerEvents(root.ownerDocument, true);
    setMotion('left', false);
    setMotion('right', false);
    setRailFlag('dragging', pointerDragging || retreated);
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
        for (const side of ['left', 'right']) item(active[side])?.adapter.restore({ root });
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
    setMotion('left', false);
    lastPointer = null;
    if (peekFrame) view?.cancelAnimationFrame(peekFrame);
    peekFrame = 0;
    setRailFlag('near', false);
    retreated = !!value && item(active.left)?.adapter.canRetreat === true;
    setRailFlag('dragging', pointerDragging || retreated);
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
