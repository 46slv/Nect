import { resolveArtworkMoveTarget } from './artwork-move-target.js';
import { validateMacroDocumentCandidate } from './composition-project-model.js';
import {
  hasExternalArtworkMoveConsumer,
  outputType,
  parameter,
  socketCompatibleFor,
} from './core.js';
import { MACRO_SCHEMA_VERSION } from './curve-point-reference.js';
import { snapshotNodeDeclaration } from './node-foundation.js';

const unsupported = (code, reason) => ({ supported: false, code, reason });

// Read-only first insertion capsule. The caller must capture and recheck its
// Project/Session revision and this complete edge before committing anything.
export function planMacroInsertion(document, layerId, pin, options = {}) {
  let selected;
  try {
    selected = snapshotNodeDeclaration(pin);
    if (
      !selected ||
      Object.keys(selected).length !== 2 ||
      typeof selected.definitionId !== 'string' ||
      !Number.isSafeInteger(selected.revision) ||
      selected.revision < 1
    )
      throw Error('Invalid Macro pin');
  } catch {
    return unsupported('invalid-definition', 'Choose a registered Macro definition.');
  }
  try {
    const version = Object.getOwnPropertyDescriptor(document, 'schemaVersion');
    if (
      !version?.enumerable ||
      !Object.hasOwn(version, 'value') ||
      version.value !== MACRO_SCHEMA_VERSION
    )
      throw Error('Macro is not registered');
    validateMacroDocumentCandidate(document);
    if (
      !document.macroDefinitions?.some(
        (item) => item.id === selected.definitionId && item.revision === selected.revision,
      )
    )
      throw Error('Macro definition is missing');
  } catch {
    return unsupported('missing-definition', 'The selected Macro definition is unavailable.');
  }

  const move = resolveArtworkMoveTarget(document, layerId, options);
  if (!move.supported) return move;
  const route = move.target.route,
    layer = document.layers.find((item) => item.id === layerId);
  if (
    !layer ||
    route.length !== 4 ||
    route[0].type !== 'Rectangle' ||
    route[1].type !== 'Transform' ||
    route[2].type !== 'Fill' ||
    route[3].type !== 'Output' ||
    move.target.kind !== 'terminal-transform-offset' ||
    move.target.nodeId !== route[1].nodeId ||
    move.target.outputNodeId !== layer.output
  )
    return unsupported('unsupported-route', 'Apply this Macro to a simple filled Rectangle.');
  if (layer.nodes.length >= 2000)
    return unsupported('node-limit', 'This artwork already has the maximum number of nodes.');
  const byId = new Map(layer.nodes.map((item) => [item.id, item])),
    [source, transform, fill, output] = route.map((item) => byId.get(item.nodeId)),
    path = parameter(transform, 'path'),
    fillPath = parameter(fill, 'path'),
    outputPath = parameter(output, 'path');
  if (
    !source ||
    !path ||
    !fillPath ||
    !outputPath ||
    path.locked !== false ||
    path.type !== 'Path' ||
    path.source !== source.id ||
    fillPath.source !== transform.id ||
    outputPath.source !== fill.id ||
    [path, fillPath, outputPath].some((input) => input.sourcePort !== undefined) ||
    outputType(source) !== 'Path' ||
    !socketCompatibleFor(source, transform, 'path') ||
    !socketCompatibleFor(transform, fill, 'path') ||
    !socketCompatibleFor(fill, output, 'path')
  )
    return unsupported('unavailable-edge', 'The artwork path connection cannot be changed.');

  if (
    [source, transform, fill, output].some((item) =>
      Object.entries(item.params).some(
        ([key, input]) =>
          key !== 'path' &&
          (input.expression || input.source !== undefined || input.sourcePort !== undefined),
      ),
    )
  )
    return unsupported('driven-route', 'The artwork has a driven input.');
  const allowed = [
    { source: source.id, nodeId: transform.id, key: 'path' },
    { source: transform.id, nodeId: fill.id, key: 'path' },
    { source: fill.id, nodeId: output.id, key: 'path' },
  ];
  if (
    hasExternalArtworkMoveConsumer(
      document,
      layerId,
      route.map((item) => item.nodeId),
      allowed,
    )
  )
    return unsupported('shared-route', 'The artwork path is used elsewhere.');

  return {
    supported: true,
    target: {
      layerId,
      outputNodeId: output.id,
      consumerNodeId: transform.id,
      inputKey: 'path',
      producerNodeId: source.id,
      producerPort: null,
      definitionId: selected.definitionId,
      definitionRevision: selected.revision,
    },
  };
}

export function sameMacroInsertionTarget(a, b) {
  const keys = [
    'layerId',
    'outputNodeId',
    'consumerNodeId',
    'inputKey',
    'producerNodeId',
    'producerPort',
    'definitionId',
    'definitionRevision',
  ];
  return (
    !!a &&
    !!b &&
    keys.every((key) => Object.hasOwn(a, key) && Object.hasOwn(b, key) && a[key] === b[key]) &&
    a.inputKey === 'path' &&
    a.producerPort === null
  );
}
