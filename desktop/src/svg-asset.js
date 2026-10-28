import {
  blankLayer,
  node,
  connect,
  outputType,
  nodeDependencies,
  joinInputKeys,
  evaluate,
  validate,
  serialize,
} from './core.js';
import { importSVGTree } from './svg-import.js';
import { svgComposition } from './svg-composition.js';
import { svgPartTarget } from './svg-part.js';

export const SVG_ASSET_SCHEMA_VERSION = 53;
export function validateSVGAsset(n, version) {
  if (n.type !== 'SVG') {
    if (n.svgAsset !== undefined) throw Error('SVG provenance requires an SVG node');
    return;
  }
  if (version < SVG_ASSET_SCHEMA_VERSION) throw Error('SVG requires schemaVersion 53');
  validateAssetProvenance(n.svgAsset);
  if (!n.compositionReference) throw Error('Missing SVG source');
}
export function validateAssetProvenance(a) {
  if (
    !a ||
    a.version !== 1 ||
    a.mode !== 'embedded' ||
    Object.keys(a).some(
      (k) =>
        ![
          'version',
          'mode',
          'rootId',
          'relativePath',
          'sourceLocation',
          'sha256',
          'author',
          'source',
          'license',
        ].includes(k),
    )
  )
    throw Error('Invalid SVG asset provenance');
  for (const key of ['rootId', 'relativePath', 'sourceLocation', 'author', 'source', 'license'])
    if (typeof a[key] !== 'string' || a[key].length > 2048 || /[\u0000-\u001f]/.test(a[key]))
      throw Error('Invalid SVG asset provenance');
  if (
    !/^[a-zA-Z0-9_-]{1,100}$/.test(a.rootId) ||
    !/^[a-f0-9]{64}$/.test(a.sha256) ||
    !a.relativePath ||
    a.relativePath.includes('\\') ||
    a.relativePath.startsWith('/') ||
    a.relativePath.split('/').some((p) => !p || p === '.' || p === '..' || p.includes(':'))
  )
    throw Error('Invalid SVG source identity');
}

function appendToOutput(target, output, ref, y) {
  let previous = output.params.path.source;
  const terminal = target.nodes.find((n) => n.id === previous);
  // Count edges as well as consumers: retained inactive/expressed/reference
  // dependencies must not silently acquire the newly inserted artwork.
  const edges = target.nodes.flatMap((n) =>
    nodeDependencies(n)
      .filter((d) => d.source === previous)
      .map(() => n.id),
  );
  if (
    terminal?.type === 'Join Geometry' &&
    terminal.paintMode &&
    !terminal.locked &&
    !terminal.bypassed &&
    edges.length === 1 &&
    edges[0] === output.id
  ) {
    const keys = joinInputKeys(terminal);
    const lastUsed = keys.reduce(
      (last, key, i) =>
        terminal.params[key].source ||
        terminal.params[key].expression ||
        terminal.params[key].locked
          ? i
          : last,
      -1,
    );
    const key = keys.slice(lastUsed + 1).find((key) => !terminal.params[key].locked);
    if (key) {
      connect(target, ref.id, terminal.id, key);
      return { joinId: terminal.id, reusedJoin: true };
    }
  }
  if (previous && outputType(terminal) === 'Path') {
    const fill = node('Fill', 350, y);
    fill.params.color.value = { hex: target.fill, alpha: 1 };
    target.nodes.push(fill);
    connect(target, previous, fill.id, 'path');
    previous = fill.id;
  }
  const join = node('Join Geometry', 660, y);
  target.nodes.push(join);
  if (previous) connect(target, previous, join.id, 'a');
  connect(target, ref.id, join.id, previous ? 'b' : 'a');
  connect(target, join.id, output.id, 'path');
  return { joinId: join.id, reusedJoin: false };
}

// Called only on a Session-private candidate. Import, source registration,
// layer creation, wiring and provenance share one history admission.
export function insertSVGAsset(
  doc,
  { route, layerId, source, name, provenance, at = [0, 0], joinBeforeOut = false },
  fonts = {},
) {
  if (!['layer', 'graph'].includes(route) || typeof joinBeforeOut !== 'boolean')
    throw Error('Invalid SVG placement route');
  if (typeof name !== 'string' || !name.trim() || name.length > 255 || /[\u0000-\u001f]/.test(name))
    throw Error('Invalid SVG name');
  if (
    !Array.isArray(at) ||
    at.length !== 2 ||
    !at.every((v) => Number.isFinite(v) && Math.abs(v) <= 100000)
  )
    throw Error('Invalid SVG position');
  let target;
  if (route === 'layer') {
    target = blankLayer(name.trim());
    doc.layers.push(target);
    // A fresh blank Output's UI socket lock is not authored user intent.
    target.nodes.find((n) => n.id === target.output).params.path.locked = false;
  } else {
    target = doc.layers.find((l) => l.id === layerId);
    if (!target || target.locked || target.guide || !target.visible)
      throw Error('Choose an unlocked visible layer');
  }
  const output = route === 'layer' || joinBeforeOut ? svgPartTarget(doc, target.id).output : null;
  const embedded = svgComposition(importSVGTree(source), name.trim());
  const y = Math.max(0, ...target.nodes.map((n) => n.position[1])) + 180;
  const ref = node('SVG', 40, y);
  ref.name = name.trim();
  ref.compositionReference = { compositionId: embedded.id };
  ref.svgAsset = structuredClone(provenance);
  ref.params.position.value = [...at];
  validateSVGAsset(ref, SVG_ASSET_SCHEMA_VERSION);
  target.nodes.push(ref);
  doc.layers.push(embedded);
  const wiring = output
    ? appendToOutput(target, output, ref, y)
    : { joinId: null, reusedJoin: false };
  doc.schemaVersion = Math.max(doc.schemaVersion, SVG_ASSET_SCHEMA_VERSION);
  if (target.nodes.length > 2000 || embedded.nodes.length > 2000)
    throw Error('SVG insertion exceeds the 2000-node budget');
  validate(doc);
  // Disconnected insertion still validates its source through the same evaluator.
  const result = evaluate({ ...target, output: ref.id }, fonts, doc.imageAssets, doc);
  if (result.error) throw Error(`SVG asset rejected: ${result.error}`);
  if (output) {
    const rendered = evaluate(target, fonts, doc.imageAssets, doc);
    if (rendered.error) throw Error(`SVG placement rejected: ${rendered.error}`);
  }
  serialize(doc);
  return { layerId: target.id, nodeId: ref.id, sourceLayerId: embedded.id, ...wiring };
}
