// File-lifetime protection only: observes committed encodings, never edits a Session.
export class RecoveryAutosave {
  constructor({
    begin,
    write,
    acknowledge = async () => {},
    status = () => {},
    delay = 1000,
    maxWait = 10000,
  }) {
    Object.assign(this, { begin, write, acknowledge, status, delay, maxWait });
    this.current = null;
    this.state = { phase: 'ready' };
  }
  report(context, state) {
    if (context !== this.current) return;
    this.state = state;
    this.status(state);
  }
  observe(snapshot) {
    let context = this.current;
    if (!context || context.generation !== snapshot.generation) {
      if (context?.pending) void this.flush(context);
      this.current = {
        ...snapshot,
        pending: false,
        token: null,
        running: null,
        timer: null,
        due: null,
      };
      this.report(this.current, { phase: 'ready' });
      // Imported/recovered unsaved documents also need protection before their
      // next edit. Initial launch artwork is only a baseline.
      if (snapshot.generation > 0 && snapshot.dirty) this.schedule(this.current);
      return;
    }
    const changed = context.contents !== snapshot.contents;
    const cleanChanged = context.dirty !== snapshot.dirty && !!context.token;
    Object.assign(context, snapshot);
    if (changed || cleanChanged) this.schedule(context);
  }
  schedule(context = this.current) {
    if (!context) return;
    context.pending = true;
    context.due ??= Date.now() + this.maxWait;
    clearTimeout(context.timer);
    context.timer = setTimeout(
      () => void this.flush(context),
      Math.max(0, Math.min(this.delay, context.due - Date.now())),
    );
    this.report(context, { phase: 'pending' });
  }
  preserve(sourceId) {
    this.current.sourceId = sourceId;
    this.schedule();
    return this.flush();
  }
  async flush(context = this.current) {
    if (!context) return true;
    if (context.running) {
      await context.running;
      return context.pending ? this.flush(context) : this.state.phase !== 'failed';
    }
    clearTimeout(context.timer);
    context.timer = null;
    if (!context.pending) return this.state.phase !== 'failed';
    context.pending = false;
    context.due = null;
    const snapshot = { contents: context.contents, name: context.name, recoverable: context.dirty };
    this.report(context, { phase: 'saving' });
    context.running = Promise.resolve().then(async () => {
      try {
        context.token ??= await this.begin();
        const savedAt = await this.write({ id: context.token, ...snapshot });
        // Acknowledge the old copy only after the recovered document has a durable successor.
        if (context.sourceId) {
          await this.acknowledge(context.sourceId);
          context.sourceId = null;
        }
        this.report(context, context.pending ? { phase: 'pending' } : { phase: 'saved', savedAt });
        return true;
      } catch (error) {
        this.report(context, { phase: 'failed', error: String(error.message || error) });
        return false;
      } finally {
        context.running = null;
      }
    });
    return context.running;
  }
  retry() {
    this.schedule();
    return this.flush();
  }
}
