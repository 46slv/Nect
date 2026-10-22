// Editing aids are document state, but never geometry or SVG output.
export const guideDefaults = () => ({
  guides: [],
  visible: true,
  locked: false,
  gridVisible: false,
  gridSize: 8,
  gridAnchor: 'top-left',
  gridAngle: 0,
  gridOpacity: 0.25,
  snap: false,
  snapGuides: true,
  snapArtboard: true,
  snapGrid: true,
  snapArtwork: false,
});
export const guideSettings = (doc) => ({ ...guideDefaults(), ...doc.editor });
export const GRID_ORIGIN_SCHEMA_VERSION = 48;
export const GRID_ANGLE_SCHEMA_VERSION = 49;
export const MAX_GRID_LINES = 2048;
const gridOriginFactors = {
  'top-left': [0, 0],
  'top-center': [0.5, 0],
  'top-right': [1, 0],
  'middle-left': [0, 0.5],
  center: [0.5, 0.5],
  'middle-right': [1, 0.5],
  'bottom-left': [0, 1],
  'bottom-center': [0.5, 1],
  'bottom-right': [1, 1],
};
export const gridAnchors = Object.freeze(Object.keys(gridOriginFactors));
// Missing anchors retain the phase of previously saved documents.
export function resolveGridOrigin(doc, settings = guideSettings(doc)) {
  const factor = Object.hasOwn(gridOriginFactors, settings.gridAnchor)
    ? gridOriginFactors[settings.gridAnchor]
    : gridOriginFactors['top-left'];
  return [doc.width * factor[0], doc.height * factor[1]];
}
export function resolveGridBasis(doc, settings = guideSettings(doc)) {
  const raw = settings.gridAngle;
  let angle = Number.isFinite(raw) && Math.abs(raw) <= 360 ? raw % 360 : 0;
  if (angle > 180) angle -= 360;
  if (angle <= -180) angle += 360;
  angle ||= 0;
  const cardinal = { 0: [1, 0], 90: [0, 1], 180: [-1, 0], '-90': [0, -1] }[angle];
  const [cos, sin] = cardinal || [
    Math.cos((angle * Math.PI) / 180),
    Math.sin((angle * Math.PI) / 180),
  ];
  return { origin: resolveGridOrigin(doc, settings), angle, cos, sin };
}
export function gridLocalPoint(basis, point) {
  const x = point[0] - basis.origin[0],
    y = point[1] - basis.origin[1];
  return [x * basis.cos + y * basis.sin, -x * basis.sin + y * basis.cos];
}
export function gridWorldPoint(basis, point) {
  return [
    basis.origin[0] + point[0] * basis.cos - point[1] * basis.sin,
    basis.origin[1] + point[0] * basis.sin + point[1] * basis.cos,
  ];
}
// Clip an infinite grid line algebraically; only finite canvas segments escape.
export function gridLineSegment(
  doc,
  basis,
  axis,
  position,
  bounds = [0, 0, doc.width, doc.height],
) {
  const base = gridWorldPoint(basis, axis ? [0, position] : [position, 0]);
  const direction = axis ? [basis.cos, basis.sin] : [-basis.sin, basis.cos];
  let low = -Infinity,
    high = Infinity;
  for (let a = 0; a < 2; a++) {
    if (direction[a] === 0) {
      if (base[a] < bounds[a] - 1e-9 || base[a] > bounds[a + 2] + 1e-9) return null;
    } else {
      const p = (bounds[a] - base[a]) / direction[a],
        q = (bounds[a + 2] - base[a]) / direction[a];
      low = Math.max(low, Math.min(p, q));
      high = Math.min(high, Math.max(p, q));
    }
  }
  if (!Number.isFinite(low) || !Number.isFinite(high) || low > high) return null;
  return [low, high].map((t) =>
    base.map((v, a) => Math.max(bounds[a], Math.min(bounds[a + 2], v + t * direction[a]))),
  );
}
export function gridLineLayout(
  doc,
  settings = guideSettings(doc),
  view = [0, 0, doc.width, doc.height],
) {
  const basis = resolveGridBasis(doc, settings),
    spacing = settings.gridSize;
  const bounds = [
    Math.max(0, view[0]),
    Math.max(0, view[1]),
    Math.min(doc.width, view[2]),
    Math.min(doc.height, view[3]),
  ];
  if (
    !bounds.every(Number.isFinite) ||
    bounds[0] >= bounds[2] ||
    bounds[1] >= bounds[3] ||
    !Number.isFinite(spacing) ||
    spacing < 1 ||
    spacing > 10000
  )
    return { basis, lines: [], stride: 1 };
  const corners = [
    [bounds[0], bounds[1]],
    [bounds[2], bounds[1]],
    [bounds[2], bounds[3]],
    [bounds[0], bounds[3]],
  ].map((p) => gridLocalPoint(basis, p));
  const ranges = [0, 1].map((a) => [
    Math.ceil((Math.min(...corners.map((p) => p[a])) - 1e-9) / spacing),
    Math.floor((Math.max(...corners.map((p) => p[a])) + 1e-9) / spacing),
  ]);
  const count = ranges.reduce((sum, [a, b]) => sum + Math.max(0, b - a + 1), 0);
  const stride = count <= MAX_GRID_LINES ? 1 : Math.ceil(count / (MAX_GRID_LINES - 2));
  const lines = [];
  ranges.forEach(([first, last], axis) => {
    for (let index = Math.ceil(first / stride) * stride; index <= last; index += stride) {
      const position = index * spacing,
        segment = gridLineSegment(doc, basis, axis, position, bounds);
      if (segment) lines.push({ axis, index, position, segment });
    }
  });
  return { basis, lines, stride };
}
export function validateGuides(editor, validateId) {
  if (editor === undefined) return;
  if (!editor || typeof editor !== 'object' || Array.isArray(editor))
    throw Error('Invalid editor settings');
  for (const key of [
    'visible',
    'locked',
    'gridVisible',
    'snap',
    'snapGuides',
    'snapArtboard',
    'snapGrid',
  ])
    if (typeof editor[key] !== 'boolean') throw Error('Invalid guide setting');
  if (editor.snapArtwork !== undefined && typeof editor.snapArtwork !== 'boolean')
    throw Error('Invalid artwork snap setting');
  if (!Number.isFinite(editor.gridSize) || editor.gridSize < 1 || editor.gridSize > 10000)
    throw Error('Grid spacing must be between 1 and 10000');
  if (editor.gridAnchor !== undefined && !gridAnchors.includes(editor.gridAnchor))
    throw Error('Invalid grid anchor');
  if (
    editor.gridAngle !== undefined &&
    (!Number.isFinite(editor.gridAngle) || Math.abs(editor.gridAngle) > 360)
  )
    throw Error('Grid angle must be finite and between -360 and 360 degrees');
  if (
    editor.gridOpacity !== undefined &&
    (!Number.isFinite(editor.gridOpacity) || editor.gridOpacity < 0 || editor.gridOpacity > 1)
  )
    throw Error('Grid opacity must be between 0 and 1');
  if (!Array.isArray(editor.guides) || editor.guides.length > 200)
    throw Error('Too many guides (maximum 200)');
  if (editor.layoutGrid !== undefined) validateLayoutGrid(editor.layoutGrid);
  for (const guide of editor.guides) {
    if (guide.origin !== undefined && guide.origin !== 'layout')
      throw Error('Invalid guide origin');
    validateId(guide.id);
    if (
      !['x', 'y'].includes(guide.axis) ||
      !Number.isFinite(guide.position) ||
      Math.abs(guide.position) > 100000 ||
      typeof guide.locked !== 'boolean'
    )
      throw Error('Invalid guide');
  }
}
// Reuse a render's evaluated final outputs. Graphs are layer-local, so excluding
// the whole editing layer also excludes every follower/dependent of a moved node.
// Keep this snapshot for canvas-only gesture updates; never evaluate from a move.
export function collectSnapLayerBounds(doc, evaluations, excludedLayer, boundsForPath) {
  const settings = guideSettings(doc),
    result = { guides: [], artwork: [] };
  if (!settings.snap) return result;
  const excluded = excludedLayer instanceof Set ? excludedLayer : new Set([excludedLayer]);
  for (const [layer, evaluation] of evaluations) {
    if (!layer.visible || excluded.has(layer.id) || evaluation.error) continue;
    const key = layer.guide ? 'guides' : 'artwork';
    if (layer.guide ? !settings.snapGuides || !settings.visible : !settings.snapArtwork) continue;
    const path = evaluation.path;
    if (!path || path.characterOccurrences?.some((o) => o.suspended)) continue;
    if (path.kind !== 'image' && !path.contours?.some((c) => c.points.length)) continue;
    const bounds = boundsForPath(path);
    if (
      bounds.length === 2 &&
      bounds.every((axis) => axis.length === 3 && axis.every(Number.isFinite))
    )
      result[key].push(bounds);
  }
  return result;
}
// Sources are coordinates of the original point or selected generators' bounds.
// Return a single translation per axis so a multi-selection never deforms.
export function snapTranslation(
  doc,
  sources,
  delta,
  scale = 1,
  bypass = false,
  excludedGuide = null,
  layerBounds = [],
  artworkBounds = [],
  extraCandidates = [],
  gridOptions = {},
) {
  const settings = guideSettings(doc),
    basis = resolveGridBasis(doc, settings),
    origin = basis.origin,
    matches = [],
    result = [...delta];
  if (!settings.snap || bypass) return { delta: result, matches };
  const threshold = 6 / Math.max(0.0001, Number.isFinite(scale) ? scale : 1);
  for (let axis = 0; axis < 2; axis++) {
    const candidates = [];
    if (settings.snapGuides && settings.visible)
      for (const guide of settings.guides)
        if (guide.axis === (axis ? 'y' : 'x') && guide.id !== excludedGuide)
          candidates.push({ position: guide.position, kind: 'guide' });
    if (settings.snapGuides && settings.visible)
      for (const bounds of layerBounds)
        for (const position of bounds[axis]) candidates.push({ position, kind: 'guide-layer' });
    if (settings.snapArtboard)
      for (const position of [
        0,
        doc[axis ? 'height' : 'width'] / 2,
        doc[axis ? 'height' : 'width'],
      ])
        candidates.push({ position, kind: 'artboard' });
    if (settings.snapArtwork)
      for (const bounds of artworkBounds)
        for (const position of bounds[axis]) candidates.push({ position, kind: 'artwork' });
    candidates.push(
      ...extraCandidates.filter(
        (candidate) => candidate.axis === axis && candidate.translation === undefined,
      ),
    );
    const priority = {
      guide: 0,
      character: 1,
      'character-gap': 2,
      'guide-layer': 3,
      artboard: 4,
      artwork: 5,
      grid: 6,
    };
    let best = null;
    const consider = (source, candidate) => {
      const correction =
        candidate.translation === undefined
          ? candidate.position - (source + delta[axis])
          : candidate.translation - delta[axis];
      if (
        Math.abs(correction) <= threshold &&
        (!best ||
          Math.abs(correction) < Math.abs(best.correction) ||
          (Math.abs(correction) === Math.abs(best.correction) &&
            priority[candidate.kind] < priority[best.kind]))
      )
        best = { ...candidate, correction, axis };
    };
    for (const source of sources[axis])
      for (const candidate of candidates) consider(source, candidate);
    if (sources[axis].length)
      for (const candidate of extraCandidates)
        if (candidate.axis === axis && Number.isFinite(candidate.translation))
          consider(0, candidate);
    // Explicit alignment targets win over the background grid within the snap radius.
    if (!best && settings.snapGrid && basis.angle === 0)
      for (const source of sources[axis])
        consider(source, {
          position:
            origin[axis] +
            Math.round((source + delta[axis] - origin[axis]) / settings.gridSize) *
              settings.gridSize,
          kind: 'grid',
        });
    if (best) {
      result[axis] += best.correction;
      matches.push(best);
    }
  }
  if (basis.angle !== 0 && settings.snapGrid && !matches.length) {
    const anchors = gridOptions.gridSources || sources;
    // Current point/bounds routes use 1 or 3 sources per axis. Never form an
    // unbounded Cartesian product from an external/unpaired source collection.
    if (anchors.every((a) => a.length) && anchors[0].length * anchors[1].length <= 256) {
      const gridScale = gridOptions.gridScale ?? scale;
      const radius = 6 / Math.max(0.0001, Number.isFinite(gridScale) ? gridScale : 1);
      let best = null;
      const consider = (correction, local, nearest, axes) => {
        if (!axes.length) return;
        const distance = Math.hypot(...correction);
        if (
          best &&
          (axes.length < best.axes.length ||
            (axes.length === best.axes.length && distance >= best.distance))
        )
          return;
        best = { correction, axes, distance, nearest };
      };
      for (const x of anchors[0])
        for (const y of anchors[1]) {
          if (![x, y, ...delta].every(Number.isFinite)) continue;
          const local = gridLocalPoint(basis, [x + delta[0], y + delta[1]]);
          const nearest = local.map((v) => Math.round(v / settings.gridSize) * settings.gridSize);
          const correction = nearest.map((v, a) => v - local[a]);
          const direction = gridOptions.direction;
          if (direction) {
            for (let a = 0; a < 2; a++) {
              const normal = a ? [-basis.sin, basis.cos] : [basis.cos, basis.sin];
              const denominator = normal[0] * direction[0] + normal[1] * direction[1];
              if (Math.abs(denominator) < 1e-12) continue;
              const move = direction.map((v) => (v * correction[a]) / denominator);
              if (Math.hypot(...move) <= radius) {
                const final = gridLocalPoint(basis, [
                  x + delta[0] + move[0],
                  y + delta[1] + move[1],
                ]);
                const axes = [a];
                if (Math.abs(final[1 - a] - nearest[1 - a]) < 1e-8) axes.push(1 - a);
                consider(move, local, nearest, axes);
              }
            }
          } else {
            const axes = [0, 1].filter((a) => Math.abs(correction[a]) <= radius);
            const [u, v] = correction.map((n, a) => (axes.includes(a) ? n : 0));
            consider(
              [u * basis.cos - v * basis.sin, u * basis.sin + v * basis.cos],
              local,
              nearest,
              axes,
            );
          }
        }
      if (best) {
        best.correction.forEach((v, a) => (result[a] += v));
        for (const axis of best.axes)
          matches.push({
            kind: 'grid',
            axis,
            position: best.nearest[axis],
            gridLine: true,
            line: gridLineSegment(doc, basis, axis, best.nearest[axis]),
          });
      }
    }
  }
  for (const match of matches)
    if (match.kind === 'character-gap')
      match.cross = Math.max(match.fixedCross, match.movingCross + result[1 - match.axis]);
  return { delta: result, matches };
}

export const layoutGridDefaults = () => ({
  columns: 3,
  rows: 3,
  marginX: 40,
  marginY: 40,
  gutterX: 20,
  gutterY: 20,
});
export function validateLayoutGrid(config) {
  if (!config || typeof config !== 'object' || Array.isArray(config))
    throw Error('Invalid layout grid');
  for (const key of ['columns', 'rows'])
    if (!Number.isInteger(config[key]) || config[key] < 1 || config[key] > 40)
      throw Error('Columns and rows must be integers from 1 to 40');
  for (const key of ['marginX', 'marginY', 'gutterX', 'gutterY'])
    if (!Number.isFinite(config[key]) || config[key] < 0 || config[key] > 100000)
      throw Error('Margins and gutters must be between 0 and 100000');
}
// A single generated set is replaced atomically; manual guides are never removed.
export function layoutGridLines(doc, config) {
  validateLayoutGrid(config);
  const lines = [];
  for (const [axis, size, count, margin, gutter] of [
    ['x', doc.width, config.columns, config.marginX, config.gutterX],
    ['y', doc.height, config.rows, config.marginY, config.gutterY],
  ]) {
    const cell = (size - 2 * margin - (count - 1) * gutter) / count;
    if (!Number.isFinite(cell) || cell <= 0)
      throw Error('Margins and gutters leave no space for cells');
    const positions = new Set();
    for (let i = 0; i < count; i++) {
      positions.add(Number((margin + i * (cell + gutter)).toFixed(6)));
      positions.add(Number((margin + i * (cell + gutter) + cell).toFixed(6)));
    }
    for (const position of positions) lines.push({ axis, position });
  }
  return lines;
}
export function setLayoutGrid(doc, config, makeId) {
  const a = doc.editor || guideDefaults();
  if (a.locked || a.guides.some((g) => g.origin === 'layout' && g.locked))
    throw Error('Unlock layout guides before replacing or removing them');
  const manual = a.guides.filter((g) => g.origin !== 'layout');
  const lines = config ? layoutGridLines(doc, config) : [];
  if (manual.length + lines.length > 200)
    throw Error('Maximum 200 guides; remove some guides or reduce the layout');
  const generated = lines.map((g) => ({ ...g, id: makeId(), locked: false, origin: 'layout' }));
  doc.editor = a;
  a.guides = [...manual, ...generated];
  if (config) a.layoutGrid = { ...config };
  else delete a.layoutGrid;
}
