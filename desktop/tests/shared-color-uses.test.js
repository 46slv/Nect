import test from 'node:test';
import assert from 'node:assert/strict';
import { directSharedColorUses } from '../src/shared-color-uses.js';

test('direct uses follow typed slot IDs across Compositions without inferring same values', () => {
  const project = {
    colorSlots: [
      { id: 'a', value: { hex: '#112233', alpha: 1 } },
      { id: 'b', value: { hex: '#112233', alpha: 1 } },
    ],
    compositions: [
      {
        id: 'c1',
        name: 'First',
        layers: [
          {
            id: 'l1',
            name: 'Artwork',
            nodes: [
              {
                id: 'n1',
                name: 'Fill',
                params: { color: { type: 'Color', colorSlotId: 'a' } },
                extension: { colorSlotId: 'a' },
              },
              {
                id: 'n2',
                name: 'Fill',
                params: {
                  color: { type: 'Color', colorSlotId: 'b' },
                  misc: { type: 'Scalar', colorSlotId: 'a' },
                },
              },
              {
                id: 'n3',
                name: 'Fill',
                params: { color: { type: 'Color', value: { hex: '#112233', alpha: 1 } } },
              },
            ],
          },
        ],
      },
      {
        id: 'c2',
        name: 'Second',
        layers: [
          {
            id: 'l2',
            name: 'Artwork',
            nodes: [
              {
                id: 'n4',
                name: 'Gradient',
                params: {
                  startColor: { type: 'Color', colorSlotId: 'a' },
                  endColor: { type: 'Color', colorSlotId: 'a' },
                },
              },
            ],
          },
        ],
      },
    ],
  };
  const before = structuredClone(project),
    uses = directSharedColorUses(project, 'a');
  assert.deepEqual(
    uses.map(({ compositionId, layerId, nodeId, key }) => ({
      compositionId,
      layerId,
      nodeId,
      key,
    })),
    [
      { compositionId: 'c1', layerId: 'l1', nodeId: 'n1', key: 'color' },
      { compositionId: 'c2', layerId: 'l2', nodeId: 'n4', key: 'startColor' },
      { compositionId: 'c2', layerId: 'l2', nodeId: 'n4', key: 'endColor' },
    ],
  );
  assert.deepEqual(project, before);
  assert.throws(() => directSharedColorUses(project, 'missing'), /no longer available/);
});

test('duplicate and visually equivalent names receive distinct labels without retargeting', () => {
  const node = (id, name) => ({ id, name, params: { color: { type: 'Color', colorSlotId: 'a' } } });
  const project = {
    colorSlots: [{ id: 'a' }],
    compositions: [
      {
        id: 'c',
        name: 'C',
        layers: [
          {
            id: 'l',
            name: 'L',
            nodes: [node('n1', 'が'), node('n2', 'か\u3099'), node('n3', 'が / color · 1')],
          },
        ],
      },
    ],
  };
  const uses = directSharedColorUses(project, 'a');
  assert.equal(new Set(uses.map((use) => use.label.normalize('NFC'))).size, uses.length);
  assert.deepEqual(
    uses.map((use) => use.nodeId),
    ['n1', 'n2', 'n3'],
  );
});
