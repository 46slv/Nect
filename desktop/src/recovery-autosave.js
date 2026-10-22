// Persistence observes committed encodings; it never edits the command Session.
export class RecoveryAutosave {
  constructor({ begin, write, acknowledge = async () => {}, status = () => {} }) {
    Object.assign(this, { begin, write, acknowledge, status });
    this.current = null;
    this.queue = [];
    this.active = null;
    this.draining = null;
    this.failures = new Map();
    this.contextSequence = 0;
    this.state = { phase: 'ready', savedRevision: null };
  }
  report(context, state) {
    if (context === this.current)
      this.state = {
        savedRevision: context.savedRevision,
        requestedRevision: context.revision,
        savedAt: context.savedAt,
        ...state,
      };
    this.publish();
  }
  publish() {
    try {
      this.status(this.state);
    } catch {}
  }
  get failedDocuments() {
    return [...this.failures.values()].map((context) => ({
      id: context.retryId,
      generation: context.generation,
      name: context.name,
      failedRevision: context.failedRevision,
      requestedRevision: context.revision,
      savedRevision: context.savedRevision,
      error: context.error,
      phase: this.active === context ? 'saving' : context.pending ? 'pending' : 'failed',
    }));
  }
  observe({ revision = 0, ...snapshot }) {
    snapshot = { ...snapshot, revision };
    let context = this.current;
    // A delayed observation cannot replace a newer lifetime or committed revision.
    if (
      context &&
      (snapshot.generation < context.generation ||
        (snapshot.generation === context.generation && revision < context.revision))
    )
      return false;
    if (!context || context.generation !== snapshot.generation) {
      this.current = {
        ...snapshot,
        retryId: String(++this.contextSequence),
        failedRevision: null,
        pending: false,
        token: null,
        savedRevision: null,
        savedAt: null,
        error: null,
      };
      this.report(this.current, { phase: 'ready' });
      if (snapshot.generation > 0 && snapshot.dirty) this.request(this.current);
      return;
    }
    const changed = context.contents !== snapshot.contents || context.revision !== revision;
    const cleanChanged =
      context.dirty !== snapshot.dirty &&
      (context.token || context.pending || this.active === context);
    Object.assign(context, snapshot);
    if (changed || cleanChanged) this.request(context);
  }
  request(context = this.current) {
    if (!context) return;
    context.pending = true;
    if (!this.queue.includes(context)) this.queue.push(context);
    this.report(context, { phase: 'pending' });
    this.kick();
  }
  kick() {
    if (this.draining) return;
    this.draining = Promise.resolve()
      .then(() => this.drain())
      .finally(() => {
        this.draining = null;
        if (this.queue.length) this.kick();
      });
  }
  async drain() {
    while (this.queue.length) {
      const context = this.queue.shift();
      if (!context.pending) continue;
      context.pending = false;
      this.active = context;
      // One immutable in-flight snapshot, plus one latest waiting state per document.
      const snapshot = {
        contents: context.contents,
        name: context.name,
        recoverable: context.dirty,
        revision: context.revision,
      };
      this.report(context, { phase: 'saving', savingRevision: snapshot.revision });
      try {
        context.token ??= await this.begin();
        context.savedAt = await this.write({ id: context.token, ...snapshot });
        context.savedRevision = snapshot.revision;
        if (context.failedRevision === null || snapshot.revision >= context.failedRevision) {
          context.error = null;
          context.failedRevision = null;
          this.failures.delete(context.retryId);
        }
        if (context.sourceId) {
          await this.acknowledge(context.sourceId);
          context.sourceId = null;
        }
        this.report(context, { phase: context.pending ? 'pending' : 'saved' });
      } catch (error) {
        context.error = String(error.message || error);
        context.failedRevision = snapshot.revision;
        this.failures.set(context.retryId, context);
        this.report(context, {
          phase: 'failed',
          failedRevision: snapshot.revision,
          error: context.error,
        });
      } finally {
        this.active = null;
        this.publish();
      }
    }
  }
  async flush(context = this.current) {
    if (!context) return true;
    while (this.draining || context.pending) {
      if (!this.draining) this.kick();
      await this.draining;
    }
    return !context.error;
  }
  preserve(sourceId) {
    this.current.sourceId = sourceId;
    this.request();
    return this.flush();
  }
  async retry(id = this.current?.retryId, expectedFailedRevision) {
    const context = this.current?.retryId === id ? this.current : this.failures.get(id);
    if (!context) return false;
    if (
      expectedFailedRevision !== undefined &&
      (!this.failures.has(id) || context.failedRevision !== expectedFailedRevision)
    )
      return false;
    // Retry always uses the context's latest contents. Joining a newer/duplicate
    // request must not queue a second write of an older captured failure payload.
    if (!context.pending && this.active !== context) this.request(context);
    return this.flush(context);
  }
}
