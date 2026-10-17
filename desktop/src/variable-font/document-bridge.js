import { createVariablePreparer } from './prepare.js';
import { canonicalAxes, LIMITS, freeze, requireThat as check } from './contract.js';
import { hasCharacterAdjustments } from '../character-identity.js';

export const VARIABLE_KIND = 'variable-glyf-v1';
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
export function createVariableDocumentPreparer({ resolveFonts, validate, ...options } = {}) {
  const preparer = createVariablePreparer(options);
  const registered = new Map();
  let generation = 0,
    currentFonts = {},
    disposed = false,
    activeUsage = { runs: 0, commands: 0, serializedBytes: 0 };
  let pendingTicket = null;
  const clearTicket = () => {
    pendingTicket?.signal?.removeEventListener('abort', pendingTicket.abort);
    pendingTicket = null;
  };
  return {
    async prepareDocument(input, { signal } = {}) {
      check(typeof resolveFonts === 'function', 'VARIABLE_RESOLVER_REQUIRED');
      check(!disposed, 'DISPOSED');
      const revision = ++generation;
      clearTicket();
      preparer.setRevision(revision);
      check(!signal?.aborted, 'CANCELLED');
      const doc = structuredClone(input);
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
            generation === revision && !signal?.aborted,
            signal?.aborted ? 'CANCELLED' : 'STALE_RESULT',
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
          const run = await preparer.prepare(assetId, request, { signal });
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
        check(!signal?.aborted, 'CANCELLED');
        assertVariableDocumentReady(doc, fonts);
        const ticket = Object.freeze({});
        pendingTicket = {
          ticket,
          revision,
          signal,
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
        signal?.addEventListener('abort', pendingTicket.abort, { once: true });
        return ticket;
      } finally {
        if (!preparer.stats().pending) preparer.releaseWorker();
      }
    },
    fonts: () => ({ ...currentFonts }),
    assertFresh(ticket) {
      check(
        !disposed &&
          pendingTicket?.ticket === ticket &&
          pendingTicket.revision === generation &&
          !pendingTicket.signal?.aborted,
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
    activate(ticket) {
      this.assertFresh(ticket);
      currentFonts = pendingTicket.fonts;
      activeUsage = pendingTicket.usage;
      clearTicket();
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
