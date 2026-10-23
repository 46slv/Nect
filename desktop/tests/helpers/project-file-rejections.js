import { blankDocument, node } from '../../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../../src/composition-project-model.js';

export function projectFileRejections() {
  const valid = migrateLegacyDocumentToCompositionProject(blankDocument());
  const invalidNode = () => ({ ...node('Rectangle', 0, 0), type: 'Not a Nect node' });
  const dimensions = structuredClone(valid);
  dimensions.compositions[0].width = 0;
  const activeGraph = structuredClone(valid);
  activeGraph.compositions[0].layers[0].nodes.push(invalidNode());
  const inactive = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  inactive.id = 'invalid-inactive-composition';
  inactive.layers[0].nodes.push(invalidNode());
  const inactiveGraph = {
    ...structuredClone(valid),
    compositions: [...valid.compositions, inactive],
  };
  return [
    ['malformed', '{invalid JSON', /JSON|property name/i, false],
    ['dimensions', JSON.stringify(dimensions), /Invalid Composition width/, false],
    ['active-graph', JSON.stringify(activeGraph), /Invalid node/, true],
    ['inactive-graph', JSON.stringify(inactiveGraph), /Invalid node/, true],
  ];
}

// Passive browser-test observation: a copied/stale status is not a new rejection.
// Arming on the chosen file's change event excludes earlier menu/dialog renders.
export function observeProjectRejection({ name, prefix }, document = globalThis.document) {
  const observation = { text: null };
  let messageAtChange = null;
  const observer = new document.defaultView.MutationObserver((records) => {
    const message = document.querySelector('.status .message');
    if (!message?.isConnected) return;
    if (message !== messageAtChange) {
      messageAtChange = message;
      return;
    }
    if (!records.some(({ target }) => target === message || target.parentNode === message)) return;
    if (!message.textContent.startsWith(prefix)) return;
    observation.text = message.textContent;
    observation.dispose();
  });
  const changed = ({ target }) => {
    if (target?.type !== 'file' || target.files?.[0]?.name !== name) return;
    messageAtChange = document.querySelector('.status .message');
    document.removeEventListener('change', changed, true);
    observer.observe(document.body, { childList: true, characterData: true, subtree: true });
  };
  observation.dispose = () => {
    document.removeEventListener('change', changed, true);
    observer.disconnect();
  };
  document.addEventListener('change', changed, true);
  return observation;
}
