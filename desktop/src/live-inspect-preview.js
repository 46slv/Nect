import { Channel, invoke } from '@tauri-apps/api/core';
import { createLiveInspectChannel } from './live-inspect-channel.js';

export async function mountLiveInspectPreviewIfAvailable(options) {
  const call = options.call ?? invoke;
  const available = await call('live_preview_available', {});
  if (available) mountLiveInspectPreview({ ...options, fixturePeer: available.fixturePeer });
}

// Optional read-only diagnostic. A disposable host can bind one test client.
export function mountLiveInspectPreview({
  readTarget,
  canInspect,
  dom = document,
  host = window,
  call = invoke,
  makeChannel = () => new Channel(),
  fixturePeer,
}) {
  const pipe = createLiveInspectChannel({ readTarget, canInspect, pinDocument: true });
  const panel = dom.createElement('details');
  panel.dataset.liveInspectPreview = '';
  panel.style.cssText =
    'position:fixed;bottom:12px;right:12px;z-index:100;padding:8px;background:#222;color:#eee';
  const title = dom.createElement('summary');
  title.textContent = 'Local inspection preview';
  const status = dom.createElement('p');
  status.setAttribute('role', 'status');
  status.textContent = 'Off. Document inspection disabled.';
  let binding = null;
  let detach = null;
  let pending = 0;
  let candidate = null;
  let chooser = null;
  let selector = null;
  let peerOption = null;
  if (
    Number.isInteger(fixturePeer?.pid) &&
    fixturePeer.pid > 0 &&
    fixturePeer.pid <= 0xffffffff &&
    typeof fixturePeer.executable === 'string' &&
    fixturePeer.executable.length > 0
  ) {
    candidate = fixturePeer.pid;
    chooser = dom.createElement('label');
    chooser.textContent = 'Read-only connection: ';
    selector = dom.createElement('select');
    selector.dataset.liveInspectClient = '';
    const local = dom.createElement('option');
    local.value = 'in-app';
    local.textContent = 'In-app only';
    peerOption = dom.createElement('option');
    peerOption.value = 'test-client';
    peerOption.textContent = 'Test client: ' + fixturePeer.executable + ' (PID ' + candidate + ')';
    selector.append(local, peerOption);
    selector.value = 'in-app';
    chooser.append(selector);
  }
  const button = (text, action, canCancel = false) => {
    const el = dom.createElement('button');
    el.type = 'button';
    el.textContent = text;
    el.addEventListener('click', async (event) => {
      if (!event.isTrusted || (pending && !canCancel)) return;
      pending++;
      refresh();
      try {
        await action();
      } catch {
        status.textContent = 'Unavailable';
      } finally {
        pending--;
        refresh();
      }
    });
    return el;
  };
  async function stop() {
    const current = binding;
    binding = null;
    detach?.();
    detach = null;
    const epoch = await current?.catch(() => null);
    if (epoch) await call('live_preview_detach', { epoch });
  }
  const enable = button('Enable', async () => {
    const channel = makeChannel();
    detach = pipe.attach(channel, (id, reply) => {
      const current = binding;
      void current
        ?.then((epoch) => {
          if (epoch && binding === current)
            return call('live_preview_reply', { epoch, id, receipt: JSON.stringify(reply) });
        })
        .catch(() => {});
    });
    if (!detach) throw new Error('No active document or inspection already bound');
    const selectedTestClient = selector?.value === 'test-client' ? candidate : null;
    if (selectedTestClient) {
      // The native attempt may consume the fixture even if setup fails.
      candidate = null;
      peerOption.disabled = true;
      selector.value = 'in-app';
    }
    const current = call('live_preview_attach', { channel, selectedTestClient });
    binding = current;
    refresh();
    try {
      if (!(await current)) throw new Error('Inspection unavailable');
      if (binding === current)
        status.textContent = selectedTestClient
          ? 'On. Read-only document inspection enabled. One read offered to the selected test client.'
          : 'On. Read-only document inspection enabled.';
    } catch (error) {
      if (binding !== current) return;
      await stop().catch(() => {});
      throw error;
    }
  });
  const check = button('Check document', async () => {
    const current = binding;
    try {
      const epoch = await current;
      const ready = epoch && binding === current && (await call('live_preview_read', { epoch }));
      if (binding === current)
        status.textContent = ready ? 'Active document confirmed' : 'No ready document';
    } catch (error) {
      if (binding === current) throw error;
    }
  });
  const disable = button(
    'Disable',
    async () => {
      await stop();
      status.textContent = 'Off. Document inspection disabled.';
    },
    true,
  );
  function refresh() {
    enable.disabled = pending > 0 || !!binding;
    check.disabled = pending > 0 || !binding;
    disable.disabled = !binding;
    if (selector) selector.disabled = pending > 0 || !!binding || !candidate;
  }
  panel.append(title, enable, check, disable, status);
  if (chooser) panel.append(chooser);
  dom.body.append(panel);
  refresh();
  host.addEventListener(
    'pagehide',
    () => {
      void stop().catch(() => {});
    },
    { once: true },
  );
}
