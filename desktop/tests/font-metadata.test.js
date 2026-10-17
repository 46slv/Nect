import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import { fontMetadata, parseFont, makeFontAsset, resolveFonts } from '../src/fonts.js';

const bytes = (path) => {
  const data = fs.readFileSync(path);
  return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
};
const bundled = new URL('../public/fonts/DejaVuSans.ttf', import.meta.url);
const empty = {
  family: null,
  subfamily: null,
  fullName: null,
  postScriptName: null,
  weightClass: null,
  widthClass: null,
  fsSelection: null,
  italic: null,
  bold: null,
  regular: null,
  oblique: null,
};

test('bundled DejaVu metadata is derived and leaves exact bytes and geometry intact', () => {
  const data = bytes(bundled),
    before = data.slice(0);
  const font = parseFont(data),
    reference = opentype.parse(data);
  assert.deepEqual(font.nectMetadata, {
    family: 'DejaVu Sans',
    subfamily: 'Book',
    fullName: 'DejaVu Sans',
    postScriptName: 'DejaVuSans',
    weightClass: 400,
    widthClass: 5,
    fsSelection: 64,
    italic: false,
    bold: false,
    regular: true,
    oblique: null,
  });
  assert.ok(Object.isFrozen(font.nectMetadata));
  assert.deepEqual(
    font.getPath('AVg', 0, 0, 40).commands,
    reference.getPath('AVg', 0, 0, 40).commands,
  );
  assert.equal(font.getAdvanceWidth('AVg', 40), reference.getAdvanceWidth('AVg', 40));
  assert.deepEqual(data, before);
  const made = makeFontAsset(data, 'regular.ttf');
  assert.deepEqual(Buffer.from(made.asset.data, 'base64'), Buffer.from(before));
  assert.deepEqual(Object.keys(made.asset).sort(), ['data', 'fileName', 'name']);
  assert.deepEqual(
    resolveFonts({ fontAssets: { [made.id]: made.asset } })[made.id].nectMetadata,
    font.nectMetadata,
  );
});

test('Japanese CFF and TrueType fixture metadata remains available with finite outlines', () => {
  const families = [];
  for (const extension of ['otf', 'ttf']) {
    const data = bytes(
      new URL(`./fixtures/fonts/NectJPFixture-Regular.${extension}`, import.meta.url),
    );
    const font = parseFont(data),
      metadata = fontMetadata(font);
    assert.match(metadata.family, /Nect/);
    assert.equal(metadata.subfamily, 'Regular');
    assert.equal(metadata.weightClass, 400);
    assert.equal(metadata.widthClass, 5);
    assert.deepEqual(
      font.getPath('日本', 0, 0, 40).commands,
      opentype.parse(data).getPath('日本', 0, 0, 40).commands,
    );
    families.push(metadata.family);
  }
  assert.notEqual(families[0], families[1]);
});

test('typographic records win even when localized; English and other language fallback is deterministic', () => {
  const names = {
    preferredFamily: { ja: '原作書体', fr: 'Famille originale' },
    preferredSubfamily: { ja: '細字' },
    fontFamily: { en: 'Legacy linked family' },
    fontSubfamily: { en: 'Regular' },
    fullName: { ja: '原作書体 細字', 'en-US': 'Original Light', en: 'Original Thin' },
    postScriptName: { ja: 'Original-Thin' },
  };
  const metadata = fontMetadata({ names });
  assert.equal(metadata.family, 'Famille originale');
  assert.equal(metadata.subfamily, '細字');
  assert.equal(metadata.fullName, 'Original Thin');
  assert.equal(metadata.postScriptName, 'Original-Thin');
  delete names.fullName.en;
  assert.equal(fontMetadata({ names }).fullName, 'Original Light');
  names.preferredFamily = { en: '\u0000bad', ja: '   ' };
  assert.equal(fontMetadata({ names }).family, 'Legacy linked family');
});

test('optional absent or malformed metadata stays unknown, never guessed from names', () => {
  for (const input of [null, undefined, {}, { names: {}, tables: {} }])
    assert.deepEqual(fontMetadata(input), empty);
  const font = {
    names: { fontFamily: ['bad'], fontSubfamily: { en: 'x'.repeat(257) }, fullName: { en: 42 } },
    tables: { os2: { usWeightClass: 1001, usWidthClass: 0, fsSelection: -1 } },
  };
  assert.deepEqual(fontMetadata(font), empty);
  for (const value of [NaN, Infinity, 1.5, '400', -1, 0]) {
    assert.equal(fontMetadata({ tables: { os2: { usWeightClass: value } } }).weightClass, null);
  }
  assert.equal(fontMetadata({ names: { fontSubfamily: { en: 'Bold Italic' } } }).bold, null);
  for (const value of [1, 1000])
    assert.equal(fontMetadata({ tables: { os2: { usWeightClass: value } } }).weightClass, value);
  assert.equal(fontMetadata({ tables: { os2: { usWidthClass: 10 } } }).widthClass, null);
  assert.equal(fontMetadata({ tables: { os2: { fsSelection: 65536 } } }).fsSelection, null);
});

test('OS/2 style flags are literal, with oblique gated by table version', () => {
  const metadata = fontMetadata({
    tables: { os2: { version: 4, usWeightClass: 500, usWidthClass: 9, fsSelection: 1 | 32 | 512 } },
  });
  assert.equal(metadata.italic, true);
  assert.equal(metadata.bold, true);
  assert.equal(metadata.regular, false);
  assert.equal(metadata.oblique, true);
  assert.equal(metadata.weightClass, 500);
  assert.equal(metadata.widthClass, 9);
  assert.equal(fontMetadata({ tables: { os2: { version: 1, fsSelection: 512 } } }).oblique, null);
  assert.equal(fontMetadata({ tables: { os2: { version: 4, fsSelection: 0 } } }).oblique, false);
});

// Original triangle glyphs, generated only in memory. They do not model real style fidelity.
function originalFont(width) {
  const path = new opentype.Path();
  path.moveTo(0, 0);
  path.lineTo(width, 0);
  path.lineTo(width / 2, 700);
  path.close();
  return new opentype.Font({
    familyName: 'Original Collision',
    styleName: 'Regular',
    unitsPerEm: 1000,
    ascender: 800,
    descender: -200,
    glyphs: [
      new opentype.Glyph({ name: '.notdef', advanceWidth: 500, path: new opentype.Path() }),
      new opentype.Glyph({ name: 'A', unicode: 65, advanceWidth: width + 30, path }),
    ],
  }).toArrayBuffer();
}

test('identical metadata never merges distinct byte identities; exact bytes still deduplicate', () => {
  const first = makeFontAsset(originalFont(400), 'one.otf');
  const existing = { [first.id]: first.asset };
  const second = makeFontAsset(originalFont(600), 'two.otf', existing);
  assert.deepEqual(first.font.nectMetadata, second.font.nectMetadata);
  assert.notEqual(first.id, second.id);
  assert.notEqual(first.asset.data, second.asset.data);
  assert.notDeepEqual(first.font.getPath('A').commands, second.font.getPath('A').commands);
  const encoded = Buffer.from(first.asset.data, 'base64');
  const repeat = makeFontAsset(
    encoded.buffer.slice(encoded.byteOffset, encoded.byteOffset + encoded.length),
    'renamed.otf',
    existing,
  );
  assert.equal(repeat.id, first.id);
  assert.equal(repeat.asset, first.asset);
});

test('metadata addition keeps invalid table bounds and variable font rejection', () => {
  const invalid = bytes(bundled),
    view = new DataView(invalid);
  view.setUint32(20, invalid.byteLength + 1);
  assert.throws(() => parseFont(invalid), /Invalid font table bounds/);
  const variable = bytes(bundled);
  new Uint8Array(variable, 12, 4).set([102, 118, 97, 114]);
  assert.throws(() => parseFont(variable), /Variable fonts are unsupported/);
  assert.throws(() => parseFont(new ArrayBuffer(8)), /Cannot load font/);
});

const localFaces = [
  ['DejaVuSans-Bold.ttf', 'Bold', 700, true, false],
  ['DejaVuSans-Oblique.ttf', 'Oblique', 400, false, true],
  ['DejaVuSans-BoldOblique.ttf', 'Bold Oblique', 700, true, true],
];
for (const [file, subfamily, weight, bold, italic] of localFaces) {
  const location = `/usr/share/fonts/truetype/dejavu/${file}`;
  test(`optional installed licensed face: ${file}`, { skip: !fs.existsSync(location) }, () => {
    const data = bytes(location),
      font = parseFont(data),
      reference = opentype.parse(data);
    assert.equal(font.nectMetadata.family, 'DejaVu Sans');
    assert.equal(font.nectMetadata.subfamily, subfamily);
    assert.equal(font.nectMetadata.weightClass, weight);
    assert.equal(font.nectMetadata.bold, bold);
    assert.equal(font.nectMetadata.italic, italic);
    assert.deepEqual(
      font.getPath('AVg', 0, 0, 40).commands,
      reference.getPath('AVg', 0, 0, 40).commands,
    );
    assert.notDeepEqual(
      font.getPath('AVg', 0, 0, 40).commands,
      parseFont(bytes(bundled)).getPath('AVg', 0, 0, 40).commands,
    );
  });
}
