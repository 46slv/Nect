import { decodeImage, pngSize, validateImageAssets } from '../src/images.js';
import { fixedSvgSize } from './svg-to-png-options.js';

const namespace = 'http://www.w3.org/2000/svg';
// This is Nect's emitted vocabulary, not a general-purpose SVG sanitizer.
const tags = new Set([
  'svg',
  'g',
  'defs',
  'path',
  'rect',
  'image',
  'linearGradient',
  'radialGradient',
  'stop',
  'mask',
  'filter',
  'feGaussianBlur',
  'feComponentTransfer',
  'feFuncA',
  'feFlood',
  'feComposite',
  'feColorMatrix',
]);
const attributes = new Set([
  'id',
  'data-layer',
  'width',
  'height',
  'viewBox',
  'x',
  'y',
  'd',
  'transform',
  'opacity',
  'fill',
  'fill-opacity',
  'fill-rule',
  'stroke',
  'stroke-opacity',
  'stroke-width',
  'stroke-linecap',
  'stroke-linejoin',
  'stroke-miterlimit',
  'stroke-dasharray',
  'stroke-dashoffset',
  'preserveAspectRatio',
  'href',
  'gradientUnits',
  'x1',
  'y1',
  'x2',
  'y2',
  'cx',
  'cy',
  'r',
  'fx',
  'fy',
  'fr',
  'spreadMethod',
  'color-interpolation',
  'offset',
  'stop-color',
  'stop-opacity',
  'mask',
  'maskUnits',
  'maskContentUnits',
  'style',
  'filter',
  'filterUnits',
  'primitiveUnits',
  'color-interpolation-filters',
  'in',
  'in2',
  'stdDeviation',
  'result',
  'type',
  'slope',
  'intercept',
  'flood-color',
  'flood-opacity',
  'operator',
  'values',
]);
const localReference = /^url\(#([A-Za-z_][A-Za-z0-9_.-]*)\)$/;
const solidColor =
  /^(?:#(?:[A-Fa-f0-9]{3}|[A-Fa-f0-9]{4}|[A-Fa-f0-9]{6}|[A-Fa-f0-9]{8})|white|black|transparent)$/;

export async function prepareSvgSource(source, Parser = globalThis.DOMParser) {
  if (/<!DOCTYPE|<!ENTITY|<\?(?!xml(?:\s|\?>))/i.test(source))
    throw Error('SVG declarations and external styles are unsupported');
  const xml = new Parser().parseFromString(source, 'image/svg+xml'),
    root = xml.documentElement;
  if (!root || xml.querySelector('parsererror') || root.localName !== 'svg')
    throw Error('Invalid SVG document');
  const size = fixedSvgSize({
    width: root.getAttribute('width'),
    height: root.getAttribute('height'),
    viewBox: root.getAttribute('viewBox'),
  });
  const ids = new Map(),
    references = [],
    images = new Map(),
    assets = {};
  for (const element of [root, ...root.querySelectorAll('*')]) {
    if (element.namespaceURI !== namespace || element.prefix || !tags.has(element.localName))
      throw Error(`Unsupported SVG element: ${element.tagName}`);
    if (element !== root && element.localName === 'svg') throw Error('Nested SVG is unsupported');
    for (const attribute of element.attributes) {
      const { name, value } = attribute;
      if (name === 'xmlns' && element === root && value === namespace) continue;
      if (element === root && !['width', 'height', 'viewBox'].includes(name))
        throw Error(`Unsupported SVG root attribute: ${name}`);
      if (attribute.namespaceURI || !attributes.has(name))
        throw Error(`Unsupported SVG attribute: ${name}`);
      if (name === 'style' && !(element.localName === 'mask' && value === 'mask-type:luminance'))
        throw Error('SVG styles and fonts are unsupported');
      if (name === 'href') {
        if (element.localName !== 'image' || !value.startsWith('data:image/png;base64,'))
          throw Error('SVG images must be embedded Nect PNG assets; external URLs are unsupported');
        if (!images.has(value)) {
          const data = value.slice('data:image/png;base64,'.length),
            dimensions = pngSize(decodeImage(data)),
            id = `image-cli-${images.size}`;
          assets[id] = { name: id, mime: 'image/png', data, ...dimensions };
          images.set(value, { value, dimensions });
        }
      }
      if (['fill', 'stroke', 'mask', 'filter'].includes(name)) {
        const match = localReference.exec(value);
        if (match) references.push({ id: match[1], name });
        else if (
          name === 'mask' ||
          name === 'filter' ||
          !(value === 'none' || solidColor.test(value))
        )
          throw Error('Only solid colors and internal Nect fragment references are supported');
      }
      if (['stop-color', 'flood-color'].includes(name) && !solidColor.test(value))
        throw Error('Unsupported SVG color');
      if (name === 'id') {
        if (!/^[A-Za-z_][A-Za-z0-9_.-]*$/.test(value) || ids.has(value))
          throw Error('Invalid or duplicate SVG fragment ID');
        ids.set(value, element.localName);
      }
    }
    if (element.localName === 'image' && !element.hasAttribute('href'))
      throw Error('SVG image is missing its embedded PNG');
    for (const child of element.childNodes) {
      if ((child.nodeType === 3 || child.nodeType === 4) && child.textContent.trim())
        throw Error('SVG text and fonts are unsupported; export outlines from Nect');
      if (child.nodeType === 7) throw Error('SVG processing instructions are unsupported');
    }
  }
  for (const { id, name } of references) {
    const allowed =
      name === 'mask'
        ? ['mask']
        : name === 'filter'
          ? ['filter']
          : ['linearGradient', 'radialGradient'];
    if (!allowed.includes(ids.get(id))) throw Error(`Missing or unsupported SVG fragment: ${id}`);
  }
  validateImageAssets(assets);
  // SVG-as-image can silently ignore undecodable nested images. Preflight every
  // allowed embedded asset, rather than returning a successful incomplete PNG.
  for (const { value, dimensions } of images.values()) {
    const image = new Image();
    image.src = value;
    await image.decode();
    if (image.naturalWidth !== dimensions.width || image.naturalHeight !== dimensions.height)
      throw Error('Decoded embedded PNG dimensions do not match');
  }
  // Canonical adjacent root sizes are required by the existing raster adapter's
  // replacement. This derived XML never changes the input file.
  for (const name of ['width', 'height', 'viewBox']) root.removeAttribute(name);
  root.setAttribute('width', String(size.width));
  root.setAttribute('height', String(size.height));
  root.setAttribute('viewBox', `0 0 ${size.width} ${size.height}`);
  return { svg: new XMLSerializer().serializeToString(root), size };
}
