import assert from 'node:assert/strict';
import test from 'node:test';
import { prepareInstalledTextContent } from '../src/installed-text-content.js';

const face = {
  family: 'Fixture',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const textNode = {
  type: 'Text',
  text: 'before',
  params: { size: { value: 48 }, letterSpacing: { value: 0 }, lineSpacing: { value: 1.2 } },
  osFont: { version: 3, face, axes: [], outline: { ligatures: true } },
};

test('Windows text edits retain the addressed OS face and normalize content', async () => {
  const requested = '日本語\r\nA\u030A';
  const outline = { face: { ...face } };
  const result = await prepareInstalledTextContent(
    textNode,
    requested,
    (source, exactFace, patch) => {
      assert.equal(source, textNode);
      assert.equal(exactFace, face);
      assert.deepEqual(patch, { text: requested });
      return outline;
    },
  );
  assert.deepEqual(result, { text: '日本語\n\u00C5', outline });
  assert.equal(textNode.text, 'before');
});

test('Windows text edits reject a substituted weight and never prepare legacy or private faces', async () => {
  await assert.rejects(
    prepareInstalledTextContent(textNode, 'new', () => ({ face: { ...face, weight: 700 } })),
    /Unmatched OS font response/,
  );
  const prepare = () => assert.fail('font preparation must not run');
  for (const osFont of [
    undefined,
    { ...textNode.osFont, version: 1 },
    { ...textNode.osFont, source: {} },
  ])
    assert.equal(await prepareInstalledTextContent({ ...textNode, osFont }, 'new', prepare), null);
});
