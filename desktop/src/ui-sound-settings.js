import { installUiSound } from './ui-sound.mjs';

export const UI_SOUND_STORAGE_KEY = 'nect.ui-sound.v1';

// These are the editor's icon controls, not arbitrary canvas or text content.
const ICON_CONTROLS = [
  'button:has(> svg.workspace-icon)',
  '.boundary-toggle',
  '.alignment-icon-row button',
  '.alignment-gap-row button',
].join(', ');

const installed = new WeakMap();

export function createUiSoundSettings({ document, storage, onSaveError = () => {} }) {
  if (installed.has(document)) return installed.get(document);

  let enabled = true;
  let volume = 0.12;
  try {
    const saved = JSON.parse(storage?.getItem(UI_SOUND_STORAGE_KEY) || 'null');
    if (typeof saved?.enabled === 'boolean') enabled = saved.enabled;
    if (typeof saved?.volume === 'number' && Number.isFinite(saved.volume))
      volume = Math.max(0, Math.min(1, saved.volume));
  } catch {
    // Missing or invalid preferences use the defaults.
  }

  const sound = installUiSound({ document, selector: ICON_CONTROLS, enabled, volume });
  let disposed = false;
  const save = () => {
    try {
      storage?.setItem(
        UI_SOUND_STORAGE_KEY,
        JSON.stringify({ enabled: sound.enabled, volume: sound.volume }),
      );
    } catch {
      onSaveError();
    }
  };
  const settings = {
    get enabled() {
      return sound.enabled;
    },
    get volume() {
      return sound.volume;
    },
    setEnabled(value) {
      if (disposed) return;
      sound.setEnabled(value);
      save();
    },
    setVolume(value) {
      if (disposed) return;
      sound.setVolume(value);
      save();
    },
    dispose() {
      if (disposed) return;
      disposed = true;
      sound.dispose();
      installed.delete(document);
    },
  };
  installed.set(document, settings);
  return settings;
}
