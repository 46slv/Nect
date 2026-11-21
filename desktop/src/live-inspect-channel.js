import { createLiveInspectPublication } from './live-inspect-publication.js';

const unavailable = Object.freeze({ status: 'unavailable' });

// The application supplies one private Tauri channel after explicit opt-in.
// This adapter never registers a global listener or enables a native session.
export function createLiveInspectChannel({ readTarget, canInspect }) {
  const lane = createLiveInspectPublication({ readTarget, canInspect });
  let detach = null;
  return {
    attach(channel, respond) {
      if (detach || !channel || typeof respond !== 'function') return null;
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
