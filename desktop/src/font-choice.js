import { desktopTextIntent } from './desktop-font-text.js';
import { textFeatureReason } from './opentype-features.js';

// Adapter-owned IDs, not labels, select a face. Even identical display names
// remain separate choices; no cross-catalog face equivalence is inferred here.
export function fontChoiceCatalog({ embedded = [], installed = [], windows = [], fileNative = [] } = {}) {
  const keys = new Set();
  return Object.entries({ embedded, installed, windows, 'file-native': fileNative }).flatMap(([source, rows]) =>
    rows.map((row) => {
      if (typeof row.id !== 'string' || !row.id || typeof row.label !== 'string')
        throw Error('Font choices require an opaque ID and display label');
      const key = JSON.stringify([source, row.id]);
      if (keys.has(key)) throw Error('Duplicate font choice identity');
      keys.add(key);
      return { ...row, source, key };
    }),
  );
}

// This is a routing preflight, never coverage/embedding/native preparation proof.
// The selected route must still validate its final candidate in CommandSession.
export function fontChoiceCapability(n, choice, { characterIds } = {}) {
  const blocked = (reason) => ({ route: null, reason });
  if (n?.type !== 'Text' || n.locked || n.bypassed)
    return blocked('Select an unlocked enabled Text');
  if (choice?.disabledReason) return blocked(choice.disabledReason);
  if (['windows', 'file-native'].includes(choice?.source)) {
    if (characterIds !== undefined) return blocked('Windows fonts require whole Text');
    if (n.fontFeatures !== undefined)
      return blocked('Windows fonts cannot preserve explicit OpenType settings');
    try {
      desktopTextIntent(n, choice.face);
    } catch (error) {
      return blocked(error.message);
    }
    return { route: 'set-os-font-text', reason: null };
  }
  if (!['embedded', 'installed'].includes(choice?.source))
    return blocked('Unsupported font choice');
  if (n.osFont) {
    if (characterIds !== undefined) return blocked('OS font replacement requires whole Text');
    if (choice.variable) return blocked('OS font replacement requires a static font');
    if (n.osFont.outline?.ligatures === true) {
      const reason = textFeatureReason({ ...n, osFont: undefined });
      if (reason) return blocked(reason);
    }
    return { route: 'replace-os-text-font', reason: null };
  }
  if (choice.variable || n.variation) {
    if (characterIds !== undefined) return blocked('Variable fonts require whole Text');
    if (n.fontFeatures !== undefined)
      return blocked('Variable fonts cannot preserve explicit OpenType settings');
    return { route: 'set-variable-font', reason: null };
  }
  return {
    route:
      choice.source === 'installed'
        ? 'adopt-font'
        : characterIds === undefined
          ? 'set-text-settings'
          : 'set-character-style',
    reason: null,
  };
}
