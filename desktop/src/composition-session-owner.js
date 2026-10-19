import { validateCompositionProject } from './composition-project-model.js';
import { prepareProjectCompositionsImport } from './composition-project-import.js';

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

function projectWithDocument(project, compositionId, document) {
  const { width, height, layers, editor, ...shared } = document;
  const candidate = {
    ...shared,
    compositionSchemaVersion: project.compositionSchemaVersion,
    compositions: project.compositions.map((composition) => {
      if (composition.id !== compositionId) return composition;
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
    return documentFor(project, compositionId, this.#cloneDocument);
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
    return new this.#CommandSession(this.#documentFor(project, compositionId), {
      fonts: this.#fonts,
      busy: this.#busy,
      retainHistory: false,
    });
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

  #commitProject(candidate, activeCompositionId = this.#activeCompositionId, operation) {
    validateCompositionProject(candidate);
    if (!candidate.compositions.some((entry) => entry.id === activeCompositionId))
      throw Error('Unknown active Composition ID');
    if (this.#sameDocument(this.#history.doc, candidate)) return false;

    // Prepare/validate the replacement active Document before changing project
    // History. Invalid state leaves the old project and session untouched.
    const nextActiveDocument = this.#documentFor(candidate, activeCompositionId);
    const nextSession =
      activeCompositionId === this.#activeCompositionId &&
      this.#sameDocument(this.#session.snapshot(), nextActiveDocument)
        ? this.#session
        : new this.#CommandSession(nextActiveDocument, {
            fonts: this.#fonts,
            busy: this.#busy,
            retainHistory: false,
          });
    this.#applyProjectEdit((history) => {
      replaceRecord(history, candidate);
    });
    this.#activeCompositionId = activeCompositionId;
    this.#session = nextSession;
    this.#notifyCommitted(operation);
    return true;
  }

  #stepHistory(direction, command) {
    this.#assertCurrentCommand(command);
    this.#assertQuiescent();
    const candidate = (direction === 'undo' ? this.#history.past : this.#history.future).at(-1);
    if (!candidate) return { ...this.state, changed: false, projectRevision: this.#history.revision };
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
        const candidate = projectWithDocument(
          this.#history.doc,
          prepared.compositionId,
          prepared.session.snapshot(),
        );
        this.#applyProjectEdit((history) => {
          replaceRecord(history, candidate);
        });
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
    if (command?.type === 'import-project-compositions') {
      this.#assertCurrentCommand(command);
      this.#assertQuiescent();
      const prepared = prepareProjectCompositionsImport(
        this.#history.doc,
        command.payload.sourceProject,
        command.payload.compositionIds,
      );
      const changed = this.#commitProject(
        prepared.project,
        prepared.compositionIds[0],
        'import-project-compositions',
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
    try {
      const result = session.dispatch(command);
      if (session.state.expectedRevision !== revision) {
        const candidate = projectWithDocument(
          this.#history.doc,
          compositionId,
          session.snapshot(),
        );
        this.#applyProjectEdit((history) => {
          replaceRecord(history, candidate);
        });
        this.#notifyCommitted('edit');
        return { ...result, projectRevision: this.#history.revision };
      }
      return { ...result, projectRevision: this.#history.revision };
    } catch (error) {
      // The core command has committed before the project adapter runs. If its
      // project-level validation rejects, replace that session from the
      // unchanged project snapshot so a failed adapter cannot leak an edit.
      if (session.state.expectedRevision !== revision) {
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
    return this.#commitProject(candidate, this.#activeCompositionId, 'rename-composition');
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
    this.#assertQuiescent();
    const current = this.#history.doc;
    const index = current.compositions.findIndex((entry) => entry.id === compositionId);
    if (index < 0) throw Error('Unknown Composition ID');
    if (current.compositions.length === 1) throw Error('Cannot delete the last Composition');
    const compositions = current.compositions.filter((entry) => entry.id !== compositionId);
    const nextActiveId =
      this.#activeCompositionId === compositionId
        ? compositions[Math.min(index, compositions.length - 1)].id
        : this.#activeCompositionId;
    this.#commitProject(
      { ...current, compositions },
      nextActiveId,
      'delete-composition',
    );
  }
}
