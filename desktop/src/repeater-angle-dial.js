// Editor-only angle math. Authored rotation is never normalized or rounded.
export function angleDialIndicator(degrees) {
  const angle = degrees % 360;
  return angle < 0 ? angle + 360 : angle;
}
// Document/screen convention: 0° points right, positive travel is clockwise.
// The dead zone breaks continuity, so crossing the center cannot cause a 180° jump.
export function angleDialPointer(rect, point) {
  const x = point[0] - (rect.left + rect.width / 2),
    y = point[1] - (rect.top + rect.height / 2);
  return Math.hypot(x, y) <= 4 ? null : (Math.atan2(y, x) * 180) / Math.PI;
}
export function advanceAngleDial({ angle, delta }, next) {
  let change = angle === null || next === null ? 0 : next - angle;
  if (change > 180) change -= 360;
  if (change < -180) change += 360;
  return { angle: next, delta: delta + change };
}
