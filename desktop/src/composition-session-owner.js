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
} from './composition-project-model.js';
import { keyColorRecipeInitializationSlotIds } from './key-color-recipe-document.js';
import { assertColorSlotUnused } from './color-slots.js';
import { prepareProjectCompositionsImport } from './composition-project-import.js';
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
// Only document-global identities can satisfy an Artwork reference. Character,
// contour and remap-edit IDs stay scoped to their owning node.
function documentEntityIds(document, ids = new Set()) {
  for (const slot of document.colorSlots || []) ids.add(slot.id);
  for (const preset of document.nodePresets || []) ids.add(preset.id);
  for (const guide of document.editor?.guides || []) ids.add(guide.id);
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
  #History;
  #CommandSession;
  #blankDocument;
  #cloneDocument;
  #sameDocument;
  #uid;
  #validateDocument;
  #bindProjectDocumentContext;
  #assertProjectReferenceTransition;
  #normalizedProjectSourceForDocument;
  #fonts;
  #busy;
  #onCommitted;
  #historyViewCache = new WeakMap();
  #historyView = null;
  #prepared = null;

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
      normalizedProjectSourceForDocument = null,
      activeCompositionId,
      fonts = () => ({}),
      busy = () => false,
      onCommitted,
    } = {},
  ) {
    if (
      typeof History !== 'function' ||
      typeof CommandSession !== 'function' ||
      typeof blankDocument !== 'function' ||
      typeof cloneDocument !== 'function' ||
      typeof sameDocument !== 'function' ||
      typeof uid !== 'function' ||
      typeof validateDocument !== 'function'
    )
      throw Error('Composition Session runtime adapters are required');
    validateCompositionProject(project);
    this.#History = History;
    this.#CommandSession = CommandSession;
    this.#blankDocument = blankDocument;
    this.#cloneDocument = cloneDocument;
    this.#sameDocument = sameDocument;
    this.#uid = uid;
    this.#validateDocument = validateDocument;
    this.#bindProjectDocumentContext = bindProjectDocumentContext;
    this.#assertProjectReferenceTransition = assertProjectReferenceTransition;
    this.#normalizedProjectSourceForDocument = normalizedProjectSourceForDocument;
    this.#assertProjectReferenceTransition(project, project, fonts());
    this.#history = new this.#History(project);
    this.#fonts = fonts;
    this.#busy = busy;
    this.#onCommitted = onCommitted;
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
    return prepared;
  }

  #makeSession(project, compositionId) {
    let session;
    session = new this.#CommandSession(this.#documentFor(project, compositionId), {
      fonts: this.#fonts,
      busy: this.#busy,
      retainHistory: false,
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
      beforeCandidateCommit: (before, candidate) => {
        assertProjectIdentityChangeSafe(this.#history.doc, compositionId, before, candidate);
        // Validate the whole projection before the inner Session commits;
        // a new identity or reference can affect an inactive Composition.
        const next = this.#projectWithDocument(this.#history.doc, compositionId, candidate);
        this.#assertProjectReferenceTransition(this.#history.doc, next, this.#fonts());
      },
    });
    return session;
  }

  #assertQuiescent() {
    if (
      this.#busy() ||
      this.#session.hasGesture ||
      this.#session.hasTextComposition ||
      this.#session.hasTextDrafts
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
    validateCompositionProject(candidate);
    this.#assertProjectReferenceTransition(this.#history.doc, candidate, this.#fonts());
    if (!candidate.compositions.some((entry) => entry.id === activeCompositionId))
      throw Error('Unknown active Composition ID');
    if (this.#sameDocument(this.#history.doc, candidate)) {
      precommit?.();
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
        : this.#makeSession(candidate, activeCompositionId);
    precommit?.();
    this.#applyProjectEdit((history) => {
      replaceRecord(history, candidate);
    });
    this.#activeCompositionId = activeCompositionId;
    this.#session = nextSession;
    this.#session.setProjectContext?.(this.#history.doc, activeCompositionId);
    this.#notifyCommitted(operation);
    return true;
  }

  #stepHistory(direction, command) {
    this.#assertCurrentCommand(command);
    this.#assertQuiescent();
    const candidate = (direction === 'undo' ? this.#history.past : this.#history.future).at(-1);
    if (!candidate)
      return { ...this.state, changed: false, projectRevision: this.#history.revision };
    validateCompositionProject(candidate);
    const activeCompositionId = candidate.compositions.some(
      (entry) => entry.id === this.#activeCompositionId,
    )
      ? this.#activeCompositionId
      : candidate.compositions[0].id;
    const nextSession = this.#makeSession(candidate, activeCompositionId);
    this.#history[direction]();
    this.#activeCompositionId = activeCompositionId;
    this.#session = nextSession;
    this.#session.setProjectContext?.(this.#history.doc, activeCompositionId);
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

  get gestureState() {
    return this.#session.gestureState;
  }

  activeDocument() {
    return this.#session.snapshot();
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
    this.#assertCurrentCommand(command);
    this.#assertQuiescent();
    if (command.type === 'delete-color-slot')
      assertColorSlotUnused(this.#history.doc, command.payload?.slotId);
    const session = this.#session,
      compositionId = this.#activeCompositionId,
      projectRevision = this.#history.revision;
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
      session.cancelPrepared(bridge);
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
      innerToken,
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
    if (prepared.ticket) {
      prepared.bridge.assertFresh(prepared.ticket);
      prepared.bridge.activate(prepared.ticket);
      this.#prepared = null;
      return this.#stepHistory(prepared.type, prepared.command);
    }

    const revision = prepared.session.state.expectedRevision;
    try {
      const result = prepared.session.commitPrepared(prepared.innerToken);
      if (prepared.session.state.expectedRevision !== revision) {
        const candidate = this.#projectWithDocument(
          this.#history.doc,
          prepared.compositionId,
          prepared.session.snapshot(),
        );
        this.#applyProjectEdit((history) => {
          replaceRecord(history, candidate);
        });
        this.#session.setProjectContext?.(this.#history.doc, prepared.compositionId);
        this.#notifyCommitted('edit');
      }
      this.#prepared = null;
      return { ...result, projectRevision: this.#history.revision };
    } catch (error) {
      if (prepared.session.state.expectedRevision !== revision) {
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

  switchComposition(compositionId) {
    if (!this.#history.doc.compositions.some((entry) => entry.id === compositionId))
      throw Error('Unknown active Composition ID');
    if (compositionId === this.#activeCompositionId) return false;
    this.#assertQuiescent();
    const nextSession = this.#makeSession(this.#history.doc, compositionId);
    this.#activeCompositionId = compositionId;
    this.#session = nextSession;
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
    const guardedTransfer = artworkMove || command?.type === 'paste-nodes';
    const assertOwnerCurrent = () => {
      if (this.#session !== session || this.#activeCompositionId !== compositionId)
        throw Error('STALE_SESSION: document changed');
      if (this.#history.revision !== projectRevision)
        throw Error('STALE_REVISION: document changed');
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
