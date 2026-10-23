import { node, connect } from '../../src/core.js';
import { alternateRepeaterFixture } from './repeater-alternate-scale.js';
export function fillFixture(kind = 'linear') {
  const f = alternateRepeaterFixture(kind);
  f.doc.name = `Repeater alternate fill ${kind}`;
  f.doc.schemaVersion = 46;
  f.repeat.params.alternateScale.value = 60;
  f.repeat.params.alternateColor.value = { hex: '#c43d27', alpha: 0.35 };
  f.fill.params.color.value = { hex: '#285d8f', alpha: 0.6 };
  const stroke = node('Stroke', 940, 40);
  stroke.id = 'alternate-stroke';
  stroke.params.color.value = { hex: '#17232d', alpha: 0.4 };
  stroke.params.width.value = 2;
  f.out.position = [1240, 40];
  f.layer.nodes.splice(-1, 0, stroke);
  connect(f.layer, f.fill.id, stroke.id, 'path');
  connect(f.layer, stroke.id, f.out.id, 'path');
  return { ...f, stroke };
}
