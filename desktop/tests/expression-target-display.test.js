import test from 'node:test';
import assert from 'node:assert/strict';
import { expressionTargetDisplay } from '../src/expression-target-display.js';
import { expressionCompletions } from '../src/expression-completion.js';

test('Artwork context distinguishes duplicate names without changing reference identity', () => {
  const document = {
    layers: [
      { id: 'local', name: 'Local', nodes: [{ id: 'curve-local', name: 'Curve' }] },
      {
        id: 'artwork-1234',
        name: 'が',
        nodes: [
          { id: 'curve-1', name: 'Curve' },
          { id: 'curve-2', name: 'Curve' },
        ],
      },
      { id: 'artwork-2234', name: 'か\u3099', nodes: [{ id: 'curve-3', name: 'Curve' }] },
      { id: 'artwork-3', name: 'が · …1234', nodes: [] },
      { id: 'artwork-empty', name: '', nodes: [] },
      { id: 'artwork-named', name: 'Untitled', nodes: [] },
    ],
  };
  const display = expressionTargetDisplay(document, 'local');
  const point = {
    layerId: 'artwork-1234',
    layerName: 'が',
    nodeId: 'curve-1',
    name: 'Curve',
    key: 'point-1.position',
    component: 'x',
    pointOrdinal: 1,
    label: 'Curve · Point 1 · X',
    expression: 'ref(artwork, "artwork-1234", "curve-1", "point-1.position", "x")',
  };
  const otherPoint = {
    ...point,
    layerId: 'artwork-2234',
    layerName: 'か\u3099',
    nodeId: 'curve-3',
    expression: 'ref(artwork, "artwork-2234", "curve-3", "point-1.position", "x")',
  };
  const one = display.picker(point),
    two = display.picker(otherPoint);
  assert.notEqual(one.label.normalize('NFC'), two.label.normalize('NFC'));
  assert.notEqual(one.label.split(' · Curve')[0], 'が · …1234');
  assert.equal(one.id, point.expression);
  assert.equal(one.detail, 'curve-1');
  assert.equal(two.detail, '');
  assert.match(one.searchText, /artwork-1234.*curve-1.*point-1/);
  const completion = display.completion(point);
  assert.equal(completion.expression, point.expression);
  assert.ok(completion.contextLabel);
  assert.equal(completion.contextSearchTerms[0], 'artwork-1234');
  const empty = display.completion({ ...point, layerId: 'artwork-empty', layerName: '' }),
    named = display.completion({ ...point, layerId: 'artwork-named', layerName: 'Untitled' });
  assert.match(empty.contextLabel, /^Untitled/);
  assert.notEqual(empty.contextLabel, named.contextLabel);

  const local = {
    ...point,
    layerId: 'local',
    nodeId: 'curve-local',
    expression: 'ref("curve-local", "point-1.position", "x")',
  };
  assert.equal(display.completion(local), local);
  assert.equal(display.picker(local).label, local.label);
  assert.equal(display.picker(local).id, local.expression);
});

test('long Artwork and node names stay bounded, searchable and available in full', () => {
  const layerName = `${'Artwork '.repeat(12000)}LayerTail`,
    nodeName = `${'Contour '.repeat(12000)}NodeTail`,
    otherLayerName = `${'Artwork '.repeat(12000)}OtherTail`,
    document = {
      layers: [
        { id: 'local', name: 'Local', nodes: [] },
        { id: 'source-a', name: layerName, nodes: [{ id: 'curve-a', name: nodeName }] },
        { id: 'source-b', name: otherLayerName, nodes: [{ id: 'curve-b', name: 'Curve' }] },
      ],
    },
    display = expressionTargetDisplay(document, 'local'),
    makeTarget = (index) => ({
      layerId: 'source-a',
      layerName,
      nodeId: 'curve-a',
      name: nodeName,
      key: `point-${index}.position`,
      component: 'x',
      pointOrdinal: index,
      label: `${nodeName} · Point ${index} · X`,
      expression: `ref(artwork, "source-a", "curve-a", "point-${index}.position", "x")`,
    }),
    targets = Array.from({ length: 1000 }, (_, index) => makeTarget(index + 1)),
    rows = targets.map(display.picker);
  assert.ok(rows.every((row) => row.label.length < 140));
  assert.equal(rows[0].fullName, rows[999].fullName);
  assert.equal(rows[0].fullName(), `${layerName} · ${nodeName}`);
  assert.equal(display.filter(rows, 'LayerTail NodeTail').length, 1000);
  assert.equal(display.filter(rows, 'OtherTail').length, 0);
  assert.equal(display.filter(rows, 'source-a point-1000.position')[0].id, targets[999].expression);
  const handle = {
      ...targets[0],
      key: 'point-1.handleA',
      component: 'length',
      label: `${nodeName} · Point 1 · Incoming · Length`,
    },
    scalar = {
      ...targets[0],
      key: 'radius',
      component: undefined,
      pointOrdinal: undefined,
      label: `${nodeName} · radius`,
    };
  assert.match(display.picker(handle).label, /Point 1 · Incoming · Length$/);
  assert.match(display.picker(scalar).label, / · radius$/);
  const completions = targets.map(display.completion);
  const prefix = 'NodeTail';
  assert.equal(
    expressionCompletions(prefix, prefix.length, prefix.length, completions)[0].text,
    targets[0].expression,
  );
  assert.ok(
    expressionCompletions(prefix, prefix.length, prefix.length, completions)[0].label.length < 140,
  );
  const layerPrefix = 'LayerTail';
  assert.equal(
    expressionCompletions(layerPrefix, layerPrefix.length, layerPrefix.length, completions)[0].text,
    targets[0].expression,
  );
});

test('Composition context distinguishes duplicate names and keeps exact reference identity', () => {
  const document = { layers: [{ id: 'local', name: 'Local', nodes: [] }] },
    longName = `${'Project '.repeat(12000)}CompositionTail`,
    project = {
      compositions: [
        { id: 'current', name: 'Current', layers: document.layers },
        {
          id: 'comp-1234',
          name: 'が',
          layers: [{ id: 'art-a', name: 'Shape', nodes: [{ id: 'value-a', name: 'Value' }] }],
        },
        {
          id: 'comp-2234',
          name: 'か\u3099',
          layers: [{ id: 'art-b', name: 'Shape', nodes: [{ id: 'value-b', name: 'Value' }] }],
        },
        {
          id: 'comp-long',
          name: longName,
          layers: [{ id: 'art-c', name: 'Shape', nodes: [{ id: 'value-c', name: 'Value' }] }],
        },
        {
          id: 'comp-hierarchy-a',
          name: 'A · B',
          layers: [{ id: 'art-d', name: 'C', nodes: [{ id: 'value-d', name: 'Value' }] }],
        },
        {
          id: 'comp-hierarchy-b',
          name: 'A',
          layers: [{ id: 'art-e', name: 'B · C', nodes: [{ id: 'value-e', name: 'Value' }] }],
        },
      ],
    },
    display = expressionTargetDisplay(document, 'local', {
      project,
      activeCompositionId: 'current',
    });
  const target = (compositionId, layerId, nodeId) => ({
    compositionId,
    layerId,
    nodeId,
    name: 'Value',
    key: 'value',
    type: 'Scalar',
    label: 'Value · value',
    expression: `ref(composition, "${compositionId}", "${layerId}", "${nodeId}", "value")`,
  });
  const first = target('comp-1234', 'art-a', 'value-a'),
    second = target('comp-2234', 'art-b', 'value-b'),
    long = target('comp-long', 'art-c', 'value-c'),
    rows = [first, second, long].map(display.picker);
  assert.notEqual(rows[0].label.normalize('NFC'), rows[1].label.normalize('NFC'));
  assert.match(rows[0].label, /Shape.*Value/);
  assert.equal(rows[0].id, first.expression);
  assert.equal(rows[1].id, second.expression);
  assert.ok(rows[2].label.length < 150);
  assert.equal(rows[2].fullName(), `${longName} · Shape · Value`);
  assert.equal(display.filter(rows, 'CompositionTail art-c value-c')[0].id, long.expression);
  assert.equal(display.filter(rows, 'comp-2234 Shape')[0].id, second.expression);
  const hierarchyA = display.picker(target('comp-hierarchy-a', 'art-d', 'value-d')),
    hierarchyB = display.picker(target('comp-hierarchy-b', 'art-e', 'value-e'));
  assert.notEqual(hierarchyA.label.normalize('NFC'), hierarchyB.label.normalize('NFC'));
  const completion = display.completion(long);
  assert.equal(completion.expression, long.expression);
  assert.ok(completion.contextSearchTerms.includes('comp-long'));
  assert.equal(
    expressionCompletions('CompositionTail', 15, 15, [completion])[0].text,
    long.expression,
  );
  const many = Array.from({ length: 1000 }, (_, index) => ({
      ...long,
      key: `property-${index}`,
      label: `Value · property-${index}`,
      expression: `ref(composition, "comp-long", "art-c", "value-c", "property-${index}")`,
    })),
    manyRows = many.map(display.picker);
  assert.equal(manyRows[0].fullName, manyRows.at(-1).fullName);
  assert.equal(display.filter(manyRows, 'CompositionTail').length, 1000);
  assert.equal(
    display.filter(manyRows, 'CompositionTail property-999')[0].id,
    many.at(-1).expression,
  );
});
