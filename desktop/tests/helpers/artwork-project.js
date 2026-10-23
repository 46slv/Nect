import assert from 'node:assert/strict';

const unrelatedCompositions = new WeakMap();

export async function loadArtworkProject(page, doc) {
  await page.evaluate(async (doc) => {
    await window.nect.load(JSON.stringify(doc));
    if (!doc.compositions) {
      const { blankDocument, guideDefaults } = await import('/src/core.js');
      const { appendArtwork } = await import('/src/artwork-creation.js');
      const other = blankDocument();
      appendArtwork(other, 'Rectangle', { at: [100, 90], name: 'Unrelated rectangle' });
      const project = structuredClone(window.nect.projectSnapshot());
      project.compositions.push({
        id: 'artwork-acceptance-unrelated',
        name: 'Unrelated artwork composition',
        width: 321,
        height: 234,
        editor: {
          ...guideDefaults(),
          guides: [{ id: 'artwork-acceptance-guide', axis: 'x', position: 37, locked: true }],
        },
        layers: other.layers,
      });
      await window.nect.load(JSON.stringify(project));
    }
  }, doc);
  unrelatedCompositions.set(
    page,
    (await page.evaluate(() => window.nect.projectSnapshot())).compositions.slice(1),
  );
}

export async function artworkProjectSnapshot(page) {
  const project = await page.evaluate(() => window.nect.projectSnapshot());
  assert.deepEqual(
    project.compositions.slice(1),
    unrelatedCompositions.get(page),
    'unrelated compositions retain every authored record',
  );
  return project;
}

export async function artworkDocumentSnapshot(page) {
  await artworkProjectSnapshot(page);
  return page.evaluate(() => JSON.stringify(window.nect.document));
}
