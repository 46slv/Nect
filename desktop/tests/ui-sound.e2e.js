import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));

// Keep real browser pointer/keyboard events while observing audio without relying
// on a sound device or changing the desktop's audio settings.
await page.addInitScript(() => {
  const probe = (window.soundProbe = { contexts: 0, starts: 0, gain: null });
  window.AudioContext = class {
    constructor() {
      probe.contexts++;
      this.state = 'suspended';
      this.currentTime = 0;
      this.sampleRate = 48000;
      this.destination = {};
    }
    createGain() {
      return {
        gain: { setValueAtTime: (value) => (probe.gain = value) },
        connect() {},
        disconnect() {},
      };
    }
    createBuffer(_channels, length) {
      const samples = new Float32Array(length);
      return { getChannelData: () => samples };
    }
    createBufferSource() {
      return { connect() {}, disconnect() {}, stop() {}, start: () => probe.starts++ };
    }
    resume() {
      this.state = 'running';
      return Promise.resolve();
    }
    close() {
      this.state = 'closed';
      return Promise.resolve();
    }
  };
});

const center = (box) => ({ x: box.x + box.width / 2, y: box.y + box.height / 2 });
const counts = () =>
  page.evaluate(() => ({ contexts: window.soundProbe.contexts, starts: window.soundProbe.starts }));
const preferences = () => page.evaluate(() => JSON.parse(localStorage.getItem('nect.ui-sound.v1')));
const openSound = async () => {
  const project = page.locator('header .command-menu').filter({
    has: page.locator('summary').filter({ hasText: /^Project$/ }),
  });
  await project.locator(':scope > summary').click();
  const sound = project.locator('[data-ui-sound-settings]');
  await sound.locator(':scope > summary').click();
  return sound;
};

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const first = page.locator('.tools [data-tool="direct"]');
  const second = page.locator('.tools [data-tool="select"]');
  const a = await first.boundingBox();
  const b = await second.boundingBox();
  assert.ok(a && b);

  await page.mouse.move(...Object.values(center(a)));
  assert.deepEqual(await counts(), { contexts: 0, starts: 0 }, 'hover does not unlock audio');
  await page.locator('.canvas-stage').click({ position: { x: 30, y: 30 } });
  await page.waitForFunction(() => window.soundProbe.contexts === 1);
  assert.equal((await counts()).starts, 0, 'unlock does not replay a prior hover');
  await page.mouse.move(...Object.values(center(a)));
  assert.equal((await counts()).starts, 1);
  await page.mouse.move(a.x + a.width / 2 + 1, a.y + a.height / 2 + 1);
  assert.equal((await counts()).starts, 1, 'movement within one control is silent');
  await page.mouse.move(...Object.values(center(b)));
  assert.equal((await counts()).starts, 2, 'entering another icon ticks');
  await page.mouse.down();
  await page.mouse.move(...Object.values(center(a)));
  assert.equal((await counts()).starts, 2, 'dragging across controls is silent');
  await page.mouse.up();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const layerIcon = page.locator('.layers .layer-visibility').first();
  await page.locator('.canvas-stage').hover({ position: { x: 200, y: 200 } });
  const beforeLayer = (await counts()).starts;
  await layerIcon.hover();
  assert.equal((await counts()).starts, beforeLayer + 1, 'layer icons use the same feedback');

  let sound = await openSound();
  const enabled = sound.getByLabel('UI sounds');
  const volume = sound.getByLabel('Volume');
  assert.equal(await enabled.isChecked(), true, 'sound is initially on');
  await enabled.uncheck();
  const slider = await volume.boundingBox();
  assert.ok(slider);
  await page.mouse.click(slider.x + slider.width * 0.6, slider.y + slider.height / 2);
  const selectedVolume = Number(await volume.inputValue());
  assert.ok(selectedVolume > 0 && selectedVolume !== 0.12);
  assert.deepEqual(await preferences(), { enabled: false, volume: selectedVolume });
  const muted = (await counts()).starts;
  await page.mouse.move(...Object.values(center(a)));
  await page.mouse.move(...Object.values(center(b)));
  assert.equal((await counts()).starts, muted, 'off suppresses hover ticks');

  await page.reload();
  await page.waitForFunction(() => window.nect);
  sound = await openSound();
  assert.equal(await sound.getByLabel('UI sounds').isChecked(), false);
  assert.equal(Number(await sound.getByLabel('Volume').inputValue()), selectedVolume);
  await sound.getByLabel('UI sounds').check();
  assert.deepEqual(await preferences(), { enabled: true, volume: selectedVolume });
  assert.deepEqual(await counts(), { contexts: 0, starts: 0 }, 'enabling does not replay a hover');
  await page.locator('.canvas-stage').click({ position: { x: 30, y: 30 } });
  await page.waitForFunction(() => window.soundProbe.contexts === 1);
  assert.equal(await page.evaluate(() => window.soundProbe.gain), selectedVolume);
  const reopened = await first.boundingBox();
  await page.mouse.move(...Object.values(center(reopened)));
  assert.equal((await counts()).starts, 1, 're-enabled sound uses the saved volume');
  assert.deepEqual(errors, []);
} finally {
  await browser.close();
}
