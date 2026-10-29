// REQ-211 first vertical: DOM-free source-over contribution accounting.
// The renderer supplies per-object coverage/opacity masks in back-to-front order.
// This module never infers a Color ID from an equal RGB value. Blends/filters
// require an explicit renderer attribution contract before they can use it.
// A two-stop layer may supply split:{startSlotId,endSlotId,startAlpha};
// startAlpha is already weighted full-paint coverage, not the start stop opacity.
// The complementary end mass and backdrop share the same single paint alpha.
const MAX_PIXELS = 262144;
const MAX_LAYERS = 256;
function validateAlpha(alpha, pixelCount) {
  if (
    !(Array.isArray(alpha) || alpha instanceof Float32Array || alpha instanceof Float64Array) ||
    alpha.length !== pixelCount
  )
    throw Error('Each layer needs a matching normalized alpha buffer');
  for (const value of alpha)
    if (!Number.isFinite(value) || value < 0 || value > 1)
      throw Error('Coverage alpha must be finite and between zero and one');
}
function validateSlot(id) {
  if (id !== null && id !== undefined && (typeof id !== 'string' || !idPattern.test(id)))
    throw Error('Invalid source Color slot ID');
}
const idPattern = /^[A-Za-z0-9_-]{1,128}$/;

export function measureColorCoverage(slotIds, layers, pixelCount) {
  if (
    !Array.isArray(slotIds) ||
    slotIds.length > 256 ||
    slotIds.some((id) => typeof id !== 'string' || !idPattern.test(id)) ||
    new Set(slotIds).size !== slotIds.length
  )
    throw Error('Expected unique stable Color slot IDs');
  if (!Number.isInteger(pixelCount) || pixelCount < 0 || pixelCount > MAX_PIXELS)
    throw Error('Coverage pixel count exceeds the bounded buffer');
  if (!Array.isArray(layers) || layers.length > MAX_LAYERS)
    throw Error('Coverage layer count exceeds the bounded pass');
  const indices = new Map(slotIds.map((id, index) => [id, index]));
  const otherIndex = slotIds.length;
  const prepared = Array.from(layers, (layer) => {
    if (!layer || typeof layer !== 'object')
      throw Error('Each layer needs a matching normalized alpha buffer');
    // Validate even fully occluded layers. Invalid input is never silently ignored.
    validateAlpha(layer.alpha, pixelCount);
    validateSlot(layer.slotId);
    let split;
    if (layer.split !== undefined) {
      const source = layer.split;
      if (
        !source ||
        typeof source !== 'object' ||
        Array.isArray(source) ||
        !Object.hasOwn(source, 'startSlotId') ||
        !Object.hasOwn(source, 'endSlotId') ||
        layer.slotId != null
      )
        throw Error('Invalid two-stop coverage attribution');
      validateSlot(source.startSlotId);
      validateSlot(source.endSlotId);
      validateAlpha(source.startAlpha, pixelCount);
      for (let pixel = 0; pixel < pixelCount; pixel++)
        if (source.startAlpha[pixel] > layer.alpha[pixel])
          throw Error('Stop contribution exceeds paint alpha');
      split = {
        startIndex: indices.get(source.startSlotId) ?? otherIndex,
        endIndex: indices.get(source.endSlotId) ?? otherIndex,
        startAlpha: source.startAlpha,
      };
    }
    return { index: indices.get(layer.slotId) ?? otherIndex, alpha: layer.alpha, split };
  });
  const masses = Array(slotIds.length + 1).fill(0);
  for (let pixel = 0; pixel < pixelCount; pixel++) {
    let remaining = 1;
    for (let index = prepared.length - 1; index >= 0 && remaining > 0; index--) {
      const layer = prepared[index];
      const alpha = layer.alpha[pixel];
      if (layer.split) {
        const start = layer.split.startAlpha[pixel];
        masses[layer.split.startIndex] += remaining * start;
        masses[layer.split.endIndex] += remaining * (alpha - start);
      } else masses[layer.index] += remaining * alpha;
      // Both stops belong to ONE paint: occlude the backdrop only once.
      remaining *= 1 - alpha;
    }
  }
  const total = masses.reduce((sum, mass) => sum + mass, 0);
  const rows = masses.map((contribution, index) => {
    const exact = total > 0 ? (contribution / total) * 100 : 0;
    return {
      id: slotIds[index] ?? null,
      contribution,
      percent: Math.floor(exact),
      remainder: exact % 1,
    };
  });
  if (total > 0) {
    const left = 100 - rows.reduce((sum, row) => sum + row.percent, 0);
    const ranked = [...rows].sort(
      (a, b) =>
        b.remainder - a.remainder ||
        (a.id === null ? 1 : b.id === null ? -1 : a.id < b.id ? -1 : a.id > b.id ? 1 : 0),
    );
    for (let index = 0; index < left; index++) ranked[index].percent++;
  }
  const publicRow = ({ id, contribution, percent }) => ({ id, contribution, percent });
  return {
    slots: rows.slice(0, otherIndex).map(publicRow),
    other: publicRow(rows[otherIndex]),
    paintedPixelEquivalent: total,
    samplePixels: pixelCount,
    mode: 'source-over-visible-contribution-v1',
  };
}
