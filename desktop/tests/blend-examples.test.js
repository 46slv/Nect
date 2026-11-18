import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { deserialize, serialize, evaluate, exportSVG, CommandSession } from '../src/core.js';
for (const mode of ['distance', 'spine']) {
  const load = () =>
    deserialize(
      fs.readFileSync(new URL(`../examples/blend-${mode}.json`, import.meta.url), 'utf8'),
    );
  const count = (d) => {
    const r = evaluate(d.layers[0], {}, d.imageAssets, d);
    assert.equal(r.error, null);
    return r.path.children.length;
  };
  test(`${mode} example reopens and matches its published SVG`, () => {
    const doc = load(),
      reopened = deserialize(serialize(doc));
    assert.deepEqual(reopened, doc);
    assert.equal(count(doc), mode === 'distance' ? 16 : 9);
    const expected = fs
      .readFileSync(new URL(`../examples/blend-${mode}.svg`, import.meta.url), 'utf8')
      .trim();
    assert.equal(exportSVG(doc).trim(), expected);
    assert.equal(exportSVG(reopened).trim(), expected);
    assert.deepEqual(doc.fontAssets ?? {}, {});
    assert.deepEqual(doc.imageAssets ?? {}, {});
  });
  test(`${mode} documented edit and toolbar-history contract change the actual output`, () => {
    const doc = load(),
      layer = doc.layers[0],
      blend = layer.nodes.find((n) => n.type === 'Blend');
    const session = new CommandSession(doc);
    const send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    const before = exportSVG(session.snapshot());
    send('set-parameter', {
      nodeId: blend.id,
      key: mode === 'distance' ? 'distance' : 'steps',
      value: mode === 'distance' ? 60 : 3,
    });
    assert.equal(count(session.snapshot()), mode === 'distance' ? 11 : 5);
    assert.notEqual(exportSVG(session.snapshot()), before);
    const after = exportSVG(session.snapshot());
    send('undo');
    assert.equal(exportSVG(session.snapshot()), before);
    send('redo');
    assert.equal(exportSVG(session.snapshot()), after);
    assert.equal(exportSVG(deserialize(serialize(session.snapshot()))), after);
  });
}
