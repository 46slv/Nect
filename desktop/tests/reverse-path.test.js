import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';

function scene(rawPath) {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    reverse = node('Reverse Path'),
    trimForward = node('Trim Path'),
    trimReverse = node('Trim Path'),
    fill = node('Fill'),
    output = node('Output');
  source.rawPath = rawPath;
  trimForward.params.end.value = 0.5;
  trimReverse.params.end.value = 0.5;
  layer.nodes = [source, reverse, trimForward, trimReverse, fill, output];
  layer.output = output.id;
  connect(layer, source.id, reverse.id, 'path');
  connect(layer, source.id, trimForward.id, 'path');
  connect(layer, reverse.id, trimReverse.id, 'path');
  connect(layer, reverse.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, reverse, trimForward, trimReverse };
}

function cubicSegments(contour) {
  const segments = [];
  const count = contour.closed ? contour.points.length : contour.points.length - 1;
  for (let index = 0; index < count; index++) {
    const start = contour.points[index];
    const end = contour.points[(index + 1) % contour.points.length];
    segments.push([
      [...start.position],
      start.position.map((value, axis) => value + start.handleB[axis]),
      end.position.map((value, axis) => value + end.handleA[axis]),
      [...end.position],
    ]);
  }
  return segments;
}

test('Reverse Path has only a Path input and deterministically reverses exact open Beziers', () => {
  const { doc, layer, source, reverse } = scene('M0 0C0 100 100 100 100 0'),
    before = JSON.stringify(source),
    original = evaluate(layer).run(source.id),
    result = evaluate(layer).run(reverse.id),
    repeated = evaluate(layer).run(reverse.id);

  assert.deepEqual(Object.keys(reverse.params), ['path']);
  assert.equal(reverse.params.path.type, 'Path');
  assert.deepEqual(result, repeated);
  assert.equal(result.unstable, true);
  assert.ok(result.fingerprint);
  assert.notEqual(result.contours[0].id, original.contours[0].id);
  assert.deepEqual(
    result.contours[0].points.map(({ position }) => position),
    original.contours[0].points.map(({ position }) => position).reverse(),
  );
  for (const [index, point] of result.contours[0].points.entries()) {
    const old = original.contours[0].points.at(-1 - index);
    assert.deepEqual(point.handleA, old.handleB);
    assert.deepEqual(point.handleB, old.handleA);
  }
  assert.deepEqual(
    cubicSegments(result.contours[0]),
    cubicSegments(original.contours[0])
      .reverse()
      .map((segment) => segment.reverse()),
  );
  assert.equal(JSON.stringify(source), before, 'source geometry is left untouched');

  const reopened = deserialize(serialize(doc));
  assert.deepEqual(evaluate(reopened.layers[0]).run(reverse.id), result);
  assert.match(exportSVG(doc), /<path /);
});

test('Reverse Path changes the first half selected by Trim Path', () => {
  const { layer, trimForward, trimReverse } = scene('M0 0L100 0'),
    forward = evaluate(layer).run(trimForward.id).contours[0],
    reversed = evaluate(layer).run(trimReverse.id).contours[0];

  assert.deepEqual(forward.points.map(({ position }) => position), [
    [0, 0],
    [50, 0],
  ]);
  assert.deepEqual(reversed.points.map(({ position }) => position), [
    [100, 0],
    [50, 0],
  ]);
});

test('Reverse Path preserves the exact closed curve and its original seam anchor', () => {
  const { layer, source, reverse } = scene('M0 0C0 50 50 50 50 0L50 50L0 50Z'),
    original = evaluate(layer).run(source.id).contours[0],
    result = evaluate(layer).run(reverse.id).contours[0];

  assert.equal(result.closed, true);
  assert.deepEqual(result.points[0].position, original.points[0].position);
  assert.deepEqual(
    result.points.map(({ position }) => position),
    [original.points[0].position, ...original.points.slice(1).reverse().map(({ position }) => position)],
  );
  assert.deepEqual(
    cubicSegments(result),
    cubicSegments(original)
      .reverse()
      .map((segment) => segment.reverse()),
  );
});

test('Reverse Path participates in Session add, bypass, Undo/Redo, input locks, and SVG roundtrip', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    output = node('Output');
  source.rawPath = 'M0 0L100 0';
  layer.nodes = [source, output];
  layer.output = output.id;
  connect(layer, source.id, output.id, 'path');

  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } }),
    initial = serialize(session.snapshot()),
    created = send('create-node', { nodeType: 'Reverse Path' }),
    reverseId = created.nodeId,
    added = serialize(session.snapshot());

  assert.ok(session.snapshot().layers[0].nodes.some(({ id }) => id === reverseId));
  send('undo');
  assert.equal(serialize(session.snapshot()), initial);
  send('redo');
  assert.equal(serialize(session.snapshot()), added);

  send('connect', { sourceId: source.id, nodeId: reverseId, key: 'path' });
  const connected = evaluate(session.snapshot().layers[0]).run(reverseId);
  send('set-node-bypassed', { nodeId: reverseId, bypassed: true });
  assert.deepEqual(
    evaluate(session.snapshot().layers[0]).run(reverseId),
    evaluate(session.snapshot().layers[0]).run(source.id),
  );
  send('undo');
  assert.deepEqual(evaluate(session.snapshot().layers[0]).run(reverseId), connected);
  send('redo');
  send('undo');

  send('set-parameter-lock', { nodeId: reverseId, key: 'path', locked: true });
  const locked = serialize(session.snapshot()),
    lockedState = session.state;
  assert.throws(() => send('disconnect', { nodeId: reverseId, key: 'path' }), /Parameter is locked/);
  assert.equal(serialize(session.snapshot()), locked);
  assert.deepEqual(session.state, lockedState);
});

test('Reverse Path retains Text provenance and suspends direct canvas character editing', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = opentype.parse(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    reverse = node('Reverse Path');
  layer.nodes = [text, reverse];
  connect(layer, text.id, reverse.id, 'path');

  const path = evaluate(layer, { default: font }).run(reverse.id);
  assert.equal(path.characterOccurrences.length, 1);
  assert.equal(path.characterOccurrences[0].source, text.id);
  assert.match(
    path.characterOccurrences[0].reason,
    /Canvas character editing is unavailable through Reverse Path; edit the source Text/,
  );
});
