import { evaluate, outputPorts, renderElement, pathBounds } from './core.js';

const escape = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
export function createViewerState() {
  return {
    mode: 'artwork',
    slots: [
      { binding: null, view: null },
      { binding: null, view: null },
    ],
  };
}
// Alpha is read-only inspection; the normal common renderer remains the source.
export function viewerMarkup(value, assets, index, alpha = false) {
  const id = `nect-viewer-${index}-alpha`;
  const rendered = renderElement(
    value,
    { name: `viewer-${index}` },
    assets,
    '',
    `nect-viewer-${index}-gradient`,
  );
  return alpha
    ? `<defs><filter id="${id}" x="-100%" y="-100%" width="300%" height="300%" color-interpolation-filters="sRGB"><feColorMatrix type="matrix" values="0 0 0 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 0 1"/></filter></defs><g filter="url(#${id})">${rendered}</g>`
    : rendered;
}
export function bindViewer(state, index, layer, node, port) {
  if (!outputPorts(node).some((p) => p.id === port)) throw Error('Select a public output');
  state.slots[index].binding = { layerId: layer.id, nodeId: node.id, port, label: node.name };
  state.mode = 'viewers';
}
export function resolveViewer(doc, slot, fonts = {}, evaluations = new Map()) {
  const b = slot.binding;
  if (!b)
    return {
      status: 'Unassigned',
      detail: 'Select a graph node, then assign Viewer 1 or 2 in Inspector.',
    };
  const layer = doc.layers.find((l) => l.id === b.layerId);
  const node = layer?.nodes.find((n) => n.id === b.nodeId);
  const port = node && outputPorts(node).find((p) => p.id === b.port);
  const label = node?.name || b.label;
  if (!layer || !node || !port)
    return {
      status: 'Missing',
      label,
      detail: 'Original node or public output is missing. Undo can restore it.',
    };
  const base = { label, layer: layer.name, type: port.type, port: port.label || 'Output' };
  try {
    let result = evaluations.get(layer.id);
    if (!result) {
      result = evaluate(layer, fonts, doc.imageAssets, doc);
      evaluations.set(layer.id, result);
    }
    const value = result.run(node.id, b.port);
    if (['Path', 'PaintedPath'].includes(port.type) && !value.contours.some((c) => c.points.length))
      return { ...base, status: 'Empty', detail: 'Valid empty geometry' };
    if (['Path', 'PaintedPath', 'Image'].includes(port.type)) {
      const painted =
        port.type === 'Path'
          ? {
              ...value,
              fill: null,
              stroke: {
                hex: '#475569',
                alpha: 1,
                width: 1,
                cap: 'round',
                join: 'round',
                miterLimit: 4,
                dash: [],
                dashOffset: 0,
              },
            }
          : value;
      // Render identifiers are scoped to each slot by the caller. Never alter authored paint.
      return {
        ...base,
        status: 'Live',
        value: painted,
        bounds:
          value.kind === 'image'
            ? [
                value.position[0],
                value.position[1],
                value.position[0] + value.size[0],
                value.position[1] + value.size[1],
              ]
            : (() => {
                const [x, y] = pathBounds(value);
                return [x[0], y[0], x[2], y[2]];
              })(),
      };
    }
    if (['Scalar', 'Vector2', 'Color'].includes(port.type))
      return {
        ...base,
        status: 'Live',
        detail:
          port.type === 'Vector2'
            ? `X: ${value[0]} · Y: ${value[1]}`
            : port.type === 'Color'
              ? `${value.hex} · alpha ${value.alpha}`
              : String(value),
      };
    return {
      ...base,
      status: 'Unsupported',
      detail: 'No inspection renderer for this output type.',
    };
  } catch (error) {
    return { ...base, status: 'Error', detail: error.message };
  }
}
function fittedView(result, doc) {
  const b = result.bounds;
  if (!b || !b.every(Number.isFinite)) return [0, 0, doc.width, doc.height];
  const width = Math.max(20, b[2] - b[0]),
    height = Math.max(20, b[3] - b[1]);
  return [b[0] - width * 0.1, b[1] - height * 0.1, width * 1.2, height * 1.2];
}
export function updateViewers(root, state, doc, fonts, evaluations = new Map()) {
  const panels = root.querySelector('.node-viewers');
  if (!panels || state.mode !== 'viewers') return;
  state.slots.forEach((slot, index) => {
    const result = resolveViewer(doc, slot, fonts, evaluations);
    if (!slot.view && result.value) slot.view = fittedView(result, doc);
    const panel = panels.children[index];
    panel.querySelector('[data-viewer-label]').textContent = [
      result.label,
      result.port,
      result.type,
      result.layer,
    ]
      .filter(Boolean)
      .join(' · ');
    panel.querySelector('[data-viewer-status]').textContent = result.status;
    const body = panel.querySelector('.node-viewer-body');
    body.dataset.alpha = String(!!slot.alpha);
    body.dataset.transparency = String(['Image', 'PaintedPath'].includes(result.type));
    const alpha = panel.querySelector('[data-viewer-alpha]');
    alpha.disabled = !result.value;
    alpha.setAttribute('aria-pressed', String(!!slot.alpha));
    alpha.onclick = () => {
      slot.alpha = !slot.alpha;
      updateViewers(root, state, doc, fonts);
    };
    try {
      body.innerHTML = result.value
        ? `<svg aria-label="Viewer ${index + 1} result" viewBox="${(slot.view || [0, 0, doc.width, doc.height]).join(' ')}">${viewerMarkup(result.value, doc.imageAssets, index, slot.alpha)}</svg>${result.type === 'Path' ? '<span class="viewer-inspection-note">Path · inspection outline</span>' : ''}`
        : `<p class="viewer-value">${escape(result.detail || '')}</p>`;
    } catch (error) {
      panel.querySelector('[data-viewer-status]').textContent = 'Error';
      body.innerHTML = `<p>${escape(error.message)}</p>`;
    }
    panel.querySelector('[data-viewer-fit]').onclick = () => {
      slot.view = fittedView(result, doc);
      updateViewers(root, state, doc, fonts);
    };
  });
}
export function mountViewers(
  root,
  state,
  { doc, fonts, layer, node, busy, canAssign = true, evaluations },
) {
  const wrap = root.querySelector('.canvas-wrap');
  wrap.dataset.viewerMode = state.mode;
  const tabs = document.createElement('div');
  tabs.className = 'viewer-tabs';
  tabs.innerHTML = ['artwork', 'viewers']
    .map(
      (mode) =>
        `<button data-viewer-mode="${mode}" aria-pressed="${state.mode === mode}" ${busy ? 'disabled' : ''}>${mode === 'artwork' ? 'Artwork' : 'Viewers 1 / 2'}</button>`,
    )
    .join('');
  const panels = document.createElement('div');
  panels.className = 'node-viewers';
  panels.innerHTML = state.slots
    .map(
      (_, i) =>
        `<section class="node-viewer" aria-label="Viewer ${i + 1}"><div class="node-viewer-header"><strong>Viewer ${i + 1}</strong><output data-viewer-status></output><button data-viewer-alpha aria-pressed="false">Alpha</button><button data-viewer-fit>Fit</button><button data-viewer-clear="${i}" aria-label="Clear Viewer ${i + 1}">Clear</button></div><div data-viewer-label></div><div class="node-viewer-body" tabindex="0" aria-label="Viewer ${i + 1} read-only viewport" title="Wheel to pan; Ctrl/Alt + wheel to zoom. Read-only inspection."></div></section>`,
    )
    .join('');
  wrap.prepend(tabs);
  wrap.append(panels);
  function refresh() {
    wrap.dataset.viewerMode = state.mode;
    tabs
      .querySelectorAll('button')
      .forEach((button) =>
        button.setAttribute('aria-pressed', String(button.dataset.viewerMode === state.mode)),
      );
    updateViewers(root, state, doc, fonts, evaluations);
  }
  tabs.onclick = (event) => {
    const mode = event.target.closest('[data-viewer-mode]')?.dataset.viewerMode;
    if (mode && !busy) {
      state.mode = mode;
      refresh();
    }
  };
  // Viewer focus never routes Delete, point nudge, tool keys or gestures to the editor.
  panels.addEventListener('keydown', (event) => event.stopPropagation());
  panels.addEventListener('pointerdown', (event) => {
    const body = event.target.closest('.node-viewer-body');
    if (body) {
      body.focus({ preventScroll: true });
      event.stopPropagation();
    }
  });
  panels.onclick = (event) => {
    const clear = event.target.closest('[data-viewer-clear]');
    if (clear) {
      state.slots[Number(clear.dataset.viewerClear)] = { binding: null, view: null };
      refresh();
    }
  };
  panels.addEventListener(
    'wheel',
    (event) => {
      const body = event.target.closest('.node-viewer-body');
      if (!body) return;
      event.preventDefault();
      event.stopPropagation();
      const index = [...panels.children].indexOf(body.parentElement),
        slot = state.slots[index];
      if (!slot.view) return;
      const v = slot.view,
        rect = body.getBoundingClientRect();
      const scale = Math.max(v[2] / (rect.width || 300), v[3] / (rect.height || 200));
      const units = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? rect.height || 200 : 1;
      if (event.ctrlKey || event.metaKey || event.altKey) {
        const factor = Math.exp(Math.max(-1, Math.min(1, event.deltaY * units * 0.002)));
        const width = Math.max(0.01, Math.min(1e8, v[2] * factor)),
          height = (width * v[3]) / v[2];
        slot.view = [v[0] + (v[2] - width) / 2, v[1] + (v[3] - height) / 2, width, height];
      } else
        slot.view = [
          v[0] + event.deltaX * units * scale,
          v[1] + event.deltaY * units * scale,
          v[2],
          v[3],
        ];
      body.querySelector('svg')?.setAttribute('viewBox', slot.view.join(' '));
    },
    { passive: false },
  );
  if (node) {
    const assignmentBusy = busy || !canAssign;
    const ports = outputPorts(node),
      controls = document.createElement('div');
    controls.className = 'viewer-assignment';
    controls.innerHTML = `<label>Inspect output <select aria-label="Viewer public output" ${assignmentBusy ? 'disabled' : ''}>${ports.length > 1 ? '<option value="">Choose output…</option>' : ''}${ports.map((port, i) => `<option value="${i}">${escape(port.label || 'Output')} · ${escape(port.type || 'Unsupported')}</option>`).join('')}</select></label><div>${[0, 1].map((i) => `<button data-assign-viewer="${i}" ${assignmentBusy || ports.length > 1 ? 'disabled' : ''}>Viewer ${i + 1} ←</button>`).join('')}</div>`;
    if (!canAssign) {
      const note = document.createElement('p');
      note.textContent = 'Select one graph node to assign a Viewer.';
      controls.append(note);
    }
    const select = controls.querySelector('select');
    select.onchange = () =>
      controls
        .querySelectorAll('button')
        .forEach((button) => (button.disabled = assignmentBusy || select.value === ''));
    controls.onclick = (event) => {
      const button = event.target.closest('[data-assign-viewer]');
      if (!button || button.disabled || select.value === '') return;
      bindViewer(
        state,
        Number(button.dataset.assignViewer),
        layer,
        node,
        ports[Number(select.value)].id,
      );
      refresh();
    };
    root.querySelector('.inspector-body').prepend(controls);
  }
  refresh();
}
