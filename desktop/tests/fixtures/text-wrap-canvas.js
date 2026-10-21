import { blankDocument, node, connect, createCharacterPositions } from '../../src/core.js';
import { basicTextWrap } from '../../src/text-wrap.js';

export function textWrapCanvasFixture({ transformed = false } = {}) {
  const doc = blankDocument(),
    layer = doc.layers[0];
  doc.name = 'Canvas Text wrap width';
  // Existing basic-wrap policy requires v34; the new handle needs no schema change.
  doc.schemaVersion = Math.max(34, doc.schemaVersion);
  layer.id = 'wrap-canvas-layer';
  const text = node('Text'),
    fill = node('Fill'),
    output = layer.nodes[0];
  text.id = 'wrap-canvas-text';
  fill.id = 'wrap-canvas-fill';
  output.id = 'wrap-canvas-output';
  text.text = 'Alpha beta gamma delta epsilon zeta';
  text.characterPositions = createCharacterPositions(text.text);
  text.params.position.value = [140, 160];
  text.params.size.value = 32;
  text.textWrap = basicTextWrap('horizontal-tb', 320);
  let source = text;
  const nodes = [text];
  if (transformed) {
    const transform = node('Transform');
    transform.id = 'wrap-canvas-transform';
    transform.params.scale.value = 1.5;
    transform.params.offset.value = [20, 30];
    nodes.push(transform);
    source = transform;
  }
  layer.nodes = [...nodes, fill, output];
  layer.nodes.forEach((n, i) => (n.position = [40 + i * 340, 40]));
  layer.output = output.id;
  if (transformed) connect(layer, text.id, source.id, 'path');
  connect(layer, source.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layerId: layer.id, textId: text.id, transformId: transformed ? source.id : null };
}
