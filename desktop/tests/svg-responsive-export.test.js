import assert from 'node:assert/strict';
import test from 'node:test';
import { CommandSession, document, exportSVG, node, pathData, serialize } from '../src/core.js';

function outline(text, points) {
  return {
    family: 'Available OS font',
    style: 0,
    text,
    size: 100,
    ascent: 80,
    points: points || [
      [0, 0],
      [10, 0],
      [10, 10],
      [0, 10],
    ],
    types: [0, 1, 1, 161],
  };
}

test('responsive output changes only the SVG root and leaves live Text and History intact', () => {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes.find((item) => item.id === layer.output);
  fill.params.path.source = text.id;
  output.params.path.source = fill.id;
  layer.nodes = [text, fill, output];
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: text.id, ...payload },
      });
  send('set-os-font-text', { text: 'ABC', outline: outline('ABC') });

  const source = serialize(session.snapshot()),
    state = structuredClone(session.state),
    before = session.historyView,
    past = before.past.length,
    future = before.future.length,
    fixed = exportSVG(session.snapshot(), {}, { unit: 'mm' }),
    responsive = exportSVG(session.snapshot(), {}, { unit: 'mm', responsive: true }),
    body = (svg) => svg.slice(svg.indexOf('>') + 1, svg.lastIndexOf('</svg>'));

  assert.match(fixed, /width="[^"]+mm" height="[^"]+mm" viewBox="0 0 \d+(?:\.\d+)? \d+(?:\.\d+)?"/);
  assert.match(responsive, /width="100%" height="100%" viewBox="0 0 \d+(?:\.\d+)? \d+(?:\.\d+)?"/);
  assert.equal(body(fixed), body(responsive), 'only root sizing changes');
  assert.match(responsive, /<path /);
  assert.equal(serialize(session.snapshot()), source);
  assert.deepEqual(session.state, state);
  assert.equal(session.historyView.past.length, past);
  assert.equal(session.historyView.future.length, future);

  const live = session.snapshot().layers[0].nodes.find((item) => item.id === text.id);
  assert.equal(live.type, 'Text');
  assert.equal(live.text, 'ABC');
  assert.equal(live.osFont.family, 'Available OS font');

  send('undo');
  const undone = session.snapshot().layers[0].nodes.find((item) => item.id === text.id);
  assert.equal(undone.type, 'Text');
  assert.equal(undone.text, 'Nect');
  assert.equal(undone.osFont, undefined);
  send('redo');
  const redone = session.snapshot().layers[0].nodes.find((item) => item.id === text.id);
  assert.equal(redone.type, 'Text');
  assert.equal(redone.text, 'ABC');
  assert.equal(redone.osFont.family, 'Available OS font');
});

test('SVG geometry precision changes only exported numeric attributes and leaves source state intact', () => {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes.find((item) => item.id === layer.output);
  layer.name = 'Layer 123.456';
  fill.params.path.source = text.id;
  fill.params.color.value = { hex: '#123456', alpha: 0.456789 };
  output.params.path.source = fill.id;
  layer.nodes = [text, fill, output];
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: text.id, ...payload },
      });
  send('set-os-font-text', {
    text: 'ABC',
    outline: outline('ABC', [
      [0, 0],
      [10.123456, 0],
      [10.123456, 10.123456],
      [0, 10.123456],
    ]),
  });

  const source = serialize(session.snapshot()),
    before = session.historyView,
    defaultSvg = exportSVG(session.snapshot()),
    explicitDefault = exportSVG(session.snapshot(), {}, { precision: undefined }),
    defaultMm = exportSVG(session.snapshot(), {}, { unit: 'mm' }),
    rounded = exportSVG(session.snapshot(), {}, { unit: 'mm', precision: 2 }),
    defaultPathData = [...defaultSvg.matchAll(/\bd="([^"]*)"/g)].map((match) => match[1]),
    roundedPathData = [...rounded.matchAll(/\bd="([^"]*)"/g)].map((match) => match[1]),
    root = (svg) => svg.slice(0, svg.indexOf('>') + 1);

  assert.equal(explicitDefault, defaultSvg, 'omitted precision preserves the existing SVG bytes');
  assert.notDeepEqual(roundedPathData, defaultPathData);
  assert.ok(defaultPathData.some((path) => /\d\.\d{3,}/.test(path)));
  assert.ok(roundedPathData.length > 0);
  assert.ok(roundedPathData.every((path) => !/\d\.\d{3,}/.test(path)));
  assert.equal(root(rounded), root(defaultMm), 'unit/root sizing and viewBox stay unchanged');
  assert.match(rounded, /data-layer="Layer 123\.456"/);
  assert.match(rounded, /fill="#123456"/);
  assert.match(rounded, /fill-opacity="0\.46"/);
  assert.equal(serialize(session.snapshot()), source);
  assert.deepEqual(session.historyView, before);
  const live = session.snapshot().layers[0].nodes.find((item) => item.id === text.id);
  assert.equal(live.type, 'Text');
  assert.equal(live.text, 'ABC');
});

test('chosen precision refuses to collapse a small closed contour', () => {
  const tiny = {
    contours: [
      {
        closed: true,
        points: [
          [0, 0],
          [0.0004, 0],
          [0.0004, 0.0004],
          [0, 0.0004],
        ].map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
      },
    ],
  };
  assert.throws(() => pathData(tiny, 2), /collapses small geometry/);
  assert.match(pathData(tiny, 4), /0\.0004/);
});

test('responsive SVG export validates output-copy options and precision range', () => {
  assert.throws(() => exportSVG(document(), {}, { responsive: 1 }), /Invalid SVG export options/);
  assert.throws(() => exportSVG(document(), {}, { precision: 1 }), /2 to 8/);
  assert.throws(() => exportSVG(document(), {}, { precision: 2.5 }), /2 to 8/);
  assert.throws(() => exportSVG(document(), {}, { precision: 9 }), /2 to 8/);
  assert.throws(() => exportSVG(document(), {}, { extra: true }), /Invalid SVG export options/);
});
