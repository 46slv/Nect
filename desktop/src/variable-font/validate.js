import { LIMITS, requireThat as check } from './contract.js';
import { readInstanceNames } from './named-instances.js';
/** Independent preflight; intentionally does not relax the static font parser's guard. */
export function validateVariableBytes(bytes) {
  check(
    bytes instanceof Uint8Array && bytes.byteLength >= 12 && bytes.byteLength <= LIMITS.bytes,
    'FONT_BYTES',
  );
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  check(v.getUint32(0) === 0x10000, 'UNSUPPORTED_FORMAT');
  const count = v.getUint16(4),
    end = 12 + count * 16,
    tables = Object.create(null),
    ranges = [];
  check(count > 0 && count <= 256 && end <= bytes.length, 'FONT_DIRECTORY');
  for (let i = 0; i < count; i++) {
    const p = 12 + i * 16,
      tag = String.fromCharCode(...bytes.subarray(p, p + 4));
    const offset = v.getUint32(p + 8),
      length = v.getUint32(p + 12);
    check(
      offset >= end && offset + length <= bytes.length && !Object.hasOwn(tables, tag),
      'FONT_TABLE_BOUNDS',
    );
    if (length) {
      check(!ranges.some(([a, b]) => offset < b && offset + length > a), 'FONT_TABLE_OVERLAP');
      ranges.push([offset, offset + length]);
    }
    tables[tag] = new DataView(bytes.buffer, bytes.byteOffset + offset, length);
  }
  for (const tag of [
    'CFF ',
    'CFF2',
    'COLR',
    'CPAL',
    'CBDT',
    'CBLC',
    'sbix',
    'SVG ',
    'EBDT',
    'EBLC',
    'VVAR',
    'MVAR',
  ])
    check(!tables[tag], 'UNSUPPORTED_FORMAT');
  for (const [tag, min] of [
    ['head', 54],
    ['maxp', 6],
    ['loca', 2],
    ['glyf', 1],
    ['gvar', 20],
    ['fvar', 16],
    ['OS/2', 10],
    ['hhea', 36],
    ['hmtx', 4],
    ['cmap', 4],
  ])
    check(tables[tag]?.byteLength >= min, 'FONT_TABLE_MISSING');
  const fsType = tables['OS/2'].getUint16(8);
  check(!(fsType & 0x0202) && !(fsType & 4 && !(fsType & 8)), 'EMBEDDING_RESTRICTED');
  const glyphs = tables.maxp.getUint16(4),
    format = tables.head.getInt16(50),
    units = tables.head.getUint16(18);
  check(
    glyphs > 0 &&
      glyphs <= LIMITS.glyphs &&
      [0, 1].includes(format) &&
      units >= 16 &&
      units <= 16384,
    'FONT_GLYPH_LIMIT',
  );
  check(tables.loca.byteLength >= (glyphs + 1) * (format ? 4 : 2), 'FONT_LOCA');
  let previous = 0;
  for (let i = 0; i <= glyphs; i++) {
    const n = format ? tables.loca.getUint32(i * 4) : tables.loca.getUint16(i * 2) * 2;
    check(
      n >= previous && n <= tables.glyf.byteLength && (n === previous || n - previous >= 10),
      'FONT_LOCA',
    );
    previous = n;
  }
  const metrics = tables.hhea.getUint16(34);
  check(
    metrics > 0 &&
      metrics <= glyphs &&
      tables.hmtx.byteLength >= metrics * 4 + (glyphs - metrics) * 2,
    'FONT_METRICS',
  );
  const f = tables.fvar,
    axisOffset = f.getUint16(4),
    axisCount = f.getUint16(8),
    axisSize = f.getUint16(10),
    instanceCount = f.getUint16(12),
    instanceSize = f.getUint16(14);
  check(
    f.getUint32(0) === 0x10000 &&
      axisOffset >= 16 &&
      axisCount > 0 &&
      axisCount <= LIMITS.axes &&
      axisSize === 20 &&
      instanceCount <= 256 &&
      [4 + axisCount * 4, 6 + axisCount * 4].includes(instanceSize) &&
      axisOffset + axisCount * axisSize + instanceCount * instanceSize <= f.byteLength,
    'FONT_AXIS_METADATA',
  );
  const axes = {};
  for (let i = 0; i < axisCount; i++) {
    const p = axisOffset + i * axisSize,
      tag = String.fromCharCode(...new Uint8Array(f.buffer, f.byteOffset + p, 4));
    const min = f.getInt32(p + 4) / 65536,
      def = f.getInt32(p + 8) / 65536,
      max = f.getInt32(p + 12) / 65536;
    check(
      /^[\x20-\x7e]{4}$/.test(tag) &&
        tag === tag.trim() &&
        !Object.hasOwn(axes, tag) &&
        min <= def &&
        def <= max &&
        min < max,
      'FONT_AXIS_METADATA',
    );
    Object.defineProperty(axes, tag, { value: { min, default: def, max }, enumerable: true });
  }
  const tags = Object.keys(axes),
    namedInstances = [];
  for (let i = 0; i < instanceCount; i++) {
    const p = axisOffset + axisCount * axisSize + i * instanceSize,
      nameId = f.getUint16(p),
      coordinates = {};
    check(
      (nameId === 2 || nameId === 17 || (nameId >= 256 && nameId < 32768)) &&
        f.getUint16(p + 2) === 0,
      'FONT_AXIS_METADATA',
    );
    for (let j = 0; j < axisCount; j++) {
      const tag = tags[j],
        value = f.getInt32(p + 4 + j * 4) / 65536;
      check(value >= axes[tag].min && value <= axes[tag].max, 'FONT_AXIS_METADATA');
      coordinates[tag] = value;
    }
    const isDefault = tags.every((tag) => coordinates[tag] === axes[tag].default);
    check((nameId !== 2 && nameId !== 17) || isDefault, 'FONT_AXIS_METADATA');
    if (instanceSize === 6 + axisCount * 4) {
      const ps = f.getUint16(p + 4 + axisCount * 4);
      check(
        ps === 0xffff || (ps === 6 && isDefault) || (ps >= 256 && ps < 32768),
        'FONT_AXIS_METADATA',
      );
    }
    namedInstances.push({ nameId, coordinates });
  }
  const names = instanceCount
    ? readInstanceNames(
        tables.name,
        namedInstances.map((n) => n.nameId),
      )
    : new Map();
  namedInstances.forEach((instance, index) => {
    instance.name =
      names.get(instance.nameId) || `Instance ${index + 1} (name ID ${instance.nameId})`;
  });
  const g = tables.gvar,
    shared = g.getUint16(6),
    sharedOffset = g.getUint32(8),
    dataOffset = g.getUint32(16),
    wide = g.getUint16(14) & 1;
  check(
    g.getUint32(0) === 0x10000 &&
      g.getUint16(4) === axisCount &&
      g.getUint16(12) === glyphs &&
      g.getUint16(14) <= 1 &&
      shared <= 4095 &&
      sharedOffset + shared * axisCount * 2 <= g.byteLength &&
      dataOffset >= 20 + (glyphs + 1) * (wide ? 4 : 2) &&
      dataOffset <= g.byteLength,
    'FONT_GVAR',
  );
  previous = 0;
  for (let i = 0; i <= glyphs; i++) {
    const n = wide ? g.getUint32(20 + i * 4) : g.getUint16(20 + i * 2) * 2;
    check(n >= previous && dataOffset + n <= g.byteLength, 'FONT_GVAR');
    previous = n;
  }
  return { axes, namedInstances, unitsPerEm: units, glyphCount: glyphs };
}
