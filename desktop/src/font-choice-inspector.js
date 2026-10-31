const escape = (value) =>
  String(value ?? '').replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );

export function textFontLabel(node, assets = {}) {
  return node.osFont
    ? [node.osFont.family, node.osFont.face?.name].filter(Boolean).join(' · ')
    : node.font === 'default'
      ? 'DejaVu Sans (Latin)'
      : assets[node.font]?.name || 'Unavailable font';
}

/** The normal inspector never asks the user to choose a font implementation. */
export function fontChoiceControls(
  node,
  { assets, locked = false, status = '', desktop = false } = {},
) {
  const disabled = locked || node.locked || node.bypassed ? 'disabled' : '';
  return `<section class="text-font-choice" aria-label="Font"><div class="field"><label>Font</label><span>${escape(textFontLabel(node, assets))}</span><button type="button" class="mini" data-change-font="${escape(node.id)}" ${disabled}>Change font…</button></div>${desktop ? `<button type="button" class="mini" data-open-private-font="${escape(node.id)}" ${disabled}>${node.osFont?.source ? 'Reattach / open font file…' : 'Open font file…'}</button><p class="font-note">Font file stays on this computer and is not embedded. Check its license before use.</p>` : ''}${status ? `<output role="status">${escape(status)}</output>` : ''}</section>`;
}

export function fontImportControls(node, { locked = false, desktop = false } = {}) {
  const disabled = locked || node.locked || node.bypassed ? 'disabled' : '';
  return `<details class="text-font-advanced text-inspector-details"><summary>Advanced font import</summary><button type="button" class="mini" data-load-font="${escape(node.id)}" ${disabled}>Import TTF / OTF…</button><button type="button" class="mini" data-load-variable-font="${escape(node.id)}" ${disabled || node.osFont ? 'disabled' : ''}>Import variable TTF (experimental)…</button>${node.osFont ? '<p class="font-note" role="note">Variable import cannot preserve this Text’s current font settings. Choose a supported static font first.</p>' : ''}${desktop ? '<button type="button" class="mini" data-refresh-os-fonts>Refresh available fonts</button>' : '<p class="font-note" role="note">Installed font discovery is available in the Windows desktop app, not this browser.</p>'}<p class="font-note" role="note">Imported font bytes stay local and are included in saved JSON. Use a font licensed for editable embedding; check its license before sharing. File flags do not guarantee license permission.</p></details>`;
}

export function textOutlineActions(node, { locked = false } = {}) {
  return `<details class="text-actions text-inspector-details"><summary>Text actions</summary><button type="button" class="mini" data-bake-text-outline="${escape(node.id)}" ${locked || node.locked || node.bypassed ? 'disabled' : ''}>Convert to editable paths</button><p class="font-note" role="note">Replaces editable Text with separate paths using its current appearance. Text and font editing are removed; Undo restores Text.</p></details>`;
}
