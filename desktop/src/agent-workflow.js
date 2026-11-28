import { blankDocument, deserialize, exportSVG } from './core.js';
import { createCompositionSessionOwner } from './composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  MAX_COMPOSITION_DIMENSION,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from './composition-project-model.js';
import { DocumentLifecycle } from './document-lifecycle.js';
import { decodeImage, pngSize, validateImageAssets } from './images.js';
import { isNewProjectDimension } from './new-project-dimensions.js';
import { pngDimensions } from './raster.js';

export class AgentWorkflowError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
  }
}

const fail = (code, message) => {
  throw new AgentWorkflowError(code, message);
};
const dimension = { type: 'number', exclusiveMinimum: 0, maximum: MAX_COMPOSITION_DIMENSION };
const coordinate = { type: 'number' };
const text = (maxLength) => ({ type: 'string', minLength: 1, maxLength });
const fileName = text(80);
const object = (properties, optional = []) => ({
  type: 'object',
  properties,
  required: Object.keys(properties).filter((key) => !optional.includes(key)),
  additionalProperties: false,
});
const revision = { type: 'integer', minimum: 0 };
const target = object({ sessionId: text(200), revision, bindingRevision: revision });
const compositionId = text(181);
const replacement = { target: { anyOf: [{ type: 'null' }, target] }, discard: { type: 'boolean' } };
const tool = (name, description, properties, optional) => ({
  name: `nect_${name}`,
  description,
  inputSchema: object(properties, optional),
});
const tools = [
  tool(
    'inspect',
    'Read current target, Composition catalog and active graph, file binding and Undo/Redo state.',
    {},
  ),
  tool(
    'new',
    'Create a blank project. Both dimensions are explicit CSS pixels.',
    { ...replacement, width: dimension, height: dimension },
    ['discard'],
  ),
  tool(
    'open',
    'Open a workspace .json project using its stored dimensions. Embedded PNGs require a decoder.',
    { ...replacement, fileName },
    ['discard'],
  ),
  tool(
    'create_rectangle',
    'Create a Rectangle with center (x, y) in artboard pixels.',
    { target, x: coordinate, y: coordinate, name: text(120) },
    ['name'],
  ),
  tool('move_artwork', 'Translate one supported artwork layer in artboard pixels.', {
    target,
    layerId: text(181),
    dx: coordinate,
    dy: coordinate,
  }),
  tool('rename_composition', 'Rename a Composition by exact ID without switching tabs.', {
    target,
    compositionId,
    // The project model owns the name length limit; stdio also caps request bytes.
    name: { type: 'string' },
  }),
  tool('switch_composition', 'Select a Composition by exact ID without editing the project.', {
    target,
    compositionId,
  }),
  tool('undo', 'Undo one project edit.', { target }),
  tool('redo', 'Redo one project edit.', { target }),
  tool(
    'save',
    'Save to the live binding, or supply a fresh workspace .json name if unbound.',
    { target, fileName },
    ['fileName'],
  ),
  tool('save_as', 'Save to a fresh workspace .json file and bind it. Never overwrite.', {
    target,
    fileName,
  }),
  tool(
    'export_svg',
    'Export current artwork to a fresh workspace .svg file. Embedded PNGs require a decoder.',
    { target, fileName },
  ),
  tool(
    'export_png',
    'Export current artwork at 1× to a fresh workspace .png file (requires configured headless Chromium).',
    { target, fileName },
  ),
];
const byName = new Map(tools.map((entry) => [entry.name, entry]));
export const agentWorkflowTools = () => structuredClone(tools);

// Check the small schema vocabulary above at the common API boundary, not only in MCP.
function matches(schema, value) {
  if (schema.anyOf) return schema.anyOf.some((variant) => matches(variant, value));
  switch (schema.type) {
    case 'object': {
      if (!value || Object.getPrototypeOf(value) !== Object.prototype) return false;
      const entries = Object.getOwnPropertyDescriptors(value);
      return (
        schema.required.every((k) => Object.hasOwn(entries, k)) &&
        Reflect.ownKeys(entries).every(
          (k) =>
            Object.hasOwn(schema.properties, k) &&
            Object.hasOwn(entries[k], 'value') &&
            matches(schema.properties[k], entries[k].value),
        )
      );
    }
    case 'number':
      return (
        Number.isFinite(value) &&
        (schema.exclusiveMinimum === undefined || value > schema.exclusiveMinimum) &&
        (schema.maximum === undefined || value <= schema.maximum)
      );
    case 'integer':
      return Number.isSafeInteger(value) && value >= schema.minimum;
    case 'string':
      return (
        typeof value === 'string' &&
        value.length >= (schema.minLength ?? 0) &&
        (schema.maxLength === undefined ||
          (value.length <= schema.maxLength * 2 && [...value].length <= schema.maxLength))
      );
    case 'null':
      return value === null;
    case 'boolean':
      return typeof value === 'boolean';
    default:
      return false;
  }
}

// Build a validated project candidate without creating a Session.
export function createExplicitNewProject({ width, height } = {}) {
  if (!isNewProjectDimension(width) || !isNewProjectDimension(height))
    fail(
      'INVALID_ARGUMENT',
      `New requires width and height in (0, ${MAX_COMPOSITION_DIMENSION}] CSS pixels.`,
    );
  return migrateLegacyDocumentToCompositionProject({ ...blankDocument(), width, height });
}

// Read only Image tags emitted by our own exportSVG renderer, not arbitrary SVG.
// Effects can encode new PNGs, so validating just the persisted registry would
// not check the actual output. Geometry (including display size) stays untouched.
function svgImageAssets(svg) {
  const assets = {};
  const seen = new Set();
  for (const [tag] of svg.matchAll(/<image\b[^>]*>/g)) {
    const data = / href="data:image\/png;base64,([A-Za-z0-9+/]+={0,2})"\/>$/.exec(tag)?.[1];
    if (!data) throw Error('Expected an embedded PNG');
    if (seen.has(data)) continue;
    seen.add(data);
    assets[`image-svg-${seen.size}`] = { data, ...pngSize(decodeImage(data)) };
  }
  return assets;
}

/**
 * DOM-free owner/dispatch. The trusted host supplies an asynchronous file port:
 * open(fileName) -> {contents, binding}; assertFresh(fileName, format);
 * writeProject({contents, binding, fileName}) -> binding; exportArtwork({fileName, format, svg}).
 * validateImages({imageAssets}) is a transport-independent capability: return
 * 'valid' only after decoding every supplied PNG and matching its natural size,
 * 'rejected' for bad images, or 'unavailable' if validation could not run.
 * Omit it only for image-free work. Optionally release(binding) retires a token.
 * Bindings are opaque except for fileName, token and an optional cleanupWarning.
 */
export function createAgentWorkflow({ files }) {
  let owner = null;
  let bindingRevision = 0;
  let busy = false;
  let discard = false;
  let saveName;
  let cachedProject;
  const projectCache = (sessionOwner) => {
    const snapshot = sessionOwner.projectSnapshot();
    return {
      owner: sessionOwner,
      revision: sessionOwner.projectRevision,
      contents: serializeCompositionProject(snapshot),
      compositions: new Map(
        snapshot.compositions.map(({ id, name, width, height }) => [
          id,
          { id, name, width, height },
        ]),
      ),
    };
  };
  const authoredProject = () => {
    if (!owner) return null;
    if (owner !== cachedProject?.owner || owner.projectRevision !== cachedProject.revision)
      cachedProject = projectCache(owner);
    return cachedProject;
  };
  const documents = new DocumentLifecycle({
    read: () => authoredProject()?.contents ?? null,
    release: (binding) => files.release?.(binding),
    choose: () => (discard ? 'discard' : 'cancel'),
    hasDraft: () => !!(owner?.hasGesture || owner?.hasTextDrafts),
    write: ({ contents, binding }) => files.writeProject({ contents, binding, fileName: saveName }),
  });
  const currentTarget = () =>
    owner
      ? {
          sessionId: owner.state.sessionId,
          revision: owner.projectRevision,
          bindingRevision,
        }
      : null;
  const state = () => ({
    target: currentTarget(),
    fileName: documents.binding?.fileName ?? null,
    dirty: documents.dirty,
    savedRevision: documents.savedRevision,
    lifecycleRevision: documents.revision,
    history: owner?.historyState ?? null,
  });

  function compositionName(compositionId) {
    return authoredProject().compositions.get(compositionId)?.name;
  }

  function inspect() {
    if (!owner) return { ...state(), compositions: [], composition: null };
    const compositionId = owner.activeCompositionId;
    const name = compositionName(compositionId);
    const { width, height, layers } = owner.historyView.doc;
    let remaining = 256;
    return {
      ...state(),
      compositions: Array.from(authoredProject().compositions.values(), (entry) => ({ ...entry })),
      composition: {
        id: compositionId,
        name,
        width,
        height,
        layers: layers.map((layer) => {
          const nodes = layer.nodes.slice(0, remaining).map((n) => ({
            id: n.id,
            type: n.type,
            pathSourceId: n.params?.path?.source ?? null,
          }));
          remaining -= nodes.length;
          return {
            id: layer.id,
            name: layer.name.slice(0, 120),
            locked: layer.locked,
            outputId: layer.output,
            nodeCount: layer.nodes.length,
            nodes,
          };
        }),
      },
    };
  }

  async function verifyImages(imageAssets) {
    if (!Object.keys(imageAssets ?? {}).length) return;
    let validation;
    try {
      validation = await files.validateImages?.({ imageAssets });
    } catch {
      // Do not expose host errors (which can contain local paths) to callers.
    }
    if (validation === 'valid') return;
    if (validation === 'rejected') rejectImages();
    fail(
      'IMAGE_VALIDATION_UNAVAILABLE',
      'Embedded PNGs could not be verified. Enable a working image decoder (stdio: --chromium-path) and retry.',
    );
  }

  function rejectImages() {
    fail(
      'IMAGE_REJECTED',
      'An embedded PNG could not be decoded or its dimensions differ. Repair or replace the image in the source project.',
    );
  }

  async function replace(name, args) {
    discard = args.discard === true;
    if (!(await documents.guard(name)))
      fail('DIRTY_DOCUMENT', 'Current project has unsaved changes; Save or explicitly discard.');
    let candidate,
      binding = null;
    if (name === 'nect_new') {
      candidate = createCompositionSessionOwner(createExplicitNewProject(args));
    } else {
      const opened = await files.open(args.fileName);
      let preparedCache;
      try {
        const project = deserializeCompositionProject(opened.contents, deserialize);
        // Desktop Open preflights the global registry, including PNGs used only
        // in inactive Compositions or not referenced at all.
        validateImageAssets(project.imageAssets);
        await verifyImages(project.imageAssets);
        candidate = createCompositionSessionOwner(project);
        preparedCache = projectCache(candidate);
      } catch (error) {
        await documents.releaseUnused(opened.binding);
        if (error instanceof AgentWorkflowError) throw error;
        fail('INVALID_PROJECT', 'Project JSON cannot be decoded and validated by this session.');
      }
      binding = opened.binding;
      cachedProject = preparedCache;
    }
    owner = candidate;
    bindingRevision = 0;
    await documents.replace({ binding, saved: binding !== null });
    return state();
  }

  function edit(type, payload) {
    let result;
    try {
      result = owner.dispatch({ ...owner.state, type, payload });
    } catch (error) {
      fail(
        'EDIT_REJECTED',
        (error instanceof Error ? error.message : 'Edit rejected').slice(0, 300),
      );
    }
    const { changed, layerId, nodeId } = result;
    return {
      ...state(),
      changed,
      ...(layerId ? { layerId } : {}),
      ...(nodeId ? { nodeId } : {}),
    };
  }

  async function run(name, args) {
    if (name === 'nect_inspect') return inspect();
    const replacing = name === 'nect_new' || name === 'nect_open';
    if (!replacing && !owner) fail('NO_DOCUMENT', 'Create or open a project first.');
    const live = currentTarget();
    if (
      live?.sessionId !== args.target?.sessionId ||
      live?.revision !== args.target?.revision ||
      live?.bindingRevision !== args.target?.bindingRevision
    )
      fail(
        'STALE_TARGET',
        'Inspect again; the project, active Composition, revision or file binding changed.',
      );
    if (documents.hasPendingEdits()) fail('BUSY', 'Finish the active edit first.');
    if (replacing) return replace(name, args);
    switch (name) {
      case 'nect_create_rectangle':
        return edit('create-artwork', {
          artworkType: 'Rectangle',
          at: [args.x, args.y],
          name: args.name,
        });
      case 'nect_move_artwork':
        return edit('translate-artworks', { layerIds: [args.layerId], delta: [args.dx, args.dy] });
      case 'nect_rename_composition': {
        const { compositionId, name } = args;
        const expectedName = compositionName(compositionId);
        if (expectedName === undefined)
          fail('UNKNOWN_COMPOSITION', 'No Composition matches that ID.');
        // Like the GUI, allow an unchanged stored name, even a legacy blank one.
        if (name !== expectedName && !name.trim())
          fail('INVALID_ARGUMENT', 'Composition name cannot be blank.');
        return edit('rename-composition', {
          compositionId,
          name,
          expectedName,
          expectedProjectRevision: owner.projectRevision,
        });
      }
      case 'nect_switch_composition': {
        const { compositionId } = args;
        if (compositionName(compositionId) === undefined)
          fail('UNKNOWN_COMPOSITION', 'No Composition matches that ID.');
        let changed;
        try {
          // A real switch replaces the command Session, rotating currentTarget's sessionId.
          changed = owner.switchComposition(compositionId);
        } catch (error) {
          if (error?.code !== 'PREPARATION_REQUIRED') throw error;
          fail(
            'PREPARATION_REQUIRED',
            'This Composition needs font preparation. Use the native Nect Desktop editor to select it.',
          );
        }
        return { ...state(), changed };
      }
      case 'nect_undo':
      case 'nect_redo': {
        const type = name === 'nect_undo' ? 'undo' : 'redo';
        if (!owner.historyState[type === 'undo' ? 'canUndo' : 'canRedo'])
          fail('NOT_APPLICABLE', `Nothing to ${type}.`);
        return edit(type);
      }
      case 'nect_save':
      case 'nect_save_as': {
        const saveAs = name === 'nect_save_as';
        if (
          !saveAs &&
          documents.binding &&
          args.fileName !== undefined &&
          args.fileName !== documents.binding.fileName
        )
          fail(
            'BINDING_MISMATCH',
            'Save only updates the current binding; use Save As for a new file.',
          );
        if (!documents.binding && args.fileName === undefined)
          fail('NAME_REQUIRED', 'First Save requires an explicit fresh .json name.');
        const newBinding = saveAs || !documents.binding;
        saveName = args.fileName;
        if (!(await documents.save(saveAs))) fail('SAVE_CANCELLED', 'Save did not complete.');
        if (newBinding) bindingRevision++;
        const warning = documents.binding?.cleanupWarning;
        return { ...state(), ...(warning ? { warning } : {}) };
      }
      case 'nect_export_svg':
      case 'nect_export_png': {
        const format = name === 'nect_export_svg' ? 'svg' : 'png';
        await files.assertFresh(args.fileName, format);
        const doc = owner.historyView.doc;
        let size, svg;
        if (format === 'png') {
          try {
            size = pngDimensions(doc);
          } catch {
            fail('RASTER_SIZE', 'Composition is outside the existing 1× PNG size budget.');
          }
        }
        try {
          svg = exportSVG(doc);
        } catch (error) {
          fail(
            'EXPORT_REJECTED',
            (error instanceof Error ? error.message : 'Export rejected').slice(0, 300),
          );
        }
        if (format === 'svg') {
          let imageAssets;
          try {
            imageAssets = svgImageAssets(svg);
          } catch {
            rejectImages();
          }
          await verifyImages(imageAssets);
        }
        const warning = await files.exportArtwork({ fileName: args.fileName, format, svg });
        return {
          ...state(),
          output: { fileName: args.fileName, format, ...size },
          ...(warning ? { warning } : {}),
        };
      }
    }
  }

  return Object.freeze({
    async dispatch(name, args = {}) {
      if (busy)
        return {
          ok: false,
          error: { code: 'BUSY', message: 'A workflow request is in progress.' },
        };
      busy = true;
      try {
        const definition = byName.get(name);
        if (!definition) fail('UNKNOWN_OPERATION', 'Operation is not in this workflow.');
        if (!matches(definition.inputSchema, args))
          fail(
            'INVALID_ARGUMENT',
            name === 'nect_new'
              ? `New requires numeric width and height in (0, ${MAX_COMPOSITION_DIMENSION}] CSS pixels and a valid target.`
              : `Arguments do not match ${name}'s input schema.`,
          );
        return { ok: true, result: await run(name, args) };
      } catch (error) {
        return {
          ok: false,
          error:
            error instanceof AgentWorkflowError
              ? { code: error.code, message: error.message }
              : { code: 'INTERNAL_ERROR', message: 'Workflow operation failed unexpectedly.' },
        };
      } finally {
        saveName = undefined;
        discard = false;
        busy = false;
      }
    },
  });
}
