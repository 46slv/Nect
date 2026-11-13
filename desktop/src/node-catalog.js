import { registeredNodes } from './builtin-node-registry.js';
import { placeRegistered } from './registered-node-projections.js';

// Editor-only metadata. Never serialized into a document or used as a node type ID.
export const nodeCategories = [
  {
    id: 'create',
    label: '作成',
  },
  {
    id: 'path',
    label: 'パス編集',
  },
  {
    id: 'transform',
    label: '変形',
  },
  {
    id: 'combine',
    label: '合成',
  },
  {
    id: 'repeat',
    label: '配置・反復',
  },
  {
    id: 'paint',
    label: '色・描画',
  },
  {
    id: 'image',
    label: '画像・トーン',
  },
  {
    id: 'value',
    label: '値・制御',
  },
];
const legacyNodeCatalog = [
  {
    typeId: 'Text',
    categoryId: 'create',
    labelJa: '文字',
    aliases: ['文字', 'テキスト'],
    description: '文字 テキスト',
    creationRoute: 'add',
  },
];
export const nodeCatalog = placeRegistered(
  legacyNodeCatalog.map((entry) => [entry.typeId, entry]),
  registeredNodes.filter((row) => row.exposure === 'public'),
  'catalogAfter',
  (row) => ({ typeId: row.resolved.declaration.typeId, ...row.resolved.declaration.catalog }),
).map(([, entry]) => entry);
const privateTypes = new Set(
  registeredNodes
    .filter((row) => row.exposure === 'private')
    .map((row) => row.resolved.declaration.typeId),
);
const excludedTypes = new Set(['Output', 'Image Output', 'Image', 'SVG']);
export function nodePickerItems(definitions, label = (type) => type) {
  const metadata = new Map(nodeCatalog.map((entry) => [entry.typeId, entry]));
  return Object.keys(definitions)
    .filter((type) => !excludedTypes.has(type) && !privateTypes.has(type))
    .map((type) => {
      const entry = metadata.get(type) || {
        typeId: type,
        categoryId: 'uncategorized',
        labelJa: '',
        aliases: [],
        description: '',
        creationRoute: 'add',
      };
      const category =
        nodeCategories.find((item) => item.id === entry.categoryId)?.label || '未分類';
      return {
        ...entry,
        id: type,
        label: label(type),
        detail: `${category} · ${entry.description}`,
      };
    })
    .sort((a, b) =>
      normalize(a.label).localeCompare(normalize(b.label), undefined, { numeric: true }),
    );
}
const normalize = (value) => String(value).normalize('NFKC').toLowerCase();
export function searchNodeItems(items, query, categoryId = '') {
  const text = normalize(query).trim();
  const words = text.split(/\s+/).filter(Boolean);
  return items
    .filter(
      (item) =>
        (!categoryId || item.categoryId === categoryId) &&
        words.every((word) =>
          normalize(
            [item.typeId, item.label, item.labelJa, ...item.aliases, item.description].join(' '),
          ).includes(word),
        ),
    )
    .sort((a, b) => {
      return normalize(a.label).localeCompare(normalize(b.label), undefined, { numeric: true });
    });
}
