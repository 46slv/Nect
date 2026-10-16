import { normalizeCharacterText, reconcileCharacterEdit } from './character-identity.js';

// Session-private staging. Adapters submit observed text/range intent, never
// authored character maps. Document History changes only when Session commits.
export class TextDraft {
  constructor({ layerId, nodeId, revision, text, record }) {
    Object.assign(this, { layerId, nodeId, revision });
    this.baseText = text;
    // Native textareas normalize line endings; retain authored bytes separately.
    this.baseRaw = text.replace(/\r\n?/g, '\n');
    this.baseRecord = JSON.stringify(record ?? null);
    this.raw = this.baseRaw;
    this.record = structuredClone(record);
    this.snapshots = [{ raw: this.raw, record: this.record }];
    this.cursor = 0;
    this.version = 0;
    this.composition = null;
    this.status = 'unchanged';
    this.reason = null;
  }
  get state() {
    return {
      draftRevision: this.version,
      text: this.raw,
      composing: !!this.composition,
      changed: this.raw !== this.baseRaw || JSON.stringify(this.record ?? null) !== this.baseRecord,
      status: this.status,
      reason: this.reason,
    };
  }
  assertCurrent(node, revision, version = this.version) {
    if (
      this.revision !== revision ||
      node?.text !== this.baseText ||
      JSON.stringify(node?.characterPositions ?? null) !== this.baseRecord
    )
      throw Error('STALE_TEXT_DRAFT: document changed');
    if (version !== this.version) throw Error('STALE_TEXT_DRAFT: input changed');
  }
  stage({
    expectedDraftRevision,
    action,
    previousRaw,
    nextRaw,
    selectionStart,
    selectionEnd,
    inputType,
    forceReplacement,
  }) {
    if (expectedDraftRevision !== this.version) throw Error('STALE_TEXT_DRAFT: input changed');
    if (
      (inputType !== undefined && typeof inputType !== 'string') ||
      (forceReplacement !== undefined && typeof forceReplacement !== 'boolean')
    )
      throw Error('Invalid text edit intent');
    if (action === 'composition-start') {
      if (this.composition || previousRaw !== this.raw) throw Error('Invalid composition start');
      if (
        !Number.isInteger(selectionStart) ||
        !Number.isInteger(selectionEnd) ||
        selectionStart < 0 ||
        selectionEnd < selectionStart ||
        selectionEnd > previousRaw.length
      )
        throw Error('Invalid composition selection');
      normalizeCharacterText(previousRaw);
      this.composition = { previousRaw, selectionStart, selectionEnd };
      this.version++;
      return this.state;
    }
    if (!['edit', 'composition-end'].includes(action)) throw Error('Invalid text draft action');
    if (action === 'composition-end' ? !this.composition : !!this.composition)
      throw Error('Finish or cancel text composition first');
    if (action === 'edit' && previousRaw !== this.raw)
      throw Error('STALE_TEXT_DRAFT: input changed');
    normalizeCharacterText(nextRaw);
    if (action === 'edit' && ['historyUndo', 'historyRedo'].includes(inputType)) {
      const index = this.cursor + (inputType === 'historyUndo' ? -1 : 1),
        snapshot = this.snapshots[index];
      if (snapshot?.raw === nextRaw) {
        this.cursor = index;
        this.raw = snapshot.raw;
        this.record = snapshot.record;
        this.version++;
        this.status = 'restored';
        this.reason = null;
        return this.state;
      }
    }
    const composing = action === 'composition-end',
      transaction = composing
        ? {
            ...this.composition,
            nextRaw,
            inputType: 'insertCompositionText',
            composition: true,
            forceReplacement,
          }
        : {
            previousRaw: this.raw,
            nextRaw,
            selectionStart,
            selectionEnd,
            inputType,
            forceReplacement,
          };
    const updated = reconcileCharacterEdit(this.record, transaction);
    this.raw = nextRaw;
    this.record = updated.record;
    this.status = updated.status;
    this.reason = updated.reason;
    this.composition = null;
    this.snapshots = this.snapshots.slice(0, this.cursor + 1);
    this.snapshots.push({ raw: this.raw, record: this.record });
    this.cursor++;
    if (this.snapshots.length > 100) {
      this.snapshots.shift();
      this.cursor--;
    }
    this.version++;
    return this.state;
  }
}
