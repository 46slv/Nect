// Move existing controls into boundary disclosures. Their command owners and
// document state stay in main.js; only the visibility of view chrome lives here.
const scopes = new WeakMap();

export function ensureBoundaryControls(app, document) {
  let scope = scopes.get(app);
  if (!scope) {
    scope = { graphOpen: false, viewOpen: false, presetOpen: false };
    scopes.set(app, scope);
    document.defaultView.addEventListener('resize', () => scope.reposition?.());
    document.addEventListener(
      'keydown',
      (event) => {
        if (
          event.key !== 'Escape' ||
          event.defaultPrevented ||
          event.isComposing ||
          event.target.closest?.(
            'dialog[open], input, textarea, select, [contenteditable="true"]',
          ) ||
          document.querySelector('.guides-panel')
        )
          return;
        const toolbar = event.target.closest?.('.graph-toolbar, .canvas-toolbar');
        if (!toolbar || !scope.graphToggle || !scope.viewToggle) return;
        const groups = scope.graphToolbar.querySelector('[data-node-groups]');
        if (toolbar === scope.graphToolbar && groups?.open) {
          groups.open = false;
          groups.querySelector('summary')?.focus({ preventScroll: true });
        } else if (toolbar === scope.graphToolbar && scope.preset?.open) {
          scope.preset.open = false;
          scope.presetSummary.focus({ preventScroll: true });
        } else if (toolbar === scope.graphToolbar && scope.graphOpen) {
          scope.graphOpen = false;
          scope.sync();
          scope.graphToggle.focus({ preventScroll: true });
        } else if (toolbar === scope.canvasToolbar && scope.viewOpen) {
          scope.viewOpen = false;
          scope.sync();
          scope.viewToggle.focus({ preventScroll: true });
        } else return;
        event.preventDefault();
        event.stopImmediatePropagation();
      },
      true,
    );
  }
  const graph = app.querySelector('.graph-toolbar');
  const canvas = app.querySelector('.canvas-toolbar');
  const panel = app.querySelector('.graph-panel');
  if (!graph || !canvas || !panel || scope.graphToolbar === graph) return;
  scope.observer?.disconnect();
  const add = graph.querySelector('#add-node');
  const graphExtras = document.createElement('div');
  graphExtras.className = 'boundary-graph-extras';
  for (const node of [...graph.children]) {
    if (node === add) continue;
    if (node.matches('strong, .graph-layer-name, .spacer')) node.remove();
    else graphExtras.append(node);
  }
  const graphToggle = document.createElement('button');
  graphToggle.type = 'button';
  graphToggle.className = 'boundary-toggle';
  graphToggle.setAttribute('aria-label', 'Toggle node graph controls');
  graphToggle.setAttribute('aria-controls', 'boundary-graph-extras');
  graphExtras.id = 'boundary-graph-extras';
  const preset = document.createElement('details');
  preset.className = 'boundary-popover';
  preset.open = scope.presetOpen;
  const presetSummary = document.createElement('summary');
  presetSummary.textContent = 'Presets';
  const presetControls = panel.querySelector('.node-preset-controls');
  if (presetControls) preset.append(presetSummary, presetControls);
  const placePopup = (details, popup) => {
    if (!details.open || !popup || !details.isConnected) return;
    const rect = details.querySelector('summary').getBoundingClientRect();
    const view = document.defaultView;
    const width = Math.min(420, Math.max(0, view.innerWidth - 16));
    popup.style.maxHeight = `${Math.max(0, view.innerHeight - 16)}px`;
    popup.style.left = `${Math.max(8, Math.min(rect.left, view.innerWidth - width - 8))}px`;
    const height = Math.min(
      popup.getBoundingClientRect().height,
      Math.max(0, view.innerHeight - 16),
    );
    const below = view.innerHeight - rect.bottom - 8;
    const above = rect.top - 8;
    const preferred = below >= height || below >= above ? rect.bottom + 4 : rect.top - height - 4;
    popup.style.top = `${Math.max(8, Math.min(preferred, view.innerHeight - height - 8))}px`;
  };
  preset.addEventListener('toggle', () => {
    scope.presetOpen = preset.open;
    placePopup(preset, presetControls);
  });
  presetSummary.addEventListener('click', () => {
    queueMicrotask(() => {
      scope.presetOpen = preset.open;
    });
  });
  const groups = panel.querySelector('.node-group-controls');
  if (groups) {
    groups.classList.add('boundary-popover');
    const content = document.createElement('div');
    content.className = 'boundary-group-content';
    for (const node of [...groups.children]) if (!node.matches('summary')) content.append(node);
    groups.append(content);
    groups.addEventListener('toggle', () => placePopup(groups, content));
  }
  graphExtras.append(preset);
  if (groups) graphExtras.append(groups);
  graph.append(add, graphToggle, graphExtras);
  const output = canvas.querySelector('[data-zoom="canvas"]');
  const canvasExtras = document.createElement('div');
  canvasExtras.className = 'boundary-canvas-extras';
  canvasExtras.id = 'boundary-canvas-extras';
  for (const node of [...canvas.children]) {
    if (node === output || node.matches('[data-pen-finish], [data-pen-cancel]')) continue;
    canvasExtras.append(node);
  }
  const viewToggle = document.createElement('button');
  viewToggle.type = 'button';
  viewToggle.className = 'boundary-toggle';
  viewToggle.textContent = 'View';
  viewToggle.setAttribute('aria-label', 'Toggle artboard view controls');
  viewToggle.setAttribute('aria-controls', 'boundary-canvas-extras');
  canvas.prepend(output, viewToggle, canvasExtras);
  // A fixed-width E-shoulder joins each control surface to the boundary.
  // It is decorative only: neither hit targets nor camera geometry move.
  for (const toolbar of [graph, canvas]) {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('class', 'boundary-shoulder');
    svg.setAttribute('viewBox', '0 0 48 34');
    svg.setAttribute('aria-hidden', 'true');
    const face = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    face.setAttribute('d', 'M0 0 H48 C41 0 37 2 32 6 L19 25 C14 31 9 34 0 34 Z');
    face.setAttribute('class', 'boundary-shoulder-face');
    const edge = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    edge.setAttribute('d', 'M0 0 H48 C41 0 37 2 32 6 L19 25 C14 31 9 34 0 34');
    edge.setAttribute('class', 'boundary-shoulder-edge');
    svg.append(face, edge);
    toolbar.append(svg);
  }
  panel.append(canvas);
  Object.assign(scope, {
    graphToolbar: graph,
    canvasToolbar: canvas,
    graphToggle,
    viewToggle,
    preset,
    presetSummary,
  });
  scope.sync = () => {
    graphExtras.hidden = !scope.graphOpen;
    graphExtras.inert = !scope.graphOpen;
    canvasExtras.hidden = !scope.viewOpen;
    canvasExtras.inert = !scope.viewOpen;
    graphToggle.setAttribute('aria-expanded', String(scope.graphOpen));
    graphToggle.textContent = scope.graphOpen ? '‹' : '›';
    viewToggle.setAttribute('aria-expanded', String(scope.viewOpen));
    scope.reposition?.();
  };
  graphToggle.addEventListener('click', (event) => {
    event.stopPropagation();
    const opening = !scope.graphOpen;
    if (
      (!opening && graphExtras.contains(document.activeElement)) ||
      (opening && canvasExtras.contains(document.activeElement))
    )
      graphToggle.focus({ preventScroll: true });
    scope.graphOpen = opening;
    if (opening) scope.viewOpen = false;
    scope.sync();
  });
  viewToggle.addEventListener('click', (event) => {
    event.stopPropagation();
    const opening = !scope.viewOpen;
    if (
      (!opening && canvasExtras.contains(document.activeElement)) ||
      (opening && graphExtras.contains(document.activeElement))
    )
      viewToggle.focus({ preventScroll: true });
    scope.viewOpen = opening;
    if (opening) scope.graphOpen = false;
    scope.sync();
  });
  scope.reposition = () => {
    placePopup(preset, presetControls);
    if (groups) placePopup(groups, groups.querySelector('.boundary-group-content'));
  };
  graphExtras.addEventListener('scroll', scope.reposition);
  if (document.defaultView.ResizeObserver) {
    scope.observer = new document.defaultView.ResizeObserver(scope.reposition);
    scope.observer.observe(panel);
  }
  scope.sync();
  syncBoundaryViewerMode(app);
  scope.reposition();
}

export function syncBoundaryViewerMode(app) {
  const canvas = app.querySelector('.graph-panel > .canvas-toolbar');
  if (!canvas) return;
  const viewer = app.querySelector('.canvas-wrap')?.dataset.viewerMode === 'viewers';
  if (viewer && canvas.contains(app.ownerDocument.activeElement))
    app.querySelector('[data-viewer-mode="viewers"]')?.focus({ preventScroll: true });
  canvas.hidden = viewer;
  canvas.inert = viewer;
}
