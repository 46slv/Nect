import { invoke, isTauri, Channel } from '@tauri-apps/api/core';
export { DocumentLifecycle } from './document-lifecycle.js';
export const desktop = isTauri();
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
  if (result[2] !== operation) throw Error('Unmatched save response');
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

export async function listSystemFonts() {
  return desktop ? invoke('list_system_fonts') : [];
}
export async function readSystemFont(id) {
  if (!desktop) throw Error('Installed fonts require the Windows desktop app');
  const bytes = await invoke('read_system_font', { id });
  return bytes instanceof ArrayBuffer ? bytes : new Uint8Array(bytes).buffer;
}
