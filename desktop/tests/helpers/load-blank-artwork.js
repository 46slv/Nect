import { blankDocument, serialize } from '../../src/core.js';

// Install the former startup artwork explicitly in tests that exercise a live workspace.
export function loadBlankArtwork(win) {
  win.nect.load(serialize(blankDocument()));
}
