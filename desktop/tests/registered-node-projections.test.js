import test from 'node:test';
import assert from 'node:assert/strict';
import { placeRegistered, manifestBindings } from '../src/registered-node-projections.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { registeredNodes } from '../src/builtin-node-registry.js';
const row = (id, after) => ({
  resolved: { declaration: { typeId: id } },
  placement: { definitionAfter: after },
});
test('registered projections use anchors, preserve legacy order and fail closed on ambiguity', () => {
  const rows = [row('math', 'value'), row('separate', 'vector-math'), row('vector-math', 'math')];
  assert.deepEqual(
    placeRegistered(
      [
        ['value', 1],
        ['after', 2],
      ],
      rows,
      'definitionAfter',
      (x) => x.resolved.declaration.typeId,
    ).map(([id]) => id),
    ['value', 'math', 'vector-math', 'separate', 'after'],
  );
  assert.throws(
    () =>
      placeRegistered(
        [
          ['a', 1],
          ['a', 2],
        ],
        [],
        'definitionAfter',
        () => 0,
      ),
    /Duplicate/,
  );
  assert.throws(
    () => placeRegistered([['a', 1]], [row('a', 'a')], 'definitionAfter', () => 0),
    /Duplicate/,
  );
  assert.throws(
    () => placeRegistered([['a', 1]], [row('b', 'c'), row('c', 'b')], 'definitionAfter', () => 0),
    /cyclic/,
  );
});
test('before anchors preserve order, resolve registered targets and reject ambiguous descriptors', () => {
  const rows = [row('first', { before: 'second' }), row('second', { before: 'legacy' })];
  assert.deepEqual(
    placeRegistered(
      [['legacy', 1]],
      rows,
      'definitionAfter',
      (x) => x.resolved.declaration.typeId,
    ).map(([id]) => id),
    ['first', 'second', 'legacy'],
  );
  assert.deepEqual(
    placeRegistered(
      [['legacy', 1]],
      [row('a', { before: 'legacy' }), row('b', { before: 'legacy' })],
      'definitionAfter',
      () => 0,
    ).map(([id]) => id),
    ['a', 'b', 'legacy'],
  );
  let called = false;
  const accessor = {
    get before() {
      called = true;
      return 'legacy';
    },
  };
  for (const invalid of [[], {}, { before: '' }, { before: 'legacy', after: 'legacy' }, accessor])
    assert.throws(
      () => placeRegistered([['legacy', 1]], [row('x', invalid)], 'definitionAfter', () => 0),
      /Invalid placement/,
    );
  assert.equal(called, false);
  assert.throws(
    () =>
      placeRegistered(
        [['legacy', 1]],
        [row('x', { before: 'y' }), row('y', { before: 'x' })],
        'definitionAfter',
        () => 0,
      ),
    /cyclic/,
  );
});
test('manifest exposure and implementation ambiguity are checked, same trusted function may be reused', () => {
  const fn = () => 1,
    first = {
      declaration: { implementationId: 'shared' },
      binding: { evaluate: fn },
      exposure: 'public',
    };
  assert.equal(manifestBindings([first, { ...first, exposure: 'private' }]).size, 1);
  assert.throws(() => manifestBindings([{ ...first, exposure: 'unknown' }]), /exposure/);
  assert.throws(
    () => manifestBindings([first, { ...first, binding: { evaluate: () => 2 } }]),
    /Conflicting/,
  );
  assert.ok(Object.isFrozen(builtinManifest));
  assert.ok(Object.isFrozen(builtinManifest[0]));
  assert.ok(Object.isFrozen(builtinManifest[0].declaration));
  assert.ok(Object.isFrozen(builtinManifest[0].placement));
  assert.ok(Object.isFrozen(registeredNodes));
});
