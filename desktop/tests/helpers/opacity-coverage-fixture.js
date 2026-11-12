import { blankDocument, node, connect } from '../../src/core.js';

export function opacityCoverageDocument({
  count = 2,
  start = 25,
  end = 75,
  copiesAbove = true,
} = {}) {
  const doc = blankDocument(),
    foreground = doc.layers[0],
    background = blankDocument().layers[0],
    output = foreground.nodes[0],
    backOutput = background.nodes[0],
    path = node('Imported Path'),
    repeater = node('Geometry Repeater'),
    fill = node('Fill'),
    backShape = node('Rectangle'),
    backFill = node('Fill');
  doc.schemaVersion = 54;
  background.name = 'Background';
  doc.width = 64;
  doc.height = 48;
  doc.colorSlots = [
    { id: 'red', name: 'Red', value: { hex: '#ff0000', alpha: 0.7 } },
    { id: 'blue', name: 'Blue', value: { hex: '#0000ff', alpha: 0.6 } },
  ];
  path.rawPath = 'M4.4 4.2H40.1V36.3H4.4Z M12.2 12.4V24.1H20.3V12.4Z';
  repeater.operation = 'ae-2d';
  repeater.params.count.value = count;
  repeater.params.position.value = [16, 3];
  repeater.params.startOpacity.value = start;
  repeater.params.endOpacity.value = end;
  repeater.params.copiesAbove.value = copiesAbove;
  fill.params.color.colorSlotId = 'red';
  foreground.nodes = [path, repeater, fill, output];
  connect(foreground, path.id, repeater.id, 'path');
  connect(foreground, repeater.id, fill.id, 'path');
  connect(foreground, fill.id, output.id, 'path');
  backShape.params.center.value = [32, 24];
  backShape.params.size.value = [64, 48];
  backFill.params.color.colorSlotId = 'blue';
  background.nodes = [backShape, backFill, backOutput];
  connect(background, backShape.id, backFill.id, 'path');
  connect(background, backFill.id, backOutput.id, 'path');
  doc.layers = [background, foreground];
  return doc;
}
