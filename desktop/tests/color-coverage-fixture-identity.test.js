import test from 'node:test';
import assert from 'node:assert/strict';
import { fixturePaintSlotId } from './helpers/color-coverage-fixture-identity.js';

for (const kind of [undefined, 'radial']) {
  test(`${kind ?? 'linear'} fixture identity uses every authored stop, not equal HEX or solid fallback`, () => {
    for (const [startId, endId, expected] of [
      ['first', 'first', 'first'],
      ['first', 'second', null],
      ['first', null, null],
      [undefined, 'second', null],
      [null, undefined, null],
      [undefined, undefined, null],
    ]) {
      const rectangle = {
        id: 'irrelevant-solid-fallback',
        gradient: { kind, startId, endId, startHex: '#123456', endHex: '#123456' },
      };
      assert.equal(fixturePaintSlotId(rectangle, 'fill', 'outline'), expected);
      assert.equal(fixturePaintSlotId(rectangle, 'stroke', 'outline'), 'outline');
    }
  });
}

test('angular fixture identity examines all four stops and normalizes unbound stops', () => {
  for (const [ids, expected] of [
    [['first', 'first', 'first', 'first'], 'first'],
    [['first', 'first', 'second', 'first'], null],
    [[null, undefined, null, undefined], null],
    [[null, null, null, 'last'], null],
  ]) {
    const rectangle = { gradient: { kind: 'angular', ids } };
    assert.equal(fixturePaintSlotId(rectangle, 'fill', 'outline'), expected);
    assert.equal(fixturePaintSlotId(rectangle, 'stroke', 'outline'), 'outline');
  }
});

test('solid fixture fill and stroke retain independent authored identities', () => {
  assert.equal(fixturePaintSlotId({ id: 'solid' }, 'fill', 'outline'), 'solid');
  assert.equal(fixturePaintSlotId({}, 'fill', 'outline'), null);
  assert.equal(fixturePaintSlotId({ id: 'solid' }, 'stroke', 'outline'), 'outline');
});
