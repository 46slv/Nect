import { blankDocument, node, connect } from '../../src/core.js';
import { createKeyColorRecipe, editKeyColorRecipe } from '../../src/key-color-recipe.js';
import { DEFAULT_HARMONY_WHEEL } from '../../src/harmony-wheel.js';

// Explicit test-only coefficients, never product presets or a UI bootstrap.
export function retainedRecipeFixture() {
  let snapshot = createKeyColorRecipe({
    paletteId: 'test-only-experimental-palette',
    keyColor: { id: 'key', role: 'Identity', value: { hex: '#aBcD09', alpha: 0 } },
    preset: {
      id: 'test-only-coefficients-not-defaults',
      version: 1,
      harmony: 'triadic',
      hueSteps: 12,
      slots: [
        {
          id: 'derived',
          role: 'Background',
          position: 1,
          lightnessScale: 0.7,
          lightnessOffset: 0.1,
          chromaScale: 0.8,
          chromaOffset: 0,
        },
        {
          id: 'dormant',
          role: 'Accent',
          position: 2,
          lightnessScale: 0.7,
          lightnessOffset: 0.1,
          chromaScale: 0.8,
          chromaOffset: 0,
          alpha: 0.22,
        },
      ],
    },
    slots: [
      { id: 'derived', role: 'Background', deltaL: 0.03, deltaRatioCmax: 0.07 },
      { id: 'dormant', role: 'Accent', deltaL: -0.02, deltaRatioCmax: 0.05 },
    ],
    settings: { areaAware: false },
  });
  snapshot = editKeyColorRecipe(snapshot, {
    type: 'preset',
    expectedRevision: snapshot.revision,
    preset: { ...snapshot.intent.preset, slots: snapshot.intent.preset.slots.slice(0, 1) },
  });
  const doc = blankDocument();
  doc.name = 'Key color experimental editor fixture';
  doc.schemaVersion = 58;
  doc.width = 320;
  doc.height = 240;
  doc.colorSlots = ['key', 'derived', 'dormant', 'other'].map((id, i) => ({
    id,
    name: `Shared ${i}`,
    value: { hex: '#123456', alpha: 0.25 + i / 4 },
  }));
  doc.keyColorRecipe = { version: 1, mode: 'retained-only', committedSnapshot: snapshot };
  doc.colorDesign = {
    version: 2,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['dormant'],
    wheel: { ...DEFAULT_HARMONY_WHEEL },
    palette: [
      { id: 'key', kind: 'chromatic', position: 0, ring: 3, role: 'Primary', overrideC: 0.12 },
      { id: 'derived', kind: 'neutral', L: 0.91, role: 'Background' },
    ],
  };
  const layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    color = node('Color'),
    out = layer.nodes[0];
  shape.params.center.value = [160, 120];
  shape.params.size.value = [100, 100];
  fill.params.color.colorSlotId = 'key';
  color.params.value.colorSlotId = 'derived';
  layer.nodes = [shape, fill, color, out];
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  return doc;
}
