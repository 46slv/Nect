import { hexToHsl, hslToHex } from './hsl-color-picker.js';

const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
const sameHex = (a, b) => a.toLowerCase() === b.toLowerCase();

// The picker is appended outside the graph so graph and Inspector rerenders cannot detach it.
// The owning editor supplies a validated, revision-checked typed Session write on completion.
export function createHslColorPicker({ document, onCommit, onError = () => {} }) {
  let popover = null,
    target = null,
    hsl = null,
    hex = null,
    committedHex = null,
    pointer = null,
    paintedHue = null,
    trigger = null,
    triggerSurface = null;

  function drawPlane() {
    const canvas = popover?.querySelector('[data-hsl-plane]');
    if (!canvas || paintedHue === hsl.h) return;
    paintedHue = hsl.h;
    let context;
    try {
      context = canvas.getContext('2d');
    } catch {
      return;
    }
    if (!context) return;
    const { width, height } = canvas;
    const image = context.createImageData(width, height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const color = hslToHex(hsl.h, (x / (width - 1)) * 100, (1 - y / (height - 1)) * 100);
        const i = (y * width + x) * 4;
        for (let c = 0; c < 3; c++)
          image.data[i + c] = parseInt(color.slice(1 + c * 2, 3 + c * 2), 16);
        image.data[i + 3] = 255;
      }
    }
    context.putImageData(image, 0, 0);
  }

  function paint(skip = null) {
    if (!popover) return;
    drawPlane();
    const values = { h: hsl.h, s: hsl.s, l: hsl.l };
    for (const [channel, value] of Object.entries(values)) {
      const input = popover.querySelector('[data-hsl-number="' + channel + '"]');
      if (input !== skip) input.value = String(Number(value.toFixed(2)));
    }
    const range = popover.querySelector('[data-hsl-hue]');
    if (range !== skip) range.value = String(hsl.h);
    const hexInput = popover.querySelector('[data-hsl-hex]');
    if (hexInput !== skip) hexInput.value = hex.toUpperCase();
    popover.querySelector('[data-hsl-preview]').style.background = hex;
    const marker = popover.querySelector('[data-hsl-marker]');
    marker.style.left = hsl.s + '%';
    marker.style.top = 100 - hsl.l + '%';
    marker.style.borderColor = hsl.l > 65 ? '#252525' : '#ffffff';
  }

  function setHsl(next, skip = null) {
    const nextHex = hslToHex(next.h, next.s, next.l);
    hsl = next;
    hex = nextHex;
    paint(skip);
  }

  function commit() {
    if (!popover || !target || sameHex(hex, committedHex)) return;
    try {
      if (onCommit({ target, hex }) === false) throw Error('Color edit was not applied');
      committedHex = hex;
    } catch (error) {
      onError(error);
      close();
    }
  }

  function fromPlane(event) {
    const plane = popover.querySelector('[data-hsl-plane]');
    const rect = plane.getBoundingClientRect();
    if (!(rect.width > 0 && rect.height > 0)) return;
    const s = clamp(((event.clientX - rect.left) / rect.width) * 100, 0, 100);
    const l = clamp((1 - (event.clientY - rect.top) / rect.height) * 100, 0, 100);
    setHsl({ ...hsl, s: Number(s.toFixed(2)), l: Number(l.toFixed(2)) });
  }

  function close(restoreFocus = false) {
    const oldTrigger = trigger,
      oldTarget = target,
      oldSurface = triggerSurface;
    pointer = null;
    popover?.remove();
    popover = null;
    target = null;
    trigger = null;
    triggerSurface = null;
    paintedHue = null;
    if (restoreFocus) {
      const scope = oldSurface ? document.querySelector(oldSurface) : document;
      const replacement = oldTrigger?.isConnected
        ? oldTrigger
        : [...(scope?.querySelectorAll('[data-hsl-picker-open]') || [])].find((item) => {
            const field = item.closest('.field');
            return (
              field?.dataset.node === oldTarget?.nodeId && field?.dataset.key === oldTarget?.key
            );
          });
      replacement?.focus({ preventScroll: true });
    }
  }

  function open({ anchor, label, color, context }) {
    close();
    if (!/^#[0-9a-f]{6}$/i.test(color?.hex || '')) throw Error('Invalid Color value');
    target = context;
    trigger = anchor;
    // The editor rerenders both copies of a field after a commit. Preserve the
    // originating surface, not just the shared node/key identity.
    triggerSurface = anchor.closest('.inspector')
      ? '.inspector'
      : anchor.closest('.graph')
        ? '.graph'
        : null;
    committedHex = color.hex;
    hex = color.hex;
    hsl = hexToHsl(hex);
    const el = document.createElement('section');
    popover = el;
    el.className = 'hsl-color-popover';
    el.setAttribute('role', 'dialog');
    el.setAttribute('aria-label', label + ' HSL color picker');
    el.innerHTML = [
      '<header class="hsl-picker-header"><strong>HSL</strong><button type="button" data-hsl-close aria-label="Close color picker">×</button></header>',
      '<div class="hsl-picker-plane-wrap"><canvas data-hsl-plane width="180" height="132" aria-label="Saturation left to right; lightness bottom to top"></canvas><span data-hsl-marker aria-hidden="true"></span></div>',
      '<label class="hsl-picker-hue"><span>Hue</span><input type="range" data-hsl-hue min="0" max="360" step="1" aria-label="Hue slider"></label>',
      '<div class="hsl-picker-numbers">',
      '<label>H <input type="number" data-hsl-number="h" min="0" max="360" step="any" aria-label="Hue degrees"><span>°</span></label>',
      '<label>S <input type="number" data-hsl-number="s" min="0" max="100" step="any" aria-label="Saturation percent"><span>%</span></label>',
      '<label>L <input type="number" data-hsl-number="l" min="0" max="100" step="any" aria-label="Lightness percent"><span>%</span></label>',
      '</div>',
      '<div class="hsl-picker-footer"><span data-hsl-preview aria-hidden="true"></span><label>HEX <input type="text" data-hsl-hex maxlength="7" spellcheck="false" aria-label="Hex color"></label></div>',
    ].join('');
    document.body.append(el);
    const rect = anchor.getBoundingClientRect(),
      width = el.offsetWidth || 268,
      height = el.offsetHeight || 320,
      win = document.defaultView,
      maxX = Math.max(8, (win?.innerWidth || 1000) - width - 8),
      maxY = Math.max(8, (win?.innerHeight || 800) - height - 8);
    el.style.left =
      clamp(
        rect.right + width + 16 <= (win?.innerWidth || 1000)
          ? rect.right + 8
          : rect.left - width - 8,
        8,
        maxX,
      ) + 'px';
    el.style.top = clamp(rect.top, 8, maxY) + 'px';
    paint();
    el.querySelector('[data-hsl-number="h"]').focus({ preventScroll: true });
    return el;
  }

  function handleEdit(input, commitChange) {
    if (!popover) return;
    if (input.matches('[data-hsl-number]')) {
      if (!input.value.trim()) {
        if (commitChange) {
          hex = committedHex;
          hsl = hexToHsl(hex);
          paint();
        }
        return;
      }
      const value = Number(input.value),
        channel = input.dataset.hslNumber,
        limit = channel === 'h' ? 360 : 100;
      if (!Number.isFinite(value) || value < 0 || value > limit) {
        if (commitChange) {
          hex = committedHex;
          hsl = hexToHsl(hex);
          paint();
        }
        return;
      }
      setHsl({ ...hsl, [channel]: value }, input);
    } else if (input.matches('[data-hsl-hue]')) {
      setHsl({ ...hsl, h: Number(input.value) }, input);
    } else if (input.matches('[data-hsl-hex]')) {
      if (!/^#[0-9a-f]{6}$/i.test(input.value)) {
        if (commitChange) {
          hex = committedHex;
          hsl = hexToHsl(hex);
          paint();
        }
        return;
      }
      hex = input.value.toLowerCase();
      hsl = hexToHsl(hex);
      paint(input);
    } else return;
    if (commitChange) commit();
  }

  // Local interaction never reaches graph shortcuts or numeric-expression handlers.
  document.addEventListener(
    'keydown',
    (event) => {
      // Pointer capture does not imply keyboard focus: the nonfocusable plane
      // can be dragged with BODY focused. Escape still owns that pending edit.
      if (!popover || (!pointer && !popover.contains(event.target))) return;
      event.stopImmediatePropagation();
      if (event.key === 'Escape' && !event.isComposing && event.keyCode !== 229) {
        event.preventDefault();
        close(true);
      }
    },
    true,
  );
  document.addEventListener('click', (event) => {
    if (!popover || popover.contains(event.target) || trigger?.contains(event.target)) return;
    close();
  });
  document.addEventListener('resize', () => close(), true);
  document.defaultView?.addEventListener('resize', () => close());

  document.addEventListener(
    'input',
    (event) => {
      if (popover?.contains(event.target)) handleEdit(event.target, false);
    },
    true,
  );
  document.addEventListener(
    'change',
    (event) => {
      if (popover?.contains(event.target)) handleEdit(event.target, true);
    },
    true,
  );
  document.addEventListener(
    'click',
    (event) => {
      if (event.target.closest?.('[data-hsl-close]') && popover?.contains(event.target))
        close(true);
    },
    true,
  );
  document.addEventListener(
    'pointerdown',
    (event) => {
      if (
        !popover ||
        event.target !== popover.querySelector('[data-hsl-plane]') ||
        event.button !== 0 ||
        event.isPrimary === false
      )
        return;
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = { id: event.pointerId, baseline: committedHex };
      try {
        event.target.setPointerCapture?.(event.pointerId);
      } catch {}
      fromPlane(event);
    },
    true,
  );
  document.addEventListener(
    'pointermove',
    (event) => {
      if (!pointer || event.pointerId !== pointer.id || !popover) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      fromPlane(event);
    },
    true,
  );
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
    document.addEventListener(
      type,
      (event) => {
        if (!pointer || event.pointerId !== pointer.id) return;
        event.stopImmediatePropagation();
        pointer = null;
        if (type === 'pointerup') commit();
        else {
          hex = committedHex;
          hsl = hexToHsl(hex);
          paint();
        }
      },
      true,
    );
  }
  document.defaultView?.addEventListener('blur', () => close());
  return {
    open,
    close,
    get element() {
      return popover;
    },
  };
}
