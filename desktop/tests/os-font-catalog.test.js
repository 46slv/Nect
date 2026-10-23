import test from 'node:test';
import assert from 'node:assert/strict';
import { readOsFontCatalog, osFontAvailability } from '../src/os-font-catalog.js';
const face = {
  family: 'Named face',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const modern = { version: 2, face },
  legacy = { version: 1, family: face.family, style: 0 };
test('failed refresh drops previous availability and keeps independent catalog success', async () => {
  const first = await readOsFontCatalog({
    listFamilies: async () => [{ family: face.family, styles: [0] }],
    listFaces: async () => [face],
  });
  assert.equal(osFontAvailability(modern, first), '');
  const failed = await readOsFontCatalog({
    listFamilies: async () => first.families,
    listFaces: () => {
      throw Error('Unavailable catalog');
    },
  });
  assert.equal(failed.faces, null);
  assert.match(osFontAvailability(modern, failed), /check failed/);
  assert.equal(osFontAvailability(legacy, failed), '');
  const partial = await readOsFontCatalog({
    listFamilies: async () => {
      throw Error('Legacy failure');
    },
    listFaces: async () => [face],
  });
  assert.equal(osFontAvailability(modern, partial), '');
  assert.match(osFontAvailability(legacy, partial), /check failed/);
  const recovered = await readOsFontCatalog({
    listFamilies: async () => [],
    listFaces: async () => [face],
  });
  assert.equal(osFontAvailability(modern, recovered), '');
  assert.equal(recovered.facesError, false);
});
test('only successful absence reports missing; pending, unchecked and non-Windows remain distinct', () => {
  assert.match(osFontAvailability(modern, { faces: [] }), /unavailable.*saved outlines retained/);
  assert.match(osFontAvailability(modern), /not checked/);
  assert.match(osFontAvailability(modern, { pending: true, facesError: true }), /Checking/);
  assert.match(
    osFontAvailability(modern, { desktop: false, facesError: true }),
    /Windows required/,
  );
  assert.match(
    osFontAvailability(legacy, { families: [{ family: face.family, styles: [1] }] }),
    /unavailable/,
  );
});
test('saved face matches all native metadata and checking never changes the reference', () => {
  const before = JSON.stringify(modern);
  for (const key of ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color']) {
    const altered = {
      ...face,
      [key]:
        typeof face[key] === 'string'
          ? face[key] + ' other'
          : typeof face[key] === 'boolean'
            ? !face[key]
            : face[key] + 1,
    };
    assert.match(osFontAvailability(modern, { faces: [altered] }), /unavailable/, key);
  }
  assert.equal(JSON.stringify(modern), before);
});
