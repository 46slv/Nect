// Persistence observes committed encodings; it never edits the command Session.
export class RecoveryAutosave {
  constructor({ begin, write, acknowledge = async () => {}, status = () => {} }) {
    Object.assign(this, { begin, write, acknowledge, status });
    this.current = null;
    this.queue = [];
    this.active = null;
    this.draining = null;
    this.state = { phase: 'ready', savedRevision: null };
  }
  report(context, state) {
    if (context !== this.current) return;
    this.state = {
      savedRevision: context.savedRevision,
      requestedRevision: context.revision,
      savedAt: context.savedAt,
      ...state,
    };
    try {
      this.status(this.state);
    } catch {}
  }
  observe({ revision = 0, ...snapshot }) {
    snapshot = { ...snapshot, revision };
    let context = this.current;
    if (!context || context.generation !== snapshot.generation) {
      this.current = {
        ...snapshot,
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
        context.error = null;
        if (context.sourceId) {
          await this.acknowledge(context.sourceId);
          context.sourceId = null;
        }
        this.report(context, { phase: context.pending ? 'pending' : 'saved' });
      } catch (error) {
        context.error = String(error.message || error);
        this.report(context, {
          phase: 'failed',
          failedRevision: snapshot.revision,
          error: context.error,
        });
      } finally {
        this.active = null;
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
  retry() {
    this.request();
    return this.flush();
  }
}
