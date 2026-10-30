import { pngDimensions, pngPixelsPerMetre } from '../src/raster.js';

export const SVG_MAX_BYTES = 50_000_000;
export const SVG_PNG_USAGE =
  'Usage: node desktop/cli/svg-to-png.js INPUT.svg OUTPUT.png [--scale NUMBER | --ppi NUMBER] [--chromium-path PATH]';
const decimal = /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i;

export function finiteNumber(value) {
  if (typeof value !== 'string' || !decimal.test(value) || !Number.isFinite(Number(value)))
    throw Error('Expected a finite decimal number');
  return Number(value);
}

export function parseSvgPngArgs(args, env = process.env) {
  if (args.length === 1 && args[0] === '--help') return { help: true };
  const paths = [];
  let sizing = 1,
    ppi = null,
    selected = false,
    chromiumPath = env.CHROMIUM_PATH,
    chromeSelected = false;
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--scale' || arg === '--ppi') {
      if (selected) throw Error('--scale and --ppi are mutually exclusive and cannot repeat');
      selected = true;
      const number = finiteNumber(args[++i]);
      if (arg === '--scale') sizing = number;
      else {
        ppi = number;
        sizing = { mode: 'ppi', ppi };
        pngPixelsPerMetre(ppi);
      }
      pngDimensions({ width: 1, height: 1 }, sizing);
    } else if (arg === '--chromium-path') {
      if (chromeSelected || !args[i + 1] || args[i + 1].startsWith('--'))
        throw Error('Provide --chromium-path once with an installed Chrome executable');
      chromeSelected = true;
      chromiumPath = args[++i];
    } else if (arg.startsWith('--')) throw Error(`Unknown option: ${arg}`);
    else paths.push(arg);
  }
  if (paths.length !== 2 || paths.some((path) => !path)) throw Error(SVG_PNG_USAGE);
  if (!chromiumPath) throw Error('Set CHROMIUM_PATH or --chromium-path to installed Chrome');
  return { input: paths[0], output: paths[1], sizing, ppi, chromiumPath };
}

// The viewBox is already in CSS pixels (96 px/in). Physical root units do not
// participate in PNG sizing, so mm/cm exports cannot be converted twice.
export function fixedSvgSize({ width, height, viewBox }) {
  for (const length of [width, height]) {
    if (typeof length !== 'string') throw Error('SVG requires fixed width and height');
    const match = /^([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?)(?:px|mm|cm)?$/i.exec(length);
    if (!match || finiteNumber(match[1]) <= 0)
      throw Error('SVG requires positive fixed dimensions');
  }
  const parts = typeof viewBox === 'string' ? viewBox.trim().split(/[\s,]+/) : [];
  if (parts.length !== 4) throw Error('SVG requires viewBox="0 0 widthPx heightPx"');
  const [x, y, widthPx, heightPx] = parts.map(finiteNumber);
  if (x !== 0 || y !== 0 || widthPx <= 0 || heightPx <= 0)
    throw Error('SVG requires viewBox="0 0 widthPx heightPx" with positive dimensions');
  return { width: widthPx, height: heightPx };
}
