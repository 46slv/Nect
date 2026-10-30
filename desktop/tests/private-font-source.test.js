import test from 'node:test';
import assert from 'node:assert/strict';
import {
  PRIVATE_FONT_SCHEMA_VERSION,
  validatePrivateFontSource,
  samePrivateFontSource,
  privateFontChoiceId,
} from '../src/private-font-source.js';
import {
  desktopOsReference,
  desktopTextIntent,
  validateDesktopText,
} from '../src/desktop-font-text.js';

const source = (patch = {}) => ({
  kind: 'file-native-v1',
  sha256: 'a'.repeat(64),
  faceIndex: 1,
  ...patch,
});
const face = {
  family: 'Fixture family',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const node = () => ({
  id: 'text-fixture',
  type: 'Text',
  text: 'ABC',
  font: 'default',
  params: {
    size: { value: 100 },
    letterSpacing: { value: 0 },
    lineSpacing: { value: 1.2 },
  },
});
function shaped() {
  const n = node();
  return {
    engine: 'directwrite-v1',
    ...desktopTextIntent(n, face, { ligatures: false }),
    points: [
      [0, 0],
      [20, 0],
      [20, 10],
      [0, 10],
    ],
    types: [0, 1, 1, 129],
    ascent: 80,
    advanceWidth: 20,
    lineCount: 1,
    glyphCount: 3,
  };
}

test('private source identity is exact content plus collection index, never names or capabilities', () => {
  assert.equal(PRIVATE_FONT_SCHEMA_VERSION, 35);
  assert.equal(validatePrivateFontSource(source()).faceIndex, 1);
  assert.ok(samePrivateFontSource(source(), source()));
  assert.ok(!samePrivateFontSource(source(), source({ faceIndex: 0 })));
  assert.ok(!samePrivateFontSource(source(), source({ sha256: 'b'.repeat(64) })));
  assert.ok(!samePrivateFontSource(undefined, source()));
  assert.ok(samePrivateFontSource(undefined, undefined));
  assert.notEqual(privateFontChoiceId(source()), privateFontChoiceId(source({ faceIndex: 2 })));
  for (const invalid of [
    null,
    {},
    source({ kind: 'installed' }),
    source({ sha256: 'A'.repeat(64) }),
    source({ sha256: '../file' }),
    source({ faceIndex: -1 }),
    source({ faceIndex: 256 }),
    source({ faceIndex: 1.5 }),
    source({ token: 'secret' }),
    source({ path: 'C:/font.otf' }),
    source({ bytes: [] }),
  ])
    assert.throws(() => validatePrivateFontSource(invalid), /Invalid private font source/);
});

test('private source snapshot is schema-gated, cloned and free of paths, bytes and runtime tokens', () => {
  const identity = source(),
    n = node(),
    raw = shaped();
  n.osFont = desktopOsReference(raw, identity);
  validateDesktopText(n, 35);
  assert.throws(() => validateDesktopText(n, 34), /schema 35/);
  identity.faceIndex = 2;
  assert.equal(n.osFont.source.faceIndex, 1);
  const saved = JSON.parse(JSON.stringify(n));
  validateDesktopText(saved, 35);
  assert.deepEqual(saved.osFont.source, source());
  for (const key of ['token', 'path', 'bytes'])
    assert.ok(!JSON.stringify(saved).includes(`"${key}"`));
  for (const key of ['token', 'path', 'bytes']) {
    const invalid = structuredClone(saved);
    invalid.osFont.source[key] = key === 'bytes' ? [] : 'not allowed';
    assert.throws(() => validateDesktopText(invalid, 35), /Invalid private font source/);
  }
  const legacy = node();
  legacy.osFont = desktopOsReference(raw);
  validateDesktopText(legacy, 29);
  assert.ok(!Object.hasOwn(legacy.osFont, 'source'));
});

test('axis retention cannot cross private files, collection members or installed route', () => {
  const n = node();
  n.osFont = {
    version: 3,
    face,
    source: source(),
    outline: { ligatures: false },
    axes: [
      {
        tag: 'wght',
        name: 'Weight',
        min: 100,
        max: 900,
        defaultValue: 400,
        baseValue: 400,
        value: 600,
        hidden: false,
      },
    ],
  };
  assert.deepEqual(desktopTextIntent(n, face, {}, source()).axes, { wght: 600 });
  assert.equal(desktopTextIntent(n, face).axes, undefined);
  assert.equal(desktopTextIntent(n, face, {}, source({ faceIndex: 0 })).axes, undefined);
  assert.equal(desktopTextIntent(n, face, {}, source({ sha256: 'b'.repeat(64) })).axes, undefined);
});
