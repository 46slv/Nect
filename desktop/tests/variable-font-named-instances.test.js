import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { validateVariableBytes } from '../src/variable-font/validate.js';
import { matchingNamedInstance, readInstanceNames } from '../src/variable-font/named-instances.js';
const bytes = new Uint8Array(
  fs.readFileSync(new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url)),
);
function table(bytes, tag) {
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let i = 0; i < v.getUint16(4); i++) {
    const p = 12 + i * 16;
    if (String.fromCharCode(...bytes.slice(p, p + 4)) === tag)
      return new DataView(bytes.buffer, bytes.byteOffset + v.getUint32(p + 8), v.getUint32(p + 12));
  }
}
const instanceStart = (f) => f.getUint16(4) + f.getUint16(8) * f.getUint16(10);
test('real fvar names and full coordinates; exact matching and explicit defaults', () => {
  const metadata = validateVariableBytes(bytes);
  assert.equal(metadata.namedInstances.length, 18);
  assert.deepEqual(metadata.namedInstances[17], {
    nameId: 283,
    coordinates: { wght: 900, wdth: 75 },
    name: 'Condensed Black',
  });
  assert.equal(matchingNamedInstance(metadata, {}), 3);
  assert.equal(matchingNamedInstance(metadata, { wght: 400, wdth: 100 }), 3);
  assert.equal(matchingNamedInstance(metadata, { wght: 900, wdth: 75 }), 17);
  assert.equal(matchingNamedInstance(metadata, { wght: 899.999, wdth: 75 }), -1);
  assert.throws(() => matchingNamedInstance(metadata, { other: 1 }), /INVALID_AXES/);
  const copy = bytes.slice();
  table(copy, 'fvar').setUint16(12, 0);
  assert.deepEqual(validateVariableBytes(copy).namedInstances, []);
});
test('malformed instance records and name bounds fail closed before shaping', () => {
  for (const mutate of [
    (f) => f.setUint16(instanceStart(f), 255),
    (f) => f.setUint16(instanceStart(f), 2), // only the default may use name ID 2/17
    (f) => f.setUint16(instanceStart(f) + 2, 1),
    (f) => f.setInt32(instanceStart(f) + 4, 901 * 65536),
    (f) => f.setUint16(14, 7),
    (f) => f.setUint16(12, 257),
    (f) => f.setUint16(12, 256), // truncated instance array
  ]) {
    const copy = bytes.slice();
    mutate(table(copy, 'fvar'));
    assert.throws(() => validateVariableBytes(copy), /FONT_AXIS_METADATA/);
  }
  for (const mutate of [
    (n) => n.setUint16(0, 2),
    (n) => n.setUint16(2, 4097),
    (n) => n.setUint16(4, 0),
    (n) => n.setUint16(6 + 10, 65535),
  ]) {
    const copy = bytes.slice();
    mutate(table(copy, 'name'));
    assert.throws(() => validateVariableBytes(copy), /FONT_NAME_METADATA/);
  }
});
function nameTable({
  version = 0,
  platform = 3,
  encoding = 1,
  language = 0x409,
  text = 'Bold',
  raw,
} = {}) {
  const units = raw || [...text].map((s) => s.charCodeAt(0));
  const storage = version ? 20 : 18;
  const v = new DataView(new ArrayBuffer(storage + units.length * 2));
  [version, 1, storage, platform, encoding, language, 256, units.length * 2, 0].forEach((x, i) =>
    v.setUint16(i * 2, x),
  );
  units.forEach((x, i) => v.setUint16(storage + i * 2, x));
  return v;
}
test('bounded Unicode naming v0/v1, unsupported encoding fallback and malformed strings', () => {
  assert.equal(readInstanceNames(nameTable(), [256]).get(256), 'Bold');
  assert.equal(
    readInstanceNames(
      nameTable({ version: 1, platform: 0, encoding: 4, language: 0, text: '細字' }),
      [256],
    ).get(256),
    '細字',
  );
  assert.equal(readInstanceNames(nameTable({ platform: 1, encoding: 0 }), [256]).size, 0);
  assert.equal(readInstanceNames(nameTable({ version: 1, language: 0x8000 }), [256]).size, 0);
  for (const raw of [[0xd800], [0], [0x202e]])
    assert.throws(() => readInstanceNames(nameTable({ raw }), [256]), /FONT_NAME_METADATA/);
  const odd = nameTable();
  odd.setUint16(14, 1);
  assert.throws(() => readInstanceNames(odd, [256]), /FONT_NAME_METADATA/);
  assert.throws(
    () => readInstanceNames(nameTable({ text: 'a'.repeat(513) }), [256]),
    /FONT_NAME_METADATA/,
  );
});
test('optional PostScript IDs validate without becoming identity; duplicate labels keep records', () => {
  for (const ps of [0xffff, 256, 32767, 6, 0, 255, 32768]) {
    const copy = bytes.slice(),
      f = table(copy, 'fvar');
    f.setUint16(12, 1);
    f.setUint16(14, 6 + f.getUint16(8) * 4);
    f.setUint16(instanceStart(f) + 4 + f.getUint16(8) * 4, ps);
    if ([0xffff, 256, 32767].includes(ps))
      assert.equal(validateVariableBytes(copy).namedInstances[0].name, 'Thin');
    else assert.throws(() => validateVariableBytes(copy), /FONT_AXIS_METADATA/);
  }
  const copy = bytes.slice(),
    f = table(copy, 'fvar'),
    start = instanceStart(f);
  f.setUint16(start + f.getUint16(14), f.getUint16(start));
  const metadata = validateVariableBytes(copy);
  assert.equal(metadata.namedInstances[0].name, metadata.namedInstances[1].name);
  assert.equal(matchingNamedInstance(metadata, { wght: 200 }), 1);
  // Preserve record coordinates when a name has only an unsupported legacy encoding.
  const n = table(copy, 'name');
  for (let i = 0; i < n.getUint16(2); i++) {
    const p = 6 + i * 12;
    if (n.getUint16(p + 6) === 259) n.setUint16(p, 1);
  }
  assert.equal(validateVariableBytes(copy).namedInstances[0].name, 'Instance 1 (name ID 259)');
});
