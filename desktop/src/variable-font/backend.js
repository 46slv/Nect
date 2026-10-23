// Worker-only module. Browser conditional export is verified by the packaging test.
import { create } from 'fontkit';
import {
  ENGINE,
  LIMITS,
  canonicalAxes,
  validateRequest,
  requireThat as check,
  freeze,
} from './contract.js';
import { validateVariableBytes } from './validate.js';
export function createVariableBackend() {
  let assetId,
    base,
    metadata,
    instances = new Map(),
    runs = new Map(),
    commandTotal = 0;
  const clear = () => {
    assetId = base = metadata = undefined;
    instances.clear();
    runs.clear();
    commandTotal = 0;
  };
  return {
    clear,
    prepare(id, bytes, request) {
      validateRequest(request);
      if (id !== assetId) {
        clear();
        metadata = validateVariableBytes(bytes);
        base = create(bytes);
        assetId = id;
        check(
          base.unitsPerEm === metadata.unitsPerEm && base.numGlyphs === metadata.glyphCount,
          'FONT_METRICS',
        );
      }
      const { effective, sparse } = canonicalAxes(metadata.axes, request.axes);
      const instanceKey = JSON.stringify([ENGINE, id, 0, effective]),
        key = JSON.stringify([
          instanceKey,
          request.text,
          'latn',
          'dflt',
          'ltr',
          'default-features',
        ]);
      if (runs.has(key)) return runs.get(key);
      let font = instances.get(instanceKey);
      if (!font) {
        font = base.getVariation(effective);
        if (instances.size >= LIMITS.instances) instances.delete(instances.keys().next().value);
        instances.set(instanceKey, font);
      }
      const shaped = font.layout(request.text, undefined, 'latn', undefined, 'ltr');
      // We do not infer clusters from codePoints. Source spans are admitted only after
      // count, order, nominal glyph identity and exact single-scalar mapping agree.
      check(
        shaped.glyphs.length === request.text.length &&
          shaped.positions.length === request.text.length,
        'UNSUPPORTED_MAPPING',
      );
      const commands = [],
        glyphs = [];
      let pen = 0;
      for (let i = 0; i < shaped.glyphs.length; i++) {
        const glyph = shaped.glyphs[i],
          p = shaped.positions[i],
          cp = request.text.charCodeAt(i);
        check(
          glyph.id > 0 &&
            glyph.id === font.glyphForCodePoint(cp).id &&
            glyph.codePoints.length === 1 &&
            glyph.codePoints[0] === cp &&
            p.xOffset === 0 &&
            p.yOffset === 0 &&
            p.yAdvance === 0,
          'UNSUPPORTED_MAPPING',
        );
        check(Number.isFinite(p.xAdvance) && Math.abs(p.xAdvance) <= 1e6, 'OUTPUT_METRICS');
        const start = commands.length;
        let current = [0, 0];
        for (const c of glyph.path.commands) {
          check(commands.length < LIMITS.commands, 'OUTPUT_LIMIT');
          let type,
            vals = c.args;
          check(
            vals.every((v) => Number.isFinite(v) && Math.abs(v) <= 1e6),
            'OUTPUT_COORDINATE',
          );
          switch (c.command) {
            case 'moveTo':
              type = 'M';
              break;
            case 'lineTo':
              type = 'L';
              break;
            case 'closePath':
              type = 'Z';
              break;
            case 'bezierCurveTo':
              type = 'C';
              break;
            case 'quadraticCurveTo': {
              type = 'C';
              const [cx, cy, x, y] = vals;
              vals = [
                current[0] + (2 / 3) * (cx - current[0]),
                current[1] + (2 / 3) * (cy - current[1]),
                x + (2 / 3) * (cx - x),
                y + (2 / 3) * (cy - y),
                x,
                y,
              ];
              break;
            }
            default:
              check(false, 'OUTPUT_COMMAND');
          }
          check(vals.length === (type === 'C' ? 6 : type === 'Z' ? 0 : 2), 'OUTPUT_COMMAND');
          if (vals.length) current = vals.slice(-2);
          const keys = type === 'C' ? ['x1', 'y1', 'x2', 'y2', 'x', 'y'] : ['x', 'y'],
            out = { type };
          vals.forEach((v, j) => (out[keys[j]] = j % 2 ? -v : v + pen));
          commands.push(out);
        }
        glyphs.push({
          id: glyph.id,
          sourceStart: i,
          sourceEnd: i + 1,
          xAdvance: p.xAdvance,
          xOffset: 0,
          yOffset: 0,
          yAdvance: 0,
          commandStart: start,
          commandEnd: commands.length,
        });
        pen += p.xAdvance;
      }
      const result = {
        engine: ENGINE,
        assetId,
        instanceKey,
        key,
        axes: metadata.axes,
        coordinates: sparse,
        effectiveCoordinates: effective,
        unitsPerEm: metadata.unitsPerEm,
        advance: pen,
        glyphs,
        commands,
      };
      check(
        new TextEncoder().encode(JSON.stringify(result)).length <= LIMITS.outputBytes,
        'OUTPUT_LIMIT',
      );
      freeze(result);
      while (
        runs.size &&
        (runs.size >= LIMITS.runs || commandTotal + commands.length > LIMITS.cachedCommands)
      ) {
        const oldest = runs.keys().next().value;
        commandTotal -= runs.get(oldest).commands.length;
        runs.delete(oldest);
      }
      runs.set(key, result);
      commandTotal += commands.length;
      return result;
    },
  };
}
