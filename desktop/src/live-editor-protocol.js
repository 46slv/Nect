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

export function createLiveEditorProtocol({ readTarget, setBlendDistance, writeSvg, writePng, saveCopy }) {
  return function handle(request) {
    if (!closed(request, ['version', 'operation', 'arguments']))
      return { status: 'invalid-request' };
    if (request.version !== 1) return { status: 'unsupported-version' };

    const args = request.arguments;
    if (request.operation === 'editor.inspect') {
      if (!closed(args, [])) return { status: 'invalid-arguments' };
      return { status: 'ok', target: captureEditorTarget(readTarget()) };
    }
    const output =
      request.operation === 'artwork.exportSvg'
        ? writeSvg
        : request.operation === 'artwork.exportPng'
          ? writePng
          : request.operation === 'document.saveCopy'
            ? saveCopy
            : undefined;
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
