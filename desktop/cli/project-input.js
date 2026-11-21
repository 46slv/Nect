import { deserialize, validate } from '../src/core.js';
import { MAX_PROJECT_BYTES } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

export function deserializeExportDocument(text, compositionId) {
  if (text.length > MAX_PROJECT_BYTES || Buffer.byteLength(text, 'utf8') > MAX_PROJECT_BYTES)
    throw Error('Project exceeds 50 MB');
  const project = deserializeCompositionProject(text, deserialize);
  if (compositionId === undefined) {
    if (project.compositions.length !== 1)
      throw Error('Multiple Compositions: specify --composition <id>');
    compositionId = project.compositions[0].id;
  }
  if (!project.compositions.some((composition) => composition.id === compositionId))
    throw Error(`Unknown Composition ID: ${compositionId}`);

  let selected;
  for (const composition of project.compositions) {
    // Match the production owner's all-Composition core validation. Projections
    // are detached so core upgrades cannot change the saved project or siblings.
    const document = validate(activeCompositionDocument(project, composition.id));
    if (composition.id === compositionId) selected = document;
  }
  return selected;
}
