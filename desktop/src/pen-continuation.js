// Stable endpoint/contour selection only; common Session owns all mutation.
export function penContourIds(node, contourId) {
  if (node.type === 'Curve') {
    if (contourId !== undefined) throw Error('Curve continuation has no separate contour target');
    return node.points.map((p) => p.id);
  }
  if (node.type === 'Multi Path') {
    const contour = node.contours.find((c) => c.id === contourId);
    if (!contour) throw Error('Missing continuation contour');
    return contour.points;
  }
  throw Error('Only owned Curve or Multi Path endpoints can be continued');
}
export function resolvePenEndpoint(layer, nodeId, endpointId, contourId) {
  if (!layer || layer.locked || !layer.visible || layer.guide)
    throw Error('Select unlocked visible artwork');
  const node = layer.nodes.find((n) => n.id === nodeId);
  if (!node || node.locked || node.bypassed) throw Error('Select an unlocked editable path');
  const ids = penContourIds(node, contourId);
  if (ids.length < 2) throw Error('Continuation needs an existing path with two points');
  if (node.type === 'Curve') {
    const q = node.params.closed;
    if (q.value || q.source || q.expression || q.locked)
      throw Error('Continue an unlocked open path with literal closure');
  } else if (node.contours.find((c) => c.id === contourId).closed)
    throw Error('Continue an open contour');
  if (endpointId !== ids[0] && endpointId !== ids.at(-1))
    throw Error('Choose an open endpoint, not an interior point');
  const endpoint = node.points.find((p) => p.id === endpointId);
  if (
    !endpoint ||
    ['position', 'handleA', 'handleB'].some(
      (k) => endpoint[k].locked || endpoint[k].source || endpoint[k].expression,
    )
  )
    throw Error('The endpoint is locked or driven');
  const side = endpointId === ids[0] ? 'head' : 'tail';
  return {
    node,
    side,
    contourId,
    pointId: endpointId,
    closingPointId: side === 'head' ? ids.at(-1) : ids[0],
  };
}
export function insertContinuationPoint(node, contourId, side, point) {
  if (node.points.length >= 10000) throw Error('Pen paths are limited to 10000 points');
  if (node.type === 'Curve') node.points[side === 'head' ? 'unshift' : 'push'](point);
  else {
    const ids = penContourIds(node, contourId);
    node.points.push(point);
    ids[side === 'head' ? 'unshift' : 'push'](point.id);
  }
}
