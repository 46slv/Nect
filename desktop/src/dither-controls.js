export const DITHER_SCHEMA_VERSION = 36;
export const IMAGE_DITHER_SCHEMA_VERSION = 41;
export const imageDitherTypes = ['Ordered Dither Image', 'Error Diffusion Dither Image'];
export const isImageDither = (type) => imageDitherTypes.includes(type);
export const isDither = (type) =>
  ['Ordered Dither', 'Error Diffusion Dither'].includes(type) || isImageDither(type);
export const hasDotControls = (type) => type === 'Halftone' || isDither(type);

export function ditherSettings(shape, dotSize, sides = 4, rotation = 0, innerRatio = 0.25) {
  if (!['legacy', 'square', 'circle', 'polygon', 'star'].includes(shape))
    throw Error('Dither dot shape must be Square, Circle, Polygon or Star');
  if (!Number.isFinite(dotSize) || dotSize < 1 || dotSize > 1024)
    throw Error('Dither dot size must be from 1 to 1024 px');
  if (!Number.isInteger(sides) || sides < 3 || sides > 32)
    throw Error('Dither sides / star points must be an integer from 3 to 32');
  if (!Number.isFinite(rotation) || Math.abs(rotation) > 360)
    throw Error('Dither dot rotation must be within ±360 degrees');
  if (!Number.isFinite(innerRatio) || innerRatio < 0.05 || innerRatio > 0.95)
    throw Error('Dither star inner radius ratio must be from 0.05 to 0.95');
  return { shape, dotSize, sides, rotation, innerRatio };
}

export function readDotSettings(node, read) {
  return hasDotControls(node.type)
    ? ditherSettings(
        node.type === 'Halftone' && node.operation === 'subtract' ? 'legacy' : node.operation,
        read('dotSize'),
        read('dotSides'),
        read('dotRotation'),
        read('starInnerRatio'),
      )
    : {};
}

export function dotFingerprint(settings, pitch) {
  return settings.shape &&
    settings.shape !== 'legacy' &&
    (settings.shape !== 'square' || settings.dotSize !== pitch || settings.rotation !== 0)
    ? [settings]
    : [];
}

// Copy the whole authored pitch parameter, including a socket/expression, so
// legacy full-cell squares retain their exact size when a file is reopened.
export function migrateDither(node, sourceVersion) {
  if (!hasDotControls(node.type) || sourceVersion >= DITHER_SCHEMA_VERSION) return;
  if (!Object.hasOwn(node.params, 'dotSize'))
    node.params.dotSize = structuredClone(node.params.pitch);
  for (const [key, value] of [
    ['dotSides', 4],
    ['dotRotation', 0],
    ['starInnerRatio', 0.25],
  ])
    if (!Object.hasOwn(node.params, key))
      node.params[key] = { type: 'Scalar', value, expression: '', locked: false };
  if (
    hasDotControls(node.type) &&
    ['union', 'subtract', 'intersect', 'difference'].includes(node.operation)
  )
    node.operation = isDither(node.type) ? 'square' : 'subtract';
}

export function ditherVertexBuilder(settings, maximumVertices = 100000, modulateSize = false) {
  const { shape, dotSize, sides, rotation, innerRatio } = ditherSettings(
    settings.shape,
    settings.dotSize,
    settings.sides,
    settings.rotation,
    settings.innerRatio,
  );
  // Multiples of four put vertices exactly on each cardinal axis. The circle
  // is inscribed; polygon sagitta stays below 0.1 document px.
  let segments = 24;
  while ((dotSize / 2) * (1 - Math.cos(Math.PI / segments)) > 0.1) segments += 4;
  let vertices = 0;
  return (cell) => {
    const count =
      shape === 'square'
        ? 4
        : shape === 'polygon'
          ? sides
          : shape === 'star'
            ? sides * 2
            : segments;
    vertices += count;
    if (vertices > maximumVertices)
      throw Error(`Luminance mark output is limited to ${maximumVertices} generated vertices`);
    const half = (dotSize / 2) * (modulateSize ? Math.sqrt(cell.darkness) : 1);
    const angleOffset = (rotation * Math.PI) / 180;
    let polygon =
      shape === 'square'
        ? [
            [cell.x - half, cell.y - half],
            [cell.x + half, cell.y - half],
            [cell.x + half, cell.y + half],
            [cell.x - half, cell.y + half],
          ]
        : Array.from({ length: count }, (_, index) => {
            const angle =
              (2 * Math.PI * index) / count + angleOffset - (shape === 'circle' ? 0 : Math.PI / 2);
            const radius = half * (shape === 'star' && index % 2 ? innerRatio : 1);
            return [cell.x + radius * Math.cos(angle), cell.y + radius * Math.sin(angle)];
          });
    if (shape === 'square' && rotation !== 0)
      polygon = polygon.map(([x, y]) => {
        const dx = x - cell.x,
          dy = y - cell.y;
        return [
          cell.x + dx * Math.cos(angleOffset) - dy * Math.sin(angleOffset),
          cell.y + dx * Math.sin(angleOffset) + dy * Math.cos(angleOffset),
        ];
      });
    if (!polygon.every((point) => point.every(Number.isFinite)))
      throw Error('Dither dot coordinates exceed the finite document range');
    return polygon;
  };
}
