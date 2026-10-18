import test from 'node:test';
import assert from 'node:assert/strict';
import { definitions } from '../src/core.js';
import {
  nodeCatalog,
  nodeCategories,
  nodePickerItems,
  searchNodeItems,
} from '../src/node-catalog.js';

test('each currently addable type has exactly one built-in category; unknown types remain discoverable', () => {
  assert.equal(nodeCategories.length, 8);
  const items = nodePickerItems(definitions);
  assert.deepEqual(
    new Set(nodeCatalog.map((entry) => entry.typeId)),
    new Set(items.map((item) => item.id)),
  );
  for (const item of items) {
    assert.equal(nodeCatalog.filter((entry) => entry.typeId === item.id).length, 1);
    assert.ok(nodeCategories.some((category) => category.id === item.categoryId));
    assert.equal(item.creationRoute, 'add');
  }
  assert.ok(!items.some((item) => ['Image', 'Output', 'Image Output'].includes(item.id)));
  assert.equal(nodePickerItems({ Future: {} })[0].categoryId, 'uncategorized');
  assert.deepEqual(
    items.map((item) => item.label),
    [...items]
      .sort((a, b) => a.label.localeCompare(b.label, undefined, { numeric: true }))
      .map((item) => item.label),
  );
});

test('search keeps type IDs, Japanese aliases, NFKC, AND matching and stable ranking', () => {
  const items = nodePickerItems(definitions, (type) =>
    type === 'Join Geometry' ? 'Join Paths' : type,
  );
  for (const query of ['Join Geometry', 'Join Paths', '独立パス'])
    assert.equal(searchNodeItems(items, query)[0].id, 'Join Geometry');
  assert.equal(searchNodeItems(items, '角丸')[0].id, 'Corner Round');
  assert.equal(searchNodeItems(items, 'ＦＩＬＬ')[0].id, 'Fill');
  assert.equal(searchNodeItems(items, 'linear 塗り')[0].id, 'Linear Gradient Fill');
  assert.deepEqual(searchNodeItems(items, '角丸', 'transform'), []);
  assert.ok(searchNodeItems(items, 'ベクトル').every((item) => item.categoryId === 'value'));
  assert.deepEqual(searchNodeItems(items, ''), items);
  assert.deepEqual(
    searchNodeItems(items, '結合').map((item) => item.id),
    ['Boolean', 'Join Geometry'],
  );
});
