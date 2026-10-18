import { sampleLuminanceGrid } from './luminance-field.js';
import { sampleImageLuminanceGrid } from './image-luminance-field.js';
import { prepareLuminanceMask } from './luminance-mask.js';
import { selectLuminanceCenters, luminanceInstanceSettings } from './luminance-instances.js';

// A procedural Path of disconnected anchors. Per-point transforms are derived
// data consumed directly by Instance on Points, never a second authored model.
export function luminanceToPoints(source, options) {
  if (!source) throw Error('Luminance to Points requires a Source fill or Image');
  const { prefix = 'luminance-points', asset, pitch, origin, invert, tonePower } = options;
  const sampling = { pitch, origin, invert, tonePower };
  const settings = luminanceInstanceSettings((key) => options[key]);
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Luminance points need a stable ID prefix');
  let field, mask;
  if (source.kind === 'image') {
    field = sampleImageLuminanceGrid(asset, source.position, source.size, sampling);
  } else {
    if (!source.fill) throw Error('Luminance to Points requires a filled source');
    if (source.contours?.length === 0)
      field = sampleLuminanceGrid(source.fill, { minX: 0, minY: 0, maxX: 0, maxY: 0 }, sampling);
    else {
      mask = prepareLuminanceMask(source);
      field = sampleLuminanceGrid(source.fill, mask.bounds, sampling);
    }
  }
  const cells = selectLuminanceCenters(field.cells, mask, source.kind === 'image' ? source : null);
  const contours = [];
  for (const cell of cells) {
    const scale = settings.minScale + (settings.maxScale - settings.minScale) * cell.darkness;
    const rotation =
      settings.minRotation + (settings.maxRotation - settings.minRotation) * cell.darkness;
    if (scale === 0) continue;
    if (contours.length >= 256)
      throw Error('Luminance to Points exceeds the 256 active-point instance budget');
    const id = `${prefix}:cell:${cell.i}:${cell.j}`;
    contours.push({
      id,
      closed: false,
      points: [
        {
          id: `${id}:p`,
          position: [cell.x, cell.y],
          handleA: [0, 0],
          handleB: [0, 0],
          instanceTransform: { scale, rotation },
          luminance: cell.luminance,
          darkness: cell.darkness,
        },
      ],
    });
  }
  return { contours };
}
