// DOM-free file-lifetime coordinator. This is not the editing command Session.
// `read` is the current authored snapshot; the staged UI command migration is separate.
export class DocumentLifecycle {
  constructor({ read, write, choose, release = async () => {}, hasDraft = () => false }) {
    Object.assign(this, { read, write, choose, release, hasDraft });
    this.pendingReleases = new Map();
    this.releasing = null;
    this.cleanupError = null;
    this.generation = 0;
    this.operation = 0;
    this.revision = 0;
    this.savedRevision = null;
    this.binding = null;
    this.saved = null;
    this.observed = read();
    this.saving = false;
    this.transitioning = false;
  }
  observe() {
    const contents = this.read();
    if (contents !== this.observed) {
      this.revision++;
      this.observed = contents;
    }
    return contents;
  }
  get dirty() {
    return this.observe() !== this.saved;
  }
  replace({ binding = null, saved = false } = {}) {
    const previous = this.binding;
    this.generation++;
    this.revision = 0;
    this.observed = this.read();
    this.saved = saved ? this.observed : null;
    this.savedRevision = saved ? 0 : null;
    this.binding = binding;
    return this.releaseUnused(previous);
  }
  async releaseUnused(binding) {
    if (binding?.token && binding.token !== this.binding?.token)
      this.pendingReleases.set(binding.token, binding);
    if (this.releasing) await this.releasing;
    return this.flushReleases();
  }
  flushReleases() {
    if (this.releasing) return this.releasing;
    this.releasing = (async () => {
      for (const [token, binding] of this.pendingReleases) {
        if (token !== this.binding?.token) {
          try {
            await this.release(binding);
          } catch (error) {
            this.cleanupError = error;
            return false;
          }
        }
        this.pendingReleases.delete(token);
      }
      this.cleanupError = null;
      return true;
    })().finally(() => {
      this.releasing = null;
    });
    return this.releasing;
  }
  async save(saveAs = false) {
    if (this.saving) return false;
    if (this.hasDraft()) throw Error('Finish or cancel the active edit before saving.');
    const contents = this.observe(),
      generation = this.generation,
      revision = this.revision,
      operation = ++this.operation;
    this.saving = true;
    try {
      const result = await this.write({
        contents,
        binding: saveAs ? null : this.binding,
        operation,
      });
      if (!result) {
        await this.flushReleases();
        return false;
      }
      if (generation !== this.generation || operation !== this.operation) {
        await this.releaseUnused(result);
        return false;
      }
      const previous = this.binding;
      this.binding = result;
      this.saved = contents;
      this.savedRevision = revision;
      await this.releaseUnused(previous);
      return true;
    } finally {
      this.saving = false;
    }
  }
  async guard(action) {
    if (this.saving || this.transitioning) return false;
    if (this.hasDraft()) throw Error('Finish or cancel the active edit before changing documents.');
    this.transitioning = true;
    const generation = this.generation,
      before = this.observe();
    try {
      if (!this.dirty) return true;
      const choice = await this.choose(action);
      if (generation !== this.generation || before !== this.observe() || this.hasDraft())
        return false;
      if (choice === 'discard') return true;
      if (choice !== 'save') return false;
      return (
        (await this.save()) && !this.dirty && !this.hasDraft() && generation === this.generation
      );
    } finally {
      this.transitioning = false;
    }
  }
}
