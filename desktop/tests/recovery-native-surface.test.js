import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

// Test the verifier's actual descriptor reader/assertions without attaching to
// a browser or claiming a new native acceptance run.
const source = await fs.readFile(
  new URL('../scripts/verify-recovery-native-test.mjs', import.meta.url),
  'utf8',
);
const start = source.indexOf('      const marker = Object.getOwnPropertyDescriptor');
const readSurface = new Function('window', source.slice(start, source.indexOf('\n    });', start)));
const assertionsStart = source.indexOf('    assert.equal(receipt.TestSurface.Marker, true);');
const verifySurface = new Function(
  'receipt',
  'assert',
  source.slice(assertionsStart, source.indexOf('    await page.waitForFunction(', assertionsStart)),
);

function surface() {
  const window = {};
  Object.defineProperty(window, '__NECT_RECOVERY_TEST_INSTANCE__', { value: true });
  Object.defineProperty(window, '__NECT_RECOVERY_ACCEPTANCE_RESULT__', { get: () => ({}) });
  return readSurface(window);
}

test('native verifier accepts readonly descriptors with no callable global runner', () => {
  verifySurface({ TestSurface: surface() }, assert);
});

for (const [key, value] of [
  ['Marker', false],
  ['MarkerWritable', true],
  ['MarkerConfigurable', true],
  ['ResultGetter', 'undefined'],
  ['ResultSetter', 'function'],
  ['ResultConfigurable', true],
  ['CallableRunner', 'function'],
])
  test('native verifier rejects unexpected test surface ' + key, () => {
    assert.throws(
      () => verifySurface({ TestSurface: { ...surface(), [key]: value } }, assert),
      assert.AssertionError,
    );
  });
