import assert from 'node:assert/strict';
import test from 'node:test';
import { CommandSession, document, exportSVG, node, serialize } from '../src/core.js';

function outline(text) {
  return {
    family: 'Available OS font',
    style: 0,
    text,
    size: 100,
    ascent: 80,
    points: [
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

test('responsive SVG export rejects unknown output-copy options', () => {
  assert.throws(() => exportSVG(document(), {}, { responsive: 1 }), /Invalid SVG export options/);
  assert.throws(() => exportSVG(document(), {}, { precision: 3 }), /Invalid SVG export options/);
});
