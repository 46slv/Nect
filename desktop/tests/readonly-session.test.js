import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, exportSVG, validate, evaluate } from '../src/core.js';

test('History view protects authored states and arrays while retaining selection identity through no-op/Undo', () => {
  const s = new CommandSession(document()),
    view = s.historyView,
    original = view.doc;
  assert.equal(s.legacyHistory, undefined);
  assert.equal(view.apply, undefined);
  assert.equal(view.record, undefined);
  assert.throws(() => {
    view.doc = document();
  }, TypeError);
  assert.throws(() => {
    view.doc.layers[0].nodes[0].params.radius.value = 900;
  }, TypeError);
  assert.throws(() => view.past.push(document()), TypeError);
  const layer = original.layers[0],
    id = layer.nodes[0].id;
  s.dispatch({
    ...s.state,
    type: 'set-parameter',
    payload: { layerId: layer.id, nodeId: id, key: 'radius', value: 120 },
  });
  assert.equal(view.doc, original);
  s.dispatch({
    ...s.state,
    type: 'set-parameter',
    payload: { layerId: layer.id, nodeId: id, key: 'radius', value: 150 },
  });
  const previous = view.past.at(-1),
    next = view.doc;
  assert.throws(() => {
    previous.layers[0].name = 'Foreign';
  }, TypeError);
  s.dispatch({ ...s.state, type: 'undo' });
  assert.equal(view.doc, previous);
  s.dispatch({ ...s.state, type: 'redo' });
  assert.equal(view.doc, next);
});

test('validation and export can read frozen connected authored states without rewriting sockets', () => {
  const s = new CommandSession(document()),
    doc = s.historyView.doc;
  const before = JSON.stringify(doc);
  assert.equal(validate(doc), doc);
  assert.match(exportSVG(doc), /<svg/);
  assert.equal(evaluate(doc.layers[0]).error, null);
  assert.equal(JSON.stringify(doc), before);
  const snapshot = s.snapshot();
  snapshot.layers[0].nodes[0].params.radius.value = 900;
  assert.equal(s.historyView.doc.layers[0].nodes[0].params.radius.value, 120);
});
