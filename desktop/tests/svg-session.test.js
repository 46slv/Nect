import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { CommandSession, document, exportSVG } from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';

const Parser = new Window().DOMParser;
const source = () =>
  readSVGSource(
    '<svg viewBox="0 0 200 200"><g data-layer="Imported"><path d="M0 0H100V100H0Z M20 20V80H80V20Z" fill="#123456"/></g></svg>',
    Parser,
  );
test('portable SVG syntax imports through a DOM-free Session with core IDs and one Undo', () => {
  const s = new CommandSession(document()),
    before = s.snapshot(),
    tree = source();
  const result = s.dispatch({ ...s.state, type: 'import-svg', payload: { source: tree } });
  const imported = s.snapshot();
  assert.equal(imported.layers[0].id, result.layerId);
  assert.equal(imported.layers[0].nodes[0].id, result.nodeId);
  assert.match(exportSVG(imported), /#123456/);
  tree.children.length = 0;
  assert.deepEqual(s.snapshot(), imported);
  s.dispatch({ ...s.state, type: 'undo' });
  assert.deepEqual(s.snapshot(), before);
  s.dispatch({ ...s.state, type: 'redo' });
  assert.deepEqual(s.snapshot(), imported);
  const other = new CommandSession(before);
  other.dispatch({ ...other.state, type: 'import-svg', payload: { source: source() } });
  assert.notEqual(other.snapshot().layers[0].id, imported.layers[0].id);
});
test('SVG command refuses unsupported/malformed syntax and stale envelopes without dropping edits or redo', () => {
  const s = new CommandSession(document()),
    send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  send('import-svg', { source: source() });
  send('undo');
  const before = s.snapshot(),
    revision = s.state.expectedRevision;
  for (const bad of [
    {
      ...source(),
      children: [{ tag: 'script', namespace: null, attributes: [], children: ['alert(1)'] }],
    },
    {
      ...source(),
      attributes: [
        ['width', '200'],
        ['width', '300'],
      ],
    },
    { ...source(), document: before },
  ]) {
    assert.throws(() => send('import-svg', { source: bad }));
    assert.deepEqual(s.snapshot(), before);
    assert.equal(s.state.expectedRevision, revision);
  }
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        expectedRevision: revision - 1,
        type: 'import-svg',
        payload: { source: source() },
      }),
    /STALE_REVISION/,
  );
  send('redo');
  assert.match(exportSVG(s.snapshot()), /#123456/);
});
