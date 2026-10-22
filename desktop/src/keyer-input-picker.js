import { sampleKeyerInput } from './keyer-input-pick.js';

// Preview coordinates are intrinsic image pixels, independent of artwork placement.
export function keyerPreviewPoint(svg, client) {
  const matrix = svg.getScreenCTM();
  if (!matrix || matrix.is2D === false) throw Error('Input preview transform is unavailable');
  const inverse = matrix.inverse();
  if (!['a', 'b', 'c', 'd', 'e', 'f'].every((k) => Number.isFinite(inverse[k])))
    throw Error('Input preview transform is unavailable');
  const point = new DOMPoint(...client).matrixTransform(inverse);
  if (![point.x, point.y].every(Number.isFinite))
    throw Error('Input preview transform is unavailable');
  return [point.x, point.y];
}
export function openKeyerInputPicker({ context, title, isCurrent, onPick, onClose }) {
  const previousFocus = document.activeElement,
    dialog = document.createElement('dialog');
  dialog.className = 'keyer-input-picker';
  dialog.dataset.keyerInputPicker = context.nodeId;
  dialog.innerHTML =
    '<header><strong></strong><button data-keyer-pick-cancel>Cancel</button></header><p data-keyer-input-identity></p><svg data-keyer-input-preview tabindex="0" role="img" aria-label="Keyer input pixels" preserveAspectRatio="xMidYMid meet"><image x="0" y="0" preserveAspectRatio="none"/></svg><output aria-live="polite">Click a visible input pixel</output>';
  dialog.querySelector('strong').textContent = title;
  dialog.querySelector('[data-keyer-input-identity]').textContent =
    `${context.node.name} · ${context.asset.name} · ${context.asset.width} × ${context.asset.height} input pixels`;
  const svg = dialog.querySelector('svg'),
    image = svg.querySelector('image'),
    status = dialog.querySelector('output');
  svg.setAttribute('viewBox', `0 0 ${context.asset.width} ${context.asset.height}`);
  image.setAttribute('width', context.asset.width);
  image.setAttribute('height', context.asset.height);
  image.setAttribute('href', `data:${context.asset.mime};base64,${context.asset.data}`);
  let closed = false;
  function close() {
    if (closed) return;
    closed = true;
    window.removeEventListener('blur', close);
    document.removeEventListener('pointerdown', blockOutside, true);
    dialog.remove();
    onClose();
    if (previousFocus?.isConnected) previousFocus.focus({ preventScroll: true });
  }
  function validate() {
    if (closed) return false;
    if (!isCurrent()) {
      close();
      return false;
    }
    return true;
  }
  function blockOutside(event) {
    if (!closed && !dialog.contains(event.target)) {
      event.preventDefault();
      event.stopImmediatePropagation();
    }
  }
  function sample(event, commit) {
    if (!validate()) return;
    try {
      const point = keyerPreviewPoint(svg, [event.clientX, event.clientY]),
        sample = sampleKeyerInput(context, point);
      status.textContent =
        sample.state === 'outside'
          ? 'Outside input image'
          : sample.state === 'transparent'
            ? 'Transparent pixel'
            : `${sample.x}, ${sample.y} · ${sample.hex}`;
      if (commit && sample.state === 'color') onPick(point, close);
    } catch (error) {
      status.textContent = error.message;
    }
  }
  svg.addEventListener('pointermove', (event) => sample(event, false));
  svg.addEventListener('click', (event) => {
    if (event.button === 0) sample(event, true);
  });
  dialog.querySelector('[data-keyer-pick-cancel]').onclick = close;
  dialog.addEventListener('cancel', (event) => {
    event.preventDefault();
    close();
  });
  dialog.addEventListener('close', close);
  dialog.addEventListener('pointercancel', close);
  dialog.addEventListener('keydown', (event) => {
    event.stopPropagation();
    if (event.key === 'Escape') {
      event.preventDefault();
      close();
    }
  });
  document.addEventListener('pointerdown', blockOutside, true);
  window.addEventListener('blur', close);
  try {
    document.body.append(dialog);
    dialog.showModal();
    svg.focus();
  } catch (error) {
    close();
    throw error;
  }
  return { close, validate };
}
