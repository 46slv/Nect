const targetKeys = [
  'documentGeneration',
  'compositionId',
  'sessionId',
  'sessionRevision',
  'projectRevision',
];

// In-process, application-owned operation. A caller supplies intent and the
// target version, never the document or a replacement destination writer.
export function createLiveArtworkOutput({ readTarget, readSnapshot, exportArtwork, write }) {
  let pending = false;

  function currentTarget() {
    const values = readTarget();
    return Object.freeze(Object.fromEntries(targetKeys.map((key) => [key, values[key]])));
  }

  async function run({ target, format, svgOptions, pngOptions }) {
    const current = currentTarget();
    if (!target || targetKeys.some((key) => current[key] !== target[key]))
      return { status: 'stale-target' };
    if (pending) return { status: 'busy' };

    pending = true;
    try {
      const snapshot = readSnapshot();
      const result = await exportArtwork({ ...snapshot, format, svgOptions, pngOptions }, write);
      return { ...result, status: result.written ? 'accepted-by-writer' : 'canceled' };
    } finally {
      pending = false;
    }
  }

  return Object.freeze({ inspect: currentTarget, run });
}
