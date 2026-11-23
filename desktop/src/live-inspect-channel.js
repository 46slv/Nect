import { createLiveInspectPublication } from './live-inspect-publication.js';

const unavailable = Object.freeze({ status: 'unavailable' });
const documentScope = ['documentGeneration', 'compositionId', 'sessionId'];

// The application supplies one private Tauri channel after explicit opt-in.
// This adapter never registers a global listener or enables a native session.
export function createLiveInspectChannel({ readTarget, canInspect, pinDocument = false }) {
  let scope = null;
  let lost = false;
  const lane = createLiveInspectPublication({
    canInspect,
    readTarget: () => {
      const target = readTarget();
      if (!scope) return target;
      if (!target || !documentScope.every((key) => target[key] === scope[key])) lost = true;
      return lost ? null : target;
    },
  });
  let detach = null;
  return {
    attach(channel, respond) {
      if (detach || !channel || typeof respond !== 'function') return null;
      if (pinDocument) {
        const target = readTarget();
        if (
          !target ||
          !Number.isSafeInteger(target.documentGeneration) ||
          target.documentGeneration < 0 ||
          typeof target.compositionId !== 'string' ||
          !target.compositionId ||
          typeof target.sessionId !== 'string' ||
          !target.sessionId
        )
          return null;
        scope = Object.freeze(Object.fromEntries(documentScope.map((key) => [key, target[key]])));
        lost = false;
      }
      const owner = Object.freeze({});
      lane.bind(owner);
      let pending = null;
      let sequence = 0;
      let active = true;
      channel.onmessage = (frame) => {
        if (!active || !frame || Object.keys(frame).length !== 2) return;
        const { phase, requestId } = frame;
        if (!Number.isSafeInteger(requestId) || requestId < 1) return;
        if (phase === 'prepare') {
          if (pending || requestId <= sequence) return;
          sequence = requestId;
          const ticket = lane.prepare(owner);
          pending = { requestId, ticket };
          return;
        }
        if (pending?.requestId !== requestId) return;
        if (phase === 'discard') {
          lane.revoke(owner);
          lane.bind(owner);
          pending = null;
          return;
        }
        if (phase !== 'publish') return;
        const { ticket } = pending;
        pending = null;
        respond(requestId, (ticket && lane.take(owner, ticket)) || unavailable);
      };
      detach = () => {
        active = false;
        lane.revoke(owner);
        pending = null;
        scope = null;
        lost = false;
        channel.onmessage = () => {};
        detach = null;
      };
      const close = detach;
      return () => {
        if (detach === close) close();
      };
    },
  };
}
