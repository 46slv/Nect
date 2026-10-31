import { workspaceIcon } from './ui-icons.js';

export const textVariants = Object.freeze([
  { id: 'horizontal-tb', label: 'Horizontal Text' },
  { id: 'vertical-rl', label: 'Vertical Text' },
]);

// Workspace input only. The caller owns the single semantic creation command.
export function textToolGroup({ root, active, available, activate, create, point, delay = 450 }) {
  const doc = root.ownerDocument;
  const win = doc.defaultView;
  let variant = textVariants[0].id,
    press = null,
    menu = null,
    creation = null,
    suppressClick = false;
  const button = () => root.querySelector('[data-tool="Text"]');
  const composing = (e) => e.isComposing || e.keyCode === 229;
  const stop = (e) => {
    e.preventDefault();
    e.stopImmediatePropagation();
  };
  function clearPress() {
    if (press) win.clearTimeout(press.timer);
    press = null;
  }
  function close(focus = false) {
    menu?.remove();
    menu = null;
    button()?.setAttribute('aria-expanded', 'false');
    if (focus) button()?.focus({ preventScroll: true });
  }
  function cancel(focus = false) {
    clearPress();
    if (creation) {
      const old = creation;
      creation = null;
      if (old.stage.hasPointerCapture?.(old.id)) old.stage.releasePointerCapture(old.id);
    }
    close(focus);
  }
  function sync() {
    const b = button(),
      v = textVariants.find((v) => v.id === variant);
    if (!b) return;
    b.innerHTML = workspaceIcon(v.id === 'vertical-rl' ? 'Text-vertical' : 'Text');
    b.dataset.textVariant = variant;
    b.title = `${v.label} — Hold for variants`;
    b.setAttribute('aria-label', v.label);
    b.setAttribute('aria-haspopup', 'menu');
    b.setAttribute('aria-expanded', String(!!menu));
    b.setAttribute('aria-pressed', String(active()));
    b.classList.toggle('active', active());
  }
  function select(id) {
    if (!available() || !textVariants.some((v) => v.id === id)) return;
    variant = id;
    cancel();
    activate();
    sync();
  }
  function open() {
    clearPress();
    if (!available() || menu) return;
    const anchor = button();
    if (!anchor) return;
    menu = doc.createElement('div');
    menu.className = 'text-tool-menu';
    menu.setAttribute('role', 'menu');
    menu.setAttribute('aria-label', 'Text variants');
    for (const v of textVariants) {
      const row = doc.createElement('button');
      row.type = 'button';
      row.textContent = v.label;
      row.dataset.textVariant = v.id;
      row.setAttribute('role', 'menuitemradio');
      row.setAttribute('aria-checked', String(v.id === variant));
      row.addEventListener('click', () => {
        if (row.parentElement === menu) select(v.id);
      });
      menu.append(row);
    }
    const bounds = anchor.getBoundingClientRect();
    doc.body.append(menu);
    menu.style.left = `${Math.max(0, Math.min(bounds.right + 4, win.innerWidth - menu.offsetWidth))}px`;
    menu.style.top = `${Math.max(0, Math.min(bounds.top, win.innerHeight - menu.offsetHeight))}px`;
    anchor.setAttribute('aria-expanded', 'true');
    menu.querySelector('[aria-checked="true"]').focus({ preventScroll: true });
  }
  root.addEventListener(
    'click',
    (e) => {
      if (!e.target.closest('[data-tool="Text"]')) return;
      stop(e);
      clearPress();
      if (suppressClick) {
        suppressClick = false;
        return;
      }
      if (menu) {
        close(true);
        return;
      }
      select(variant);
    },
    true,
  );
  root.addEventListener(
    'pointerdown',
    (e) => {
      const b = e.target.closest('[data-tool="Text"]');
      if (b) {
        if (e.button !== 0 || e.isPrimary === false) return;
        // Prevent a field blur/commit merely from choosing a Tool.
        stop(e);
        cancel();
        suppressClick = false;
        if (!available()) return;
        const p = { id: e.pointerId, x: e.clientX, y: e.clientY };
        press = p;
        p.timer = win.setTimeout(() => {
          if (press !== p) return;
          suppressClick = true;
          open();
        }, delay);
        return;
      }
      const stage = e.target.closest('.canvas-stage');
      if (!stage || !active()) return;
      if (e.target.closest('input,textarea,select,button,[contenteditable],.character-floating'))
        return;
      if (
        e.button !== 0 ||
        e.isPrimary === false ||
        e.ctrlKey ||
        e.metaKey ||
        e.altKey ||
        !available(true)
      )
        return;
      stop(e);
      const at = point(e);
      if (!at.every(Number.isFinite)) return;
      creation = { id: e.pointerId, stage, at, x: e.clientX, y: e.clientY };
      stage.focus({ preventScroll: true });
      stage.setPointerCapture?.(e.pointerId);
    },
    true,
  );
  doc.addEventListener(
    'pointerdown',
    (e) => {
      if (menu && !menu.contains(e.target) && !e.target.closest('[data-tool="Text"]')) {
        // Dismissal consumes this press, so it cannot also author on the Canvas.
        cancel();
        stop(e);
      }
    },
    true,
  );
  doc.addEventListener(
    'pointermove',
    (e) => {
      if (press?.id === e.pointerId && Math.hypot(e.clientX - press.x, e.clientY - press.y) > 6) {
        clearPress();
        suppressClick = true;
      }
      if (
        creation?.id === e.pointerId &&
        Math.hypot(e.clientX - creation.x, e.clientY - creation.y) > 6
      )
        cancel();
    },
    true,
  );
  doc.addEventListener(
    'pointerup',
    (e) => {
      if (press?.id === e.pointerId) clearPress();
      if (creation?.id !== e.pointerId) return;
      const old = creation;
      cancel();
      stop(e);
      const bounds = old.stage.getBoundingClientRect();
      if (
        e.button === 0 &&
        active() &&
        available(true) &&
        e.clientX >= bounds.left &&
        e.clientX <= bounds.right &&
        e.clientY >= bounds.top &&
        e.clientY <= bounds.bottom
      )
        create(old.at, variant);
    },
    true,
  );
  doc.addEventListener(
    'pointercancel',
    () => {
      suppressClick = true;
      cancel();
    },
    true,
  );
  root.addEventListener(
    'lostpointercapture',
    (e) => {
      if (creation?.id === e.pointerId) cancel();
    },
    true,
  );
  root.addEventListener(
    'pointerout',
    (e) => {
      if (
        press &&
        e.target.closest('[data-tool="Text"]') &&
        !e.relatedTarget?.closest?.('[data-tool="Text"]')
      ) {
        clearPress();
        suppressClick = true;
      }
    },
    true,
  );
  doc.addEventListener(
    'keydown',
    (e) => {
      if (composing(e)) return;
      if (e.key === 'Escape' && (menu || press || creation)) {
        stop(e);
        suppressClick = true;
        cancel(true);
        return;
      }
      if (menu && menu.contains(e.target)) {
        if (['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) {
          stop(e);
          const rows = [...menu.children],
            i = rows.indexOf(doc.activeElement);
          rows[
            e.key === 'Home'
              ? 0
              : e.key === 'End'
                ? rows.length - 1
                : (i + (e.key === 'ArrowDown' ? 1 : rows.length - 1)) % rows.length
          ].focus();
        } else if (e.key === 'Tab') close();
        else if (e.repeat && ['Enter', ' '].includes(e.key)) stop(e);
        return;
      }
      if (!e.target.closest?.('[data-tool="Text"]')) return;
      if (e.repeat && ['Enter', ' '].includes(e.key)) {
        stop(e);
        return;
      }
      if (['Enter', ' '].includes(e.key)) suppressClick = false;
      if (['ArrowDown', 'ArrowRight'].includes(e.key)) {
        stop(e);
        open();
      }
    },
    true,
  );
  doc.addEventListener('focusin', (e) => {
    if (menu && !menu.contains(e.target) && e.target !== button()) close();
  });
  win.addEventListener('blur', () => {
    suppressClick = true;
    cancel();
  });
  doc.addEventListener('visibilitychange', () => {
    if (doc.hidden) cancel();
  });
  return { sync, cancel };
}
