// View-only input: this module knows no document, Session, selection or history.
export const JOYSTICK_DEAD_ZONE = 0.12;
export const JOYSTICK_SPEED = 600; // CSS pixels / second, independent of graph zoom.

export function joystickVector(x, y, radius, deadZone = JOYSTICK_DEAD_ZONE) {
  const length = Math.hypot(x, y);
  if (![x, y, radius, length].every(Number.isFinite) || radius <= 0 || !length)
    return { knob: [0, 0], velocity: [0, 0] };
  const distance = Math.min(length, radius),
    strength = Math.max(0, (distance / radius - deadZone) / (1 - deadZone));
  return {
    knob: [x, y].map((value) => (value / length) * distance),
    velocity: [x, y].map((value) => (value / length) * strength),
  };
}

export function joystickMotion(velocity, elapsedMs) {
  // Ignore backwards clocks and cap a stalled frame, not normal 30/60/144 Hz motion.
  const seconds = Math.max(0, Math.min(50, elapsedMs)) / 1000;
  return velocity.map((value) => value * JOYSTICK_SPEED * seconds);
}

export function joystickLayout(width, height, scrollbarX = 16, scrollbarY = 16) {
  // Dimensions exclude native scrollbars. Even a very short graph keeps its
  // existing layout: the control scales down rather than covering another row.
  return {
    size: Math.max(0, Math.min(80, width - 16, height - 16)),
    right: Math.max(0, scrollbarX) + 8,
    bottom: Math.max(0, scrollbarY) + 8,
  };
}

export function joystickOverviewLayout(width, height, joystick) {
  // Include the map border and an 8px gap, even at the exact compact boundary.
  const alongside = height < joystick.size + 146;
  const right = alongside ? joystick.right + joystick.size + 12 : 24;
  return {
    right,
    width: Math.max(0, Math.min(240, width - right - 8, (height - 18) * 2)),
  };
}

export function createGraphPanJoystick({ container, viewport, canPan, pan, onLayout }) {
  const doc = container.ownerDocument,
    win = doc.defaultView,
    element = doc.createElement('button'),
    knob = doc.createElement('span');
  element.type = 'button';
  element.id = 'graph-pan-joystick';
  element.className = 'graph-pan-joystick';
  element.setAttribute('aria-label', 'Pan node graph');
  element.setAttribute('aria-keyshortcuts', 'ArrowUp ArrowDown ArrowLeft ArrowRight Escape');
  element.setAttribute('aria-describedby', 'graph-pan-joystick-help');
  element.title = 'Drag to pan · Arrow keys · Escape to stop';
  knob.className = 'graph-pan-joystick-knob';
  knob.setAttribute('aria-hidden', 'true');
  const help = doc.createElement('span');
  help.id = 'graph-pan-joystick-help';
  help.className = 'visually-hidden';
  help.textContent = 'Drag or hold arrow keys to pan in that direction. Release or Escape to stop.';
  element.append(knob, help);
  container.append(element);
  let pointer = null,
    keys = new Set(),
    velocity = [0, 0],
    frame = null,
    previousTime = null,
    disposed = false,
    layout = { size: 80, right: 24, bottom: 24 };
  const listeners = [];
  const listen = (target, name, fn, capture = false) => {
    target.addEventListener(name, fn, capture);
    listeners.push(() => target.removeEventListener(name, fn, capture));
  };
  const active = () => pointer !== null || keys.size > 0;
  const usable = () =>
    !disposed && element.isConnected && layout.size > 0 && !doc.hidden && canPan();
  const cancelFrame = () => {
    if (frame !== null) win.cancelAnimationFrame(frame);
    frame = previousTime = null;
  };
  const show = (point) => {
    knob.style.transform = `translate(${point[0]}px, ${point[1]}px)`;
  };
  const stop = () => {
    const oldPointer = pointer;
    pointer = null;
    keys.clear();
    velocity = [0, 0];
    cancelFrame();
    show([0, 0]);
    element.classList.remove('is-active');
    if (oldPointer !== null && element.hasPointerCapture?.(oldPointer))
      element.releasePointerCapture(oldPointer);
  };
  const tick = (time) => {
    frame = null;
    if (!active() || !usable()) return stop();
    const elapsed = previousTime === null ? 0 : time - previousTime;
    previousTime = time;
    if (elapsed > 0) pan(joystickMotion(velocity, elapsed));
    if (active() && velocity.some(Boolean)) frame = win.requestAnimationFrame(tick);
  };
  const setVector = (x, y) => {
    const next = joystickVector(x, y, layout.size * 0.225);
    velocity = next.velocity;
    show(next.knob);
    element.classList.toggle('is-active', active());
    if (!velocity.some(Boolean)) cancelFrame();
    else if (frame === null) frame = win.requestAnimationFrame(tick);
  };
  const move = (event) => {
    const rect = element.getBoundingClientRect();
    setVector(
      event.clientX - rect.left - rect.width / 2,
      event.clientY - rect.top - rect.height / 2,
    );
  };
  // Window capture runs before document-level menu dismissal and native focus
  // changes. Rejected presses must not blur/commit an unrelated editable draft.
  listen(
    win,
    'pointerdown',
    (event) => {
      if (!element.contains(event.target)) {
        if (active()) stop();
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      if (event.button !== 0 || active() || !usable()) return;
      pointer = event.pointerId;
      element.focus({ preventScroll: true });
      try {
        element.setPointerCapture?.(pointer);
      } catch {
        stop();
        return;
      }
      move(event);
    },
    true,
  );
  listen(
    win,
    'pointermove',
    (event) => {
      if (pointer === null || event.pointerId !== pointer) return;
      event.preventDefault();
      event.stopPropagation();
      if (!usable() || (event.pointerType === 'mouse' && event.buttons === 0)) return stop();
      move(event);
    },
    true,
  );
  for (const name of ['pointerup', 'pointercancel', 'lostpointercapture'])
    listen(
      win,
      name,
      (event) => {
        if (pointer === null || event.pointerId !== pointer) return;
        event.preventDefault();
        event.stopPropagation();
        stop();
      },
      true,
    );
  const arrows = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
  const keyboardVector = () => {
    const delta = [...keys].reduce(
      (sum, key) => sum.map((v, axis) => v + arrows[key][axis]),
      [0, 0],
    );
    setVector(...delta.map((v) => v * layout.size));
  };
  listen(element, 'keydown', (event) => {
    if (event.isComposing || event.keyCode === 229) return;
    // A focused view control must not leak modified Delete to global editing.
    if (['Escape', 'Delete', 'Backspace'].includes(event.key)) {
      event.preventDefault();
      event.stopPropagation();
      if (event.key === 'Escape') stop();
      return;
    }
    if (event.ctrlKey || event.metaKey || event.altKey) return;
    if ([' ', 'Enter'].includes(event.key)) {
      event.preventDefault();
      event.stopPropagation();
      return;
    }
    if (!arrows[event.key]) return;
    event.preventDefault();
    event.stopPropagation();
    if (pointer !== null || !usable() || (event.repeat && !keys.has(event.key))) return;
    keys.add(event.key);
    keyboardVector();
  });
  listen(element, 'keyup', (event) => {
    if (!keys.has(event.key)) return;
    event.preventDefault();
    event.stopPropagation();
    keys.delete(event.key);
    if (!keys.size) stop();
    else keyboardVector();
  });
  // A click is only the end of the drag, never a toolbar/document command.
  listen(element, 'click', (event) => {
    event.preventDefault();
    event.stopPropagation();
  });
  listen(element, 'contextmenu', (event) => {
    event.preventDefault();
    event.stopPropagation();
  });
  listen(element, 'blur', stop);
  listen(win, 'blur', stop);
  listen(doc, 'visibilitychange', () => {
    if (doc.hidden) stop();
  });
  listen(doc, 'compositionstart', stop, true);
  const resize = () => {
    if (disposed) return;
    stop();
    layout = joystickLayout(
      viewport.clientWidth,
      viewport.clientHeight,
      Math.max(0, viewport.offsetWidth - viewport.clientWidth),
      Math.max(0, viewport.offsetHeight - viewport.clientHeight),
    );
    element.hidden = layout.size === 0;
    element.style.width = element.style.height = `${layout.size}px`;
    element.style.right = `${layout.right}px`;
    element.style.bottom = `${layout.bottom}px`;
    onLayout?.();
  };
  const observer = win.ResizeObserver && new win.ResizeObserver(resize);
  observer?.observe(viewport);
  listen(win, 'resize', resize);
  resize();
  return {
    element,
    get active() {
      return active();
    },
    get layout() {
      return layout;
    },
    stop,
    destroy() {
      stop();
      disposed = true;
      observer?.disconnect();
      listeners.forEach((remove) => remove());
      element.remove();
    },
  };
}
