// Prepares a text edit without allowing callers to pick a font or file source.
import { desktopTextIntent } from './desktop-font-text.js';
import { sameFontFace } from './font-choice-ui.js';

export function isInstalledOsText(node) {
  return node?.type === 'Text' && node.osFont?.version >= 2 && node.osFont.source === undefined;
}

export async function prepareInstalledTextContent(node, text, prepare) {
  if (!isInstalledOsText(node)) return null;
  const { face } = node.osFont;
  const intent = desktopTextIntent(node, face, { text });
  const outline = await prepare(node, face, { text });
  if (!outline || !sameFontFace(outline.face, face)) throw Error('Unmatched OS font response');
  return { text: intent.text, outline };
}
