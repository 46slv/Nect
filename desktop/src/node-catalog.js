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
  {
    typeId: 'Transform',
    categoryId: 'transform',
    labelJa: '移動',
    aliases: ['移動', '回転', '拡大縮小'],
    description: '形状の移動・回転・拡大縮小',
    creationRoute: 'add',
  },
  {
    typeId: 'Boolean',
    categoryId: 'combine',
    labelJa: '結合',
    aliases: ['結合', 'ブーリアン', 'シルエット'],
    description: '輪郭を演算して合成する',
    creationRoute: 'add',
  },
  {
    typeId: 'Geometry Repeater',
    categoryId: 'repeat',
    labelJa: '繰り返し',
    aliases: ['繰り返し', '複製'],
    description: '繰り返し 複製',
    creationRoute: 'add',
  },
  {
    typeId: 'Halftone',
    categoryId: 'image',
    labelJa: '網点',
    aliases: ['網点', 'ハーフトーン'],
    description: '網点 ハーフトーン',
    creationRoute: 'add',
  },
  {
    typeId: 'Ordered Dither',
    categoryId: 'image',
    labelJa: '規則ディザ',
    aliases: ['規則ディザ'],
    description: '規則ディザ',
    creationRoute: 'add',
  },
  {
    typeId: 'Error Diffusion Dither',
    categoryId: 'image',
    labelJa: '誤差拡散ディザ',
    aliases: ['誤差拡散ディザ'],
    description: '誤差拡散ディザ',
    creationRoute: 'add',
  },
  {
    typeId: 'Ordered Dither Image',
    categoryId: 'image',
    labelJa: '規則ディザ Image',
    aliases: ['規則ディザ', '画像', 'ラスター'],
    description: '規則ディザ Image',
    creationRoute: 'add',
  },
  {
    typeId: 'Error Diffusion Dither Image',
    categoryId: 'image',
    labelJa: '誤差拡散ディザ Image',
    aliases: ['誤差拡散ディザ', '画像', 'ラスター'],
    description: '誤差拡散ディザ Image',
    creationRoute: 'add',
  },
  {
    typeId: 'Line Screen',
    categoryId: 'image',
    labelJa: '線スクリーン',
    aliases: ['線スクリーン'],
    description: '線スクリーン',
    creationRoute: 'add',
  },
  {
    typeId: 'Composition Reference',
    categoryId: 'combine',
    labelJa: 'Composition参照',
    aliases: ['コンポジション参照', 'コンポ', 'composition output', 'parts'],
    description: '別Compositionの出力を配置',
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
