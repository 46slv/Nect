import { node, blankLayer, connect, outputType, evaluate, validate, serialize } from './core.js';
import { importSVGTree } from './svg-import.js';

// SVG parts reuse saved vector graphs and Composition Reference. There is no
// browser SVG renderer, external URL, file binding or second scene authority.
export function appendSVGPart(
  doc,
  { layerId, source, name = 'Imported SVG', at = [0, 0] },
  fonts = {},
) {
  const target = doc.layers.find((l) => l.id === layerId);
  if (!target || target.locked || target.guide || !target.visible)
    throw Error('Choose an unlocked visible Composition for the SVG node');
  if (typeof name !== 'string' || !name.trim() || name.length > 255 || /[\u0000-\u001f]/.test(name))
    throw Error('Invalid SVG name');
  if (
    !Array.isArray(at) ||
    at.length !== 2 ||
    !at.every((n) => Number.isFinite(n) && Math.abs(n) <= 100000)
  )
    throw Error('Invalid SVG Position');
  const output = target.nodes.find((n) => n.id === target.output);
  if (output?.type !== 'Output') throw Error('Add SVG to a path Composition with a regular Output');
  const socket = output.params.path;
  if (output.locked || socket.locked || socket.expression || socket.sourcePort !== undefined)
    throw Error('Unlock the Composition Output before adding SVG');
  const imported = importSVGTree(source);
  // One hidden, editable source graph retains the original paint order. The
  // parser has already minted all IDs and rejected unsupported markup.
  const embedded = blankLayer(`SVG source · ${name.trim()}`);
  embedded.visible = false;
  embedded.nodes = [];
  let sources = [];
  for (const [index, l] of imported.layers.entries()) {
    const out = l.nodes.find((n) => n.id === l.output);
    if (!out?.params.path.source) throw Error('SVG contains an empty output');
    const nodes = l.nodes.filter((n) => n.id !== out.id);
    for (const n of nodes) n.position = [n.position[0], n.position[1] + index * 180];
    embedded.nodes.push(...nodes);
    let sourceId = out.params.path.source;
    if (outputType(nodes.find((n) => n.id === sourceId)) === 'Path') {
      const transparent = node('Fill', 1250, index * 180 + 40);
      transparent.params.color.value.alpha = 0;
      embedded.nodes.push(transparent);
      connect(embedded, sourceId, transparent.id, 'path');
      sourceId = transparent.id;
    }
    sources.push(sourceId);
  }
  while (sources.length > 1) {
    const next = [];
    for (let i = 0; i < sources.length; i += 2) {
      if (i + 1 === sources.length) {
        next.push(sources[i]);
        continue;
      }
      const join = node('Join Geometry', 1500, i * 90);
      embedded.nodes.push(join);
      connect(embedded, sources[i], join.id, 'a');
      connect(embedded, sources[i + 1], join.id, 'b');
      next.push(join.id);
    }
    sources = next;
  }
  const final = node('Output', 1850, 40);
  embedded.nodes.push(final);
  connect(embedded, sources[0], final.id, 'path');
  embedded.output = final.id;
  doc.layers.push(embedded);
  const y = Math.max(0, ...target.nodes.map((n) => n.position[1])) + 180;
  const ref = node('Composition Reference', 40, y);
  ref.name = `SVG · ${name.trim()}`;
  ref.compositionReference = { compositionId: embedded.id };
  ref.params.position.value = [...at];
  target.nodes.push(ref);
  let previous = socket.source;
  if (previous) {
    const previousNode = target.nodes.find((n) => n.id === previous);
    if (outputType(previousNode) === 'Path') {
      // A bare Path Output historically uses the layer's solid fill. Make that
      // appearance explicit before combining it with independently painted SVG.
      const fill = node('Fill', 350, y);
      fill.params.color.value = { hex: target.fill, alpha: 1 };
      target.nodes.push(fill);
      connect(target, previous, fill.id, 'path');
      previous = fill.id;
    }
    const join = node('Join Geometry', 660, y);
    target.nodes.push(join);
    connect(target, previous, join.id, 'a');
    connect(target, ref.id, join.id, 'b');
    connect(target, join.id, output.id, 'path');
  } else connect(target, ref.id, output.id, 'path');
  doc.schemaVersion = Math.max(doc.schemaVersion, 31);
  validate(doc);
  const result = evaluate(target, fonts, doc.imageAssets, doc);
  if (result.error) throw Error(`SVG node rejected: ${result.error}`);
  serialize(doc);
  return { layerId, nodeId: ref.id, sourceLayerId: embedded.id };
}
