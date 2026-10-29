// Independent authored-fixture oracle; never inspect production coverage passes
// or infer shared identity from the fixture's equal display colors.
export function fixturePaintSlotId(rectangle, paint, strokeId) {
  if (paint === 'stroke') return strokeId;
  const gradient = rectangle.gradient;
  if (!gradient) return rectangle.id ?? null;
  const ids = (gradient.kind === 'angular' ? gradient.ids : [gradient.startId, gradient.endId]).map(
    (id) => id ?? null,
  );
  return ids.every((id) => id === ids[0]) ? ids[0] : null;
}
