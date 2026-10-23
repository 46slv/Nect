import { hasCharacterAdjustments } from './character-identity.js';

export const OPENTYPE_SCHEMA_VERSION = 38;

// The pinned opentype.js 1.3.4 Latin shaper accepts liga lookup 4 / format 1.
// Do not infer feature support merely from a tag in the font's feature list.
export function standardLigatureSupport(font) {
  const gsub = font?.tables?.gsub;
  const script = gsub?.scripts?.find((entry) => entry.tag === 'latn')?.script;
  const features = (script?.defaultLangSys?.featureIndexes || [])
    .map((index) => gsub.features?.[index])
    .filter((entry) => entry?.tag === 'liga');
  const indexes = features.flatMap((entry) => entry.feature?.lookupListIndexes || []);
  if (!indexes.length) return false;
  return indexes.every((index) => {
    const lookup = gsub.lookups?.[index];
    return (
      lookup?.lookupType === 4 &&
      lookup.lookupFlag === 0 &&
      lookup.subtables?.length > 0 &&
      lookup.subtables.every((sub) => sub.substFormat === 1)
    );
  });
}

export function textFeatureReason(node, font) {
  if (
    node.type !== 'Text' ||
    node.osFont ||
    node.variation ||
    font?.nectVariable ||
    (node.writingMode && node.writingMode !== 'horizontal-tb') ||
    node.textWrap ||
    node.pathAttachment ||
    node.characterArrangement ||
    hasCharacterAdjustments(node.characterPositions) ||
    /[^\p{Script=Latin}\p{Script=Common}\n\r]/u.test(node.text || '')
  )
    return 'Standard ligatures require whole static horizontal Latin Text without wrap, path or character adjustments.';
  return null;
}

export function validateTextFeatures(node, schemaVersion = OPENTYPE_SCHEMA_VERSION, asset) {
  const features = node.fontFeatures;
  if (features === undefined) return;
  if (schemaVersion < OPENTYPE_SCHEMA_VERSION)
    throw Error(`OpenType settings require schemaVersion ${OPENTYPE_SCHEMA_VERSION}`);
  if (
    !features ||
    Object.getPrototypeOf(features) !== Object.prototype ||
    Object.keys(features).length !== 1 ||
    typeof features.liga !== 'boolean' ||
    !Object.hasOwn(features, 'liga')
  )
    throw Error('Only the boolean OpenType liga setting is supported');
  const reason = textFeatureReason(node);
  if (reason || asset?.kind) throw Error(reason || 'OpenType settings require a static font');
}

export function textFeatureOptions(font, node) {
  validateTextFeatures(node);
  if (node.fontFeatures === undefined) return {};
  const reason = textFeatureReason(node, font);
  if (reason) throw Error(reason);
  if (node.fontFeatures.liga && !standardLigatureSupport(font))
    throw Error('This font has no supported Latin standard ligature lookup (liga)');
  // Retain the previous required-ligature policy; do not enable unrelated tags.
  return { features: { liga: node.fontFeatures.liga, rlig: false } };
}
