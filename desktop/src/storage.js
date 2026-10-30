import { invoke, isTauri, Channel } from '@tauri-apps/api/core';
export { DocumentLifecycle } from './document-lifecycle.js';
export const desktop = isTauri();
export const queryBuildIdentity = () => invoke('query_build_identity').catch(nativeError);
export const beginRecovery = () => invoke('begin_recovery');
export const writeRecovery = (copy) => invoke('write_recovery', copy).catch(nativeError);
export const listRecovery = () => invoke('list_recovery').catch(nativeError);
export const readRecovery = (id) => invoke('read_recovery', { id }).catch(nativeError);
export const acknowledgeRecovery = (id) =>
  invoke('acknowledge_recovery', { id }).catch(nativeError);
const nativeError = (error) => {
  throw error instanceof Error ? error : Error(String(error));
};
export async function saveFile(contents, name, kind) {
  if (desktop && kind === 'png')
    return invoke('save_png', {
      bytes: Array.from(new Uint8Array(await contents.arrayBuffer())),
      suggestedName: name,
    });
  if (desktop) return invoke('save_document', { contents, suggestedName: name, kind });
  const a = document.createElement('a'),
    url = URL.createObjectURL(
      new Blob([contents], {
        type: kind === 'svg' ? 'image/svg+xml' : kind === 'png' ? 'image/png' : 'application/json',
      }),
    );
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  return true;
}
export async function openFile() {
  if (!desktop) return null;
  const result = await invoke('open_project').catch(nativeError);
  return result && { binding: { token: result[0], path: result[1] }, contents: result[2] };
}

export async function openSVGSource() {
  if (!desktop) return null;
  const result = await invoke('open_svg_source').catch(nativeError);
  return result && { name: result[0], contents: result[1] };
}

export async function releaseProject(binding) {
  if (desktop && binding?.token)
    await invoke('release_project', { token: binding.token }).catch(nativeError);
}

export async function writeProject({ contents, binding, operation }) {
  if (!desktop) {
    await saveFile(contents, 'nect-project.json', 'json');
    return { path: 'nect-project.json', download: true };
  }
  const result = await invoke('save_project', {
    contents,
    token: binding?.token || null,
    operation,
  }).catch(nativeError);
  if (!result) return null;
  if (result[2] !== operation) {
    // A Save As response can allocate a token even when its envelope is rejected.
    if (result[0] !== binding?.token) await releaseProject({ token: result[0] });
    throw Error('Unmatched save response');
  }
  return { token: result[0], path: result[1] };
}

export async function chooseUnsaved(action) {
  if (desktop) return invoke('choose_unsaved', { action });
  return new Promise((resolve) => {
    const dialog = document.createElement('dialog');
    dialog.innerHTML =
      '<form method="dialog"><h3>Unsaved changes</h3><p>Save this document before continuing?</p><button value="save">Save</button> <button value="discard">Discard</button> <button value="cancel">Cancel</button></form>';
    dialog.addEventListener(
      'close',
      () => {
        const result = dialog.returnValue || 'cancel';
        dialog.remove();
        resolve(result);
      },
      { once: true },
    );
    document.body.append(dialog);
    dialog.showModal();
  });
}

export async function registerClose(handler) {
  if (!desktop) return;
  const channel = new Channel();
  channel.onmessage = async () => {
    if (await handler()) await invoke('finish_close');
  };
  await invoke('enable_close_guard', { channel });
}

// The first three tuple positions remain the legacy flat catalog API. Metadata
// is optional display information, never an ID, read path, or font validator.
export function normalizeSystemFontEntry(entry) {
  const [id, name, reason, source] = entry;
  if (!source || typeof source !== 'object' || Array.isArray(source)) return [id, name, reason];
  const text = (value) => {
    if (
      typeof value !== 'string' ||
      value.length > 256 ||
      /[\u0000-\u001f\u007f-\u009f]/u.test(value)
    )
      return null;
    return value.trim() || null;
  };
  const integer = (value, max) =>
    Number.isInteger(value) && value >= 1 && value <= max ? value : null;
  const flag = (value) => (typeof value === 'boolean' ? value : null);
  return [
    id,
    name,
    reason,
    Object.freeze({
      family: text(source.family),
      subfamily: text(source.subfamily),
      fullName: text(source.fullName),
      postScriptName: text(source.postScriptName),
      ...(Array.isArray(source.localizedNames)
        ? {
            localizedNames: Object.freeze([
              ...new Set(source.localizedNames.slice(0, 64).map(text).filter(Boolean)),
            ]),
          }
        : {}),
      weightClass: integer(source.weightClass, 1000),
      widthClass: integer(source.widthClass, 9),
      fsSelection:
        Number.isInteger(source.fsSelection) &&
        source.fsSelection >= 0 &&
        source.fsSelection <= 65535
          ? source.fsSelection
          : null,
      italic: flag(source.italic),
      bold: flag(source.bold),
      regular: flag(source.regular),
      oblique: flag(source.oblique),
    }),
  ];
}
export async function listSystemFonts() {
  return desktop ? (await invoke('list_system_fonts')).map(normalizeSystemFontEntry) : [];
}
export async function readSystemFont(id) {
  if (!desktop) throw Error('Installed fonts require the Windows desktop app');
  const bytes = await invoke('read_system_font', { id });
  return bytes instanceof ArrayBuffer ? bytes : new Uint8Array(bytes).buffer;
}

export async function listOsOutlineFonts() {
  if (!desktop) return [];
  return invoke('list_os_outline_fonts').catch(nativeError);
}
export async function outlineOsText(intent) {
  if (!desktop) throw Error('OS font outlines require the Windows app');
  return invoke('outline_os_text', intent).catch(nativeError);
}
export async function listDesktopFontFaces() {
  return desktop ? invoke('list_desktop_font_faces').catch(nativeError) : [];
}
export async function listDesktopFontAxes(face) {
  return desktop ? invoke('list_desktop_font_axes', { face }).catch(nativeError) : [];
}
export async function shapeDesktopText(request) {
  if (!desktop) throw Error('Windows font shaping requires the Windows app');
  return invoke('shape_desktop_text', { request }).catch(nativeError);
}

// Opaque tokens remain in memory only. JavaScript never provides native paths.
export async function openPrivateFontFile() {
  if (!desktop) throw Error('Font files require the Windows desktop app');
  return invoke('open_private_font_file').catch(nativeError);
}
export async function listPrivateFontFiles() {
  return desktop ? invoke('list_private_font_files').catch(nativeError) : [];
}
export async function listPrivateFontAxes(token, faceIndex) {
  if (!desktop) throw Error('Font files require the Windows desktop app');
  return invoke('list_private_font_axes', { token, faceIndex }).catch(nativeError);
}
export async function shapePrivateFontText(token, faceIndex, request) {
  if (!desktop) throw Error('Font files require the Windows desktop app');
  return invoke('shape_private_font_text', { token, faceIndex, request }).catch(nativeError);
}
