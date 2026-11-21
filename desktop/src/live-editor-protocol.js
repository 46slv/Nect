// In-process boundary for the live editor. The transport must authenticate its
// caller before forwarding a request. This module defines operations, not I/O.
import { captureEditorTarget } from './live-editor-target.js';

const object = (value) =>
  value !== null && typeof value === 'object' && Object.getPrototypeOf(value) === Object.prototype;
const closed = (value, keys) =>
  object(value) &&
  Object.keys(value).length === keys.length &&
  keys.every((key) => Object.hasOwn(value, key));
const id = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;
const version = (value) => Number.isSafeInteger(value) && value >= 0;
const targetKeys = [
  'documentGeneration',
  'compositionId',
  'sessionId',
  'sessionRevision',
  'projectRevision',
];
function validTarget(target) {
  return (
    closed(target, targetKeys) &&
    version(target.documentGeneration) &&
    id(target.compositionId) &&
    id(target.sessionId) &&
    version(target.sessionRevision) &&
    version(target.projectRevision)
  );
}

export function createLiveEditorProtocol({
  readTarget,
  setBlendDistance,
  stepHistory,
  createArtwork,
  setTextContent,
  writeSvg,
  writePng,
  saveCopy,
  reopenCopy,
  saveAsBoundary,
}) {
  return function handle(request) {
    if (!closed(request, ['version', 'operation', 'arguments']))
      return { status: 'invalid-request' };
    if (request.version !== 1) return { status: 'unsupported-version' };

    const args = request.arguments;
    if (request.operation === 'editor.inspect') {
      if (!closed(args, [])) return { status: 'invalid-arguments' };
      return { status: 'ok', target: captureEditorTarget(readTarget()) };
    }
    if (
      (request.operation === 'history.undo' || request.operation === 'history.redo') &&
      stepHistory
    ) {
      if (!closed(args, ['target']) || !validTarget(args.target))
        return { status: 'invalid-arguments' };
      try {
        const { status } = stepHistory(args.target, request.operation === 'history.redo');
        if (status === 'changed' || status === 'unchanged')
          return { status, target: captureEditorTarget(readTarget()) };
        return { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (createArtwork && request.operation === 'artwork.create') {
      if (!closed(args, ['target', 'artworkType', 'at']) || !validTarget(args.target))
        return { status: 'invalid-arguments' };
      try {
        const { status, layerId, nodeId } = createArtwork(args);
        if (status !== 'changed') return { status };
        return { status, layerId, nodeId, target: captureEditorTarget(readTarget()) };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (setTextContent && request.operation === 'text.setContent') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'text']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        typeof args.text !== 'string' ||
        args.text.length > 1024
      )
        return { status: 'invalid-arguments' };
      return Promise.resolve()
        .then(() => setTextContent(args))
        .then(({ status }) =>
          status === 'changed' || status === 'unchanged'
            ? { status, target: captureEditorTarget(readTarget()) }
            : { status },
        )
        .catch(() => ({ status: 'outcome-unknown' }));
    }
    if (saveAsBoundary && request.operation === 'document.saveState') {
      if (!closed(args, [])) return { status: 'invalid-arguments' };
      const state = saveAsBoundary.inspect();
      return {
        status: 'ok',
        saveEpoch: state.saveEpoch,
        dirty: state.dirty,
        pending: state.pending,
      };
    }
    if (saveAsBoundary && request.operation === 'document.saveReceipt') {
      if (!closed(args, ['requestId']) || !id(args.requestId))
        return { status: 'invalid-arguments' };
      const { status, saveEpoch } = saveAsBoundary.getReceipt(args.requestId);
      return version(saveEpoch) ? { status, saveEpoch } : { status };
    }
    if (saveAsBoundary && request.operation === 'document.saveAs') {
      if (
        !closed(args, ['target', 'saveEpoch', 'requestId']) ||
        !validTarget(args.target) ||
        !version(args.saveEpoch) ||
        !id(args.requestId)
      )
        return { status: 'invalid-arguments' };
      return Promise.resolve()
        .then(() => saveAsBoundary.run(args))
        .then(({ status, saveEpoch }) => (version(saveEpoch) ? { status, saveEpoch } : { status }))
        .catch(() => ({ status: 'outcome-unknown' }));
    }
    let output;
    switch (request.operation) {
      case 'artwork.exportSvg':
        output = writeSvg;
        break;
      case 'artwork.exportPng':
        output = writePng;
        break;
      case 'document.saveCopy':
        output = saveCopy;
        break;
      case 'document.reopenCopy':
        output = reopenCopy;
        break;
    }
    if (output) {
      if (!closed(args, ['target']) || !validTarget(args.target))
        return { status: 'invalid-arguments' };
      return Promise.resolve()
        .then(() => output(args.target))
        .then(({ status }) => ({ status }))
        .catch(() => ({ status: 'outcome-unknown' }));
    }
    if (request.operation !== 'blend.setDistance') return { status: 'unsupported-operation' };
    if (
      !closed(args, ['target', 'layerId', 'nodeId', 'value']) ||
      !validTarget(args.target) ||
      !id(args.layerId) ||
      !id(args.nodeId) ||
      !Number.isFinite(args.value)
    )
      return { status: 'invalid-arguments' };

    try {
      const { status } = setBlendDistance(args);
      if (status === 'changed' || status === 'unchanged')
        return { status, target: captureEditorTarget(readTarget()) };
      return { status };
    } catch {
      // A host error (e.g. after updating history) is not proof of nonexecution.
      return { status: 'outcome-unknown' };
    }
  };
}
