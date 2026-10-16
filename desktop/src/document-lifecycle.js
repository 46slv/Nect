// DOM-free file-lifetime coordinator. This is not the editing command Session.
// `read` is the current authored snapshot; the staged UI command migration is separate.
export class DocumentLifecycle {
  constructor({ read, write, choose, hasDraft = () => false }) {
    Object.assign(this, { read, write, choose, hasDraft });
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
    this.generation++;
    this.revision = 0;
    this.observed = this.read();
    this.saved = saved ? this.observed : null;
    this.savedRevision = saved ? 0 : null;
    this.binding = binding;
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
      if (!result || generation !== this.generation || operation !== this.operation) return false;
      this.binding = result;
      this.saved = contents;
      this.savedRevision = revision;
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
