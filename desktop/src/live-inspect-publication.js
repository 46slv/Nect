import { createLiveEditorProtocol } from './live-editor-protocol.js';
import { editorTargetMatches } from './live-editor-target.js';

const inspect = Object.freeze({
  version: 1,
  operation: 'editor.inspect',
  arguments: Object.freeze({}),
});

// In-process, single-slot renderer boundary. The caller owns connection admission,
// transport cancellation and final write serialization. No wire or global access.
export function createLiveInspectPublication({ readTarget, canInspect }) {
  const handle = createLiveEditorProtocol({ readTarget, canInspect });
  let binding = null;
  let pending = null;

  return {
    bind(owner) {
      if (!owner || typeof owner !== 'object') throw new TypeError('Missing inspect owner');
      binding = owner;
      pending = null;
    },
    revoke(owner) {
      if (binding !== owner) return;
      binding = null;
      pending = null;
    },
    prepare(owner) {
      if (!binding || binding !== owner || pending) return null;
      const reply = handle(inspect);
      if (reply.status !== 'ok') return null;
      const ticket = Object.freeze({});
      pending = { ticket, reply };
      return ticket;
    },
    take(owner, ticket) {
      if (!binding || binding !== owner || !pending || pending.ticket !== ticket) return null;
      const prepared = pending.reply;
      pending = null;
      const current = handle(inspect);
      if (current.status !== 'ok' || !editorTargetMatches(current.target, prepared.target))
        return null;
      return prepared;
    },
  };
}
