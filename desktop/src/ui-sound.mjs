// Delegated pointer feedback. No external assets or dependencies.
const installed = new WeakMap();
const clamp = (value) => {
  try {
    const n = Number(value);
    return Number.isFinite(n) ? Math.max(0, Math.min(1, n)) : 0;
  } catch {
    return 0;
  }
};

// Approved v2 audition waveform: 288 signed PCM samples at 48 kHz.
// Stored at the audition's 0.12 master gain; divide by 0.12 so the shared
// master gain reproduces the audition at its proposed default setting.
const REFERENCE_RATE = 48000,
  REFERENCE_GAIN = 0.12;
const APPROVED_PCM = new Int16Array([
  0, -68, 303, -184, 7, 854, -138, -81, -901, 116, -295, 287, 722, -398, 139, 240, -791, 175, 174,
  67, -348, 267, -284, 206, -58, 306, -395, 208, 265, 80, -242, 194, -647, 375, 269, -79, 47, -44,
  -386, 23, 386, -63, -324, 287, 67, -455, 117, 312, 63, -388, 238, 153, -343, -115, 50, -106, 382,
  -13, -18, -24, -296, 143, 72, -59, 172, -187, -32, 65, 163, -266, 110, -15, 21, 44, 41, -121, 148,
  -161, 32, 116, 18, -155, 34, 123, -91, 26, -24, 99, -194, 124, -144, -71, 46, 15, 7, 114, -164,
  88, 47, -86, 103, -81, 31, 31, 67, -187, 55, 60, -22, -46, 61, -93, 29, 47, -29, -47, 61, -55, 50,
  -64, 78, -54, 71, -104, 121, 13, -49, -70, 8, 35, -19, 14, 58, -84, 48, -65, -2, 52, -30, 67, 9,
  -70, 54, -14, -22, 47, -77, 35, 13, -9, -10, -35, 20, 36, -30, 41, 1, -23, -7, 25, -12, -48, 21,
  -11, 26, 9, -13, 28, 4, -25, -15, 20, 14, 1, -14, -24, -17, 37, -6, -7, -6, 15, 11, 2, 6, -23, 12,
  -13, -3, 16, -2, -30, 26, 5, -8, -2, 1, 15, -36, 28, -3, -27, 0, -2, 15, -15, 19, -4, 2, -17, 20,
  6, 1, 4, -11, -7, 17, -20, 1, 8, 9, -3, 5, -9, -10, 13, -7, -1, -2, 1, 4, 0, -8, 2, 12, -12, -5,
  3, -4, 6, 1, -4, 4, 3, -5, -1, 7, -8, 7, -11, 7, -2, 0, -2, 6, -8, 9, 0, -3, -4, 9, -7, 7, -2, -6,
  -2, 9, -5, -2, 7, 0, -9, 8, 0, -8, 3, 6, -2, -7, 5, 4, -1, -1, -5, 4, 3, -4, 1,
]);
/** Precompute once per AudioContext, with linear interpolation at its sample rate. */
export function createClockTickSamples(sampleRate) {
  if (!Number.isFinite(sampleRate) || sampleRate < 8000 || sampleRate > 384000)
    throw new RangeError('Unsupported sample rate');
  const samples = new Float32Array(Math.round((sampleRate * APPROVED_PCM.length) / REFERENCE_RATE));
  for (let i = 0; i < samples.length; i++) {
    const p = (i * REFERENCE_RATE) / sampleRate;
    const j = Math.floor(p),
      fraction = p - j;
    const a = APPROVED_PCM[j] ?? 0,
      b = APPROVED_PCM[j + 1] ?? 0;
    samples[i] = (a + (b - a) * fraction) / 32767 / REFERENCE_GAIN;
  }
  return samples;
}

/** Install once per document. Only elements matching selector are candidates. */
export function installUiSound({
  document: doc = globalThis.document,
  selector = '[data-ui-sound="tick"]',
  enabled = true,
  volume = 0.12,
  createAudioContext = () => new (globalThis.AudioContext || globalThis.webkitAudioContext)(),
} = {}) {
  if (!doc || typeof doc.addEventListener !== 'function')
    throw new TypeError('A document is required');
  if (typeof selector !== 'string' || !selector.trim())
    throw new TypeError('A nonempty eligibility selector is required');
  if (installed.has(doc)) return installed.get(doc);

  let disposed = false,
    on = Boolean(enabled),
    level = clamp(volume),
    pressed = false,
    nativeDrag = false,
    windowFocused = true;
  let context = null,
    master = null,
    tickBuffer = null,
    unlocking = false,
    last = null;
  const active = new Set();
  const win = doc.defaultView;
  // A missing/throwing hasFocus is treated as unfocused (fail silent).
  const visible = () => {
    try {
      return (
        windowFocused &&
        !doc.hidden &&
        doc.visibilityState !== 'hidden' &&
        typeof doc.hasFocus === 'function' &&
        doc.hasFocus()
      );
    } catch {
      return false;
    }
  };
  const stopVoice = (voice, stop = true) => {
    active.delete(voice);
    try {
      voice.onended = null;
      if (stop) voice.stop();
    } catch {}
    try {
      voice.disconnect();
    } catch {}
  };
  const setMaster = () => {
    if (!master || !context) return;
    try {
      master.gain.setValueAtTime(on ? level : 0, context.currentTime);
    } catch {}
  };
  const eligible = (target) => {
    try {
      const el = target?.closest?.(selector);
      if (!el || !doc.contains(el)) return null;
      for (let node = el; node && node.nodeType === 1; node = node.parentElement) {
        if (
          node.hidden ||
          node.inert ||
          node.hasAttribute('inert') ||
          node.getAttribute('aria-hidden') === 'true' ||
          node.hasAttribute('disabled') ||
          node.getAttribute('aria-disabled') === 'true' ||
          node.matches(':disabled')
        )
          return null;
        const style = win?.getComputedStyle?.(node);
        if (
          style &&
          (style.display === 'none' ||
            style.visibility === 'hidden' ||
            style.visibility === 'collapse' ||
            style.contentVisibility === 'hidden' ||
            Number.parseFloat(style.opacity) === 0)
        )
          return null;
      }
      if (typeof el.getClientRects === 'function' && !el.getClientRects().length) return null;
      return el;
    } catch {
      return null;
    }
  };
  const unlock = (event) => {
    if (disposed || !on || !visible() || event.isTrusted !== true || unlocking) return;
    try {
      if (!context || context.state === 'closed') {
        context = createAudioContext();
        try {
          master = context.createGain();
          master.connect(context.destination);
          const samples = createClockTickSamples(context.sampleRate);
          tickBuffer = context.createBuffer(1, samples.length, context.sampleRate);
          tickBuffer.getChannelData(0).set(samples);
          setMaster();
        } catch (error) {
          try {
            master?.disconnect();
          } catch {}
          try {
            Promise.resolve(context.close()).catch(() => {});
          } catch {}
          context = null;
          master = null;
          tickBuffer = null;
          throw error;
        }
      }
      if (context.state === 'suspended' || context.state === 'interrupted') {
        unlocking = true;
        // No pending hover is retained or replayed when resume finishes.
        Promise.resolve(context.resume())
          .catch(() => {})
          .finally(() => {
            unlocking = false;
          });
      }
    } catch {
      unlocking = false;
      // If initialization failed, discard the partially created context.
      if (context && !master) {
        try {
          Promise.resolve(context.close()).catch(() => {});
        } catch {}
        context = null;
      }
    }
  };
  const tick = () => {
    if (
      !on ||
      !visible() ||
      !context ||
      context.state !== 'running' ||
      unlocking ||
      !master ||
      !tickBuffer
    )
      return;
    let voice;
    try {
      voice = context.createBufferSource();
      active.add(voice);
      voice.buffer = tickBuffer;
      voice.connect(master);
      voice.onended = () => stopVoice(voice, false);
      voice.start(context.currentTime);
    } catch {
      if (voice) stopVoice(voice);
    }
  };
  const down = (e) => {
    if (e.isTrusted === true) nativeDrag = false;
    pressed = true;
    last = null;
    unlock(e);
  };
  const up = () => {
    pressed = false;
    last = null;
  };
  const key = (e) => {
    if (!e.repeat) unlock(e);
  };
  const over = (e) => {
    // A release may have happened outside the document. Trusted mouse state is
    // authoritative on re-entry; a held button must still suppress drag ticks.
    if (e.isTrusted === true && e.pointerType === 'mouse' && e.buttons === 0) pressed = false;
    if (
      disposed ||
      e.isTrusted !== true ||
      !visible() ||
      pressed ||
      nativeDrag ||
      e.buttons ||
      e.pointerType !== 'mouse' ||
      win?.matchMedia?.('(pointer: coarse)').matches
    ) {
      last = null;
      return;
    }
    const control = eligible(e.target);
    if (!control) {
      last = null;
      return;
    }
    if (control === last || control === eligible(e.relatedTarget)) return;
    last = control;
    tick();
  };
  const out = (e) => {
    if (!eligible(e.relatedTarget)) last = null;
  };
  const silence = () => {
    pressed = false;
    last = null;
    for (const voice of [...active]) stopVoice(voice);
  };
  const visibility = () => {
    if (!visible()) silence();
  };
  const blur = () => {
    windowFocused = false;
    silence();
  };
  const focus = () => {
    windowFocused = true;
    last = null;
  };
  const dragstart = () => {
    nativeDrag = true;
    last = null;
  };
  const dragend = () => {
    nativeDrag = false;
    pressed = false;
    last = null;
  };
  const listeners = [
    ['pointerdown', down],
    ['pointerup', up],
    ['pointercancel', up],
    ['pointerover', over],
    ['pointerout', out],
    ['keydown', key],
    ['visibilitychange', visibility],
    ['dragstart', dragstart],
    ['dragend', dragend],
  ];
  for (const [type, handler] of listeners) doc.addEventListener(type, handler, true);
  win?.addEventListener?.('blur', blur);
  win?.addEventListener?.('focus', focus);
  const api = {
    setEnabled(value) {
      on = Boolean(value);
      last = null;
      setMaster();
      if (!on) for (const voice of [...active]) stopVoice(voice);
    },
    setVolume(value) {
      level = clamp(value);
      setMaster();
    },
    get enabled() {
      return on;
    },
    get volume() {
      return level;
    },
    dispose() {
      if (disposed) return;
      disposed = true;
      for (const [type, handler] of listeners) doc.removeEventListener(type, handler, true);
      win?.removeEventListener?.('blur', blur);
      win?.removeEventListener?.('focus', focus);
      for (const voice of [...active]) stopVoice(voice);
      try {
        master?.disconnect();
      } catch {}
      try {
        if (context) Promise.resolve(context.close()).catch(() => {});
      } catch {}
      context = null;
      master = null;
      tickBuffer = null;
      installed.delete(doc);
    },
  };
  installed.set(doc, api);
  return api;
}
