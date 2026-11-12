import { bindProjectDocumentContext, evaluate, remapFingerprintMatches } from './core.js';
import { activeCompositionDocument } from './composition-project-model.js';
import { resolveFonts } from './fonts.js';
import { isVector2 } from './node-value-validation.js';
import { handlePolar, polarHandleValue } from './handle-polar.js';

const keys = ['position', 'handleA', 'handleB'];

// Committed documents and active font resources are immutable. A drag may reuse
// a checked snapshot while both retain their identity, then recheck at commit.
export function createRemapSnapshotGuard(document, fonts, snapshot, read) {
  const { fingerprint, ...geometry } = snapshot;
  const fontEntries = Object.entries(fonts),
    expected = JSON.stringify(geometry);
  let checked = false,
    invalidated = false;
  return (currentDocument, currentFonts, fresh = false) => {
    if (
      invalidated ||
      currentDocument !== document ||
      Object.keys(currentFonts).length !== fontEntries.length ||
      fontEntries.some(
        ([key, value]) => !Object.hasOwn(currentFonts, key) || currentFonts[key] !== value,
      )
    ) {
      invalidated = true;
      return false;
    }
    if (!fresh && checked) return true;
    const current = read(fresh);
    if (current) {
      const { fingerprint: currentFingerprint, ...currentGeometry } = current;
      checked = fingerprint === currentFingerprint && JSON.stringify(currentGeometry) === expected;
    } else checked = false;
    if (!checked) invalidated = true;
    return checked;
  };
}

function indexedPoints(path, wanted) {
  const indexed = new Map();
  for (const contour of path?.contours || [])
    for (const point of contour.points || [])
      if (!wanted || wanted.has(point.id))
        indexed.set(point.id, indexed.has(point.id) ? null : { contourId: contour.id, point });
  return indexed;
}

// A fresh Project copy prevents cached Layer evaluation from hiding a change in
// an existing font object, including in a referenced Composition.
export function inspectFreshRemapOutput(owner, compositionId, layerId, nodeId, fonts, pointIds) {
  const project = owner.projectSnapshot(),
    doc = activeCompositionDocument(project, compositionId);
  bindProjectDocumentContext(doc, project, compositionId);
  return inspectRemapOutput(doc, layerId, nodeId, resolveFonts(doc, fonts), null, pointIds);
}

// The returned snapshots are temporary UI baselines; the Session retains all
// authored Remap deltas and their fingerprint ownership.
export function inspectRemapOutput(
  doc,
  layerId,
  nodeId,
  fonts,
  evaluation = null,
  pointIds = null,
) {
  const layer = doc.layers.find((item) => item.id === layerId),
    node = layer?.nodes.find((item) => item.id === nodeId);
  if (node?.type !== 'Path Remap' || !node.params.path.source) return new Map();
  const result = evaluation || evaluate(layer, fonts, doc.imageAssets, doc),
    source = result.run(node.params.path.source, node.params.path.sourcePort),
    output = result.run(node.id),
    offset = result.parameterValue(node.id, 'offset'),
    wanted = pointIds ? new Set(pointIds) : null,
    sourcePoints = indexedPoints(source, wanted),
    outputPoints = indexedPoints(output, wanted),
    snapshots = new Map();
  if (!isVector2(offset)) return snapshots;
  for (const [id, from] of sourcePoints) {
    const to = outputPoints.get(id);
    if (
      !from ||
      !to ||
      from.contourId !== to.contourId ||
      !keys.every((key) => isVector2(from.point[key]) && isVector2(to.point[key]))
    )
      continue;
    const edit = node.edits.find((item) => item.pointId === id),
      suspended = !!(edit && source.unstable && !remapFingerprintMatches(source, edit.fingerprint)),
      editable = {},
      delta = {};
    for (const key of keys) {
      const parameter = edit?.params?.[key],
        value = parameter ? parameter.value : edit?.[key] || [0, 0];
      if (!isVector2(value)) continue;
      delta[key] = [...value];
      editable[key] = !(
        layer.locked ||
        node.locked ||
        node.bypassed ||
        suspended ||
        node.params.path.sourcePort !== undefined ||
        parameter?.locked ||
        parameter?.source ||
        parameter?.expression
      );
    }
    snapshots.set(id, {
      sourceId: node.params.path.source,
      sourcePort: node.params.path.sourcePort ?? null,
      fingerprint: source.fingerprint ?? null,
      unstable: !!source.unstable,
      contourId: from.contourId,
      suspended,
      source: Object.fromEntries(keys.map((key) => [key, [...from.point[key]]])),
      output: Object.fromEntries(keys.map((key) => [key, [...to.point[key]]])),
      offset: [...offset],
      delta,
      editable,
    });
  }
  return snapshots;
}

export function remapOutputValue(snapshot, key, component) {
  if (key === 'position' && ['x', 'y'].includes(component))
    return snapshot.output.position[component === 'x' ? 0 : 1];
  if (['handleA', 'handleB'].includes(key) && ['angle', 'length'].includes(component))
    return handlePolar(snapshot.output[key])[component];
  throw Error('Invalid Remap point control');
}

export function remapOutputDelta(snapshot, key, component, value) {
  if (!snapshot.editable[key] || !Number.isFinite(value)) throw Error('Remap point is unavailable');
  if (value === remapOutputValue(snapshot, key, component)) return [...snapshot.delta[key]];
  if (key === 'position' && ['x', 'y'].includes(component)) {
    const axis = component === 'x' ? 0 : 1,
      delta = [...snapshot.delta.position],
      base = snapshot.source.position[axis] + snapshot.offset[axis];
    delta[axis] = value - base;
    return delta;
  }
  if (['handleA', 'handleB'].includes(key) && ['angle', 'length'].includes(component)) {
    const desired = polarHandleValue(snapshot.output[key], component, value);
    return desired.map((entry, axis) => entry - snapshot.source[key][axis]);
  }
  throw Error('Invalid Remap point control');
}
