import { captureEditorTarget, editorTargetMatches } from './live-editor-target.js';
import { MISSING_GLYPH_CODE } from './text-layout.js';

// In-process, application-owned operation. A caller supplies intent and the
// target version, never the document or a replacement destination writer.
export function createLiveArtworkOutput({ readTarget, readSnapshot, exportArtwork, write }) {
  let pending = false;

  function currentTarget() {
    const values = readTarget();
    return captureEditorTarget(values);
  }

  async function run({ target, format, svgOptions, pngOptions }) {
    const current = currentTarget();
    if (!editorTargetMatches(current, target)) return { status: 'stale-target' };
    if (pending) return { status: 'busy' };

    pending = true;
    try {
      const snapshot = readSnapshot();
      const result = await exportArtwork({ ...snapshot, format, svgOptions, pngOptions }, write);
      return { ...result, status: result.written ? 'accepted-by-writer' : 'canceled' };
    } catch (error) {
      if (error?.code === MISSING_GLYPH_CODE) return { status: 'unsupported-glyph' };
      throw error;
    } finally {
      pending = false;
    }
  }

  return Object.freeze({ inspect: currentTarget, run });
}
