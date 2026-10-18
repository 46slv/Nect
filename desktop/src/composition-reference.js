import { transformCharacterPoint } from './character-geometry.js';
const id = /^[A-Za-z0-9][A-Za-z0-9:_-]{0,180}$/;
export const isPaintedGroup = (value) => value?.kind === 'painted-group';
export function validateCompositionReference(n, version) {
  if (n.type !== 'Composition Reference') {
    if (n.compositionReference !== undefined)
      throw Error('Composition target requires a Composition Reference');
    return;
  }
  if (version < 31) throw Error('Composition Reference requires schemaVersion 31');
  const ref = n.compositionReference;
  if (ref === null) return;
  if (
    !ref ||
    Object.getPrototypeOf(ref) !== Object.prototype ||
    Object.keys(ref).length !== 1 ||
    typeof ref.compositionId !== 'string' ||
    !id.test(ref.compositionId)
  )
    throw Error('Invalid Composition reference target');
}
export function assertCompositionAcyclic(doc) {
  const layers = new Map(doc.layers.map((l) => [l.id, l])),
    seen = new Set(),
    active = new Set();
  function visit(id) {
    if (active.has(id)) throw Error('Composition reference would create a cycle');
    if (seen.has(id) || !layers.has(id)) return;
    if (active.size >= 100) throw Error('Composition reference depth exceeds 100');
    active.add(id);
    for (const n of layers.get(id).nodes)
      if (n.type === 'Composition Reference' && n.compositionReference)
        visit(n.compositionReference.compositionId);
    active.delete(id);
    seen.add(id);
  }
  for (const layer of doc.layers) visit(layer.id);
}
export function compositionReferenceTargets(doc, layerId) {
  return doc.layers.filter((l) => l.id !== layerId).map((l) => ({ id: l.id, name: l.name }));
}
function imageContours(value) {
  const [x, y] = value.position,
    [w, h] = value.size;
  return [
    {
      id: 'image-bounds',
      closed: true,
      points: [
        [x, y],
        [x + w, y],
        [x + w, y + h],
        [x, y + h],
      ].map((position, i) => ({ id: `image:${i}`, position, handleA: [0, 0], handleB: [0, 0] })),
    },
  ];
}
// A group's aggregate contours are display/bounds geometry only. Its ordered
// children carry the actual paints/images; SVG never paints the bounds carrier.
export function paintedGroup(values, { matrix = [1, 0, 0, 1, 0, 0], prefix = 'composition' } = {}) {
  if (
    !Array.isArray(values) ||
    values.length > 256 ||
    matrix.length !== 6 ||
    !matrix.every(Number.isFinite)
  )
    throw Error('Invalid composition appearance');
  let leaves = 0;
  function check(value, depth) {
    if (depth > 32) throw Error('Composition appearance exceeds 32 nested groups');
    if (isPaintedGroup(value)) {
      for (const child of value.children) check(child, depth + 1);
    } else if (++leaves > 256) throw Error('Composition appearance exceeds 256 paint elements');
  }
  for (const value of values) check(value, 1);
  const contours = [];
  let points = 0;
  const linear = [...matrix.slice(0, 4), 0, 0];
  for (const [vi, value] of values.entries()) {
    const groups = new Map();
    const source = value.kind === 'image' ? imageContours(value) : value.contours;
    if (!Array.isArray(source)) throw Error('Composition output has no supported appearance');
    for (const [ci, c] of source.entries()) {
      if (!groups.has(c.group)) groups.set(c.group, groups.size);
      const contour = {
        id: `${prefix}:v${vi}:c${ci}`,
        closed: c.closed,
        group: `${prefix}:v${vi}:g${groups.get(c.group)}`,
        points: [],
      };
      for (const [pi, p] of c.points.entries()) {
        if (++points > 100000) throw Error('Composition geometry exceeds 100000 vertices');
        const next = {
          id: `${contour.id}:p${pi}`,
          position: transformCharacterPoint(matrix, p.position),
          handleA: transformCharacterPoint(linear, p.handleA),
          handleB: transformCharacterPoint(linear, p.handleB),
        };
        if (
          ![...next.position, ...next.handleA, ...next.handleB].every(
            (v) => Number.isFinite(v) && Math.abs(v) <= 1000000000,
          )
        )
          throw Error('Composition coordinates exceed the supported range');
        contour.points.push(next);
      }
      contours.push(contour);
      if (contours.length > 10000) throw Error('Composition exceeds 10000 contours');
    }
  }
  const children = structuredClone(values);
  function strip(value) {
    delete value.characterOccurrences;
    if (isPaintedGroup(value)) {
      delete value.contours;
      for (const child of value.children) strip(child);
    }
  }
  for (const value of children) strip(value);
  const exportBlockers = [
    ...new Set(
      values.flatMap((value) => [
        ...(value.exportBlockers || []),
        ...(value.characterOccurrences || []).flatMap((o) =>
          o.arrangementStatus?.suspended
            ? [`Maintained ink gaps suspended: ${o.arrangementStatus.reason}`]
            : o.suspended
              ? ['Character positions are suspended; restore source text/mapping before export']
              : [],
        ),
      ]),
    ),
  ];
  return {
    kind: 'painted-group',
    children,
    matrix: [...matrix],
    contours,
    unstable: true,
    ...(exportBlockers.length ? { exportBlockers } : {}),
  };
}
export function groupGeometry(value) {
  return {
    contours: structuredClone(value.contours),
    unstable: true,
    fingerprint: value.fingerprint,
    ...(value.exportBlockers ? { exportBlockers: [...value.exportBlockers] } : {}),
  };
}
