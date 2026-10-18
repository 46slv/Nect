// Authored Unicode examples only. No external font or private document data.
// Offsets are UTF-16 textarea offsets; these are NOT OpenType shaping clusters.
export const graphemeSamples = [
  { name: 'combining accent', text: 'x\u0301', boundaries: [0, 2] },
  { name: 'multiple combining marks', text: 'x\u0301\u0323', boundaries: [0, 3] },
  { name: 'surrogate pair', text: '\u{1d11e}', boundaries: [0, 2] },
  {
    name: 'family ZWJ',
    text: '\u{1f469}\u200d\u{1f469}\u200d\u{1f467}\u200d\u{1f466}',
    boundaries: [0, 11],
  },
  { name: 'regional flag', text: '\u{1f1ef}\u{1f1f5}', boundaries: [0, 4] },
  { name: 'variation selector', text: '\u2708\ufe0f', boundaries: [0, 2] },
  { name: 'skin tone', text: '\u{1f44d}\u{1f3fd}', boundaries: [0, 4] },
  { name: 'Indic conjunct ZWJ', text: '\u0915\u094d\u200d\u0937', boundaries: [0, 4] },
  { name: 'Hangul jamo', text: '\u1100\u1161\u11a8', boundaries: [0, 3] },
  { name: 'emoji keycap', text: '1\ufe0f\u20e3', boundaries: [0, 3] },
];

export const scalarSamples = [
  { name: 'ASCII', text: 'abc', boundaries: [0, 1, 2, 3] },
  // A font may shape ffi as one glyph; editing still has three graphemes.
  { name: 'potential ffi ligature', text: 'ffi', boundaries: [0, 1, 2, 3] },
  { name: 'Japanese', text: '\u304b\u306a', boundaries: [0, 1, 2] },
];

export function expectedBoundaries(sample) {
  return [0, ...sample.boundaries.map((index) => index + 1), sample.text.length + 2];
}

// Deliberately independent of fonts: model a scalar-based layout with nonzero
// boxes so every source boundary can be hit, including combining components.
export function scalarGeometryFixture(raw) {
  const text = raw.replace(/\r\n?/g, '\n');
  const characters = Array.from(text).flatMap((part, sourceIndex) =>
    part === '\n'
      ? []
      : [
          {
            text: part,
            sourceIndex,
            base: [sourceIndex * 50, 100],
            bounds: [sourceIndex * 50, 20, sourceIndex * 50 + 45, 100],
            pivot: [sourceIndex * 50 + 25, 60],
            offset: [0, 0],
            scale: 1,
            rotation: 0,
            size: 80,
            width: 1,
            height: 1,
            font: 'default',
            advance: 50,
          },
        ],
  );
  return {
    node: {
      text: raw,
      font: 'default',
      params: { size: { value: 80 }, position: { value: [0, 100] } },
    },
    layout: {
      characters,
      glyphs: characters,
      lines: [{ sourceStart: 0, sourceEnd: Array.from(text).length }],
    },
    occurrence: { matrix: [1, 0, 0, 1, 0, 0] },
  };
}
