import {
  bypassContract,
  characterPositionStatus,
  evaluate,
  inheritProjectDocumentContext,
  parameters,
  remapFingerprintMatches,
  uid,
  validate,
} from './core.js';
import {
  artworkReferencesInLayers,
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  nodesHaveComponentCompositionReferenceSyntax,
  nodesHaveQualifiedColorReferenceSyntax,
  nodesHaveCompositionPropertyReferenceSyntax,
  danglingExpressionIds,
  remapExpressionReferences,
  remapArtworkExpressionReferences,
  reserveDanglingArtworkReferenceIds,
} from './curve-point-reference.js';
import { candidateDocument } from './artwork-candidate.js';
import { isPaintedGroup } from './composition-reference.js';
import { resolveArtworkMoveTarget } from './artwork-move-target.js';
import { insertDuplicatedLayer, layerFolders } from './layer-folders.js';

const unsupported = (code, reason) => ({ supported: false, code, reason });
function hasExternalSource(value, nodeIds) {
  if (!value || typeof value !== 'object') return false;
  if (value.source !== undefined && !nodeIds.has(value.source)) return true;
  return Object.values(value).some((child) => hasExternalSource(child, nodeIds));
}

/**
 * Whole-layer eligibility is deliberately narrower than artwork movement:
 * every node must belong to its complete supported route. An optional already
 * resolved move is for the current synchronous UI render only; staging always
 * resolves afresh. No document, asset record or evaluation cache is mutated.
 */
export function resolveArtworkDuplicateTarget(doc, layerId, options = {}, movement) {
  try {
    if (!doc || !Array.isArray(doc.layers) || doc.layers.length < 1)
      return unsupported('invalid-document', 'Choose artwork in a valid document.');
    if (doc.layers.length >= 100)
      return unsupported('layer-limit', 'Duplicate in place requires fewer than 100 layers.');
    if (doc.schemaVersion === 1)
      return unsupported(
        'legacy-document',
        'Upgrade the legacy project before duplicating artwork.',
      );
    const move = movement || resolveArtworkMoveTarget(doc, layerId, options);
    if (!move.supported)
      return unsupported(
        move.code,
        `Duplicate in place uses artwork-move eligibility. ${move.reason}`,
      );
    const layer = doc.layers.find((layer) => layer.id === layerId),
      route = new Set(move.target.route.map(({ nodeId }) => nodeId));
    const selected = new Set(options.selectedLayerIds || [layerId]);
    if (
      doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
      layer &&
      nodesHaveComponentCompositionReferenceSyntax(layer.nodes)
    )
      return unsupported(
        'component-composition-reference',
        'Duplicating artwork with Component Composition references is not supported.',
      );
    if (
      doc.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
      layer &&
      nodesHaveQualifiedColorReferenceSyntax(layer.nodes)
    )
      return unsupported(
        'qualified-color-reference',
        'Duplicating artwork with qualified Color references is not supported.',
      );
    if (
      doc.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
      layer &&
      nodesHaveCompositionPropertyReferenceSyntax(layer.nodes)
    )
      return unsupported(
        'composition-property-reference',
        'Duplicating artwork with Composition property references is not supported.',
      );
    if (
      doc.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION &&
      layer &&
      artworkReferencesInLayers([layer]).some((ref) => !selected.has(ref.layerId))
    )
      return unsupported('external-source', 'Select every Artwork referenced by this duplicate.');
    if (!layer || route.size !== layer.nodes.length || layer.nodes.some((n) => !route.has(n.id)))
      return unsupported(
        'extra-nodes',
        'Duplicate in place initially requires a complete supported route with no disconnected or extra nodes.',
      );
    if (hasExternalSource(layer, route))
      return unsupported(
        'external-source',
        'An artwork source points outside the supported copy route.',
      );
    for (const n of layer.nodes) {
      if (
        !Array.isArray(n.points) ||
        !Array.isArray(n.edits) ||
        n.edits.length ||
        (!['Curve', 'Multi Path'].includes(n.type) && n.points.length) ||
        n.reference ||
        n.pathAttachment
      )
        return unsupported(
          'unsupported-records',
          'This artwork contains unsupported graph records.',
        );
      if (n.type === 'Text' && characterPositionStatus(n.text, n.characterPositions).suspended)
        return unsupported(
          'suspended-characters',
          'Restore or reset suspended character adjustments before duplicating artwork.',
        );
    }
    return { supported: true, target: move.target };
  } catch (error) {
    return unsupported('invalid-artwork', error.message || 'The artwork is unavailable.');
  }
}

/**
 * Pure staging: deep-copy the complete layer, remap graph/Curve IDs, and validate
 * an asset-safe isolated candidate before returning anything commit-ready.
 * Text-local character IDs intentionally stay local; their records are deep copies.
 * Immutable font/image assets remain referenced by their existing IDs and bytes.
 */
export function stageArtworkDuplicate(doc, layerId, { fonts = {}, imageAssets } = {}) {
  const eligibility = resolveArtworkDuplicateTarget(doc, layerId, { fonts, imageAssets });
  if (!eligibility.supported) throw Error(eligibility.reason);
  const source = doc.layers.find((layer) => layer.id === layerId),
    index = doc.layers.indexOf(source) + 1,
    layer = copyArtwork(source, occupiedArtworkIds(doc));
  const candidate = candidateDocument(doc);
  // Validation may normalize legacy records. Keep it on a separate clone so the
  // committed copy retains every authored parameter and graph layout exactly.
  insertDuplicatedLayer(candidate, source.id, index, structuredClone(layer));
  validate(candidate);
  return { layer, index, layerId: layer.id, outputNodeId: layer.output };
}

function occupiedArtworkIds(doc) {
  const occupied = new Set();
  for (const guide of doc.editor?.guides || []) occupied.add(guide.id);
  for (const folder of layerFolders(doc)) occupied.add(folder.id);
  for (const l of doc.layers) {
    for (const id of danglingExpressionIds(l.nodes)) occupied.add(id);
    occupied.add(l.id);
    for (const group of l.nodeGroups || []) occupied.add(group.id);
    for (const n of l.nodes) {
      occupied.add(n.id);
      for (const pt of n.points) occupied.add(pt.id);
    }
  }
  if (doc.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    reserveDanglingArtworkReferenceIds(doc.layers, occupied);
  return occupied;
}

function copyArtwork(source, occupied) {
  const layer = structuredClone(source),
    ids = new Map();
  function fresh(old, prefix) {
    const id = uid(prefix);
    if (occupied.has(id) || ids.has(old)) throw Error('Duplicate artwork ID collision');
    occupied.add(id);
    ids.set(old, id);
    return id;
  }
  layer.id = fresh(source.id, 'layer');
  for (const n of layer.nodes) {
    n.id = fresh(n.id, 'node');
    for (const pt of n.points) pt.id = fresh(pt.id, 'pt');
    if (n.type === 'Multi Path') {
      const contours = new Map(n.contours.map((c, i) => [c.id, `${n.id}:c${i}`]));
      n.contours = n.contours.map((c) => ({
        ...c,
        id: contours.get(c.id),
        points: c.points.map((id) => ids.get(id)),
      }));
      n.units = n.units.map((u, i) => ({
        id: `${n.id}:u${i}`,
        contours: u.contours.map((id) => contours.get(id)),
      }));
    }
  }
  layer.output = ids.get(source.output);
  if (layer.nodeGroups)
    layer.nodeGroups = layer.nodeGroups.map((g) => ({
      id: fresh(g.id, 'node-group'),
      name: g.name,
      nodeIds: g.nodeIds.map((id) => ids.get(id)),
    }));
  layer.name = `${source.name} copy`;
  const nodeIds = new Set(source.nodes.map((n) => n.id));
  function remapSources(value) {
    if (!value || typeof value !== 'object') return;
    if (value.source !== undefined) {
      if (!nodeIds.has(value.source)) throw Error('Artwork source points outside the copy');
      value.source = ids.get(value.source);
    }
    for (const child of Object.values(value)) remapSources(child);
  }
  remapSources(layer);
  remapExpressionReferences(source.nodes, layer.nodes);
  return layer;
}

/** Resolve every member afresh; one unsupported artwork refuses the entire set. */
export function resolveArtworkDuplicateSelection(doc, layerIds, options = {}) {
  try {
    const ids = [...layerIds];
    if (!ids.length || ids.length > 100 || new Set(ids).size !== ids.length)
      return unsupported('invalid-selection', 'Choose a nonempty set of unique artwork layers.');
    if (!Array.isArray(doc?.layers))
      return unsupported('invalid-document', 'Choose artwork in a valid document.');
    if (doc.layers.length + ids.length > 100)
      return unsupported(
        'layer-limit',
        `Duplicating ${ids.length} artwork layers would exceed 100 layers. No selected artwork duplicated.`,
      );
    for (const id of ids) {
      const resolved = resolveArtworkDuplicateTarget(doc, id, {
        ...options,
        selectedLayerIds: ids,
      });
      if (!resolved.supported) {
        const name = doc.layers.find((l) => l.id === id)?.name || id;
        return unsupported(
          resolved.code,
          `${name}: ${resolved.reason} No selected artwork duplicated.`,
        );
      }
    }
    const selected = new Set(ids);
    return {
      supported: true,
      layerIds: doc.layers.filter((l) => selected.has(l.id)).map((l) => l.id),
    };
  } catch (error) {
    return unsupported(
      'invalid-selection',
      error.message || 'The artwork selection is unavailable.',
    );
  }
}

/**
 * Each copy sits immediately above its own source, matching single duplication.
 * Source and copy relative paint order is stable, independent of click order.
 * All copies share one occupied-ID set and one isolated candidate validation.
 */
export function stageArtworkDuplicates(doc, layerIds, options) {
  const eligibility = resolveArtworkDuplicateSelection(doc, layerIds, options);
  if (!eligibility.supported) throw Error(eligibility.reason);
  const occupied = occupiedArtworkIds(doc),
    selected = new Set(eligibility.layerIds),
    copies = [];
  doc.layers.forEach((source, index) => {
    if (!selected.has(source.id)) return;
    const layer = copyArtwork(source, occupied);
    copies.push({
      sourceLayerId: source.id,
      layer,
      index: index + 1 + copies.length,
      layerId: layer.id,
      outputNodeId: layer.output,
    });
  });
  const sourceLayers = copies.map((copy) =>
    doc.layers.find((layer) => layer.id === copy.sourceLayerId),
  );
  if (doc.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    remapArtworkExpressionReferences(
      sourceLayers,
      copies.map((copy) => copy.layer),
    );
  const candidate = candidateDocument(doc);
  for (const copy of copies)
    insertDuplicatedLayer(candidate, copy.sourceLayerId, copy.index, structuredClone(copy.layer));
  validate(candidate);
  return copies;
}

/** Caller owns a single History transaction and selection snapshot. */
export function duplicateArtworkLayers(doc, layerIds, options) {
  const copies = stageArtworkDuplicates(doc, layerIds, options);
  for (const copy of copies) insertDuplicatedLayer(doc, copy.sourceLayerId, copy.index, copy.layer);
  return copies.map(({ sourceLayerId, layerId, outputNodeId }) => ({
    sourceLayerId,
    layerId,
    outputNodeId,
  }));
}

/** Caller owns the one History transaction and final-artwork selection. */
export function duplicateArtworkLayer(doc, layerId, options) {
  const staged = stageArtworkDuplicate(doc, layerId, options);
  insertDuplicatedLayer(doc, layerId, staged.index, staged.layer);
  return { layerId: staged.layerId, outputNodeId: staged.outputNodeId };
}

// Return only an input edge whose evaluated operation retains its point IDs.
// A bypassed Mask still creates a painted group; a bypassed painted Transform
// can instead pass plain geometry through. Inspect the actual output, not just
// the socket's declared type, before accepting a painted bypass.
function identityPathInput(source, node, evaluation, remapPoints = false) {
  if (node.bypassed) {
    const contract = bypassContract(node, source);
    if (!contract.supported || !['Path', 'PaintedPath'].includes(contract.type)) return null;
    return isPaintedGroup(evaluation.run(node.id)) ? null : contract.input;
  }
  if (['Transform', 'Path Remap', 'Text to Path'].includes(node.type)) {
    if (node.type === 'Transform' && isPaintedGroup(evaluation.run(node.id))) return null;
    return 'path';
  }
  if (
    [
      'Convert to Path',
      'Fill',
      'Stroke',
      'Linear Gradient Fill',
      'Radial Gradient Fill',
      'Angular Gradient Fill',
    ].includes(node.type)
  ) {
    const input = evaluation.run(node.params.path.source, node.params.path.sourcePort);
    return isPaintedGroup(input) ? null : 'path';
  }
  if (node.type === 'Closed Path Offset' && evaluation.parameterValue(node.id, 'amount') === 0)
    return 'path';
  // These operators mark their result unstable, so Text attachments remain
  // unsupported. The Remap guard separately checks their fingerprints.
  if (remapPoints) {
    if (node.type === 'Polar Transform') return 'path';
    if (node.type === 'Dash Path' && evaluation.parameterValue(node.id, 'gapLength') === 0)
      return 'path';
    if (node.type === 'Corner Round' && evaluation.parameterValue(node.id, 'radius') === 0)
      return 'path';
  }
  return null;
}

// A copied authored point has a provable identity only through operators that
// retain their input point IDs. Never infer a generated ID from a prefix,
// position or point order. Other routes can still pass if their evaluated ID
// remains unchanged in the copy.
function copiedAuthoredPoints(source, copy, evaluation, inputId) {
  const nodes = new Map(source.nodes.map((node, index) => [node.id, { node, index }]));
  const seen = new Set();
  let id = inputId;
  while (id && !seen.has(id)) {
    seen.add(id);
    const pair = nodes.get(id);
    if (!pair) return null;
    const original = pair.node,
      duplicate = copy.nodes[pair.index];
    if (['Curve', 'Multi Path'].includes(original.type) && !original.bypassed) {
      return new Map(original.points.map((point, index) => [point.id, duplicate.points[index].id]));
    }
    const input = identityPathInput(source, original, evaluation, true);
    if (!input) return null;
    const next = original.params?.[input];
    const copiedNext = duplicate.params?.[input];
    const nextPair = nodes.get(next?.source);
    if (
      next?.sourcePort !== undefined ||
      copiedNext?.sourcePort !== undefined ||
      !nextPair ||
      copiedNext?.source !== copy.nodes[nextPair.index].id
    )
      return null;
    id = next.source;
  }
  return null;
}

function indexedPathPoints(path) {
  if (!Array.isArray(path?.contours)) throw Error('input did not evaluate to a Path');
  const points = new Map();
  for (const contour of path.contours)
    for (const point of contour.points) {
      if (points.has(point.id)) points.set(point.id, null);
      else points.set(point.id, point);
    }
  return points;
}

function pointGeometry(point) {
  return JSON.stringify([point.position, point.handleA, point.handleB]);
}

// Check the evaluator's actual binding and fingerprint rules on isolated
// snapshots before any layer or History entry is committed. A suspended edit
// may remain suspended, but must never become bound to a different point.
function isolatedDuplicateCandidate(doc) {
  const isolated = inheritProjectDocumentContext(doc, { ...doc });
  const deeplyFrozen = (value, seen = new WeakSet()) => {
    if (!value || typeof value !== 'object' || seen.has(value)) return true;
    if (!Object.isFrozen(value)) return false;
    seen.add(value);
    return Object.values(value).every((child) => deeplyFrozen(child, seen));
  };
  for (const key of ['fontAssets', 'imageAssets']) {
    const assets = isolated[key];
    if (assets && typeof assets === 'object' && !Array.isArray(assets))
      isolated[key] = Object.fromEntries(
        Object.entries(assets).map(([id, asset]) => [
          id,
          deeplyFrozen(asset) ? asset : structuredClone(asset),
        ]),
      );
  }
  return candidateDocument(isolated);
}

function assertCopiedRemapBindings(
  doc,
  source,
  copy,
  index,
  fonts,
  sourceDocument = doc,
  sourceEvaluationId = source.id,
) {
  const remaps = source.nodes
    .map((node, index) => ({ node, index }))
    .filter(({ node }) => node.type === 'Path Remap' && node.edits.length);
  if (!remaps.length) return;

  const originalDocument = isolatedDuplicateCandidate(sourceDocument),
    copiedDocument = isolatedDuplicateCandidate(doc),
    original = originalDocument.layers.find((layer) => layer.id === sourceEvaluationId);
  insertDuplicatedLayer(copiedDocument, source.id, index, copy);
  const originalEvaluation = evaluate(
    original,
    fonts,
    originalDocument.imageAssets || {},
    originalDocument,
    null,
    true,
  );
  const checks = [];
  for (const { node: sourceNode, index: nodeIndex } of remaps) {
    const node = original.nodes[nodeIndex],
      copiedNode = copy.nodes[nodeIndex];
    try {
      const input = originalEvaluation.run(node.params.path.source, node.params.path.sourcePort),
        points = indexedPathPoints(input),
        authoredPoints = copiedAuthoredPoints(
          original,
          copy,
          originalEvaluation,
          node.params.path.source,
        );
      for (const [editIndex, edit] of node.edits.entries()) {
        const oldPoint = points.get(edit.pointId);
        if (points.has(edit.pointId) && !oldPoint) throw Error('point ID is ambiguous');
        const mapped = oldPoint && authoredPoints?.get(edit.pointId);
        const copiedEdit = copiedNode.edits[editIndex];
        copiedEdit.pointId = mapped || edit.pointId;
        if (edit.fingerprint === undefined) delete copiedEdit.fingerprint;
        else copiedEdit.fingerprint = edit.fingerprint;
        checks.push({ node, copiedNode, edit, copiedEdit, input, oldPoint });
      }
    } catch (error) {
      throw Error(
        `Cannot duplicate Path Remap “${sourceNode.name}”: unable to verify its input (${error.message}).`,
      );
    }
  }

  const copiedEvaluation = evaluate(
      copy,
      fonts,
      copiedDocument.imageAssets || {},
      copiedDocument,
      null,
      true,
    ),
    inputs = new Map(),
    outputs = new Map();
  for (const check of checks) {
    const { node, copiedNode, edit, copiedEdit, input, oldPoint } = check;
    try {
      if (!inputs.has(copiedNode.id)) {
        const path = copiedEvaluation.run(
          copiedNode.params.path.source,
          copiedNode.params.path.sourcePort,
        );
        inputs.set(copiedNode.id, { path, points: indexedPathPoints(path) });
      }
      const copiedInput = inputs.get(copiedNode.id),
        copiedPoint = copiedInput.points.get(copiedEdit.pointId);
      if (copiedInput.points.has(copiedEdit.pointId) && !copiedPoint)
        throw Error('copied point ID is ambiguous');
      const active =
          !!oldPoint && (!input.unstable || remapFingerprintMatches(input, edit.fingerprint)),
        copiedActive =
          !!copiedPoint &&
          (!copiedInput.path.unstable ||
            remapFingerprintMatches(copiedInput.path, copiedEdit.fingerprint));
      if ((!oldPoint && copiedPoint) || active !== copiedActive)
        throw Error('the copied point binding would change or become suspended');
      if (!active) continue;
      if (!outputs.has(node.id))
        outputs.set(node.id, {
          original: indexedPathPoints(originalEvaluation.run(node.id)),
          copied: indexedPathPoints(copiedEvaluation.run(copiedNode.id)),
        });
      const output = outputs.get(node.id),
        oldResult = output.original.get(edit.pointId),
        copiedResult = output.copied.get(copiedEdit.pointId);
      if (
        !oldResult ||
        !copiedResult ||
        pointGeometry(oldPoint) !== pointGeometry(copiedPoint) ||
        pointGeometry(oldResult) !== pointGeometry(copiedResult)
      )
        throw Error('the copied point or its applied correction would differ');
    } catch (error) {
      throw Error(`Cannot duplicate Path Remap “${node.name}”: ${error.message}.`);
    }
  }
}

// Text attachments address a contour, not an arbitrary string elsewhere in
// the document. Follow only the known identity-preserving route to its owner.
function copiedAttachmentContour(source, copy, nodeIds, contours, evaluation, start, contourId) {
  const nodes = new Map(source.nodes.map((node, index) => [node.id, { node, index }]));
  const seen = new Set();
  let id = start;
  while (id && !seen.has(id)) {
    seen.add(id);
    const pair = nodes.get(id);
    if (!pair) return null;
    const n = pair.node,
      copied = copy.nodes[pair.index];
    if (n.type === 'Curve') return contourId === n.id ? copied.id : null;
    if (n.type === 'Multi Path') return contours.get(n.id)?.get(contourId) || null;
    if (['Circle', 'Ellipse', 'Rectangle'].includes(n.type))
      return contourId === `${n.id}:c0` ? `${copied.id}:c0` : null;
    if (['Polygon', 'Star'].includes(n.type)) {
      const prefix = `${n.id}:${n.type}:`;
      return contourId.startsWith(prefix) && /^\d+$/.test(contourId.slice(prefix.length))
        ? `${copied.id}:${n.type}:${contourId.slice(prefix.length)}`
        : null;
    }
    const input = identityPathInput(source, n, evaluation);
    if (!input) return null;
    const next = n.params[input];
    if (
      next?.sourcePort !== undefined ||
      copied.params[input]?.source !== nodeIds.get(next?.source)
    )
      return null;
    id = next.source;
  }
  return null;
}

/** Generic layer duplication for the layer-panel command surface. */
export function duplicateLayerGeneric(
  doc,
  layerId,
  { fonts = {}, snapshot = null, snapshotSchemaVersion = doc?.schemaVersion } = {},
) {
  const anchor = doc?.layers?.find((layer) => layer.id === layerId);
  if (!anchor) throw Error('Layer is unavailable; select it again');
  if (anchor.locked) throw Error('Unlock this layer before duplicating it.');
  if (snapshot && (snapshot.id !== anchor.id || doc.schemaVersion < snapshotSchemaVersion))
    throw Error('Copied Layer requires its original schema; restore it or copy again');
  const source = snapshot || anchor,
    reservationDocument = snapshot
      ? inheritProjectDocumentContext(doc, { ...doc, layers: [...doc.layers, source] })
      : doc;
  if (
    doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
    nodesHaveComponentCompositionReferenceSyntax(source.nodes)
  )
    throw Error('Duplicating artwork with Component Composition references is not supported');
  if (
    doc.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
    nodesHaveQualifiedColorReferenceSyntax(source.nodes)
  )
    throw Error('Duplicating artwork with qualified Color references is not supported');
  if (
    doc.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
    nodesHaveCompositionPropertyReferenceSyntax(source.nodes)
  )
    throw Error('Duplicating artwork with Composition property references is not supported');
  if (source.locked) throw Error('Unlock this layer before duplicating it.');
  if (doc.layers.length >= 100) throw Error('Duplicate requires fewer than 100 layers.');
  // Documents are JSON trees, but programmatic clients can share descriptor
  // objects. Copy each occurrence so an authored parameter rewrite cannot
  // modify an aliased metadata record or a second parameter behind its back.
  const copyOccurrences = (value) => {
    if (Array.isArray(value)) return value.map(copyOccurrences);
    if (value && typeof value === 'object')
      return Object.fromEntries(
        Object.entries(value).map(([key, child]) => [key, copyOccurrences(child)]),
      );
    return value;
  };
  const layer = copyOccurrences(source);
  const occupied = occupiedArtworkIds(doc);
  if (snapshot) for (const id of occupiedArtworkIds(reservationDocument)) occupied.add(id);
  for (const slot of doc.colorSlots || []) occupied.add(slot.id);
  // Reserve dangling structural references too, so fresh IDs cannot revive one.
  for (const document of snapshot ? [doc, reservationDocument] : [doc])
    for (const current of document.layers)
      for (const n of current.nodes) {
        for (const [, q] of parameters(n)) if (q?.source) occupied.add(q.source);
        if (n.type === 'Reference' && n.reference?.nodeId) occupied.add(n.reference.nodeId);
        if (n.type === 'Text' && n.pathAttachment?.nodeId) occupied.add(n.pathAttachment.nodeId);
        if (n.type === 'Text' && n.pathAttachment?.contourId)
          occupied.add(n.pathAttachment.contourId);
        if (n.type === 'Path Remap') for (const edit of n.edits) occupied.add(edit.pointId);
        if (n.type === 'Multi Path') {
          for (const contour of n.contours) occupied.add(contour.id);
          for (const unit of n.units) occupied.add(unit.id);
        }
      }
  const fresh = () => {
    let next;
    do next = uid('copy');
    while (occupied.has(next));
    occupied.add(next);
    return next;
  };
  layer.id = fresh();
  // The captured Layer is evaluated as a distinct provisional copy. Foreign
  // Artwork references still resolve through the live document, including a
  // reference chain that returns to the current source Layer.
  const sourceDocument = snapshot
      ? inheritProjectDocumentContext(doc, {
          ...doc,
          layers: [...doc.layers, { ...source, id: layer.id }],
        })
      : doc,
    sourceEvaluationId = snapshot ? layer.id : source.id;
  const nodeIds = new Map(),
    contours = new Map();
  source.nodes.forEach((n, index) => {
    const copy = layer.nodes[index];
    copy.id = fresh();
    nodeIds.set(n.id, copy.id);
    const pointIds = new Map();
    n.points.forEach((point, i) => {
      copy.points[i].id = fresh();
      pointIds.set(point.id, copy.points[i].id);
    });
    if (n.type === 'Multi Path') {
      const contourIds = new Map();
      n.contours.forEach((contour, i) => {
        copy.contours[i].id = fresh();
        contourIds.set(contour.id, copy.contours[i].id);
        copy.contours[i].points = contour.points.map((id) => pointIds.get(id) || id);
      });
      contours.set(n.id, contourIds);
      n.units.forEach((unit, i) => {
        copy.units[i].id = fresh();
        copy.units[i].contours = unit.contours.map((id) => contourIds.get(id) || id);
      });
    }
  });
  layer.output = nodeIds.get(source.output) || source.output;
  for (const n of layer.nodes) {
    for (const [, q] of parameters(n)) if (nodeIds.has(q?.source)) q.source = nodeIds.get(q.source);
    if (n.type === 'Reference' && nodeIds.has(n.reference?.nodeId))
      n.reference.nodeId = nodeIds.get(n.reference.nodeId);
  }
  if (layer.nodeGroups)
    layer.nodeGroups.forEach((group, index) => {
      group.id = fresh();
      group.nodeIds = source.nodeGroups[index].nodeIds.map((id) => nodeIds.get(id) || id);
    });
  remapExpressionReferences(source.nodes, layer.nodes);
  const attachments = source.nodes
    .map((node, index) => ({ node, index }))
    .filter(
      ({ node }) =>
        node.type === 'Text' && node.pathAttachment && nodeIds.has(node.pathAttachment.nodeId),
    );
  let attachmentEvaluation;
  if (attachments.length) {
    const originalDocument = isolatedDuplicateCandidate(sourceDocument),
      originalLayer = originalDocument.layers.find((item) => item.id === sourceEvaluationId);
    attachmentEvaluation = evaluate(
      originalLayer,
      fonts,
      originalDocument.imageAssets || {},
      originalDocument,
      null,
      true,
    );
    for (const { node, index } of attachments) {
      let mappedContour;
      try {
        mappedContour = copiedAttachmentContour(
          source,
          layer,
          nodeIds,
          contours,
          attachmentEvaluation,
          node.pathAttachment.nodeId,
          node.pathAttachment.contourId,
        );
      } catch (error) {
        throw Error(`Cannot safely duplicate this Text path attachment (${error.message}).`);
      }
      if (!mappedContour) throw Error('Cannot safely duplicate this Text path attachment contour.');
      layer.nodes[index].pathAttachment.nodeId = nodeIds.get(node.pathAttachment.nodeId);
      layer.nodes[index].pathAttachment.contourId = mappedContour;
    }
  }
  layer.name = `${source.name} copy`;
  const index = doc.layers.indexOf(anchor) + 1;
  assertCopiedRemapBindings(doc, source, layer, index, fonts, sourceDocument, sourceEvaluationId);
  if (attachments.length) {
    const copyDocument = isolatedDuplicateCandidate(doc);
    insertDuplicatedLayer(copyDocument, source.id, index, layer);
    const copyEvaluation = evaluate(
      layer,
      fonts,
      copyDocument.imageAssets || {},
      copyDocument,
      null,
      true,
    );
    for (const { node, index } of attachments) {
      try {
        const oldPath = attachmentEvaluation.run(node.pathAttachment.nodeId),
          newAttachment = layer.nodes[index].pathAttachment,
          newPath = copyEvaluation.run(newAttachment.nodeId),
          oldContours = oldPath.contours.filter(
            (contour) => contour.id === node.pathAttachment.contourId,
          ),
          newContours = newPath.contours.filter(
            (contour) => contour.id === newAttachment.contourId,
          ),
          geometry = (contour) =>
            JSON.stringify([
              contour.closed,
              contour.points.map((point) => [point.position, point.handleA, point.handleB]),
            ]);
        if (
          oldContours.length !== newContours.length ||
          oldContours.length > 1 ||
          !!oldPath.unstable !== !!newPath.unstable ||
          !!oldPath.remapSuspended !== !!newPath.remapSuspended ||
          (oldContours.length && geometry(oldContours[0]) !== geometry(newContours[0]))
        )
          throw Error('the copied contour or attachment state would change');
      } catch (error) {
        throw Error(`Cannot safely duplicate this Text path attachment (${error.message}).`);
      }
    }
  }
  const candidate = isolatedDuplicateCandidate(doc);
  insertDuplicatedLayer(candidate, source.id, index, structuredClone(layer));
  validate(candidate);
  insertDuplicatedLayer(doc, source.id, index, layer);
  return { layerId: layer.id, sourceLayerId: source.id };
}
