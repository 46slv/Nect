import { test } from 'node:test';
import assert from 'node:assert/strict';
import { installUiSound, createClockTickSamples } from './ui-sound.mjs';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';

class Events {
  constructor() { this.listeners = new Map(); }
  addEventListener(t, f) { if (!this.listeners.has(t)) this.listeners.set(t, new Set()); this.listeners.get(t).add(f); }
  removeEventListener(t, f) { this.listeners.get(t)?.delete(f); }
  emit(t, e = {}) { for (const f of this.listeners.get(t) || []) f({ isTrusted: true, ...e }); }
  count() { return [...this.listeners.values()].reduce((n, s) => n + s.size, 0); }
}
class Node {
  constructor(parent = null, attrs = {}) { this.nodeType = 1; this.parentElement = parent; this.attrs = attrs; this.hidden = false; this.inert = false; this.style = {}; }
  closest() { for (let n = this; n; n = n.parentElement) if (n.attrs.tick) return n; return null; }
  hasAttribute(a) { return Object.hasOwn(this.attrs, a); }
  getAttribute(a) { return this.attrs[a] ?? null; }
  matches(s) { return s === ':disabled' && !!this.attrs.disabledByFieldset; }
  getClientRects() { return this.attrs.noRect ? [] : [1]; }
}
class Param { constructor() { this.values = []; } setValueAtTime(v) { this.values.push(v); } exponentialRampToValueAtTime(v) { this.values.push(v); } }
class AudioNode { constructor() { this.gain = new Param(); this.disconnected = false; } connect() {} disconnect() { this.disconnected = true; } start() { this.started = true; } stop() { this.stops = (this.stops || 0) + 1; } end() { this.onended?.(); } }
class Context {
  constructor() { this.state = 'suspended'; this.currentTime = 0; this.sampleRate = 48000; this.destination = {}; this.voices = []; this.gains = []; this.buffers = []; this.resumes = 0; }
  createGain() { const x = new AudioNode(); this.gains.push(x); return x; }
  createBuffer(channels, length, sampleRate) { const data = new Float32Array(length); const x = {channels, length, sampleRate, getChannelData: () => data}; this.buffers.push(x); return x; }
  createBufferSource() { const x = new AudioNode(); this.voices.push(x); return x; }
  resume() { this.resumes++; this.state = 'running'; return Promise.resolve(); }
  close() { this.state = 'closed'; this.closed = true; return Promise.resolve(); }
}
function fixture(options = {}) {
  const doc = new Events(), win = new Events();
  doc.defaultView = win; doc.hidden = false; doc.visibilityState = 'visible'; doc.focused = true; doc.hasFocus = () => doc.focused; doc.contains = () => true;
  win.getComputedStyle = n => n.style; win.matchMedia = () => ({ matches: !!win.coarse });
  let creates = 0; const ctx = new Context();
  const api = installUiSound({document: doc, createAudioContext: () => { creates++; return ctx; }, ...options});
  const pointer = (type, target, extra = {}) => doc.emit(type, { target, pointerType: 'mouse', buttons: 0, ...extra });
  const gesture = async () => { pointer('pointerdown'); pointer('pointerup'); await flush(); };
  return {doc, win, ctx, api, pointer, gesture, creates: () => creates};
}
const flush = async () => { await Promise.resolve(); await Promise.resolve(); await Promise.resolve(); };
const button = () => new Node(null, {tick: true});

test('lazy trusted unlock, no queue, shared context and distinct rapid ticks', async () => {
  const f = fixture(), a = button(), b = button(), c = button();
  f.pointer('pointerover', a); assert.equal(f.creates(), 0);
  f.doc.emit('keydown', {isTrusted: false}); assert.equal(f.creates(), 0);
  await f.gesture(); assert.equal(f.creates(), 1); assert.equal(f.ctx.voices.length, 0);
  f.pointer('pointerover', a); f.pointer('pointerover', b, {relatedTarget: a}); f.pointer('pointerover', c, {relatedTarget: b});
  assert.equal(f.ctx.voices.length, 3);
  assert.equal(f.ctx.buffers.length, 1);
  assert.ok(f.ctx.voices.every(voice => voice.buffer === f.ctx.buffers[0]));
  f.ctx.voices[0].end(); assert.equal(f.ctx.voices[0].disconnected, true);
  await f.gesture(); assert.equal(f.creates(), 1); f.api.dispose();
  assert.ok(f.ctx.voices.every(voice => voice.disconnected));
});
test('dedupes child movement within the same eligible control', async () => {
  const f = fixture(), a = button(), child = new Node(a); await f.gesture();
  f.pointer('pointerover', a); f.pointer('pointerover', child, {relatedTarget: a}); f.pointer('pointerover', a);
  assert.equal(f.ctx.voices.length, 1); f.api.dispose();
});
test('off immediately mutes, volume is clamped and sanitized', async () => {
  const f = fixture({enabled: false, volume: 2}), a = button();
  await f.gesture(); assert.equal(f.creates(), 0); assert.equal(f.api.volume, 1);
  f.api.setEnabled(true); await f.gesture(); f.pointer('pointerover', a); assert.equal(f.ctx.voices.length, 1);
  f.api.setEnabled(false); assert.equal(f.ctx.gains[0].gain.values.at(-1), 0);
  f.pointer('pointerover', button()); assert.equal(f.ctx.voices.length, 1);
  f.api.setVolume(Symbol('invalid')); assert.equal(f.api.volume, 0);
  f.api.setVolume(NaN); assert.equal(f.api.volume, 0); f.api.setVolume(-4); assert.equal(f.api.volume, 0);
  f.api.setVolume(0.25); f.api.setEnabled(true); assert.equal(f.ctx.gains[0].gain.values.at(-1), 0.25); f.api.dispose();
});
test('ignores drag, touch, coarse pointer, disabled, hidden, inert, and background', async () => {
  const f = fixture(); await f.gesture();
  const a = button(); f.pointer('pointerdown', a, {buttons: 1}); f.pointer('pointerover', button(), {buttons: 1}); f.pointer('pointerup');
  f.pointer('pointerover', button(), {buttons: 1}); f.pointer('pointerover', button(), {pointerType: 'touch'});
  f.win.coarse = true; f.pointer('pointerover', button()); f.win.coarse = false;
  for (const attrs of [{disabled:''}, {'aria-disabled':'true'}, {disabledByFieldset:true}, {inert:''}, {'aria-hidden':'true'}]) {
    const parent = new Node(null, attrs); f.pointer('pointerover', new Node(parent, {tick:true}));
  }
  const noRect = button(); noRect.attrs.noRect = true; f.pointer('pointerover', noRect);
  const hidden = button(); hidden.hidden = true; f.pointer('pointerover', hidden);
  const css = button(); css.style.display = 'none'; f.pointer('pointerover', css);
  const transparent = button(); transparent.style.opacity = '0'; f.pointer('pointerover', transparent);
  const skipped = button(); skipped.style.contentVisibility = 'hidden'; f.pointer('pointerover', skipped);
  f.doc.hidden = true; f.pointer('pointerover', button());
  f.doc.hidden = false; f.doc.visibilityState = 'hidden'; f.pointer('pointerover', button());
  assert.equal(f.ctx.voices.length, 0); f.api.dispose();
});
test('resume failures are swallowed, no replay, and a subsequent gesture can retry', async () => {
  const f = fixture(); f.ctx.resume = () => { f.ctx.resumes++; return Promise.reject(new Error('blocked')); };
  await f.gesture(); f.pointer('pointerover', button()); assert.equal(f.ctx.voices.length, 0);
  f.ctx.resume = () => { f.ctx.resumes++; f.ctx.state = 'running'; return Promise.resolve(); };
  await f.gesture(); assert.equal(f.ctx.voices.length, 0); f.pointer('pointerover', button());
  assert.equal(f.ctx.resumes, 2); assert.equal(f.ctx.voices.length, 1); f.api.dispose();
});
test('pending resume does not accumulate hover events', async () => {
  const f = fixture(); let resolve;
  f.ctx.resume = () => new Promise(r => { resolve = () => { f.ctx.state = 'running'; r(); }; });
  f.pointer('pointerdown'); f.pointer('pointerup'); f.pointer('pointerover', button()); f.pointer('pointerover', button());
  resolve(); await flush(); assert.equal(f.ctx.voices.length, 0); f.pointer('pointerover', button()); assert.equal(f.ctx.voices.length, 1); f.api.dispose();
});
test('singleton, disposal removes listeners and active nodes; subsequent setup allowed', async () => {
  const f = fixture(); assert.equal(installUiSound({document:f.doc}), f.api); assert.equal(f.doc.count(), 9);
  await f.gesture(); f.pointer('pointerover', button()); const voice = f.ctx.voices[0];
  f.api.dispose(); f.api.dispose(); assert.equal(f.doc.count(), 0); assert.equal(f.win.count(), 0);
  assert.equal(voice.disconnected, true); assert.equal(f.ctx.closed, true);
  f.pointer('pointerover', button()); assert.equal(f.ctx.voices.length, 1);
  const fresh = installUiSound({document:f.doc, createAudioContext: () => new Context()}); assert.notEqual(fresh, f.api); fresh.dispose();
});
test('audio construction and playback errors do not escape event dispatch', async () => {
  const f = fixture({createAudioContext: () => { throw new Error('unsupported'); }}); await f.gesture(); f.pointer('pointerover', button()); f.api.dispose();
  const g = fixture(); await g.gesture(); g.ctx.createBufferSource = () => { throw new Error('device lost'); };
  assert.doesNotThrow(() => g.pointer('pointerover', button())); g.api.dispose();
});

test('partial source and buffer initialization failures clean up safely', async () => {
  const f = fixture(); await f.gesture();
  const createSource = f.ctx.createBufferSource.bind(f.ctx);
  f.ctx.createBufferSource = () => { const source = createSource(); source.connect = () => { throw new Error('connect failed'); }; return source; };
  f.pointer('pointerover', button());
  assert.equal(f.ctx.voices.length, 1); assert.equal(f.ctx.voices[0].disconnected, true);
  f.ctx.createBufferSource = createSource; f.pointer('pointerover', button());
  assert.equal(f.ctx.voices.length, 2); f.api.dispose();
  const ctx = new Context(); ctx.createBuffer = () => { throw new Error('buffer failed'); };
  const g = fixture({createAudioContext: () => ctx});
  await g.gesture(); assert.equal(ctx.closed, true); assert.equal(ctx.gains[0].disconnected, true); g.api.dispose();
});

test('visible but unfocused window is silent, blur stops voices and focus recovers', async () => {
  const f = fixture(); await f.gesture(); f.pointer('pointerover', button());
  const voice = f.ctx.voices[0];
  f.doc.focused = false; f.win.emit('blur');
  assert.equal(voice.disconnected, true); assert.ok(voice.stops >= 1);
  f.pointer('pointerover', button()); assert.equal(f.ctx.voices.length, 1);
  f.doc.focused = true; f.win.emit('focus'); f.pointer('pointerover', button());
  assert.equal(f.ctx.voices.length, 2); f.api.dispose();
});
test('missing or failing hasFocus fails silent', async () => {
  const f = fixture(); delete f.doc.hasFocus; await f.gesture(); assert.equal(f.creates(), 0);
  f.doc.hasFocus = () => { throw new Error('not available'); }; await f.gesture(); assert.equal(f.creates(), 0); f.api.dispose();
});
test('mouse reentry reconciles missed outside release, but active and native drags stay silent', async () => {
  const f = fixture(); await f.gesture();
  f.pointer('pointerdown', button(), {buttons: 1});
  f.pointer('pointerover', button(), {buttons: 1}); assert.equal(f.ctx.voices.length, 0);
  // pointerup outside document is not delivered; mouse returns with no buttons.
  f.pointer('pointerover', button(), {buttons: 0}); assert.equal(f.ctx.voices.length, 1);
  f.pointer('pointerdown', button(), {buttons: 1}); f.doc.emit('dragstart'); f.doc.emit('pointercancel');
  f.pointer('pointerover', button(), {buttons: 0}); assert.equal(f.ctx.voices.length, 1);
  f.doc.emit('dragend'); f.pointer('pointerover', button(), {buttons: 0}); assert.equal(f.ctx.voices.length, 2); f.api.dispose();
});

test('chosen v2 WAV matches at 48 kHz; 44.1 kHz remains bounded and short', () => {
  const wav = readFileSync(new URL('./nect-ui-sound-single-tick.wav', import.meta.url));
  assert.equal(createHash('sha256').update(wav).digest('hex'), '5e8df60431605217566dfd9f3717638c240133d68f98f72b50b61e0c7a74d12c');
  assert.equal(wav.toString('ascii', 0, 4), 'RIFF'); assert.equal(wav.toString('ascii', 8, 12), 'WAVE');
  assert.equal(wav.readUInt32LE(24), 48000); assert.equal(wav.readUInt16LE(22), 1); assert.equal(wav.readUInt16LE(34), 16);
  const reference = createClockTickSamples(48000); assert.equal(reference.length, 288);
  let peak = 0;
  for (let i = 0; i < reference.length; i++) {
    const expected = wav.readInt16LE(44 + (9600 + i) * 2) / 32767;
    assert.ok(Math.abs(reference[i] * 0.12 - expected) < 1e-8);
    peak = Math.max(peak, Math.abs(reference[i] * 0.12));
  }
  assert.ok(peak <= 901 / 32767 + 1e-8);
  const resampled = createClockTickSamples(44100); assert.equal(resampled.length, 265);
  assert.ok(Math.abs(resampled.length / 44100 - 0.006) <= 0.5 / 44100);
  assert.ok(Math.max(...resampled.map(Math.abs)) * 0.12 <= 901 / 32767 + 1e-8);
  assert.ok([...resampled].every(Number.isFinite));
  for (let i = 0; i < resampled.length; i++) {
    const p = i * 48000 / 44100, index = Math.floor(p), frac = p - index;
    const a = wav.readInt16LE(44 + (9600 + index) * 2) / 32767;
    const b = index + 1 < 288 ? wav.readInt16LE(44 + (9600 + index + 1) * 2) / 32767 : 0;
    assert.ok(Math.abs(resampled[i] * 0.12 - (a * (1 - frac) + b * frac)) < 1e-8);
  }
  assert.throws(() => createClockTickSamples(NaN), RangeError);
});
