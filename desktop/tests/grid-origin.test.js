import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  blankDocument,
  guideDefaults,
  gridAnchors,
  resolveGridOrigin,
  snapTranslation,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
} from '../src/core.js';

const anchors = {
  'top-left': [0, 0],
  'top-center': [0.5, 0],
  'top-right': [1, 0],
  'middle-left': [0, 0.5],
  center: [0.5, 0.5],
  'middle-right': [1, 0.5],
  'bottom-left': [0, 1],
  'bottom-center': [0.5, 1],
  'bottom-right': [1, 1],
};
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });

test('all nine origins follow exact canvas dimensions and grid-only snapping at different zooms', () => {
  assert.deepEqual(gridAnchors, Object.keys(anchors));
  const doc = blankDocument();
  Object.assign(doc.editor, { snap: true, snapArtboard: false, gridSize: 40 });
  for (const [anchor, factors] of Object.entries(anchors)) {
    doc.editor.gridAnchor = anchor;
    for (const [width, height] of [
      [961, 541],
      [960.5, 540.25],
      [1001.25, 733.5],
    ]) {
      Object.assign(doc, { width, height });
      const expected = [width * factors[0], height * factors[1]];
      assert.deepEqual(resolveGridOrigin(doc), expected);
      // Check a translated grid intersection, not just the origin itself.
      for (const zoom of [0.5, 2]) {
        const point = [expected[0] + 80, expected[1] - 40];
        const snap = snapTranslation(doc, [[point[0] - 2], [point[1] + 1]], [0, 0], zoom);
        assert.deepEqual(snap.delta, [2, -1]);
        assert.deepEqual(
          snap.matches.map((m) => m.position),
          point,
        );
        assert.ok(snap.matches.every((m) => m.kind === 'grid'));
        assert.deepEqual(
          snapTranslation(doc, [[point[0] - 2], [point[1] + 1]], [0, 0], zoom, true).matches,
          [],
        );
      }
    }
  }
});

test('every anchor is one Session Undo/Redo, persists, and never moves artwork or absolute guides', () => {
  for (const anchor of gridAnchors) {
    const doc = document();
    doc.editor = guideDefaults();
    doc.editor.guides.push({ id: 'absolute-guide', axis: 'x', position: 123.25, locked: false });
    const session = new CommandSession(doc),
      before = serialize(session.snapshot()),
      svg = exportSVG(doc);
    send(session, 'set-guide-setting', { key: 'gridAnchor', value: anchor });
    const changed = session.snapshot(),
      saved = serialize(changed);
    assert.equal(
      changed.schemaVersion,
      anchor === 'top-left' ? doc.schemaVersion : anchor === 'center' ? 27 : 48,
    );
    assert.deepEqual(changed.layers, doc.layers);
    assert.deepEqual(changed.editor.guides, doc.editor.guides);
    assert.equal(changed.editor.gridSize, 8);
    assert.equal(exportSVG(changed), svg);
    assert.equal(serialize(deserialize(saved)), saved);
    assert.equal(session.historyView.past.length, anchor === 'top-left' ? 0 : 1);
    send(session, 'set-guide-setting', { key: 'gridAnchor', value: anchor });
    assert.equal(session.historyView.past.length, anchor === 'top-left' ? 0 : 1);
    if (anchor !== 'top-left') {
      send(session, 'undo');
      assert.equal(serialize(session.snapshot()), before);
      send(session, 'redo');
      assert.equal(serialize(session.snapshot()), saved);
    }
    send(session, 'set-document-settings', { key: 'width', value: 1023.5 });
    send(session, 'set-document-settings', { key: 'height', value: 601.25 });
    assert.deepEqual(resolveGridOrigin(session.snapshot()), [
      1023.5 * anchors[anchor][0],
      601.25 * anchors[anchor][1],
    ]);
    assert.deepEqual(session.snapshot().layers, doc.layers);
    assert.deepEqual(session.snapshot().editor.guides, doc.editor.guides);
  }
});

test('legacy/default phases and versions remain; only added authored anchors require schema48', () => {
  const legacy = document();
  const incomingVersion = legacy.schemaVersion;
  for (const editor of [undefined, guideDefaults(), { ...guideDefaults(), gridAnchor: 'center' }]) {
    if (editor) legacy.editor = editor;
    else delete legacy.editor;
    legacy.schemaVersion = editor?.gridAnchor === 'center' ? 27 : incomingVersion;
    const saved = serialize(legacy);
    assert.equal(serialize(deserialize(saved)), saved);
    assert.deepEqual(
      resolveGridOrigin(legacy),
      editor?.gridAnchor === 'center' ? [480, 270] : [0, 0],
    );
  }
  assert.equal(blankDocument().editor.gridAnchor, 'center');
  assert.equal(blankDocument().schemaVersion, 27);
  for (const anchor of gridAnchors.filter((a) => !['top-left', 'center'].includes(a))) {
    legacy.editor.gridAnchor = anchor;
    legacy.schemaVersion = 47;
    assert.throws(() => deserialize(serialize(legacy)), /schemaVersion 48/);
    legacy.schemaVersion = 48;
    assert.equal(deserialize(serialize(legacy)).editor.gridAnchor, anchor);
  }
  for (const anchor of ['unknown', 'toString', '__proto__', null]) {
    assert.deepEqual(resolveGridOrigin(legacy, { gridAnchor: anchor }), [0, 0]);
  }
});

test('invalid or stale origin changes reject atomically without history or revision changes', () => {
  const session = new CommandSession(blankDocument());
  for (const value of ['unknown', 'toString', null, 1, false, [0, 1]]) {
    const before = serialize(session.snapshot()),
      state = session.state,
      past = session.historyView.past.length;
    assert.throws(
      () => send(session, 'set-guide-setting', { key: 'gridAnchor', value }),
      /Invalid grid anchor/,
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.past.length, past);
  }
  const stale = session.state;
  send(session, 'set-guide-setting', { key: 'gridAnchor', value: 'bottom-right' });
  const saved = serialize(session.snapshot());
  assert.throws(
    () =>
      session.dispatch({
        ...stale,
        type: 'set-guide-setting',
        payload: { key: 'gridAnchor', value: 'top-right' },
      }),
    /revision|stale/i,
  );
  assert.equal(serialize(session.snapshot()), saved);
});
