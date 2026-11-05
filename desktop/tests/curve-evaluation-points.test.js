import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, CommandSession, connect, evaluate, node, point } from '../src/core.js';

for (const type of ['Curve', 'Multi Path']) {
  test(`${type} evaluates current point values, expressions and sources without mutation`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      path = node(type),
      vector = node('Vector');
    path.id = `path-${type.replace(' ', '-')}`;
    path.points = Array.from({ length: 512 }, (_, index) => {
      const item = point(index, index * 2);
      item.id = `anchor-${index}`;
      return item;
    });
    if (type === 'Multi Path') {
      path.contours = [{ id: 'contour', closed: false, points: path.points.map((p) => p.id) }];
      path.units = [{ id: 'unit', contours: ['contour'] }];
    }
    path.points[0].position.expression = 'value + 5; value + 7';
    path.points[1].handleA.expression = 'value + 2; value - 3';
    vector.params.value.value = [42, 43];
    layer.nodes.push(path, vector);
    connect(layer, vector.id, path.id, 'anchor-2.position');
    const authored = JSON.stringify(layer),
      first = evaluate(layer).run(path.id).contours[0].points;
    assert.deepEqual(first[0].position, [5, 7]);
    assert.deepEqual(first[1].handleA, [2, -3]);
    assert.deepEqual(first[2].position, [42, 43]);
    assert.equal(JSON.stringify(layer), authored);

    const session = new CommandSession(doc);
    const sessionAnchor = () =>
      evaluate(session.snapshot().layers[0])
        .run(path.id)
        .contours[0].points.find((item) => item.id === 'anchor-3').position;
    assert.deepEqual(sessionAnchor(), [3, 6]);
    session.dispatch({
      ...session.state,
      type: 'translate-points',
      payload: { layerId: layer.id, nodeId: path.id, pointIds: ['anchor-3'], delta: [8, 9] },
    });
    assert.deepEqual(sessionAnchor(), [11, 15]);
    session.dispatch({ ...session.state, type: 'undo' });
    assert.deepEqual(sessionAnchor(), [3, 6]);
    session.dispatch({ ...session.state, type: 'redo' });
    assert.deepEqual(sessionAnchor(), [11, 15]);

    path.points[0].position.value = [10, 20];
    path.points.reverse();
    const afterReorder = evaluate(layer).run(path.id).contours[0].points,
      firstAnchor = afterReorder.find((item) => item.id === 'anchor-0');
    assert.deepEqual(firstAnchor.position, [15, 27]);
    assert.equal(afterReorder[0].id, type === 'Curve' ? 'anchor-511' : 'anchor-0');
    path.points = path.points.filter((item) => item.id !== 'anchor-0');
    if (type === 'Multi Path') path.contours[0].points.shift();
    const afterDelete = evaluate(layer).run(path.id).contours[0].points;
    assert.equal(afterDelete.length, 511);
    assert.equal(
      afterDelete.some((item) => item.id === 'anchor-0'),
      false,
    );
  });
}

test('direct evaluation preserves first-match behavior for duplicate point keys', () => {
  // Direct evaluation may receive an unvalidated in-memory layer. This is not
  // an accepted saved document, but its existing lookup order must not change.
  const layer = blankDocument().layers[0],
    curve = node('Curve');
  curve.points[0].position.value = [10, 20];
  curve.points[1].id = curve.points[0].id;
  curve.points[1].position.value = [30, 40];
  layer.nodes.push(curve);
  const points = evaluate(layer).run(curve.id).contours[0].points;
  assert.deepEqual(points[0].position, [10, 20]);
  assert.deepEqual(points[1].position, [10, 20]);
});
