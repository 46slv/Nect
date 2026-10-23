import { canonicalAxes, requireThat as check } from './contract.js';

// Only display labels come from name strings. Identity remains the validated fvar
// record index/coordinates; duplicate labels cannot redirect an instance choice.
export function readInstanceNames(table, wanted) {
  check(table?.byteLength >= 6, 'FONT_NAME_METADATA');
  const version = table.getUint16(0),
    count = table.getUint16(2),
    storage = table.getUint16(4);
  let end = 6 + count * 12,
    languages = 0;
  check(version <= 1 && count <= 4096 && end <= table.byteLength, 'FONT_NAME_METADATA');
  if (version === 1) {
    check(end + 2 <= table.byteLength, 'FONT_NAME_METADATA');
    languages = table.getUint16(end);
    const start = end + 2;
    end = start + languages * 4;
    check(end <= table.byteLength, 'FONT_NAME_METADATA');
    for (let i = 0; i < languages; i++) {
      const p = start + i * 4,
        length = table.getUint16(p),
        offset = table.getUint16(p + 2);
      check(
        length % 2 === 0 && storage + offset + length <= table.byteLength,
        'FONT_NAME_METADATA',
      );
    }
  }
  check(storage >= end && storage <= table.byteLength, 'FONT_NAME_METADATA');
  const names = new Map(),
    priorities = new Map(),
    ids = new Set(wanted);
  const decoder = new TextDecoder('utf-16be', { fatal: true });
  for (let i = 0; i < count; i++) {
    const p = 6 + i * 12,
      platform = table.getUint16(p),
      encoding = table.getUint16(p + 2),
      language = table.getUint16(p + 4),
      id = table.getUint16(p + 6),
      length = table.getUint16(p + 8),
      offset = table.getUint16(p + 10);
    check(storage + offset + length <= table.byteLength, 'FONT_NAME_METADATA');
    if (!ids.has(id) || (language >= 0x8000 && language - 0x8000 >= languages)) continue;
    const unicode =
      (platform === 0 && encoding <= 6) || (platform === 3 && [0, 1, 10].includes(encoding));
    if (!unicode) continue; // Legacy encodings are not guessed or misdecoded.
    check(length % 2 === 0 && length <= 1024, 'FONT_NAME_METADATA');
    let name;
    try {
      name = decoder.decode(
        new Uint8Array(table.buffer, table.byteOffset + storage + offset, length),
      );
    } catch {
      check(false, 'FONT_NAME_METADATA');
    }
    check(
      !/[\u0000-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/u.test(name),
      'FONT_NAME_METADATA',
    );
    name = name.trim();
    const priority = platform === 3 && language === 0x0409 ? 2 : platform === 0 ? 1 : 0;
    if (name && (!names.has(id) || priority > priorities.get(id))) {
      names.set(id, name);
      priorities.set(id, priority);
    }
  }
  return names;
}

export function matchingNamedInstance(metadata, authored = {}) {
  const { effective } = canonicalAxes(metadata.axes, authored);
  return metadata.namedInstances.findIndex((instance) =>
    Object.keys(effective).every((tag) => effective[tag] === instance.coordinates[tag]),
  );
}
