import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { Window } from 'happy-dom';
import { CommandSession, blankDocument, node, validate, serialize, evaluate } from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { readSVGSource } from '../src/svg-tree.js';
import { featherFixture } from './fixtures/mask-feather.js';

const Parser = new Window().DOMParser;
const source = () =>
  readSVGSource(
    '<svg width="100" height="100"><rect width="50" height="50" fill="#123456"/></svg>',
    Parser,
  );
function fixture() {
  const f = featherFixture(4);
  const bytes = readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const font = makeFontAsset(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    'DejaVuSans.ttf',
  );
  f.doc.fontAssets = { [font.id]: font.asset };
  const text = node('Text');
  text.font = font.id;
  text.text = 'Untouched sibling';
  f.layer.nodes.push(text);
  const value = node('Value');
  f.layer.nodes.push(value);
  const session = new CommandSession(f.doc);
  session.dispatch({
    ...session.state,
    type: 'save-node-preset',
    payload: { layerId: f.layer.id, nodeIds: [value.id], name: 'Shared value' },
  });
  const owner = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(session.snapshot()),
  );
  const siblingId = owner.projectSnapshot().compositions[0].id;
  owner.createComposition({ name: 'SVG target' });
  return { owner, siblingId, f };
}
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

test('SVG import preserves inactive Composition assets, presets and schema through edits, history and reopen', () => {
  const { owner, siblingId, f } = fixture();
  const before = owner.projectSnapshot();
  send(owner, 'import-svg', { source: source() });
  const after = owner.projectSnapshot();
  assert.equal(after.schemaVersion, before.schemaVersion);
  assert.equal(after.name, before.name);
  for (const key of ['imageAssets', 'fontAssets', 'nodePresets'])
    assert.deepEqual(after[key], before[key]);
  assert.deepEqual(after.compositions[0], before.compositions[0]);
  assert.notDeepEqual(after.compositions[1].layers, before.compositions[1].layers);
  assert.equal(after.compositions[1].width, 100);
  validate(owner.snapshot());
  send(owner, 'set-document-settings', { key: 'width', value: 120 });
  for (const key of ['imageAssets', 'fontAssets', 'nodePresets'])
    assert.deepEqual(owner.projectSnapshot()[key], before[key]);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), after);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), after);
  owner.switchComposition(siblingId);
  const doc = owner.snapshot();
  assert.doesNotThrow(() => evaluate(doc.layers[0], {}, doc.imageAssets, doc).run(f.mask.id));
  const json = serializeCompositionProject(owner.projectSnapshot());
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(json));
  assert.equal(serializeCompositionProject(reopened.projectSnapshot()), json);
  assert.deepEqual(reopened.projectSnapshot().compositions[0], before.compositions[0]);
});

test('invalid SVG or stale command preserves the complete project and redo', () => {
  const { owner } = fixture();
  send(owner, 'import-svg', { source: source() });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  const bad = {
    ...source(),
    children: [{ tag: 'script', namespace: null, attributes: [], children: ['bad'] }],
  };
  assert.throws(() => send(owner, 'import-svg', { source: bad }));
  assert.throws(
    () =>
      owner.dispatch({
        ...owner.state,
        expectedRevision: owner.state.expectedRevision - 1,
        type: 'import-svg',
        payload: { source: source() },
      }),
    /STALE_REVISION/,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  assert.doesNotThrow(() => createCompositionSessionOwner(owner.projectSnapshot()));
});

test('legacy SVG import retains its minimum schema without synthesizing absent registries', () => {
  const doc = blankDocument();
  const session = new CommandSession(doc);
  send(session, 'import-svg', { source: source() });
  const imported = session.snapshot();
  assert.ok(imported.schemaVersion >= doc.schemaVersion);
  for (const key of ['imageAssets', 'fontAssets', 'nodePresets'])
    assert.equal(Object.hasOwn(imported, key), false);
  assert.doesNotThrow(() => serialize(imported));
});

test('SVG viewport transform can raise the preserved source schema', () => {
  const session = new CommandSession(blankDocument());
  assert.equal(session.snapshot().schemaVersion, 27);
  send(session, 'import-svg', {
    source: readSVGSource(
      '<svg width="200" height="100" viewBox="10 20 100 50"><rect width="50" height="50"/></svg>',
      Parser,
    ),
  });
  assert.equal(session.snapshot().schemaVersion, 31);
  assert.doesNotThrow(() => serialize(session.snapshot()));
});
