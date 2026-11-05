import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  setNodeBypassed,
  validate,
} from '../src/core.js';

test('Reverse Path distinguishes absent input from a connected zero-contour path', () => {
  for (const kind of ['unconnected', 'bypass-null', 'zero-contours', 'bypass-empty']) {
    const doc = document(),
      layer = doc.layers[0],
      source = node('Multi Path'),
      reverse = node('Reverse Path'),
      out = node('Output');
    doc.schemaVersion = 31;
    layer.id = 'layer';
    source.id = 'source';
    reverse.id = 'reverse';
    out.id = 'out';
    layer.nodes = [source, reverse, out];
    layer.output = out.id;
    if (kind === 'zero-contours' || kind === 'bypass-empty')
      connect(layer, source.id, reverse.id, 'path');
    if (kind.startsWith('bypass')) setNodeBypassed(layer, reverse.id, true);
    connect(layer, reverse.id, out.id, 'path');
    const saved = serialize(doc);
    assert.equal(serialize(deserialize(saved)), saved);
    const first = evaluate(layer),
      second = evaluate(layer);
    assert.deepEqual(first.run(source.id).contours, []);
    if (kind === 'unconnected' || kind === 'bypass-null') {
      assert.throws(
        () => first.run(reverse.id),
        kind === 'unconnected' ? /Connect a Path input/ : /Connect path to bypass Reverse Path/,
      );
    } else {
      const value = first.run(reverse.id);
      assert.deepEqual(value.contours, []);
      assert.equal(value.unstable, true);
      assert.equal(
        value.fingerprint,
        kind === 'bypass-empty'
          ? '[]'
          : 'sha256:4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945',
      );
      assert.deepEqual(first.run(out.id), value);
      assert.deepEqual(second.run(reverse.id), value);
      assert.deepEqual(second.stats, { computed: 0, reused: 3 });
    }
    assert.equal(serialize(doc), saved);
  }
});

test('Reverse Path paintMode remains invalid in schema 31', () => {
  const doc = document(),
    layer = doc.layers[0],
    reverse = node('Reverse Path'),
    out = node('Output');
  doc.schemaVersion = 31;
  reverse.id = 'reverse';
  out.id = 'out';
  layer.nodes = [reverse, out];
  layer.output = out.id;
  reverse.paintMode = true;
  assert.throws(
    () => validate(doc),
    /Paint routing requires schemaVersion 31 and Transform or Join Geometry/,
  );
  assert.throws(
    () => deserialize(serialize(doc)),
    /Paint routing requires schemaVersion 31 and Transform or Join Geometry/,
  );
});
