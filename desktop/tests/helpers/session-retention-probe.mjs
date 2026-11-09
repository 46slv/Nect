// Isolated --expose-gc child fixture; invoked by prepared-session-retention.test.js.
// The repository's normal dependencies must already be available.
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

if (typeof global.gc !== 'function') throw new Error('Run Node with --expose-gc');
const root = process.argv[2];
if (!root) throw new Error('Supply the Nect repository root as the first argument');
const switchCount = Number(process.argv[3] ?? 30);
assert.ok(Number.isSafeInteger(switchCount) && switchCount > 0);
const sourceUrl = (name) => pathToFileURL(resolve(root, 'desktop', 'src', name)).href;
const core = await import(sourceUrl('core.js'));
const { CompositionSessionOwner } = await import(sourceUrl('composition-session-owner.js'));
const { migrateLegacyDocumentToCompositionProject } = await import(
  sourceUrl('composition-project-model.js')
);
const nextTurn = () => new Promise((resolveTurn) => setImmediate(resolveTurn));

async function probe(prepared) {
  const references = [];
  class TrackedSession extends core.CommandSession {
    constructor(...args) {
      super(...args);
      references.push(new WeakRef(this));
    }
  }
  const project = migrateLegacyDocumentToCompositionProject(core.blankDocument());
  project.compositions[0].id = 'a';
  project.compositions.push({
    ...migrateLegacyDocumentToCompositionProject(core.blankDocument()).compositions[0],
    id: 'b',
  });
  const owner = new CompositionSessionOwner(project, {
    ...core,
    CommandSession: TrackedSession,
    validateDocument: core.validate,
  });
  // This bridge retains neither requests nor Sessions.
  const bridge = {
    prepareDocument: async () => ({}),
    assertFresh() {},
    preparedFonts: () => ({}),
    activate() {},
    cancel() {},
  };
  for (let index = 0; index < switchCount; index++) {
    const target = index % 2 ? 'a' : 'b';
    if (prepared) {
      const token = await owner.prepareCompositionSwitch(target, bridge);
      owner.commitPreparedCompositionSwitch(token);
    } else {
      owner.switchComposition(target);
    }
  }
  // Never dereference before this loop or between collections. A successful
  // WeakRef.deref() keeps its target alive until the end of the current job.
  for (let index = 0; index < 8; index++) {
    await nextTurn();
    global.gc();
  }
  const alive = references.filter((reference) => reference.deref() !== undefined).length;
  // Access owner after collection so the current Session is intentionally live.
  return {
    prepared,
    created: references.length,
    alive,
    active: owner.activeCompositionId,
  };
}

const synchronous = await probe(false);
const prepared = await probe(true);
console.log(JSON.stringify({ node: process.version, switchCount, synchronous, prepared }, null, 2));
