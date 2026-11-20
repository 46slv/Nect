import { createVariablePreparer } from './prepare.js';
import { canonicalAxes, LIMITS, freeze, requireThat as check } from './contract.js';
import { hasCharacterAdjustments } from '../character-identity.js';

export const VARIABLE_KIND = 'variable-glyf-v1';
// Public signals identify only controllers created here. The Worker and final
// commit use a separate private signal and flag that callers cannot replace.
const preparationSignals = new WeakMap();
const NativeAbortController = AbortController,
  nativeAddListener = EventTarget.prototype.addEventListener,
  nativeRemoveListener = EventTarget.prototype.removeEventListener,
  nativeAbortController = NativeAbortController.prototype.abort;

function preparationAborted(state) {
  for (let current = state; current; current = current.parent) if (current.aborted) return true;
  return false;
}

function makeVariablePreparationController(parent = null) {
  const visible = new NativeAbortController(),
    internal = new NativeAbortController(),
    state = { aborted: false, internal, parent };
  const privateSignal = internal.signal;
  Object.defineProperties(privateSignal, {
    aborted: { get: () => preparationAborted(state) },
    addEventListener: {
      value: (...args) => nativeAddListener.call(privateSignal, ...args),
    },
    removeEventListener: {
      value: (...args) => nativeRemoveListener.call(privateSignal, ...args),
    },
  });
  preparationSignals.set(visible.signal, state);
  return Object.freeze({
    signal: visible.signal,
    abort(reason) {
      if (state.aborted) return;
      state.aborted = true;
      try {
        nativeAbortController.call(internal, reason);
      } finally {
        nativeAbortController.call(visible, reason);
      }
    },
  });
}

export function createVariablePreparationController() {
  return makeVariablePreparationController();
}

function preparationState(signal) {
  if (signal == null) return null;
  const state = preparationSignals.get(signal);
  check(state, 'INVALID_ABORT_SIGNAL');
  return state;
}

export function captureVariablePreparationOptions(options = {}) {
  check(options && typeof options === 'object' && !Array.isArray(options), 'INVALID_ABORT_SIGNAL');
  const descriptor = Object.getOwnPropertyDescriptor(options, 'signal');
  check(descriptor || !('signal' in options), 'INVALID_ABORT_SIGNAL');
  check(
    !descriptor || (descriptor.enumerable && Object.hasOwn(descriptor, 'value')),
    'INVALID_ABORT_SIGNAL',
  );
  const signal = descriptor?.value,
    state = preparationState(signal);
  check(!preparationAborted(state), 'CANCELLED');
  return Object.freeze({ signal });
}

// A tab request owns its child; canceling it must not abort the caller or a
// separate request that happens to share the caller's parent signal.
export function createVariablePreparationChild(parentSignal) {
  const parent = preparationState(parentSignal),
    controller = makeVariablePreparationController(parent),
    relay = () => controller.abort();
  if (preparationAborted(parent)) controller.abort();
  else parent?.internal.signal.addEventListener('abort', relay, { once: true });
  return Object.freeze({
    controller,
    unlink() {
      parent?.internal.signal.removeEventListener('abort', relay);
    },
  });
}
export const VARIABLE_DOCUMENT_LIMITS = Object.freeze({
  runs: 8,
  commands: 100000,
  serializedBytes: 32000000,
  retainedBanks: 2,
  runtimeSerializedBytes: 96000000,
});
export function validateVariableText(node, asset, schemaVersion = 25) {
  const variable = asset?.kind === VARIABLE_KIND;
  if (!variable) {
    check(node.variation === undefined, 'VARIABLE_FONT_MISMATCH');
    return;
  }
  const v = node.variation;
  check(schemaVersion >= 25 && node.type === 'Text', 'VARIABLE_SCHEMA');
  check(
    v &&
      Object.getPrototypeOf(v) === Object.prototype &&
      v.axes &&
      v.version === 1 &&
      v.fontId === node.font &&
      Object.keys(v).every((k) => ['version', 'fontId', 'axes'].includes(k)),
    'VARIABLE_FONT_MISMATCH',
  );
  check((node.writingMode || 'horizontal-tb') === 'horizontal-tb', 'VARIABLE_VERTICAL_UNSUPPORTED');
  check(!node.textWrap, 'VARIABLE_WRAP_UNSUPPORTED');
  check(!node.pathAttachment, 'VARIABLE_PATH_UNSUPPORTED');
  check(!node.characterArrangement, 'VARIABLE_ARRANGEMENT_UNSUPPORTED');
  check(!hasCharacterAdjustments(node.characterPositions), 'VARIABLE_RANGE_UNSUPPORTED');
  check(node.kerning !== false, 'VARIABLE_FEATURES_UNSUPPORTED');
}
export function variableRequest(node, font) {
  check(font?.nectVariable, 'PREPARATION_REQUIRED');
  const { sparse } = canonicalAxes(font.nectVariable.axes, node.variation.axes);
  return { text: node.text, axes: sparse };
}
export function assertVariableDocumentReady(doc, fonts) {
  for (const layer of doc.layers)
    for (const node of layer.nodes) {
      const asset = doc.fontAssets?.[node.font];
      validateVariableText(node, asset, doc.schemaVersion);
      if (node.type === 'Text' && asset?.kind === VARIABLE_KIND) {
        const font = fonts[node.font];
        check(font?.nectVariable?.data === asset.data, 'PREPARATION_REQUIRED');
        font.readVariable(variableRequest(node, font));
      }
      for (const style of Object.values(node.characterPositions?.styles || {}))
        check(doc.fontAssets?.[style.font]?.kind !== VARIABLE_KIND, 'VARIABLE_RANGE_UNSUPPORTED');
    }
}
/** Document-scoped runtime only. Never serialize this object or prepared outlines. */
export function createVariableDocumentPreparer({
  resolveFonts,
  validate,
  cloneDocument = structuredClone,
  ...options
} = {}) {
  const preparer = createVariablePreparer(options);
  const registered = new Map();
  const preparationChecks = new WeakMap();
  let generation = 0,
    currentFonts = {},
    disposed = false,
    activeUsage = { runs: 0, commands: 0, serializedBytes: 0 };
  let pendingTicket = null;
  const detachTicket = (pending) => {
    if (!pending || pending.detachAttempted) return;
    // Never retry an externally supplied teardown after a throw or reentry.
    pending.detachAttempted = true;
    pending.signal?.removeEventListener('abort', pending.abort);
    pending.detached = true;
  };
  const clearTicket = () => {
    const pending = pendingTicket;
    try {
      detachTicket(pending);
    } finally {
      // A callback may have installed a newer ticket while detaching the old one.
      if (pendingTicket === pending) pendingTicket = null;
    }
  };
  return {
    preparationEpoch() {
      const token = Object.freeze({});
      preparationChecks.set(token, { generation, pending: pendingTicket });
      return token;
    },
    assertPreparationEpoch(epoch) {
      const captured = preparationChecks.get(epoch);
      check(
        !disposed &&
          captured &&
          generation === captured.generation &&
          pendingTicket === captured.pending,
        'STALE_RESULT',
      );
    },
    async prepareDocument(input, options = {}) {
      const beforeGeneration = generation,
        beforePending = pendingTicket,
        { signal } = captureVariablePreparationOptions(options),
        signalState = preparationState(signal),
        workerSignal = signalState?.internal.signal;
      check(generation === beforeGeneration && pendingTicket === beforePending, 'STALE_RESULT');
      check(typeof resolveFonts === 'function', 'VARIABLE_RESOLVER_REQUIRED');
      check(!disposed, 'DISPOSED');
      const revision = ++generation;
      clearTicket();
      preparer.setRevision(revision);
      check(!preparationAborted(signalState), 'CANCELLED');
      const doc = cloneDocument(input);
      validate(doc);
      const fonts = resolveFonts(doc);
      const requests = new Map(),
        banks = new Map();
      for (const [id, asset] of Object.entries(doc.fontAssets || {})) {
        if (asset.kind !== VARIABLE_KIND) continue;
        let record = registered.get(asset.data);
        if (!record) {
          const binary = atob(asset.data);
          const bytes = Uint8Array.from(binary, (c) => c.charCodeAt(0));
          const assetId = await preparer.register(bytes);
          check(
            generation === revision && !preparationAborted(signalState),
            preparationAborted(signalState) ? 'CANCELLED' : 'STALE_RESULT',
          );
          record = { assetId, metadata: fonts[id].nectVariable };
          registered.set(asset.data, record);
        }
        const bank = new Map();
        banks.set(id, bank);
        fonts[id] = Object.freeze({
          nectVariable: record.metadata,
          readVariable(request) {
            const sparse = canonicalAxes(record.metadata.axes, request.axes).sparse;
            const result = bank.get(JSON.stringify([request.text, sparse]));
            check(result, 'PREPARATION_REQUIRED');
            return result;
          },
        });
      }
      for (const layer of doc.layers)
        for (const node of layer.nodes) {
          validateVariableText(node, doc.fontAssets?.[node.font], doc.schemaVersion);
          if (node.type !== 'Text' || !fonts[node.font]?.nectVariable) continue;
          const request = variableRequest(node, fonts[node.font]);
          const assetId = registered.get(doc.fontAssets[node.font].data).assetId;
          const key = JSON.stringify([assetId, request]);
          if (!requests.has(key)) requests.set(key, { assetId, request, ids: new Set() });
          requests.get(key).ids.add(node.font);
        }
      check(requests.size <= LIMITS.runs, 'VARIABLE_DOCUMENT_RUN_LIMIT');
      let commands = 0,
        serializedBytes = 0;
      try {
        for (const { assetId, request, ids } of requests.values()) {
          const run = await preparer.prepare(assetId, request, { signal: workerSignal });
          check(generation === revision, 'STALE_RESULT');
          commands += run.commands.length;
          serializedBytes += new TextEncoder().encode(JSON.stringify(run)).byteLength;
          check(
            serializedBytes <= VARIABLE_DOCUMENT_LIMITS.serializedBytes,
            'VARIABLE_DOCUMENT_BYTE_LIMIT',
          );
          check(commands <= LIMITS.cachedCommands, 'VARIABLE_DOCUMENT_COMMAND_LIMIT');
          for (const id of ids)
            banks.get(id).set(JSON.stringify([request.text, request.axes]), run);
        }
        check(!preparationAborted(signalState), 'CANCELLED');
        assertVariableDocumentReady(doc, fonts);
        const ticket = Object.freeze({});
        pendingTicket = {
          ticket,
          revision,
          signal: workerSignal,
          signalState,
          usage: { runs: requests.size, commands, serializedBytes },
          document: freeze(doc),
          fonts: Object.freeze(fonts),
        };
        pendingTicket.abort = () => {
          if (pendingTicket?.ticket === ticket) {
            clearTicket();
            preparer.setRevision(++generation);
          }
        };
        workerSignal?.addEventListener('abort', pendingTicket.abort, { once: true });
        return ticket;
      } finally {
        if (!preparer.stats().pending) preparer.releaseWorker();
      }
    },
    fonts: () => ({ ...currentFonts }),
    assertFresh(ticket) {
      const pending = pendingTicket;
      check(
        !disposed && pending?.ticket === ticket && pending.revision === generation,
        'STALE_RESULT',
      );
      check(
        !preparationAborted(pending.signalState) &&
          !disposed &&
          pendingTicket === pending &&
          pending.revision === generation,
        'STALE_RESULT',
      );
    },
    assertFreshForPublication(ticket) {
      // This and publishActivation form the trusted, callback-free bridge
      // boundary. Only the module-private cancellation flag is read here.
      const pending = pendingTicket;
      check(
        !disposed &&
          pending?.ticket === ticket &&
          pending.revision === generation &&
          pending.detached,
        'STALE_RESULT',
      );
      check(
        !preparationAborted(pending.signalState) &&
          !disposed &&
          pendingTicket === pending &&
          pending.revision === generation,
        'STALE_RESULT',
      );
    },
    preparedDocument(ticket) {
      this.assertFresh(ticket);
      return pendingTicket.document;
    },
    preparedFonts(ticket) {
      this.assertFresh(ticket);
      return pendingTicket.fonts;
    },
    stageActivation(ticket) {
      this.assertFresh(ticket);
      detachTicket(pendingTicket);
      this.assertFresh(ticket);
    },
    publishActivation(ticket) {
      const pending = pendingTicket;
      check(
        !disposed &&
          pending?.ticket === ticket &&
          pending.revision === generation &&
          pending.detached,
        'STALE_RESULT',
      );
      // This publication has no caller-provided callback or AbortSignal access.
      currentFonts = pending.fonts;
      activeUsage = pending.usage;
      pendingTicket = null;
    },
    discardActivation(ticket) {
      const pending = pendingTicket;
      if (!pending || pending.ticket !== ticket) return false;
      try {
        detachTicket(pending);
      } catch {
        // The listener is now inert once its candidate is discarded. Do not
        // retry a teardown callback which has already thrown.
      }
      if (pendingTicket === pending) pendingTicket = null;
      return true;
    },
    activate(ticket) {
      try {
        this.stageActivation(ticket);
        this.assertFreshForPublication(ticket);
        this.publishActivation(ticket);
      } catch (error) {
        this.discardActivation(ticket);
        throw error;
      }
    },
    cancel() {
      clearTicket();
      preparer.setRevision(++generation);
    },
    dispose() {
      disposed = true;
      clearTicket();
      generation++;
      currentFonts = {};
      activeUsage = { runs: 0, commands: 0, serializedBytes: 0 };
      registered.clear();
      preparer.dispose();
    },
    stats: () => ({
      ...preparer.stats(),
      active: { ...activeUsage },
      candidate: pendingTicket ? { ...pendingTicket.usage } : null,
    }),
  };
}
