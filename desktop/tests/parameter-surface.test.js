import test from 'node:test';
import assert from 'node:assert/strict';
import {
  defineParameterSurface,
  renderParameterPresentations,
  projectParameterPresentation,
  GRID_OPACITY_SURFACE,
  GEOMETRY_REPEATER_ROTATION_SURFACE,
} from '../src/parameter-surface.js';

test('ParameterSurface separates value, edit, presentation, connection and authority metadata', () => {
  assert.deepEqual(GRID_OPACITY_SURFACE.value, {
    kind: 'scalar',
    unit: 'ratio',
    hard: { min: 0, max: 1 },
  });
  assert.equal(GRID_OPACITY_SURFACE.edit.mode, 'live');
  assert.equal(GRID_OPACITY_SURFACE.owner.transaction, 'Session set-guide-setting');
  assert.deepEqual(
    GRID_OPACITY_SURFACE.presentations.map(({ id, type }) => [id, type]),
    [
      ['overlay', 'readout'],
      ['slider', 'slider'],
      ['numeric', 'numeric'],
    ],
  );
  assert.deepEqual(GEOMETRY_REPEATER_ROTATION_SURFACE.connection.receive, [
    { id: 'source', type: 'Scalar', owner: 'Parameter.source' },
  ]);
  assert.equal(GEOMETRY_REPEATER_ROTATION_SURFACE.value.multiTurn, true);
  assert.ok(Object.isFrozen(GEOMETRY_REPEATER_ROTATION_SURFACE));
  assert.ok(Object.isFrozen(GEOMETRY_REPEATER_ROTATION_SURFACE.presentations));
});

test('one binding snapshot projects exact numeric and modulo angle presentations', () => {
  assert.equal(
    projectParameterPresentation(GEOMETRY_REPEATER_ROTATION_SURFACE, 'numeric', { authored: 720 }),
    720,
  );
  assert.equal(
    projectParameterPresentation(GEOMETRY_REPEATER_ROTATION_SURFACE, 'angle-dial', {
      authored: 720,
    }),
    0,
  );
  const seen = [];
  const snapshot = renderParameterPresentations(
    GEOMETRY_REPEATER_ROTATION_SURFACE,
    {
      authored: 725,
      effective: 40,
      revision: 9,
      source: 'node-1',
      expression: 'value * 2',
    },
    {
      numeric: ({ value, snapshot: state }) => seen.push(['numeric', value, state]),
      'angle-dial': ({ value, snapshot: state }) => seen.push(['dial', value, state]),
    },
  );
  assert.equal(seen[0][1], 725);
  assert.equal(seen[1][1], 5);
  assert.equal(seen[0][2], snapshot);
  assert.equal(seen[1][2], snapshot);
  assert.equal(snapshot.effective, 40);
  assert.equal(snapshot.source, 'node-1');
  assert.ok(Object.isFrozen(snapshot));
});

test('presentation preflight rejects an incomplete adapter set before any render', () => {
  const calls = [];
  assert.throws(
    () =>
      renderParameterPresentations(
        GRID_OPACITY_SURFACE,
        { authored: 0.25 },
        {
          overlay: () => calls.push('overlay'),
          slider: () => calls.push('slider'),
        },
      ),
    /Missing presentation adapter: document\.gridOpacity\.numeric/,
  );
  assert.deepEqual(calls, []);
});

test('invalid contracts fail without creating a second permissive schema', () => {
  const owner = { value: 'value owner', transaction: 'transaction owner' };
  assert.throws(
    () =>
      defineParameterSurface({
        id: 'bad-range',
        value: { kind: 'scalar', hard: { min: 10, max: 1 } },
        owner,
      }),
    /min must not exceed/,
  );
  assert.throws(
    () =>
      defineParameterSurface({
        id: 'duplicate-view',
        value: { kind: 'scalar' },
        presentations: [
          { id: 'numeric', type: 'numeric' },
          { id: 'numeric', type: 'slider' },
        ],
        owner,
      }),
    /Duplicate presentation id/,
  );
  assert.throws(
    () =>
      defineParameterSurface({
        id: 'bad-mode',
        value: { kind: 'scalar' },
        edit: { mode: 'instant-magic' },
        owner,
      }),
    /Unsupported edit mode/,
  );
});
