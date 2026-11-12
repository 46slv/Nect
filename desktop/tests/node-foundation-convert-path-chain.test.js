import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  CommandSession,
  connect,
  deserialize,
  enableRemapPoint,
  evaluate,
  node,
  serialize,
} from '../src/core.js';

test('painted conversion and all path edits retain current identities and Remap history together', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Rectangle'),
    paint = node('Fill'),
    convert = node('Convert to Path'),
    trim = node('Trim Path'),
    start = node('Set Path Start'),
    dash = node('Dash Path'),
    remap = node('Path Remap'),
    fill = node('Fill');
  layer.id = 'layer';
  output.id = 'output';
  layer.output = output.id;
  for (const [item, id] of [
    [source, 'source'],
    [paint, 'paint'],
    [convert, 'convert'],
    [trim, 'trim'],
    [start, 'start'],
    [dash, 'dash'],
    [remap, 'remap'],
    [fill, 'fill'],
  ])
    item.id = id;
  source.params.center.value = [50, 50];
  source.params.size.value = [100, 100];
  start.params.start.value = 0.25;
  dash.params.dashLength.value = 30;
  dash.params.gapLength.value = 20;
  layer.nodes = [source, paint, convert, trim, start, dash, remap, fill, output];
  for (const [from, to] of [
    [source, paint],
    [paint, convert],
    [convert, trim],
    [trim, start],
    [start, dash],
    [dash, remap],
    [remap, fill],
    [fill, output],
  ])
    connect(layer, from.id, to.id, 'path');

  const baseline = evaluate(layer, {}, doc.imageAssets, doc),
    expectedFingerprints = [
      'sha256:5e5b0b13f066436315dd29990887a495fff5fb3ba303a893ab12bb73e1f3b00c',
      'sha256:59b76f848014a9122af0d5c4cbb8ed35bc53891da24c9bb759bcceb41e9c2e60',
      'sha256:a3b3e964ca66f3de4b9b8217706e149b6ef2b9e838cccef6a5fc55aa6d7721f8',
      'sha256:73204df7995bb473562a003bd031473180c0439358fd2544e859a7fc472bfa2b',
    ];
  assert.equal(baseline.error, null);
  assert.deepEqual(
    [convert, trim, start, dash].map((item) => baseline.run(item.id).fingerprint),
    expectedFingerprints,
  );
  const dashResult = baseline.run(dash.id),
    point = dashResult.contours[0].points[0];
  assert.equal(dashResult.contours.length, 8);
  assert.equal(
    point.id,
    'sha256:af9d2c92ddc38ca77b3cd29e944c9b61928032808d3a3cb6c3a3c8965067291e:c0:d0:p0',
  );
  enableRemapPoint(remap, dashResult, point.id).params.position.value = [3, 1];

  const session = new CommandSession(deserialize(serialize(doc))),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    result = () => {
      const snapshot = session.snapshot(),
        evaluated = evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot);
      return { evaluated, path: evaluated.run(remap.id) };
    },
    original = serialize(session.snapshot());
  assert.deepEqual(result().path.contours[0].points[0].position, [3, 1]);
  assert.deepEqual(result().evaluated.warnings, []);
  send('set-parameter', { nodeId: dash.id, key: 'dashLength', value: 40 });
  const changed = serialize(session.snapshot());
  assert.match(result().evaluated.warnings.join(' '), /suspended/i);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(result().path.contours[0].points[0].position, [3, 1]);
  assert.deepEqual(result().evaluated.warnings, []);
  const history = JSON.stringify(session.historyView),
    state = session.state;
  assert.throws(() => send('set-parameter', { nodeId: dash.id, key: 'dashLength', value: 0 }));
  assert.equal(serialize(session.snapshot()), original);
  assert.deepEqual(session.state, state);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  assert.match(result().evaluated.warnings.join(' '), /suspended/i);
  assert.equal(serialize(deserialize(changed)), changed);
});
