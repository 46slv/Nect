import { outputType, bypassContract, joinInputKeys } from './core.js';

const geometry = (type) => type === 'Path' || type === 'PaintedPath';
const serial = new Set(['Output', 'Fill', 'Stroke', 'Text to Path', 'Transform', 'Path Remap']);
// Content provenance, deliberately not nodeDependencies (which includes numeric refs and attachments).
export function queryShapeSources(doc, root, revision) {
  const result = { root: { ...root }, revision, candidates: [], complete: true, diagnostics: [] };
  const issue = (nodeId, reason) => {
    result.complete = false;
    result.diagnostics.push({ nodeId, reason });
  };
  const layer = doc.layers.find((item) => item.id === root.layerId);
  if (!layer) {
    issue(root.nodeId, 'Missing composition');
    return result;
  }
  const nodes = new Map(layer.nodes.map((item) => [item.id, item]));
  const queue = [{ id: root.nodeId, port: root.port, route: [], ancestors: [] }];
  const seen = new Set(),
    adjacency = new Map();
  for (let index = 0; index < queue.length; index++) {
    const visit = queue[index],
      n = nodes.get(visit.id);
    if (visit.ancestors.includes(visit.id)) {
      issue(visit.id, 'Cycle in geometry inputs');
      continue;
    }
    const identity = JSON.stringify([visit.id, visit.port]);
    if (seen.has(identity)) continue;
    seen.add(identity);
    if (!n) {
      issue(visit.id, 'Missing source node');
      continue;
    }
    if (!geometry(outputType(n, visit.port))) {
      issue(n.id, 'Not a shape-content output');
      continue;
    }
    if (['Text', 'Curve'].includes(n.type)) {
      result.candidates.push({
        layerId: layer.id,
        nodeId: n.id,
        port: visit.port,
        type: n.type,
        name: n.name,
        detail: n.type === 'Text' ? n.text.slice(0, 80) : `${n.points.length} owned points`,
        route: visit.route,
        ...directGeometryUses(layer, n.id, visit.port),
      });
      continue;
    }
    let keys;
    if (n.bypassed) {
      const contract = bypassContract(n, layer);
      if (!contract.supported || !geometry(contract.type)) {
        issue(n.id, 'Unsupported bypass');
        continue;
      }
      keys = [contract.input];
    } else if (serial.has(n.type)) keys = ['path'];
    else if (n.type === 'Join Geometry') keys = joinInputKeys(n);
    else if (n.type === 'Boolean') keys = ['a', 'b'];
    else {
      issue(n.id, `Source search unsupported for ${n.type}`);
      continue;
    }
    if (visit.route.length >= 256) {
      issue(n.id, 'Source search depth limit (256)');
      continue;
    }
    const edges = [];
    adjacency.set(n.id, edges);
    for (const key of keys) {
      const q = n.params?.[key];
      // Join's empty spare slots are not missing branches.
      if (n.type === 'Join Geometry' && !n.bypassed && q && !q.source) continue;
      if (!q || !geometry(q.type) || !q.source) {
        issue(n.id, `Missing geometry input: ${key}`);
        continue;
      }
      const source = nodes.get(q.source);
      if (!source || !geometry(outputType(source, q.sourcePort))) {
        issue(n.id, `Missing or non-geometry source: ${key}`);
        continue;
      }
      edges.push(source.id);
      queue.push({
        id: source.id,
        port: q.sourcePort,
        route: [...visit.route, { nodeId: n.id, input: key }],
        ancestors: [...visit.ancestors, n.id],
      });
    }
  }
  // Kahn's check also catches cross-branch cycles hidden by BFS deduplication.
  const degrees = new Map([...adjacency.keys()].map((id) => [id, 0]));
  for (const edges of adjacency.values())
    for (const id of edges) if (degrees.has(id)) degrees.set(id, degrees.get(id) + 1);
  const ready = [...degrees].filter(([, count]) => count === 0).map(([id]) => id);
  for (let i = 0; i < ready.length; i++)
    for (const id of adjacency.get(ready[i]) || []) {
      if (!degrees.has(id)) continue;
      degrees.set(id, degrees.get(id) - 1);
      if (degrees.get(id) === 0) ready.push(id);
    }
  if (ready.length < degrees.size && !result.diagnostics.some((d) => d.reason.startsWith('Cycle')))
    issue(root.nodeId, 'Cycle in geometry inputs');
  return result;
}

// Saved direct geometry sockets in this composition, not visible occurrences or transitive instances.
export function directGeometryUses(layer, nodeId, port) {
  let inputs = 0;
  const owners = new Set();
  for (const n of layer.nodes)
    for (const q of Object.values(n.params || {}))
      if (geometry(q.type) && q.source === nodeId && q.sourcePort === port) {
        inputs++;
        owners.add(n.id);
      }
  return { directInputs: inputs, directNodes: owners.size };
}
