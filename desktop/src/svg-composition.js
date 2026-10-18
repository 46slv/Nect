import { blankLayer, node, point, connect, evaluate } from './core.js';

const paintTypes = new Set(['Fill', 'Stroke', 'Linear Gradient Fill', 'Radial Gradient Fill']);
const sourceTypes = new Set(['Imported Path', 'Rectangle', 'Circle', 'Ellipse']);
// Materializing every anchor as three authored sockets is expensive for dense
// rectangle batches. Larger sources keep the compact, editable Remap route.
const directEditPointLimit = 1024;

// Only freshly imported SVG graphs enter here. Existing saved graphs are never
// rewritten. Keep independent contour groups and consecutive paint order: a
// geometry Join paints each input separately, including holes and alpha overlap.
export function svgComposition(imported, name) {
  const composition = blankLayer(`SVG source · ${name}`);
  composition.visible = false;
  composition.nodes = [];
  const branches = [];
  let row = 0;
  for (const layer of imported.layers) {
    const paints = layer.nodes.filter((n) => paintTypes.has(n.type));
    const key = JSON.stringify(
      paints.map((n) => [
        n.type,
        Object.entries(n.params).filter(([key]) => key !== 'path'),
        n.strokeCap,
        n.strokeJoin,
        n.strokeDash,
      ]),
    );
    const evaluated = evaluate(layer);
    if (evaluated.error) throw Error(evaluated.error);
    const sources = layer.nodes.filter((n) => sourceTypes.has(n.type));
    for (const source of sources) {
      const path = evaluated.run(source.id);
      let geometry;
      if (path.contours.reduce((sum, c) => sum + c.points.length, 0) <= directEditPointLimit) {
        geometry = node('Multi Path', 40, 40 + row * 180);
        geometry.name = sources.length === 1 ? layer.name : `${layer.name} · ${row + 1}`;
        geometry.contours = path.contours.map(({ id, closed, group, points }) => ({
          id,
          closed,
          ...(group !== undefined ? { group } : {}),
          points: points.map((p) => p.id),
        }));
        geometry.points = path.contours.flatMap((c) =>
          c.points.map((p) => {
            const authored = point(...p.position);
            authored.id = p.id;
            authored.handleA.value = [...p.handleA];
            authored.handleB.value = [...p.handleB];
            return authored;
          }),
        );
        geometry.units = path.contours.length
          ? [{ id: `${geometry.id}:unit`, contours: path.contours.map((c) => c.id) }]
          : [];
        composition.nodes.push(geometry);
      } else {
        // Preserve both dense-source performance and the Imported Path budget
        // above Multi Path's 10,000 vertices, with the existing editing route.
        source.position = [40, 40 + row * 180];
        const remap = node('Path Remap', 350, source.position[1]);
        composition.nodes.push(source, remap);
        connect(composition, source.id, remap.id, 'path');
        geometry = remap;
      }
      geometry.collapsed = true;
      const groups = new Set(path.contours.map((c) => c.group)).size;
      let branch = branches.at(-1);
      if (!branch || branch.key !== key || branch.groups + groups > 256) {
        branch = { key, paints: structuredClone(paints), sources: [], groups: 0, row };
        branches.push(branch);
      }
      branch.sources.push(geometry.id);
      branch.groups += groups;
      row++;
    }
  }
  function join(sources, x, y) {
    const joined = node('Join Geometry', x, y);
    composition.nodes.push(joined);
    sources.forEach((source, i) =>
      connect(composition, source, joined.id, i === 0 ? 'a' : joined.joinInputOrder.at(-1)),
    );
    return joined.id;
  }
  const appearances = branches.map((branch) => {
    const y = 40 + branch.row * 180;
    let source = join(branch.sources, 650, y);
    branch.paints.forEach((paint, index) => {
      // Every retained appearance branch needs distinct authored node IDs.
      const copied = { ...paint, id: node(paint.type).id, position: [960 + index * 310, y] };
      composition.nodes.push(copied);
      connect(composition, source, copied.id, 'path');
      source = copied.id;
    });
    return source;
  });
  const source = appearances.length === 1 ? appearances[0] : join(appearances, 1580, 40);
  const output = node('Output', 1890, 40);
  composition.nodes.push(output);
  connect(composition, source, output.id, 'path');
  composition.output = output.id;
  return composition;
}
