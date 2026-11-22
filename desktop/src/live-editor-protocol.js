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
export function isEditorInspectRequest(request) {
  return (
    closed(request, ['version', 'operation', 'arguments']) &&
    request.version === 1 &&
    request.operation === 'editor.inspect' &&
    closed(request.arguments, [])
  );
}

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
  canInspect = () => true,
  listArtwork,
  readNodeInputWires,
  setLayerVisibility,
  setLayerLock,
  renameLayer,
  setBlendDistance,
  setBlendSteps,
  setBlendMode,
  setBlendInput,
  readBlendSettings,
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
      try {
        if (!canInspect()) return { status: 'unavailable' };
        const target = captureEditorTarget(readTarget());
        return validTarget(target) ? { status: 'ok', target } : { status: 'unavailable' };
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (listArtwork && request.operation === 'artwork.list') {
      if (
        !closed(args, ['target', 'layerIndex', 'offset']) ||
        !validTarget(args.target) ||
        (args.layerIndex !== null && !version(args.layerIndex)) ||
        !version(args.offset)
      )
        return { status: 'invalid-arguments' };
      try {
        return listArtwork(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (readNodeInputWires && request.operation === 'graph.inputWires') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'offset']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !version(args.offset)
      )
        return { status: 'invalid-arguments' };
      try {
        return readNodeInputWires(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (renameLayer && request.operation === 'layer.rename') {
      if (
        !closed(args, ['target', 'layerId', 'name']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        typeof args.name !== 'string' ||
        args.name.length > 100000 ||
        !args.name.trim()
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = renameLayer(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    const layerFlag =
      request.operation === 'layer.setVisibility' && setLayerVisibility
        ? { set: setLayerVisibility, key: 'visible' }
        : request.operation === 'layer.setLock' && setLayerLock
          ? { set: setLayerLock, key: 'locked' }
          : null;
    if (layerFlag) {
      if (
        !closed(args, ['target', 'layerId', layerFlag.key]) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        typeof args[layerFlag.key] !== 'boolean'
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = layerFlag.set(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
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
    if (readBlendSettings && request.operation === 'blend.settings') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId)
      )
        return { status: 'invalid-arguments' };
      try {
        return readBlendSettings(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (setBlendMode && request.operation === 'blend.setMode') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'mode']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !id(args.mode)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = setBlendMode(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (setBlendInput && request.operation === 'blend.setInput') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'input', 'sourceId', 'sourcePort']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !id(args.input) ||
        !(args.sourceId === null || id(args.sourceId)) ||
        !(args.sourcePort === null || id(args.sourcePort))
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = setBlendInput(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    const setBlendSpacing =
      request.operation === 'blend.setDistance'
        ? setBlendDistance
        : request.operation === 'blend.setSteps'
          ? setBlendSteps
          : undefined;
    if (!setBlendSpacing) return { status: 'unsupported-operation' };
    if (
      !closed(args, ['target', 'layerId', 'nodeId', 'value']) ||
      !validTarget(args.target) ||
      !id(args.layerId) ||
      !id(args.nodeId) ||
      !Number.isFinite(args.value) ||
      (request.operation === 'blend.setSteps' &&
        (!Number.isInteger(args.value) || args.value < 0 || args.value > 254))
    )
      return { status: 'invalid-arguments' };

    try {
      const { status } = setBlendSpacing(args);
      if (status === 'changed' || status === 'unchanged')
        return { status, target: captureEditorTarget(readTarget()) };
      return { status };
    } catch {
      // A host error (e.g. after updating history) is not proof of nonexecution.
      return { status: 'outcome-unknown' };
    }
  };
}
