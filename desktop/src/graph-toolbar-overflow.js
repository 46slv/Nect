// Reparent the existing commands, not copies: their delegated handlers, disabled
// state and Overview identity remain owned by main.js. Disclosure state is UI-only.
const scopes = new WeakMap();
export function ensureGraphToolbarOverflow(app, document) {
  let scope = scopes.get(app);
  if (!scope) {
    scope = {};
    scopes.set(app, scope);
    document.addEventListener(
      'keydown',
      (event) => {
        if (
          event.key !== 'Escape' ||
          event.defaultPrevented ||
          event.isComposing ||
          event.keyCode === 229 ||
          event.target.closest?.('dialog[open]') ||
          !scope.menu?.open
        )
          return;
        event.preventDefault();
        event.stopImmediatePropagation();
        scope.menu.open = false;
        scope.summary.focus({ preventScroll: true });
      },
      true,
    );
    document.defaultView.addEventListener('resize', () => scope.layout?.());
  }
  const toolbar = app.querySelector('.graph-toolbar');
  if (scope.toolbar === toolbar) return;
  scope.observer?.disconnect();
  scope.toolbar = toolbar;
  scope.menu = null;
  scope.layout = null;
  if (!toolbar) return;
  const buttons = ['graph-fit-selection', 'graph-reset', 'graph-overview'].map((action) =>
    toolbar.querySelector(`[data-action="${action}"]`),
  );
  const menu = document.createElement('details');
  menu.className = 'command-menu graph-view-more';
  menu.hidden = true;
  const summary = document.createElement('summary');
  summary.textContent = 'More';
  summary.tabIndex = 0;
  summary.setAttribute('aria-label', 'More node graph view controls');
  const items = document.createElement('div');
  items.className = 'command-menu-items';
  menu.append(summary, items);
  Object.assign(scope, { menu, summary });
  menu.addEventListener('toggle', () => {
    if (menu.open)
      document.querySelectorAll('.command-menu[open]').forEach((other) => {
        if (other !== menu) other.open = false;
      });
  });
  menu.addEventListener('focusout', (event) => {
    if (!menu.contains(event.relatedTarget)) menu.open = false;
  });
  // Run after the shared action handler. Preserve its deliberate viewport focus
  // for pointer navigation; keyboard activation returns to the disclosure.
  menu.addEventListener('click', (event) => {
    if (!event.target.closest('button[data-action]')) return;
    queueMicrotask(() => {
      if (!menu.isConnected) return;
      menu.open = false;
      if (items.contains(document.activeElement)) summary.focus({ preventScroll: true });
    });
  });
  scope.layout = () => {
    // The panel, not window width, owns the breakpoint (including panel resizes).
    const width = toolbar.getBoundingClientRect().width;
    const compact = width > 0 && width < 820;
    if (toolbar.classList.contains('graph-toolbar-compact') === compact) return;
    const focused = document.activeElement;
    menu.open = false;
    toolbar.classList.toggle('graph-toolbar-compact', compact);
    menu.hidden = !compact;
    if (compact) {
      toolbar.append(menu);
      items.append(...buttons);
      if (buttons.includes(focused)) summary.focus({ preventScroll: true });
    } else {
      menu.before(...buttons);
      menu.remove();
      if (focused === summary)
        buttons.find((button) => !button.disabled)?.focus({ preventScroll: true });
      else if (buttons.includes(focused)) focused.focus({ preventScroll: true });
    }
  };
  scope.layout();
  if (document.defaultView.ResizeObserver) {
    scope.observer = new document.defaultView.ResizeObserver(scope.layout);
    scope.observer.observe(toolbar);
  }
}
