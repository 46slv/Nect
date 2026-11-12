import {
  validateCompositionProject,
  assertFreshCompositionEntityIds,
  assertNoLegacyArtworkReferencePromotion,
  assertNoLegacyQualifiedColorReferencePromotion,
  assertNoLegacyQualifiedColorDocumentPromotion,
  assertNoLegacyComponentCompositionReferencePromotion,
  assertNoLegacyComponentCompositionDocumentPromotion,
  assertNoLegacyCompositionPropertyReferencePromotion,
  assertNoLegacyCompositionPropertyDocumentPromotion,
  assertNoLegacyHandleReferencePromotion,
  validateMacroProjectAdmission,
  validateMacroProjectCandidate,
  validateMacroProjectTransition,
} from './composition-project-model.js';
import { keyColorRecipeInitializationSlotIds } from './key-color-recipe-document.js';
import { assertColorSlotUnused } from './color-slots.js';
import { prepareProjectCompositionsImport } from './composition-project-import.js';
import { resolveLayerClipboardToken } from './layer-clipboard-owner.js';
import {
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  reserveDanglingArtworkReferenceIds,
} from './curve-point-reference.js';
const PROJECT_HISTORY_TOKEN = Symbol('project history token');

function macroVersionAdmitted(classify, project) {
  const admitted = classify(project.schemaVersion);
  if (typeof admitted !== 'boolean') throw Error('Invalid Macro admission classifier');
  return admitted;
}

function capturePreparedHistoryCommand(command) {
  const captured = {};
  for (const key of ['type', 'sessionId', 'expectedRevision']) {
    const descriptor = Object.getOwnPropertyDescriptor(command, key);
    if (!descriptor || !descriptor.enumerable || !Object.hasOwn(descriptor, 'value'))
      throw Error('Invalid prepared History command');
    captured[key] = descriptor.value;
  }
  return Object.freeze(captured);
}

// A Project cannot retain a reference to a removed Layer. Explain the affected
// source and consumer before either the inner Session or Project commits.
// A standalone Document Session intentionally permits a diagnosed missing target.
function assertNoRemovedLayerConsumers(before, candidate) {
  const remaining = new Set(candidate.layers.map((layer) => layer.id));
  const removed = new Map(
    before.layers.filter((layer) => !remaining.has(layer.id)).map((layer) => [layer.id, layer]),
  );
  if (!removed.size) return;
  for (const consumer of candidate.layers)
    for (const node of consumer.nodes) {
      const target = removed.get(node.compositionReference?.compositionId);
      if (target && ['Composition Reference', 'SVG'].includes(node.type))
        throw Error(`Cannot delete "${target.name}": used by "${consumer.name}"`);
    }
}
// Only document-global identities can satisfy an Artwork reference. Character,
// contour and remap-edit IDs stay scoped to their owning node.
function documentEntityIds(document, ids = new Set()) {
  for (const slot of document.colorSlots || []) ids.add(slot.id);
  for (const preset of document.nodePresets || []) ids.add(preset.id);
  for (const guide of document.editor?.guides || []) ids.add(guide.id);
  for (const folder of document.editor?.layerFolders || []) ids.add(folder.id);
  for (const layer of document.layers || []) {
    ids.add(layer.id);
    for (const group of layer.nodeGroups || []) ids.add(group.id);
    for (const n of layer.nodes) {
      ids.add(n.id);
      for (const point of n.points || []) ids.add(point.id);
    }
  }
  return ids;
}
function projectEntityIds(project) {
  const ids = documentEntityIds(project);
  for (const composition of project.compositions) {
    ids.add(composition.id);
    documentEntityIds(composition, ids);
  }
  return ids;
}

function assertProjectIdentityChangeSafe(project, compositionId, before, candidate) {
  const previous = documentEntityIds(before),
    reserved = projectEntityIds(project);
  for (const composition of project.compositions) {
    if (composition.id === compositionId) continue;
    if (project.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
      reserveDanglingArtworkReferenceIds(composition.layers, reserved);
  }
  for (const id of documentEntityIds(candidate))
    if (!previous.has(id) && reserved.has(id))
      throw Error(
        'Operation would retarget an inactive Composition reference or collide with an existing identity',
      );
}

function documentFor(project, compositionId, cloneDocument) {
  const composition = project.compositions.find((entry) => entry.id === compositionId);
  if (!composition) throw Error('Unknown active Composition ID');
  const { compositionSchemaVersion, compositions, ...shared } = project;
  return cloneDocument({
    ...shared,
    width: composition.width,
    height: composition.height,
    ...(composition.editor === undefined ? {} : { editor: composition.editor }),
    layers: composition.layers,
  });
}

function projectWithDocument(project, compositionId, document, adapters) {
  const { width, height, layers, editor, ...shared } = document;
  shared.schemaVersion = Math.max(project.schemaVersion, shared.schemaVersion);
  if (shared.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyComponentCompositionReferencePromotion(project);
    assertNoLegacyComponentCompositionDocumentPromotion(document);
  }
  if (shared.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyQualifiedColorReferencePromotion(project);
    assertNoLegacyQualifiedColorDocumentPromotion(document);
  }
  if (shared.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) {
    assertNoLegacyCompositionPropertyReferencePromotion(project);
    assertNoLegacyCompositionPropertyDocumentPromotion(document);
  }
  if (shared.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    assertNoLegacyArtworkReferencePromotion(project);
  if (shared.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION)
    assertNoLegacyHandleReferencePromotion(project);
  const candidate = {
    ...shared,
    compositionSchemaVersion:
      shared.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION
        ? CURVE_POINT_COMPOSITION_SCHEMA_VERSION
        : project.compositionSchemaVersion,
    compositions: project.compositions.map((composition) => {
      if (composition.id !== compositionId) {
        if (
          project.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION ||
          shared.schemaVersion < COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION ||
          !adapters.normalizedProjectSourceForDocument
        )
          return composition;
        // Compare both projections before changing the project version. Keep
        // the original bytes when the new reader supplies the same defaults;
        // materialize only migrations needed to retain the old behavior.
        const migrated = adapters.normalizedProjectSourceForDocument(document, composition.id);
        const trial = adapters.cloneDocument({
          ...shared,
          width: composition.width,
          height: composition.height,
          ...(composition.editor === undefined ? {} : { editor: composition.editor }),
          layers: composition.layers,
        });
        let same;
        try {
          adapters.validateDocument(trial);
          same =
            JSON.stringify(trial.layers) === JSON.stringify(migrated.layers) &&
            JSON.stringify(trial.editor) === JSON.stringify(migrated.editor);
        } catch {
          same = false;
        }
        if (same) return composition;
        const updated = {
          ...composition,
          width: migrated.width,
          height: migrated.height,
          layers: migrated.layers,
        };
        if (migrated.editor === undefined) delete updated.editor;
        else updated.editor = migrated.editor;
        return updated;
      }
      const updated = { ...composition, width, height, layers };
      if (editor === undefined) delete updated.editor;
      else updated.editor = editor;
      return updated;
    }),
  };
  return validateCompositionProject(candidate);
}

// Keep the retained fonts reader outside the switching request's scope.
// A shared closure context would otherwise keep retired CommandSessions alive.
function preparedCompositionFonts(fonts, bridge, ticket) {
  let preview = true;
  return {
    read: () => (preview ? { ...fonts(), ...bridge.preparedFonts(ticket) } : fonts()),
    useLive() {
      preview = false;
    },
  };
}

function replaceRecord(target, source) {
  for (const key of Object.keys(target)) delete target[key];
  Object.assign(target, source);
}

function freezeDocument(value) {
  if (value && typeof value === 'object' && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) freezeDocument(child);
    Object.freeze(value);
  }
  return value;
}

/**
 * Owns the project snapshot/history while using the existing CommandSession for
 * edits to the currently active legacy Document projection.
 *
 * Undo/Redo are project-wide and chronological. Each snapshot contains every
 * Composition plus the shared font/image registries, so an edit in another
 * Composition cannot restore a stale per-Composition asset map. Tab switching
 * replaces only the active CommandSession and never records a History step.
 */
export class CompositionSessionOwner {
  #history;
  #activeCompositionId;
  #session;
  #nodeClipboardOwner = Object.freeze({});
  #History;
  #CommandSession;
  #blankDocument;
  #cloneDocument;
  #sameDocument;
  #uid;
  #validateDocument;
  #bindProjectDocumentContext;
  #assertProjectReferenceTransition;
  #hasClipboardArtworkTargets;
  #normalizedProjectSourceForDocument;
  #fonts;
  #busy;
  #onCommitted;
  #historyViewCache = new WeakMap();
  #historyView = null;
  #prepared = null;
  #preparedCompositionSwitch = null;
  #macroProjectAdmission;

  constructor(
    project,
    {
      History,
      CommandSession,
      blankDocument,
      cloneDocument,
      sameDocument,
      uid,
      validateDocument,
      bindProjectDocumentContext = (doc) => doc,
      assertProjectReferenceTransition = () => {},
      hasClipboardArtworkTargets = () => false,
      normalizedProjectSourceForDocument = null,
      activeCompositionId,
      fonts = () => ({}),
      busy = () => false,
      onCommitted,
      // Candidate-only classifier. Normal product construction does not pass it.
      macroProjectAdmission = null,
    } = {},
  ) {
    if (
      typeof History !== 'function' ||
      typeof CommandSession !== 'function' ||
      typeof blankDocument !== 'function' ||
      typeof cloneDocument !== 'function' ||
      typeof sameDocument !== 'function' ||
      typeof hasClipboardArtworkTargets !== 'function' ||
      typeof uid !== 'function' ||
      typeof validateDocument !== 'function'
    )
      throw Error('Composition Session runtime adapters are required');
    if (macroProjectAdmission !== null && typeof macroProjectAdmission !== 'function')
      throw Error('Invalid Macro admission classifier');
    validateCompositionProject(project);
    if (macroProjectAdmission && macroVersionAdmitted(macroProjectAdmission, project))
      validateMacroProjectCandidate(project);
    this.#History = History;
    this.#CommandSession = CommandSession;
    this.#blankDocument = blankDocument;
    this.#cloneDocument = cloneDocument;
    this.#sameDocument = sameDocument;
    this.#uid = uid;
    this.#validateDocument = validateDocument;
    this.#bindProjectDocumentContext = bindProjectDocumentContext;
    this.#assertProjectReferenceTransition = assertProjectReferenceTransition;
    this.#hasClipboardArtworkTargets = hasClipboardArtworkTargets;
    this.#normalizedProjectSourceForDocument = normalizedProjectSourceForDocument;
    this.#assertProjectReferenceTransition(project, project, fonts());
    this.#history = new this.#History(project);
    this.#fonts = fonts;
    this.#busy = busy;
    this.#onCommitted = onCommitted;
    this.#macroProjectAdmission = macroProjectAdmission;
    this.#activeCompositionId = activeCompositionId ?? this.#history.doc.compositions[0].id;
    this.#session = this.#makeSession(this.#history.doc, this.#activeCompositionId);
    // Validate inactive Documents too; a broken tab must not remain hidden until
    // the user selects it.
    for (const composition of this.#history.doc.compositions) {
      if (composition.id !== this.#activeCompositionId)
        this.#validateDocument(this.#documentFor(this.#history.doc, composition.id));
    }
  }

  #documentFor(project, compositionId) {
    return this.#bindProjectDocumentContext(
      documentFor(project, compositionId, this.#cloneDocument),
      project,
      compositionId,
    );
  }

  #projectWithDocument(project, compositionId, document) {
    return projectWithDocument(project, compositionId, document, {
      cloneDocument: this.#cloneDocument,
      validateDocument: this.#validateDocument,
      normalizedProjectSourceForDocument: this.#normalizedProjectSourceForDocument,
    });
  }

  #documentView(project, compositionId) {
    let views = this.#historyViewCache.get(project);
    if (!views) {
      views = new Map();
      this.#historyViewCache.set(project, views);
    }
    if (!views.has(compositionId))
      views.set(compositionId, freezeDocument(this.#documentFor(project, compositionId)));
    return views.get(compositionId);
  }

  #projectHistoryToken(project) {
    if (!project) return null;
    let views = this.#historyViewCache.get(project);
    if (!views) {
      views = new Map();
      this.#historyViewCache.set(project, views);
    }
    if (!views.has(PROJECT_HISTORY_TOKEN)) views.set(PROJECT_HISTORY_TOKEN, Object.freeze({}));
    return views.get(PROJECT_HISTORY_TOKEN);
  }

  #applyProjectEdit(edit) {
    const previous = this.#history.doc,
      views = this.#historyViewCache.get(previous),
      revision = this.#history.revision;
    try {
      this.#history.apply(edit);
    } catch (error) {
      const restored = this.#history.doc;
      this.#historyViewCache.delete(previous);
      if (views) this.#historyViewCache.set(restored, views);
      throw error;
    }
    if (this.#history.revision !== revision) {
      const past = this.#history.past.at(-1);
      this.#historyViewCache.delete(previous);
      if (views && past) this.#historyViewCache.set(past, views);
    }
  }

  #assertMacroOwnerCurrent(session, compositionId, revision, before) {
    if (
      this.#session !== session ||
      this.#activeCompositionId !== compositionId ||
      this.#history.revision !== revision ||
      this.#history.doc !== before
    )
      throw Error('STALE_REVISION: document changed');
  }

  #assertMacroProjectTransition(before, candidate, session, compositionId, revision) {
    const classify = this.#macroProjectAdmission;
    if (!classify) return;
    const assertCurrent = () =>
      this.#assertMacroOwnerCurrent(session, compositionId, revision, before);
    const admitted = (project) => {
      const result = macroVersionAdmitted(classify, project);
      assertCurrent();
      return result;
    };
    assertCurrent();
    const previousAdmitted = admitted(before),
      candidateAdmitted = admitted(candidate);
    if (!previousAdmitted && !candidateAdmitted) return;
    if (previousAdmitted && !candidateAdmitted) throw Error('Cannot remove Macro admission');
    if (!previousAdmitted && Object.hasOwn(before, 'macroDefinitions'))
      throw Error('Cannot replace opaque Macro definitions');
    const retainedDefinitionSets = [];
    for (const project of [...this.#history.past, ...this.#history.future])
      if (admitted(project)) {
        validateMacroProjectCandidate(project);
        retainedDefinitionSets.push(project.macroDefinitions ?? []);
        assertCurrent();
      }
    if (previousAdmitted)
      validateMacroProjectTransition(before, candidate, { retainedDefinitionSets });
    else validateMacroProjectAdmission(candidate, { retainedDefinitionSets });
    assertCurrent();
  }

  #compositionFor(project) {
    return project.compositions.some((entry) => entry.id === this.#activeCompositionId)
      ? this.#activeCompositionId
      : project.compositions[0].id;
  }

  #assertPrepared(token) {
    const prepared = this.#prepared;
    if (
      !prepared ||
      prepared.token !== token ||
      prepared.projectRevision !== this.#history.revision ||
      prepared.session !== this.#session ||
      prepared.compositionId !== this.#activeCompositionId
    )
      throw Error('STALE_REVISION: document changed');
    if (
      prepared.layerClipboardSnapshot &&
      resolveLayerClipboardToken(prepared.command.payload?.token, this, prepared.compositionId) !==
        prepared.layerClipboardSnapshot
    )
      throw Error('STALE_SESSION: copied Layer changed');
    if (
      prepared !== this.#prepared ||
      prepared.projectRevision !== this.#history.revision ||
      prepared.session !== this.#session ||
      prepared.compositionId !== this.#activeCompositionId
    )
      throw Error('STALE_REVISION: document changed');
    return prepared;
  }

  #makeSession(project, compositionId, fonts = this.#fonts, nodeClipboardFrom = null) {
    let session;
    session = new this.#CommandSession(this.#documentFor(project, compositionId), {
      fonts,
      busy: this.#busy,
      retainHistory: false,
      resolveLayerClipboardSnapshot: (token) =>
        resolveLayerClipboardToken(token, this, compositionId),
      nodeClipboardOwner: this.#nodeClipboardOwner,
      nodeClipboardFrom,
      retainColorClipboardForProjectContext: (nextProject, nextCompositionId) =>
        this.#session === session &&
        this.#history.doc === nextProject &&
        this.#activeCompositionId === nextCompositionId,
      captureArtworkMoveAuthority: () => {
        const revision = this.#history.revision;
        const assertCurrent = () => {
          if (this.#session !== session || this.#activeCompositionId !== compositionId)
            throw Error('STALE_SESSION: document changed');
          if (this.#history.revision !== revision) throw Error('STALE_REVISION: document changed');
        };
        assertCurrent();
        return assertCurrent;
      },
      beforeCandidateCommit: (before, candidate, preparedFonts = {}) => {
        assertNoRemovedLayerConsumers(before, candidate);
        assertProjectIdentityChangeSafe(this.#history.doc, compositionId, before, candidate);
        const project = this.#history.doc,
          revision = this.#history.revision;
        // Validate the whole projection before the inner Session commits;
        // a new identity or reference can affect an inactive Composition.
        const next = this.#projectWithDocument(project, compositionId, candidate);
        this.#assertMacroProjectTransition(project, next, session, compositionId, revision);
        this.#assertProjectReferenceTransition(
          this.#macroProjectAdmission ? project : this.#history.doc,
          next,
          {
            ...this.#fonts(),
            ...preparedFonts,
          },
        );
        if (this.#macroProjectAdmission)
          this.#assertMacroOwnerCurrent(session, compositionId, revision, project);
      },
    });
    return session;
  }

  #assertQuiescent({ allowTextDrafts = false } = {}) {
    if (
      this.#busy() ||
      this.#session.hasGesture ||
      this.#session.hasTextComposition ||
      (!allowTextDrafts && this.#session.hasTextDrafts)
    )
      throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
  }

  #assertCurrentCommand(command) {
    const state = this.#session.state;
    if (command?.sessionId !== state.sessionId) throw Error('STALE_SESSION: document changed');
    if (command.expectedRevision !== state.expectedRevision)
      throw Error('STALE_REVISION: document changed');
    if (
      command.payload !== undefined &&
      (!command.payload || typeof command.payload !== 'object' || Array.isArray(command.payload))
    )
      throw Error('Invalid command payload');
  }

  #notifyCommitted(operation) {
    try {
      this.#onCommitted?.({
        ...this.state,
        operation,
        projectRevision: this.#history.revision,
      });
    } catch {
      // A notification adapter cannot roll back a committed project change.
    }
  }

  #commitProject(candidate, activeCompositionId = this.#activeCompositionId, operation, precommit) {
    const retainClipboard = [
      'create-composition',
      'rename-composition',
      'resize-composition',
      'delete-composition',
    ].includes(operation);
    const session = this.#session,
      revision = this.#history.revision,
      compositionId = this.#activeCompositionId,
      before = this.#history.doc,
      assertClipboard = retainClipboard ? session.captureNodeClipboardFreshness?.() : null;
    const assertCurrent = () => {
      if (!retainClipboard && !this.#macroProjectAdmission) return;
      if (
        this.#session !== session ||
        this.#history.revision !== revision ||
        this.#activeCompositionId !== compositionId ||
        (this.#macroProjectAdmission && this.#history.doc !== before)
      )
        throw Error('STALE_REVISION: document changed');
      assertClipboard?.();
    };
    validateCompositionProject(candidate);
    this.#assertMacroProjectTransition(before, candidate, session, compositionId, revision);
    this.#assertProjectReferenceTransition(
      this.#macroProjectAdmission ? before : this.#history.doc,
      candidate,
      this.#fonts(),
    );
    assertCurrent();
    if (!candidate.compositions.some((entry) => entry.id === activeCompositionId))
      throw Error('Unknown active Composition ID');
    if (this.#sameDocument(this.#history.doc, candidate)) {
      precommit?.();
      assertCurrent();
      return false;
    }

    // Match initial Open validation, including inactive imported Documents,
    // before changing project History or the active session.
    for (const composition of candidate.compositions) {
      if (composition.id !== activeCompositionId)
        this.#validateDocument(this.#documentFor(candidate, composition.id));
    }
    const nextActiveDocument = this.#documentFor(candidate, activeCompositionId);
    const nextSession =
      activeCompositionId === this.#activeCompositionId &&
      this.#sameDocument(this.#session.snapshot(), nextActiveDocument)
        ? this.#session
        : this.#makeSession(
            candidate,
            activeCompositionId,
            this.#fonts,
            retainClipboard ? session : null,
          );
    assertCurrent();
    precommit?.();
    assertCurrent();
    this.#applyProjectEdit((history) => {
      replaceRecord(history, candidate);
    });
    this.#activeCompositionId = activeCompositionId;
    this.#session = nextSession;
    this.#session.setProjectContext?.(this.#history.doc, activeCompositionId);
    this.#notifyCommitted(operation);
    return true;
  }

  #stepHistory(direction, command, prepared = null) {
    this.#assertCurrentCommand(command);
    const session = this.#session,
      revision = this.#history.revision,
      compositionId = this.#activeCompositionId,
      assertClipboard = session.captureNodeClipboardFreshness?.();
    const assertCurrent = () => {
      this.#assertCurrentCommand(command);
      if (
        (prepared && this.#prepared !== prepared) ||
        this.#session !== session ||
        this.#history.revision !== revision ||
        this.#activeCompositionId !== compositionId
      )
        throw Error('STALE_REVISION: document changed');
      assertClipboard?.();
    };
    this.#assertQuiescent();
    assertCurrent();
    const candidate = (direction === 'undo' ? this.#history.past : this.#history.future).at(-1);
    if (!candidate) {
      if (prepared) {
        prepared.bridge.assertFresh(prepared.ticket);
        assertCurrent();
        prepared.bridge.cancel();
        this.#prepared = null;
      }
      return { ...this.state, changed: false, projectRevision: this.#history.revision };
    }
    validateCompositionProject(candidate);
    if (this.#macroProjectAdmission) {
      const admitted = macroVersionAdmitted(this.#macroProjectAdmission, candidate);
      assertCurrent();
      if (admitted) validateMacroProjectCandidate(candidate);
      assertCurrent();
    }
    const activeCompositionId = candidate.compositions.some(
      (entry) => entry.id === this.#activeCompositionId,
    )
      ? this.#activeCompositionId
      : candidate.compositions[0].id;
    const preparedFonts = prepared
      ? preparedCompositionFonts(this.#fonts, prepared.bridge, prepared.ticket)
      : null;
    const nextSession = this.#makeSession(
      candidate,
      activeCompositionId,
      preparedFonts ? preparedFonts.read : this.#fonts,
      session,
    );
    assertCurrent();
    if (prepared) {
      prepared.bridge.stageActivation(prepared.ticket);
      assertCurrent();
      prepared.bridge.assertFresh(prepared.ticket);
      assertCurrent();
      this.#assertQuiescent();
      assertCurrent();
      prepared.bridge.assertFresh(prepared.ticket);
      assertCurrent();
      // The new Session already binds the target Project and inherits clipboard
      // authority. Publish the prepared bank only after all fallible preflight.
      prepared.bridge.publishActivation(prepared.ticket);
      preparedFonts.useLive();
      this.#prepared = null;
    }
    this.#history[direction]();
    this.#activeCompositionId = activeCompositionId;
    this.#session = nextSession;
    if (!prepared) this.#session.setProjectContext?.(this.#history.doc, activeCompositionId);
    this.#notifyCommitted(direction);
    return { ...this.state, changed: true, projectRevision: this.#history.revision };
  }

  get activeCompositionId() {
    return this.#activeCompositionId;
  }

  get id() {
    return this.#session.id;
  }

  get historyView() {
    if (!this.#historyView) {
      const owner = this;
      this.#historyView = Object.freeze({
        // Opaque identities are shared across Composition projections and follow
        // the same cache transfer as the immutable document views.
        get entryToken() {
          return owner.#projectHistoryToken(owner.#history.doc);
        },
        get undoToken() {
          return owner.#projectHistoryToken(owner.#history.past.at(-1));
        },
        get redoToken() {
          return owner.#projectHistoryToken(owner.#history.future.at(-1));
        },
        get doc() {
          return owner.#documentView(owner.#history.doc, owner.#activeCompositionId);
        },
        get past() {
          return Object.freeze(
            owner.#history.past.map((project) =>
              owner.#documentView(project, owner.#compositionFor(project)),
            ),
          );
        },
        get future() {
          return Object.freeze(
            owner.#history.future.map((project) =>
              owner.#documentView(project, owner.#compositionFor(project)),
            ),
          );
        },
        get revision() {
          return owner.#history.revision;
        },
      });
    }
    return this.#historyView;
  }

  get state() {
    return this.#session.state;
  }

  get projectRevision() {
    return this.#history.revision;
  }

  get historyState() {
    return Object.freeze({
      revision: this.#history.revision,
      canUndo: this.#history.past.length > 0,
      canRedo: this.#history.future.length > 0,
    });
  }

  get hasGesture() {
    return this.#session.hasGesture;
  }

  get hasTextComposition() {
    return this.#session.hasTextComposition;
  }

  get hasTextDrafts() {
    return this.#session.hasTextDrafts;
  }

  get hasChangedTextDrafts() {
    return this.#session.hasChangedTextDrafts;
  }

  get gestureState() {
    return this.#session.gestureState;
  }

  activeDocument() {
    return this.#session.snapshot();
  }

  hasLayer(compositionId, layerId) {
    return this.#history.doc.compositions.some(
      (composition) =>
        composition.id === compositionId &&
        composition.layers.some((layer) => layer.id === layerId),
    );
  }

  hasLayerClipboardResources(compositionId, resources) {
    const project = this.#history.doc,
      composition = project.compositions.find((item) => item.id === compositionId);
    if (!composition) return false;
    if (resources.layerIds.some((id) => !composition.layers.some((layer) => layer.id === id)))
      return false;
    if (resources.slotIds.some((id) => !project.colorSlots?.some((slot) => slot.id === id)))
      return false;
    if (
      resources.artworkReferences.length &&
      !this.#hasClipboardArtworkTargets(composition, resources)
    )
      return false;
    if (resources.fonts.some(([id, data]) => !data || project.fontAssets?.[id]?.data !== data))
      return false;
    return !resources.images.some(([id, data, width, height]) => {
      const asset = project.imageAssets?.[id];
      return (
        !data || !asset || asset.data !== data || asset.width !== width || asset.height !== height
      );
    });
  }

  projectSnapshot() {
    return this.#cloneDocument(this.#history.doc);
  }

  snapshot() {
    return this.#session.snapshot();
  }

  previewSnapshot(gestureId) {
    return this.#session.previewSnapshot(gestureId);
  }

  textDraftSnapshot(draftId) {
    return this.#session.textDraftSnapshot(draftId);
  }

  async prepareCommand(command, bridge, options = {}) {
    const typeDescriptor =
      command && typeof command === 'object'
        ? Object.getOwnPropertyDescriptor(command, 'type')
        : null;
    if (typeDescriptor && !Object.hasOwn(typeDescriptor, 'value'))
      throw Error('Invalid prepared command type');
    if (['undo', 'redo'].includes(typeDescriptor?.value))
      command = capturePreparedHistoryCommand(command);
    this.#assertCurrentCommand(command);
    this.#assertQuiescent();
    if (command.type === 'delete-color-slot')
      assertColorSlotUnused(this.#history.doc, command.payload?.slotId);
    const session = this.#session,
      compositionId = this.#activeCompositionId,
      projectRevision = this.#history.revision;
    const layerClipboardSnapshot =
      command.type === 'paste-layer-snapshot'
        ? resolveLayerClipboardToken(command.payload?.token, this, compositionId)
        : null;
    if (command.type === 'undo' || command.type === 'redo') {
      const project =
        (command.type === 'undo' ? this.#history.past : this.#history.future).at(-1) ||
        this.#history.doc;
      const targetCompositionId = this.#compositionFor(project);
      const document = this.#documentFor(project, targetCompositionId);
      const ticket = await bridge.prepareDocument(document, options);
      if (
        this.#session !== session ||
        this.#activeCompositionId !== compositionId ||
        this.#history.revision !== projectRevision
      ) {
        bridge.cancel();
        throw Error('STALE_REVISION: document changed');
      }
      const token = Object.freeze({});
      this.#prepared = {
        token,
        projectRevision,
        session,
        compositionId,
        type: command.type,
        command,
        bridge,
        ticket,
        document,
      };
      return token;
    }

    const innerToken = await session.prepareCommand(command, bridge, options);
    if (
      this.#session !== session ||
      this.#activeCompositionId !== compositionId ||
      this.#history.revision !== projectRevision
    ) {
      session.cancelPrepared(bridge, innerToken);
      throw Error('STALE_REVISION: document changed');
    }
    try {
      if (
        layerClipboardSnapshot &&
        resolveLayerClipboardToken(command.payload?.token, this, compositionId) !==
          layerClipboardSnapshot
      )
        throw Error('STALE_SESSION: copied Layer changed');
      if (
        this.#session !== session ||
        this.#activeCompositionId !== compositionId ||
        this.#history.revision !== projectRevision
      )
        throw Error('STALE_REVISION: document changed');
    } catch (error) {
      session.cancelPrepared(bridge, innerToken);
      throw error;
    }
    const token = Object.freeze({});
    this.#prepared = {
      token,
      projectRevision,
      session,
      compositionId,
      type: command.type,
      command,
      bridge,
      innerToken,
      layerClipboardSnapshot,
    };
    return token;
  }

  preparedSnapshot(token) {
    const prepared = this.#assertPrepared(token);
    if (!prepared.ticket) return prepared.session.preparedSnapshot(prepared.innerToken);
    prepared.bridge.assertFresh(prepared.ticket);
    return {
      ...this.state,
      document: this.#cloneDocument(prepared.document),
      fonts: { ...this.#fonts(), ...prepared.bridge.preparedFonts(prepared.ticket) },
    };
  }

  commitPrepared(token) {
    const prepared = this.#assertPrepared(token);
    this.#assertQuiescent();
    this.#assertPrepared(token);
    if (prepared.ticket) {
      if (this.#macroProjectAdmission) {
        try {
          return this.#stepHistory(prepared.type, prepared.command, prepared);
        } catch (error) {
          if (this.#prepared === prepared) {
            this.#prepared = null;
            if (!prepared.bridge.discardStagedActivation(prepared.ticket)) {
              try {
                prepared.bridge.assertFresh(prepared.ticket);
                prepared.bridge.cancel();
              } catch {
                // Do not cancel a newer ticket on the same bridge.
              }
            }
          }
          throw error;
        }
      }
      prepared.bridge.assertFresh(prepared.ticket);
      prepared.bridge.activate(prepared.ticket);
      this.#prepared = null;
      return this.#stepHistory(prepared.type, prepared.command);
    }

    const revision = prepared.session.state.expectedRevision;
    try {
      const result = prepared.session.commitPrepared(prepared.innerToken);
      if (prepared.layerClipboardSnapshot || this.#macroProjectAdmission)
        this.#assertPrepared(token);
      if (prepared.session.state.expectedRevision !== revision) {
        const candidate = this.#projectWithDocument(
          this.#history.doc,
          prepared.compositionId,
          prepared.session.snapshot(),
        );
        if (prepared.layerClipboardSnapshot || this.#macroProjectAdmission)
          this.#assertPrepared(token);
        this.#applyProjectEdit((history) => {
          replaceRecord(history, candidate);
        });
        this.#session.setProjectContext?.(this.#history.doc, prepared.compositionId);
        this.#notifyCommitted('edit');
      }
      this.#prepared = null;
      return { ...result, projectRevision: this.#history.revision };
    } catch (error) {
      if (
        prepared.session.state.expectedRevision !== revision &&
        ((!prepared.layerClipboardSnapshot && !this.#macroProjectAdmission) ||
          (prepared.session === this.#session &&
            prepared.compositionId === this.#activeCompositionId &&
            prepared.projectRevision === this.#history.revision))
      ) {
        this.#session = this.#makeSession(this.#history.doc, prepared.compositionId);
      }
      this.#prepared = null;
      throw error;
    }
  }

  cancelPrepared(bridge) {
    const prepared = this.#prepared;
    this.#prepared = null;
    if (prepared?.bridge === bridge) {
      if (prepared.ticket) bridge.cancel();
      else prepared.session.cancelPrepared(bridge);
      return;
    }
    this.#session.cancelPrepared?.(bridge);
  }

  #assertCompositionSwitchCurrent(request) {
    if (
      this.#session !== request.session ||
      this.#activeCompositionId !== request.sourceId ||
      this.#history.revision !== request.projectRevision ||
      !this.#history.doc.compositions.some((entry) => entry.id === request.targetId)
    )
      throw Error('STALE_REVISION: Composition changed');
  }

  switchComposition(compositionId, { beforeCommit = () => {} } = {}) {
    if (!this.#history.doc.compositions.some((entry) => entry.id === compositionId))
      throw Error('Unknown active Composition ID');
    if (compositionId === this.#activeCompositionId) return false;
    const request = {
      session: this.#session,
      sourceId: this.#activeCompositionId,
      targetId: compositionId,
      projectRevision: this.#history.revision,
    };
    const assertClipboard = request.session.captureNodeClipboardFreshness?.();
    const assertCurrent = () => {
      this.#assertCompositionSwitchCurrent(request);
      assertClipboard?.();
    };
    // Construct first: an unprepared target must not discard the current view/draft.
    const nextSession = this.#makeSession(
      this.#history.doc,
      compositionId,
      this.#fonts,
      request.session,
    );
    assertCurrent();
    this.#assertQuiescent({ allowTextDrafts: true });
    assertCurrent();
    beforeCommit();
    assertCurrent();
    this.#assertQuiescent();
    assertCurrent();
    this.#activeCompositionId = compositionId;
    this.#session = nextSession;
    return true;
  }

  async prepareCompositionSwitch(compositionId, bridge, options = {}) {
    if (!this.#history.doc.compositions.some((entry) => entry.id === compositionId))
      throw Error('Unknown active Composition ID');
    if (compositionId === this.#activeCompositionId) return null;
    const request = {
      session: this.#session,
      sourceId: this.#activeCompositionId,
      targetId: compositionId,
      projectRevision: this.#history.revision,
      bridge,
      ticket: null,
    };
    this.#preparedCompositionSwitch = request;
    try {
      const document = this.#documentFor(this.#history.doc, compositionId);
      request.ticket = await bridge.prepareDocument(document, options);
      if (this.#preparedCompositionSwitch !== request) throw Error('STALE_OPERATION');
      this.#assertCompositionSwitchCurrent(request);
      bridge.assertFresh(request.ticket);
      request.token = Object.freeze({});
      return request.token;
    } catch (error) {
      if (this.#preparedCompositionSwitch === request) this.#preparedCompositionSwitch = null;
      // Never invalidate a newer operation's ticket on the same bridge.
      if (request.ticket) {
        try {
          bridge.assertFresh(request.ticket);
          bridge.cancel();
        } catch {
          // The ticket has already been canceled or superseded.
        }
      }
      throw error;
    }
  }

  cancelPreparedCompositionSwitch(bridge) {
    const request = this.#preparedCompositionSwitch;
    if (!request || request.bridge !== bridge) return;
    this.#preparedCompositionSwitch = null;
    if (request.ticket) {
      try {
        bridge.assertFresh(request.ticket);
        bridge.cancel();
      } catch {
        // The ticket has already been canceled or superseded.
      }
    }
  }

  commitPreparedCompositionSwitch(token, { beforeCommit = () => {} } = {}) {
    const request = this.#preparedCompositionSwitch;
    if (!request || request.token !== token) throw Error('STALE_OPERATION');
    const { bridge, ticket, targetId } = request;
    this.#assertCompositionSwitchCurrent(request);
    const assertClipboard = request.session.captureNodeClipboardFreshness?.();
    const assertCurrent = () => {
      if (this.#preparedCompositionSwitch !== request) throw Error('STALE_OPERATION');
      this.#assertCompositionSwitchCurrent(request);
      assertClipboard?.();
    };
    bridge.assertFresh(ticket);
    assertCurrent();
    const fonts = preparedCompositionFonts(this.#fonts, bridge, ticket);
    const nextSession = this.#makeSession(this.#history.doc, targetId, fonts.read, request.session);
    assertCurrent();
    bridge.assertFresh(ticket);
    assertCurrent();
    this.#assertQuiescent({ allowTextDrafts: true });
    assertCurrent();
    beforeCommit();
    assertCurrent();
    this.#assertQuiescent();
    assertCurrent();
    bridge.assertFresh(ticket);
    assertCurrent();
    // No fallible work follows activation; publish the bank and tab together.
    bridge.activate(ticket);
    fonts.useLive();
    this.#activeCompositionId = targetId;
    this.#session = nextSession;
    this.#preparedCompositionSwitch = null;
    return true;
  }

  dispatch(command) {
    if (command?.type === 'delete-composition') {
      const session = this.#session,
        activeId = this.#activeCompositionId,
        revision = this.#history.revision;
      const assertCurrent = () => {
        if (this.#session !== session || this.#activeCompositionId !== activeId)
          throw Error('STALE_SESSION: document changed');
        if (this.#history.revision !== revision) throw Error('STALE_REVISION: document changed');
      };
      this.#assertCurrentCommand(command);
      assertCurrent();
      const { compositionId, expectedName, expectedProjectRevision } = command.payload;
      assertCurrent();
      const target = this.#history.doc.compositions.find((entry) => entry.id === compositionId);
      if (expectedProjectRevision !== revision || !target || target.name !== expectedName)
        throw Error('STALE_REVISION: Composition changed');
      const changed = this.#deleteComposition(compositionId, assertCurrent);
      return {
        ...this.state,
        changed,
        deletedCompositionId: compositionId,
        projectRevision: this.#history.revision,
      };
    }
    if (command?.type === 'rename-composition') {
      const session = this.#session,
        activeId = this.#activeCompositionId,
        revision = this.#history.revision;
      const assertCurrent = () => {
        if (this.#session !== session || this.#activeCompositionId !== activeId)
          throw Error('STALE_SESSION: document changed');
        if (this.#history.revision !== revision) throw Error('STALE_REVISION: document changed');
      };
      this.#assertCurrentCommand(command);
      assertCurrent();
      const { compositionId, name, expectedName, expectedProjectRevision } = command.payload;
      const target = this.#history.doc.compositions.find((entry) => entry.id === compositionId);
      if (expectedProjectRevision !== revision || !target || target.name !== expectedName)
        throw Error('STALE_REVISION: Composition changed');
      const changed = this.#renameComposition(compositionId, name, assertCurrent);
      return { ...this.state, changed, projectRevision: this.#history.revision };
    }
    if (command?.type === 'delete-color-slot') {
      this.#assertCurrentCommand(command);
      this.#assertQuiescent();
      assertColorSlotUnused(this.#history.doc, command.payload?.slotId);
    }
    if (
      ['initialize-key-color-recipe', 'initialize-live-key-color-recipe'].includes(command?.type)
    ) {
      this.#assertCurrentCommand(command);
      this.#assertQuiescent();
      // Reject inactive-Composition ID collisions before inner Session commit.
      // The generic post-validation fallback rebuilds a Session; that is not an
      // acceptable failure path for this exact-envelope initialization boundary.
      const proposedIds = keyColorRecipeInitializationSlotIds(command.payload);
      if (Object.hasOwn(this.#history.doc, 'keyColorRecipe'))
        throw Error('Retained key-color recipe exists; initialization is create-only');
      assertFreshCompositionEntityIds(this.#history.doc, proposedIds);
    }
    if (command?.type === 'import-project-compositions') {
      const session = this.#session;
      const document = this.#history.doc;
      const revision = this.#history.revision;
      const sessionRevision = session.state.expectedRevision;
      const compositionId = this.#activeCompositionId;
      const assertCurrent = () => {
        const checkRevision = () => {
          if (this.#session !== session || this.#activeCompositionId !== compositionId)
            throw Error('STALE_SESSION: document changed');
          if (
            this.#history.doc !== document ||
            this.#history.revision !== revision ||
            session.state.expectedRevision !== sessionRevision
          )
            throw Error('STALE_REVISION: document changed');
        };
        checkRevision();
        this.#assertQuiescent();
        checkRevision();
      };
      this.#assertCurrentCommand(command);
      assertCurrent();
      const prepared = prepareProjectCompositionsImport(
        document,
        command.payload.sourceProject,
        command.payload.compositionIds,
        command.payload,
      );
      assertCurrent();
      const changed = this.#commitProject(
        prepared.project,
        prepared.compositionIds[0],
        'import-project-compositions',
        assertCurrent,
      );
      return {
        ...this.state,
        changed,
        importedCompositionIds: prepared.compositionIds,
        projectRevision: this.#history.revision,
      };
    }
    if (command?.type === 'undo' || command?.type === 'redo')
      return this.#stepHistory(command.type, command);
    const session = this.#session;
    const compositionId = this.#activeCompositionId;
    const revision = session.state.expectedRevision;
    const projectRevision = this.#history.revision;
    const artworkMove =
      command?.type === 'translate-artworks' ||
      (command?.type === 'begin-gesture' &&
        command.payload?.target?.kind === 'artwork-translate') ||
      (['update-gesture', 'commit-gesture'].includes(command?.type) &&
        session.gestureState?.kind === 'artwork-translate');
    const guardedTransfer =
      artworkMove ||
      this.#macroProjectAdmission !== null ||
      [
        'paste-nodes',
        'paste-layer-snapshot',
        'move-layers-to-folder',
        'order-layer-folder',
        'remove-layers-from-folder',
      ].includes(command?.type);
    const layerClipboardSnapshot =
      command?.type === 'paste-layer-snapshot'
        ? resolveLayerClipboardToken(command.payload?.token, this, compositionId)
        : null;
    const assertOwnerCurrent = () => {
      if (this.#session !== session || this.#activeCompositionId !== compositionId)
        throw Error('STALE_SESSION: document changed');
      if (this.#history.revision !== projectRevision)
        throw Error('STALE_REVISION: document changed');
      if (
        layerClipboardSnapshot &&
        resolveLayerClipboardToken(command.payload?.token, this, compositionId) !==
          layerClipboardSnapshot
      )
        throw Error('STALE_SESSION: copied Layer changed');
    };
    try {
      const result = session.dispatch(command);
      if (guardedTransfer) assertOwnerCurrent();
      if (session.state.expectedRevision !== revision) {
        const candidate = this.#projectWithDocument(
          this.#history.doc,
          compositionId,
          session.snapshot(),
        );
        if (guardedTransfer) assertOwnerCurrent();
        this.#applyProjectEdit((history) => {
          replaceRecord(history, candidate);
        });
        this.#session.setProjectContext?.(this.#history.doc, compositionId);
        this.#notifyCommitted('edit');
        return { ...result, projectRevision: this.#history.revision };
      }
      return { ...result, projectRevision: this.#history.revision };
    } catch (error) {
      // The core command has committed before the project adapter runs. If its
      // project-level validation rejects, replace that session from the
      // unchanged project snapshot so a failed adapter cannot leak an edit.
      if (
        session.state.expectedRevision !== revision &&
        (!guardedTransfer ||
          (this.#session === session &&
            this.#activeCompositionId === compositionId &&
            this.#history.revision === projectRevision))
      ) {
        this.#session = this.#makeSession(this.#history.doc, compositionId);
      }
      throw error;
    }
  }

  createComposition({ name, width, height } = {}) {
    this.#assertQuiescent();
    const current = this.#history.doc;
    const blank = this.#blankDocument();
    width ??= blank.width;
    height ??= blank.height;
    const document = {
      ...this.#documentFor(current, this.#activeCompositionId),
      ...blank,
      name: current.name,
      schemaVersion: current.schemaVersion,
      width,
      height,
    };
    const composition = {
      id: this.#uid('composition'),
      name: name ?? `Composition ${current.compositions.length + 1}`,
      width,
      height,
      layers: document.layers,
      ...(document.editor === undefined ? {} : { editor: document.editor }),
    };
    const candidate = {
      ...current,
      compositions: [...current.compositions, composition],
    };
    this.#commitProject(candidate, composition.id, 'create-composition');
    return composition.id;
  }

  renameComposition(compositionId, name) {
    return this.#renameComposition(compositionId, name);
  }

  #renameComposition(compositionId, name, precommit) {
    this.#assertQuiescent();
    const current = this.#history.doc;
    if (!current.compositions.some((entry) => entry.id === compositionId))
      throw Error('Unknown Composition ID');
    const candidate = {
      ...current,
      compositions: current.compositions.map((entry) =>
        entry.id === compositionId ? { ...entry, name } : entry,
      ),
    };
    return this.#commitProject(
      candidate,
      this.#activeCompositionId,
      'rename-composition',
      precommit,
    );
  }

  resizeComposition(compositionId, width, height) {
    this.#assertQuiescent();
    const current = this.#history.doc;
    if (!current.compositions.some((entry) => entry.id === compositionId))
      throw Error('Unknown Composition ID');
    const candidate = {
      ...current,
      compositions: current.compositions.map((entry) =>
        entry.id === compositionId ? { ...entry, width, height } : entry,
      ),
    };
    return this.#commitProject(candidate, this.#activeCompositionId, 'resize-composition');
  }

  deleteComposition(compositionId) {
    this.#deleteComposition(compositionId);
  }

  #deleteComposition(compositionId, precommit) {
    const session = this.#session,
      activeId = this.#activeCompositionId,
      revision = this.#history.revision;
    const assertReady = () => {
      const assertCurrent = () => {
        if (this.#session !== session || this.#activeCompositionId !== activeId)
          throw Error('STALE_SESSION: document changed');
        if (this.#history.revision !== revision) throw Error('STALE_REVISION: document changed');
      };
      assertCurrent();
      precommit?.();
      assertCurrent();
      this.#assertQuiescent();
      assertCurrent();
    };
    assertReady();
    const current = this.#history.doc;
    const index = current.compositions.findIndex((entry) => entry.id === compositionId);
    if (index < 0) throw Error('Unknown Composition ID');
    if (current.compositions.length === 1) throw Error('Cannot delete the last Composition');
    const compositions = current.compositions.filter((entry) => entry.id !== compositionId);
    const nextActiveId =
      this.#activeCompositionId === compositionId
        ? compositions[Math.min(index, compositions.length - 1)].id
        : this.#activeCompositionId;
    return this.#commitProject(
      { ...current, compositions },
      nextActiveId,
      'delete-composition',
      assertReady,
    );
  }
}
