import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  blankLayer,
  serialize,
  deserialize,
  parameter,
  evaluate,
} from '../src/core.js';
import { featherFixture } from './fixtures/mask-feather.js';

for (const luminance of [false, true]) {
  test(`named Mask preset preserves authored schema ${luminance ? 52 : 50} through save/apply/history/reopen`, () => {
    const f = featherFixture(4);
    if (luminance) {
      f.mask.params.luminance = {
        ...parameter(f.mask, 'luminance'),
        value: true,
      };
      f.doc.schemaVersion = 52;
    }
    const target = blankLayer('Target');
    f.doc.layers.push(target);
    const session = new CommandSession(f.doc);
    const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
    const original = serialize(session.snapshot());
    const sourceNodes = [f.image.id, f.keyer.id, f.shape.id, f.mask.id];
    const { presetId } = send('save-node-preset', {
      layerId: f.layer.id,
      nodeIds: sourceNodes,
      name: 'Mask preset',
    });
    assert.equal(session.snapshot().schemaVersion, luminance ? 52 : 50);
    const saved = serialize(session.snapshot());
    send('undo');
    assert.equal(serialize(session.snapshot()), original);
    send('redo');
    assert.equal(serialize(session.snapshot()), saved);
    const { nodeIds } = send('apply-node-preset', {
      layerId: target.id,
      presetId,
    });
    const doc = session.snapshot();
    const dst = doc.layers.find((l) => l.id === target.id);
    const mask = dst.nodes.find((n) => n.type === 'Mask');
    assert.equal(mask.params.feather.value, 4);
    if (luminance) assert.equal(mask.params.luminance.value, true);
    assert.ok(nodeIds.every((id) => !sourceNodes.includes(id)));
    assert.deepEqual(
      doc.layers.find((l) => l.id === f.layer.id),
      f.layer,
    );
    assert.doesNotThrow(() => evaluate(dst, {}, doc.imageAssets, doc).run(mask.id));
    const applied = serialize(doc);
    send('undo');
    assert.equal(serialize(session.snapshot()), saved);
    send('redo');
    assert.equal(serialize(session.snapshot()), applied);
    assert.equal(serialize(deserialize(applied)), applied);
    const reopened = new CommandSession(deserialize(applied));
    assert.doesNotThrow(() =>
      reopened.dispatch({
        ...reopened.state,
        type: 'apply-node-preset',
        payload: { layerId: target.id, presetId },
      }),
    );
  });
}
