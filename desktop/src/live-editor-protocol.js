// In-process boundary for the live editor. The transport must authenticate its
// caller before forwarding a request. This module defines operations, not I/O.
import {
  captureEditorTarget,
  readEditorInspection,
  validEditorTarget as validTarget,
} from './live-editor-target.js';
import {
  validArtworkAlignment,
  validArtworkDistribution,
  validArtworkGapDistribution,
  validArtworkTranslation,
} from './live-artwork-translation.js';
import { isSolidFillAlpha, isSolidFillHex } from './live-solid-fill-color.js';
import { validBlankLayerName } from './live-layer-create.js';

const object = (value) =>
  value !== null && typeof value === 'object' && Object.getPrototypeOf(value) === Object.prototype;
const closed = (value, keys) =>
  object(value) &&
  Object.keys(value).length === keys.length &&
  keys.every((key) => Object.hasOwn(value, key));
const id = (value) => typeof value === 'string' && value.length > 0 && value.length <= 256;
const version = (value) => Number.isSafeInteger(value) && value >= 0;
export function isEditorInspectRequest(request) {
  return (
    closed(request, ['version', 'operation', 'arguments']) &&
    request.version === 1 &&
    request.operation === 'editor.inspect' &&
    closed(request.arguments, [])
  );
}

export function createLiveEditorProtocol({
  readTarget,
  canInspect = () => true,
  listArtwork,
  readLayerState,
  listLayerFolders,
  createLayerFolder,
  renameLayerFolder,
  setLayerFolderMembership,
  dissolveLayerFolder,
  readLayerFolderPlan,
  applyLayerFolderPlan,
  readNodeInputWires,
  readNodePorts,
  setLayerVisibility,
  setLayerSolo,
  setLayerGuide,
  setLayerLock,
  renameLayer,
  reorderLayer,
  duplicateLayer,
  createBlankLayer,
  deleteLayer,
  setBlendDistance,
  setBlendSteps,
  setBlendMode,
  setBlendInput,
  setVectorWire,
  setScalarWire,
  readBlendSettings,
  stepHistory,
  createArtwork,
  setSolidFillColor,
  setSolidFillOpacity,
  readSolidFillColor,
  translateArtwork,
  alignArtwork,
  distributeArtwork,
  distributeArtworkGaps,
  setTextContent,
  readTextContent,
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
      return readEditorInspection(readTarget, canInspect);
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
    if (readLayerState && request.operation === 'layer.state') {
      if (!closed(args, ['target', 'layerId']) || !validTarget(args.target) || !id(args.layerId))
        return { status: 'invalid-arguments' };
      try {
        return readLayerState(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (listLayerFolders && request.operation === 'layerFolders.list') {
      if (!closed(args, ['target', 'offset']) || !validTarget(args.target) || !version(args.offset))
        return { status: 'invalid-arguments' };
      try {
        return listLayerFolders(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (createLayerFolder && request.operation === 'layerFolders.create') {
      const validIds = (ids) =>
        Array.isArray(ids) && ids.length <= 32 && ids.every(id) && new Set(ids).size === ids.length;
      if (
        !closed(args, ['target', 'name', 'layerIds', 'folderIds']) ||
        !validTarget(args.target) ||
        typeof args.name !== 'string' ||
        !args.name.trim() ||
        args.name.length > 100 ||
        !validIds(args.layerIds) ||
        !validIds(args.folderIds) ||
        new TextEncoder().encode(JSON.stringify(args)).byteLength > 3500
      )
        return { status: 'invalid-arguments' };
      try {
        const { status, folderId } = createLayerFolder(args);
        return status === 'changed'
          ? { status, folderId, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (renameLayerFolder && request.operation === 'layerFolders.rename') {
      const validName = (name) => typeof name === 'string' && name.length <= 100 && !!name.trim();
      if (
        !closed(args, ['target', 'folderId', 'expectedName', 'name']) ||
        !validTarget(args.target) ||
        !id(args.folderId) ||
        !validName(args.expectedName) ||
        !validName(args.name)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = renameLayerFolder(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (setLayerFolderMembership && request.operation === 'layerFolders.changeMembership') {
      if (
        !closed(args, ['target', 'folderId', 'mode', 'layerIds']) ||
        !validTarget(args.target) ||
        !id(args.folderId) ||
        !['add', 'remove'].includes(args.mode) ||
        !Array.isArray(args.layerIds) ||
        !args.layerIds.length ||
        args.layerIds.length > 32 ||
        !args.layerIds.every(id) ||
        new Set(args.layerIds).size !== args.layerIds.length ||
        new TextEncoder().encode(JSON.stringify(args)).byteLength > 3500
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = setLayerFolderMembership(args);
        return status === 'changed'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (dissolveLayerFolder && request.operation === 'layerFolders.dissolve') {
      if (!closed(args, ['target', 'folderId']) || !validTarget(args.target) || !id(args.folderId))
        return { status: 'invalid-arguments' };
      try {
        const { status } = dissolveLayerFolder(args);
        return status === 'changed'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (readLayerFolderPlan && request.operation === 'layerFolders.plan') {
      if (
        !closed(args, ['target', 'request']) ||
        !validTarget(args.target) ||
        !object(args.request)
      )
        return { status: 'invalid-arguments' };
      const plan = args.request;
      if (!id(plan.folderId)) return { status: 'invalid-arguments' };
      if (plan.mode === 'order') {
        if (
          !closed(plan, ['folderId', 'mode', 'direction']) ||
          !['forward', 'backward'].includes(plan.direction)
        )
          return { status: 'invalid-arguments' };
      } else if (
        !closed(plan, ['folderId', 'mode', 'layerIds']) ||
        !['move', 'remove'].includes(plan.mode) ||
        !Array.isArray(plan.layerIds) ||
        !plan.layerIds.length ||
        plan.layerIds.length > 32 ||
        !plan.layerIds.every(id) ||
        new Set(plan.layerIds).size !== plan.layerIds.length
      )
        return { status: 'invalid-arguments' };
      if (new TextEncoder().encode(JSON.stringify(args)).byteLength > 3500)
        return { status: 'invalid-arguments' };
      try {
        return readLayerFolderPlan(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (applyLayerFolderPlan && request.operation === 'layerFolders.applyPlan') {
      if (
        !closed(args, ['target', 'planId', 'confirmed']) ||
        !validTarget(args.target) ||
        typeof args.planId !== 'string' ||
        !/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(
          args.planId,
        ) ||
        args.confirmed !== true
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = applyLayerFolderPlan(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    const readGraph =
      request.operation === 'graph.inputWires'
        ? readNodeInputWires
        : request.operation === 'graph.ports'
          ? readNodePorts
          : null;
    if (readGraph) {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'offset']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !version(args.offset)
      )
        return { status: 'invalid-arguments' };
      try {
        return readGraph(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (createBlankLayer && request.operation === 'layer.createBlank') {
      if (
        !closed(args, ['target', 'name']) ||
        !validTarget(args.target) ||
        !validBlankLayerName(args.name)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status, layerId, nodeId } = createBlankLayer(args);
        return status === 'changed'
          ? { status, layerId, nodeId, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (deleteLayer && request.operation === 'layer.delete') {
      if (
        !closed(args, ['target', 'layerId', 'confirmed']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        args.confirmed !== true
      )
        return { status: 'invalid-arguments' };
      try {
        const { status, removedLayerId, nextLayerId, replacedLastLayer } = deleteLayer(args);
        return status === 'changed'
          ? {
              status,
              removedLayerId,
              nextLayerId,
              replacedLastLayer,
              target: captureEditorTarget(readTarget()),
            }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (duplicateLayer && request.operation === 'layer.duplicate') {
      if (!closed(args, ['target', 'layerId']) || !validTarget(args.target) || !id(args.layerId))
        return { status: 'invalid-arguments' };
      try {
        const { status, sourceLayerId, layerId } = duplicateLayer(args);
        return status === 'changed'
          ? { status, sourceLayerId, layerId, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (reorderLayer && request.operation === 'layer.reorder') {
      if (
        !closed(args, ['target', 'layerId', 'beforeLayerId']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        (args.beforeLayerId !== null && !id(args.beforeLayerId))
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = reorderLayer(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
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
          : request.operation === 'layer.setSolo' && setLayerSolo
            ? { set: setLayerSolo, key: 'solo' }
            : request.operation === 'layer.setGuide' && setLayerGuide
              ? { set: setLayerGuide, key: 'guide' }
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
    if (distributeArtworkGaps && request.operation === 'artwork.distributeGaps') {
      if (
        !closed(args, ['target', 'layerIds', 'action', 'spacing']) ||
        !validTarget(args.target) ||
        !validArtworkGapDistribution(args.layerIds, args.action, args.spacing)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = distributeArtworkGaps(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (distributeArtwork && request.operation === 'artwork.distribute') {
      if (
        !closed(args, ['target', 'layerIds', 'action']) ||
        !validTarget(args.target) ||
        !validArtworkDistribution(args.layerIds, args.action)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = distributeArtwork(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (alignArtwork && request.operation === 'artwork.align') {
      if (
        !closed(args, ['target', 'layerIds', 'action', 'reference']) ||
        !validTarget(args.target) ||
        !validArtworkAlignment(args.layerIds, args.action, args.reference)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = alignArtwork(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (translateArtwork && request.operation === 'artwork.translate') {
      if (
        !closed(args, ['target', 'layerIds', 'delta']) ||
        !validTarget(args.target) ||
        !validArtworkTranslation(args.layerIds, args.delta)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = translateArtwork(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (readSolidFillColor && request.operation === 'fill.readSolidColor') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId)
      )
        return { status: 'invalid-arguments' };
      try {
        return readSolidFillColor(args);
      } catch {
        return { status: 'unavailable' };
      }
    }
    if (setSolidFillColor && request.operation === 'fill.setSolidColor') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'hex']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !isSolidFillHex(args.hex)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = setSolidFillColor(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
      } catch {
        return { status: 'outcome-unknown' };
      }
    }
    if (setSolidFillOpacity && request.operation === 'fill.setOpacity') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId', 'alpha']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId) ||
        !isSolidFillAlpha(args.alpha)
      )
        return { status: 'invalid-arguments' };
      try {
        const { status } = setSolidFillOpacity(args);
        return status === 'changed' || status === 'unchanged'
          ? { status, target: captureEditorTarget(readTarget()) }
          : { status };
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
    if (readTextContent && request.operation === 'text.readContent') {
      if (
        !closed(args, ['target', 'layerId', 'nodeId']) ||
        !validTarget(args.target) ||
        !id(args.layerId) ||
        !id(args.nodeId)
      )
        return { status: 'invalid-arguments' };
      try {
        return readTextContent(args);
      } catch {
        return { status: 'unavailable' };
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
    const setInput =
      request.operation === 'blend.setInput'
        ? setBlendInput
        : request.operation === 'graph.setVectorWire'
          ? setVectorWire
          : request.operation === 'graph.setScalarWire'
            ? setScalarWire
            : null;
    if (setInput) {
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
        const { status } = setInput(args);
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
