import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {
  document as makeDoc,
  node,
  point,
  connect,
  guideDefaults,
  pathBounds,
} from '../src/core.js';
import { transformHandleGeometry } from '../src/transform-handles.js';

// This suite loads only the built production UI; fixtures come from the local
// command model, so there are no /src imports or instrumented renderer hooks.
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1920, height: 1200 } });
const errors = [],
  scenarios = [];
page.on('pageerror', (e) => errors.push(e.message));
page.on('dialog', (d) => d.accept());
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 0.002, `${a} != ${b}`));
const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
const get = (id) =>
  page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), id);
const xy = (p) =>
  page.locator('.artboard').evaluate((svg, p) => {
    const q = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
    return [q.x, q.y];
  }, p);
const down = async (p) => {
  await page.mouse.move(...(await xy(p)));
  await page.mouse.down();
};
const move = async (p) => page.mouse.move(...(await xy(p)), { steps: 5 });
const up = () => page.mouse.up();
const undo = () => page.locator('[data-action="undo"]').click();
const redo = () => page.locator('[data-action="redo"]').click();
const load = async (d) => {
  await page.evaluate((s) => window.nect.load(s), JSON.stringify(d));
  await page.waitForFunction(() => window.nect.evaluate().error === null);
  await page.locator('.artboard').waitFor();
  await page.locator('[data-tool="select"]').click();
};
function fixture(type, snap = true) {
  const d = makeDoc(),
    l = d.layers[0],
    n = node(type),
    out = node('Output');
  d.width = 1000;
  d.height = 700;
  const key = ['Text', 'Null'].includes(type)
    ? 'position'
    : type === 'Vector'
      ? 'value'
      : type === 'Transform'
        ? 'offset'
        : 'center';
  if (n.params[key]) n.params[key].value = [250, 200];
  if (type === 'Vector') n.canvasPositionHandle = true;
  if (type === 'Text') {
    n.text = 'ABC';
    n.params.size.value = 50;
  }
  l.nodes = [n, out];
  l.output = out.id;
  if (['Null', 'Vector', 'Transform'].includes(type)) {
    const c = node('Circle');
    c.params.center.value = [100, 100];
    l.nodes.push(c);
    if (type === 'Transform') {
      connect(l, c.id, n.id, 'path');
      connect(l, n.id, out.id, 'path');
    } else {
      connect(l, n.id, c.id, 'center');
      connect(l, c.id, out.id, 'path');
    }
  } else if (['Curve', 'Path Remap'].includes(type)) {
    const c = type === 'Curve' ? n : node('Curve');
    c.points = [point(250, 200), point(450, 200), point(450, 400)];
    c.params.closed.value = false;
    if (type === 'Path Remap') {
      l.nodes.push(c);
      connect(l, c.id, n.id, 'path');
    }
    connect(l, n.id, out.id, 'path');
  } else connect(l, n.id, out.id, 'path');
  d.editor = { ...guideDefaults(), snap, snapGrid: false, snapArtboard: false };
  return { d, l, n, out, key };
}
try {
  await fs.mkdir('test-results/snap-45', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5178');
  await page.waitForFunction(() => window.nect);
  // Position operations, Snap master/Alt bypass, one gesture = one history entry.
  for (const type of [
    'Null',
    'Vector',
    'Rectangle',
    'Circle',
    'Text',
    'Transform',
    'Curve',
    'Path Remap',
  ]) {
    console.log('Production scenario:', type);
    const { d, n, key } = fixture(type, false);
    await load(d);
    const pointId = n.points?.[0]?.id;
    const marker = ['Null', 'Vector'].includes(type)
      ? '[data-position-handle]'
      : ['Curve', 'Path Remap'].includes(type)
        ? '.point-handle:not([data-handle])'
        : '.node-stage-move';
    if (type === 'Text')
      await page.waitForFunction(() => document.querySelector('.node-stage-move'));
    const target = type === 'Curve' ? `[data-point="${pointId}"]:not([data-handle])` : marker;
    if (type === 'Path Remap') {
      // The retained edit has the input source's stable point identity.
      const source = d.layers[0].nodes.find((v) => v.type === 'Curve');
      await page
        .locator(`[data-point="${source.points[0].id}"]:not([data-handle])`)
        .first()
        .waitFor();
    } else await page.locator(target).first().waitFor();
    const start = ['Null', 'Vector', 'Curve', 'Path Remap'].includes(type)
      ? [250, 200]
      : await page
          .locator(target)
          .first()
          .evaluate((el) => {
            const p = new DOMPoint(0, 0)
              .matrixTransform(el.getScreenCTM())
              .matrixTransform(document.querySelector('.artboard').getScreenCTM().inverse());
            return [p.x, p.y];
          });
    const before = await saved();
    await page.keyboard.down('Shift');
    await down(start);
    await move([start[0] + 40, start[1] + 36]);
    await up();
    await page.keyboard.up('Shift');
    const result = await get(n.id);
    if (type === 'Curve') near(result.points[0].position.value, [288, 238]);
    else if (type === 'Path Remap') near(result.edits[0].params.position.value, [38, 38]);
    else near(result.params[key].value, [288, 238]);
    const after = await saved();
    await undo();
    assert.equal(await saved(), before, type);
    assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true, type);
    await redo();
    assert.equal(await saved(), after, type);
    scenarios.push(`${type}: Shift45 + SnapOFF + Undo/Redo`);
  }
  {
    const { d, n } = fixture('Null');
    await load(d);
    await down([250, 200]);
    await move([280, 234]);
    await page.locator('[data-snap-kind="direction"]').waitFor();
    await page.screenshot({ path: 'test-results/snap-45/direction-guide.png' });
    await up();
    near((await get(n.id)).params.position.value, [282, 232]);
    await undo();
    await page.keyboard.down('Alt');
    await down([250, 200]);
    await move([280, 234]);
    await up();
    await page.keyboard.up('Alt');
    near((await get(n.id)).params.position.value, [280, 234]);
    d.editor.guides = [
      { id: 'gx', axis: 'x', position: 290, locked: false },
      { id: 'gy', axis: 'y', position: 244, locked: false },
    ];
    await load(d);
    await page.keyboard.down('Shift');
    await down([250, 200]);
    await move([288, 238]);
    assert.equal(await page.locator('.snap-match[x1="290"]').count(), 1);
    assert.equal(await page.locator('.snap-match[y1="244"]').count(), 0);
    await up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).params.position.value, [290, 240]);
    scenarios.push('Automatic45 / Alt / diagonal guide winner');
  }
  // Pen anchors and new handles use the same screen-space direction and one Undo.
  {
    const d = makeDoc();
    d.width = 1000;
    d.height = 700;
    d.editor = { ...guideDefaults(), snap: false };
    await load(d);
    const before = await saved();
    await page.locator('[data-tool="pen"]').click();
    await down([250, 200]);
    await up();
    await page.keyboard.down('Shift');
    await down([350, 290]);
    await move([390, 326]);
    await up();
    await page.keyboard.up('Shift');
    await page.keyboard.press('Enter');
    const curve = await page.evaluate(() =>
      window.nect.document.layers.flatMap((l) => l.nodes).find((n) => n.type === 'Curve'),
    );
    near(curve.points[1].position.value, [345, 295]);
    // Pointer was pressed off the snapped anchor: handle delta uses pointer movement.
    near(curve.points[1].handleB.value, [38, 38]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Pen anchor45 + Bezier45 + one Undo');
  }
  // Existing nonzero Bezier handle: angle is about the anchor, not drag start.
  {
    const { d, n } = fixture('Curve', false);
    n.points[0].handleB.value = [40, 0];
    n.points[0].handleType = 'corner';
    await load(d);
    await page.locator('[data-tool="select"]').click();
    await down([250, 200]);
    await up();
    await page.locator(`[data-point="${n.points[0].id}"][data-handle="handleB"]`).first().waitFor();
    const before = await saved();
    await page.keyboard.down('Shift');
    await down([290, 200]);
    await move([300, 230]);
    await up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).points[0].handleB.value, [40, 40]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Existing Bezier handle measured about anchor');
  }
  // Direct source point through a reflected and rotated output transform.
  {
    const { d, l, n, out } = fixture('Curve', false),
      tr = node('Transform');
    tr.params.anchor.value = [250, 200];
    tr.params.scale.value = -2;
    tr.params.rotation.value = 90;
    l.nodes.push(tr);
    connect(l, n.id, tr.id, 'path');
    connect(l, tr.id, out.id, 'path');
    await load(d);
    await page.locator('[data-tool="direct"]').click();
    const before = await saved();
    const p = await page.evaluate(() => window.nect.evaluate().path.contours[0].points[0].position);
    await page.keyboard.down('Shift');
    await down(p);
    await move([p[0] + 40, p[1] + 36]);
    await up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).points[0].position.value, [231, 219]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Source point45 through rotation90 / scale-2');
  }
  {
    const { d, n, out } = fixture('Rectangle');
    n.position = [100, 100];
    out.position = [700, 100];
    d.layers[0].nodes.forEach((n) => (n.collapsed = true));
    await load(d);
    const before = await saved();
    const box = await page.locator(`.node[data-id="${n.id}"] .node-head`).boundingBox();
    const p = [box.x + 80, box.y + 15];
    await page.mouse.move(...p);
    await page.mouse.down();
    await page.keyboard.down('Shift');
    await page.mouse.move(p[0] + 80, p[1] + 72, { steps: 5 });
    await page.locator('.graph-snap-guides [data-snap-kind="direction"]').waitFor();
    await page.mouse.up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).position, [176, 176]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Graph node45 / visible guide / one Undo');
  }
  {
    const { d, n } = fixture('Transform', false);
    await load(d);
    const before = await saved();
    const state = await page.evaluate((id) => window.nect.evaluate().transformState(id), n.id);
    const scale = await page
      .locator('.artboard')
      .evaluate((s) => Math.hypot(s.getScreenCTM().a, s.getScreenCTM().b));
    const g = transformHandleGeometry(state, pathBounds(state.input), scale);
    const a = Math.PI / 4,
      x = g.rotation[0] - g.pivot[0],
      y = g.rotation[1] - g.pivot[1];
    await page.keyboard.down('Shift');
    await down(g.rotation);
    await move([
      g.pivot[0] + x * Math.cos(a) - y * Math.sin(a),
      g.pivot[1] + x * Math.sin(a) + y * Math.cos(a),
    ]);
    await up();
    await page.keyboard.up('Shift');
    assert.equal((await get(n.id)).params.rotation.value, 45);
    await undo();
    assert.equal(await saved(), before);
    await page.keyboard.down('Shift');
    await down(g.pivot);
    await move(g.pivot.map((v, i) => v + [40, 36][i]));
    await up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).params.anchor.value, [38, 38]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Transform rotation existing45 + anchor45 + one Undo');
  }
  {
    const { d, l, n, out } = fixture('Text', false),
      tr = node('Transform');
    tr.params.anchor.value = [250, 200];
    tr.params.scale.value = -2;
    tr.params.rotation.value = 90;
    l.nodes.push(tr);
    connect(l, n.id, tr.id, 'path');
    connect(l, tr.id, out.id, 'path');
    await load(d);
    await page.locator('[data-character-mode]').click();
    await page.locator('.character-hit').first().waitFor();
    const character = await page.evaluate((id) => {
      const r = window.nect.evaluate(),
        g = r.textLayout(id).characters[0],
        m = r.path.characterOccurrences[0].matrix;
      const x = (g.bounds[0] + g.bounds[2]) / 2,
        y = (g.bounds[1] + g.bounds[3]) / 2;
      return { id: g.id, p: [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]] };
    }, n.id);
    await down(character.p);
    await up();
    const before = await saved();
    await page.keyboard.down('Shift');
    await down(character.p);
    await move(character.p.map((v, i) => v + [40, 36][i]));
    await up();
    await page.keyboard.up('Shift');
    near((await get(n.id)).characterPositions.offsets[character.id], [-19, 19]);
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Character45 through rotation90 / scale-2');
  }
  {
    const d = makeDoc(),
      l = d.layers[0],
      n = node('Multi Path'),
      out = node('Output');
    d.width = 1000;
    d.height = 700;
    d.schemaVersion = Math.max(d.schemaVersion, 22);
    n.points = [point(250, 200), point(450, 200), point(450, 400)];
    n.contours = [{ id: 'contour-45', points: n.points.map((p) => p.id), closed: true }];
    n.units = [{ id: 'unit-45', contours: ['contour-45'] }];
    l.nodes = [n, out];
    out.position = [600, 40];
    l.output = out.id;
    connect(l, n.id, out.id, 'path');
    d.editor = { ...guideDefaults(), snap: false };
    await load(d);
    const before = await saved();
    await page.locator('[data-path-unit="unit-45"]').first().click();
    const p = await page.locator('[data-path-unit-move]').evaluate((el) => {
      const p = new DOMPoint(0, 0)
        .matrixTransform(el.getScreenCTM())
        .matrixTransform(document.querySelector('.artboard').getScreenCTM().inverse());
      return [p.x, p.y];
    });
    await page.keyboard.down('Shift');
    await down(p);
    await move(p.map((v, i) => v + [40, 36][i]));
    await up();
    await page.keyboard.up('Shift');
    const moved = await get(n.id);
    n.points.forEach((p, i) =>
      near(
        moved.points[i].position.value,
        p.position.value.map((v) => v + 38),
      ),
    );
    await undo();
    assert.equal(await saved(), before);
    scenarios.push('Multi Path unit45 preserves topology + one Undo');
    const tr = node('Transform', 900, 40);
    tr.params.anchor.value = [350, 300];
    tr.params.scale.value = -2;
    tr.params.rotation.value = 90;
    l.nodes.push(tr);
    connect(l, n.id, tr.id, 'path');
    connect(l, tr.id, out.id, 'path');
    await load(d);
    await page.locator('[data-tool="direct"]').click();
    await page.locator('[data-path-unit="unit-45"]').first().click();
    const transformedBefore = await saved();
    const q = await page.locator('[data-path-unit-move]').evaluate((el) => {
      const p = new DOMPoint(0, 0)
        .matrixTransform(el.getScreenCTM())
        .matrixTransform(document.querySelector('.artboard').getScreenCTM().inverse());
      return [p.x, p.y];
    });
    await page.keyboard.down('Shift');
    await down(q);
    await move(q.map((v, i) => v + [40, 36][i]));
    await up();
    await page.keyboard.up('Shift');
    const transformed = await get(n.id);
    n.points.forEach((p, i) =>
      near(
        transformed.points[i].position.value,
        p.position.value.map((v, axis) => v + [-19, 19][axis]),
      ),
    );
    await undo();
    assert.equal(await saved(), transformedBefore);
    scenarios.push('Multi Path unit45 through rotation90 / scale-2');
  }
  assert.deepEqual(errors, []);
  await fs.writeFile(
    'test-results/snap-45/receipt.json',
    JSON.stringify({ production: true, scenarios, pageErrors: errors }, null, 2),
  );
  console.log(JSON.stringify({ production: true, scenarios, pageErrors: errors }, null, 2));
} catch (error) {
  await page.screenshot({ path: 'test-results/snap-45/failure.png' });
  console.error('Page errors:', errors);
  throw error;
} finally {
  await browser.close();
}
