import { privateFontChoiceId, samePrivateFontSource } from './private-font-source.js';
import { fontChoiceCatalog, fontChoiceCapability } from './font-choice.js';

const faceKeys = ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color'];
export const sameFontFace = (a, b) => !!a && !!b && faceKeys.every((key) => a[key] === b[key]);

// The native descriptor is the adapter's exact identity, never a family-name lookup.
export const windowsFontChoiceId = (face) => JSON.stringify(faceKeys.map((key) => face[key]));
const normalized = (value) =>
  String(value ?? '')
    .normalize('NFKC')
    .toLowerCase();
const compare = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const sourceOrder = { embedded: 0, installed: 2, windows: 1, 'file-native': 3 };
export const FONT_FILE_IMPORT_REQUIRED =
  'Font file import is required; use Advanced for a file you may embed';

// Availability is technical preflight, not proof of a license. Installed bytes
// are read only on Apply; the shared import command validates the final face.
export function fontPickerCapability(node, choice, options = {}) {
  return fontChoiceCapability(node, choice, options);
}

export function fontPickerCatalogs({
  assets = {},
  fonts = {},
  bundledFont,
  installed = [],
  windows = [],
  fileNative = [],
} = {}) {
  return {
    embedded: [
      { id: 'default', label: 'DejaVu Sans (Latin)', metadata: bundledFont?.nectMetadata },
      ...Object.entries(assets).map(([id, asset]) => ({
        id,
        label: asset.name,
        metadata: fonts[id]?.nectMetadata,
        variable: asset.kind === 'variable-glyf-v1',
      })),
    ],
    installed: installed.map(([id, label, disabledReason, metadata]) => ({
      id,
      label,
      disabledReason,
      metadata,
    })),
    fileNative: fileNative.flatMap(({ token, sha256, faces }) =>
      faces.map(({ faceIndex, face }) => {
        const sourceRef = { kind: 'file-native-v1', sha256, faceIndex };
        return {
          id: privateFontChoiceId(sourceRef),
          token,
          sourceRef,
          face,
          label: `${face.family} · ${face.name}`,
          metadata: {
            family: face.family,
            subfamily: face.name,
            weightClass: face.weight,
            italic: face.style !== 0,
          },
        };
      }),
    ),
    windows: windows.map((face) => ({
      id: windowsFontChoiceId(face),
      label: `${face.family} · ${face.name}`,
      face,
      metadata: {
        family: face.family,
        subfamily: face.name,
        weightClass: face.weight,
        italic: face.style !== 0,
      },
    })),
  };
}

/** Presentation for the existing searchable picker; no face equivalence is inferred. */
export function fontPickerItems(catalogs, node, options = {}) {
  const choices = fontChoiceCatalog(catalogs);
  const rows = choices.map((choice) => {
    const capability = fontPickerCapability(node, choice, options);
    const metadata = choice.metadata || {};
    const details = [
      metadata.weightClass == null ? 'weight unknown' : `weight ${metadata.weightClass}`,
      choice.source === 'embedded'
        ? 'In document'
        : choice.source === 'installed'
          ? 'Included in saved document'
          : choice.source === 'file-native'
            ? 'Open font file (not embedded)'
            : 'On this computer',
      capability.reason,
    ].filter(Boolean);
    return {
      id: choice.key,
      key: choice.key,
      choice,
      label: choice.label,
      metadata,
      detail: details.join(' · '),
      technicalIdentity: choice.id,
      disabled: !capability.route,
      disabledReason: capability.reason,
    };
  });
  rows.sort((a, b) => {
    for (const [left, right] of [
      [normalized(a.metadata.family || a.label), normalized(b.metadata.family || b.label)],
      [a.metadata.weightClass ?? Infinity, b.metadata.weightClass ?? Infinity],
      [normalized(a.metadata.subfamily), normalized(b.metadata.subfamily)],
      [normalized(a.label), normalized(b.label)],
      [sourceOrder[a.choice.source], sourceOrder[b.choice.source]],
      [a.key, b.key],
    ]) {
      const order = compare(left, right);
      if (order) return order;
    }
    return 0;
  });
  // Opaque IDs are authoritative but are not useful row labels. Number same-name
  // variants for display while keeping the exact ID available as optional detail.
  for (const row of rows) {
    const same = rows.filter((other) => other.label === row.label);
    if (same.length > 1) row.detail += ` · Variant ${same.indexOf(row) + 1} of ${same.length}`;
  }
  if (options.characterIds !== undefined && !node.osFont && !node.variation) {
    const choice = { source: 'embedded', id: '', label: 'Inherit default font' };
    const capability = fontChoiceCapability(node, choice, options);
    rows.unshift({
      id: 'inherit-default-font',
      key: 'inherit-default-font',
      choice,
      label: 'Inherit default font',
      metadata: {},
      detail: capability.reason || '',
      disabled: !capability.route,
      disabledReason: capability.reason,
    });
  }
  return rows;
}

export function selectedFontPickerId(node, rows) {
  return rows.find(({ choice }) =>
    node.osFont
      ? node.osFont.source
        ? choice.source === 'file-native' &&
          samePrivateFontSource(choice.sourceRef, node.osFont.source)
        : choice.source === 'windows' && sameFontFace(choice.face, node.osFont.face)
      : choice.source === 'embedded' && choice.id === node.font,
  )?.id;
}

/** Preserve existing Favorites, including the older name-based preference only. */
export function migrateFontFavorites(stored, rows) {
  const favorites = new Set(stored);
  for (const { choice, key } of rows) {
    const oldKeys = [
      choice.source === 'embedded'
        ? choice.id === 'default'
          ? 'default'
          : `asset:${choice.id}`
        : `${choice.source}:${choice.id}`,
      `font:${choice.label}`,
    ];
    if (oldKeys.some((old) => favorites.has(old))) favorites.add(key);
  }
  for (const { choice } of rows) {
    favorites.delete(
      choice.source === 'embedded'
        ? choice.id === 'default'
          ? 'default'
          : `asset:${choice.id}`
        : `${choice.source}:${choice.id}`,
    );
    favorites.delete(`font:${choice.label}`);
  }
  return favorites;
}

// A family heading is display organization, never proof that two faces are the
// same. Namespace it by adapter; unknown metadata does not invent a family.
export function fontFamilyGroups(rows) {
  const groups = new Map();
  for (const row of rows) {
    if (row.choice.id === '') continue;
    const family = row.metadata?.family;
    const id = JSON.stringify([
      row.choice.source,
      family ? ['family', family] : ['face', row.choice.id],
    ]);
    if (!groups.has(id))
      groups.set(id, {
        id,
        label: family || `${row.label} · family unknown`,
        known: !!family,
        itemIds: [],
      });
    groups.get(id).itemIds.push(row.id);
  }
  return [...groups.values()];
}
