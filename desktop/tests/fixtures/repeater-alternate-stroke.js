import { fillFixture } from './repeater-alternate-fill.js';
export function strokeFixture(kind = 'linear') {
  const f = fillFixture(kind);
  f.doc.name = `Repeater alternate stroke ${kind}`;
  f.doc.schemaVersion = 47;
  f.repeat.params.alternateFill.value = true;
  f.repeat.params.alternateStrokeColor.value = { hex: '#7828a3', alpha: 0.5 };
  f.stroke.params.width.value = 3;
  f.stroke.params.miterLimit.value = 7;
  f.stroke.params.dashOffset.value = 1.5;
  f.stroke.strokeCap = 'round';
  f.stroke.strokeJoin = 'bevel';
  f.stroke.strokeDash = [4, 2];
  return f;
}
