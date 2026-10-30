import test from 'node:test';
import assert from 'node:assert/strict';
import {
  fontPickerCatalogs,
  fontPickerItems,
  selectedFontPickerId,
  fontFamilyGroups,
} from '../src/font-choice-ui.js';
import { createFontChoiceController } from '../src/font-choice-controller.js';
import { fontChoiceControls } from '../src/font-choice-inspector.js';
const face = {
  family: 'Family',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const ref = { kind: 'file-native-v1', sha256: 'a'.repeat(64), faceIndex: 1 };
const node = {
  type: 'Text',
  id: 'n',
  text: 'abc',
  params: { size: { value: 12 }, letterSpacing: { value: 0 }, lineSpacing: { value: 1 } },
};
const files = [
  {
    token: 'secret-token',
    sha256: ref.sha256,
    faces: [
      { faceIndex: 1, face },
      { faceIndex: 2, face },
    ],
  },
];
test('private members are distinct from installed and each other, and grouped by actual source/family', () => {
  const rows = fontPickerItems(fontPickerCatalogs({ windows: [face], fileNative: files }), node);
  const privateRows = rows.filter((r) => r.choice.source === 'file-native');
  assert.equal(privateRows.length, 2);
  assert.notEqual(privateRows[0].key, privateRows[1].key);
  assert.ok(privateRows.every((r) => !r.key.includes('secret-token')));
  assert.equal(fontFamilyGroups(rows).filter((g) => g.label === 'Family').length, 2);
  const saved = { ...node, osFont: { face, source: ref } };
  assert.equal(
    selectedFontPickerId(saved, rows),
    privateRows.find((r) => r.choice.sourceRef.faceIndex === 1).id,
  );
  assert.equal(
    selectedFontPickerId(
      saved,
      rows.filter((r) => r.choice.source === 'windows'),
    ),
    undefined,
  );
  assert.ok(
    fontPickerItems(fontPickerCatalogs({ fileNative: files }), node, { characterIds: ['x'] })
      .filter((r) => r.choice.source === 'file-native')
      .every((r) => r.disabled),
  );
  assert.match(
    fontChoiceControls(saved, { desktop: true, status: 'Font file missing' }),
    /Reattach.*Font file missing/s,
  );
});
function fixture(preparePrivate) {
  const commands = [];
  const owner = {
    state: { sessionId: 's', expectedRevision: 1 },
    dispatch: (c) => (commands.push(c), {}),
  };
  const controller = createFontChoiceController({
    owner,
    envelope: owner.state,
    layerId: 'l',
    nodeId: 'n',
    source: node,
    isCurrent: () => true,
    preparePrivate,
  });
  const row = fontPickerItems(fontPickerCatalogs({ fileNative: files }), node).find(
    (r) => r.choice.sourceRef?.faceIndex === 1,
  );
  return { controller, row, commands };
}
test('private apply verifies source and face and commits exactly once without token', async () => {
  const f = fixture(async (_node, choice, { settings }) => ({
    source: choice.sourceRef,
    outline: { face: choice.face, ligatures: settings.ligatures },
  }));
  await f.controller.select(f.row);
  assert.equal(await f.controller.apply(f.row), true);
  assert.equal(f.commands.length, 1);
  assert.equal(f.commands[0].type, 'set-os-font-text');
  assert.deepEqual(f.commands[0].payload.source, ref);
  assert.ok(!JSON.stringify(f.commands).includes('secret-token'));
});
test('mismatched source, face, and cancellation cannot commit', async () => {
  for (const variant of ['source', 'face', 'cancel']) {
    let resolve;
    const f = fixture(async (_node, choice, { settings }) => {
      if (variant === 'cancel')
        await new Promise((r) => {
          resolve = r;
        });
      return {
        source: variant === 'source' ? { ...ref, faceIndex: 2 } : choice.sourceRef,
        outline: {
          face: variant === 'face' ? { ...face, name: 'Other' } : face,
          ligatures: settings.ligatures,
        },
      };
    });
    await f.controller.select(f.row);
    const applying = f.controller.apply(f.row);
    if (variant === 'cancel') {
      await Promise.resolve();
      f.controller.cancel();
      resolve();
    }
    assert.equal(await applying, false);
    assert.equal(f.commands.length, 0);
  }
});

test('choosing an installed face explicitly replaces private identity instead of dropping it accidentally', async () => {
  const original = {
    ...node,
    osFont: { version: 2, face, source: ref, outline: { ligatures: false } },
  };
  const commands = [];
  const owner = {
    state: { sessionId: 's', expectedRevision: 1 },
    dispatch: (c) => (commands.push(c), {}),
  };
  const controller = createFontChoiceController({
    owner,
    envelope: owner.state,
    layerId: 'l',
    nodeId: 'n',
    source: original,
    isCurrent: () => true,
    prepareWindows: async (_node, selected, { settings }) => ({
      face: selected,
      ligatures: settings.ligatures,
    }),
  });
  const row = fontPickerItems(fontPickerCatalogs({ windows: [face] }), original).find(
    (r) => r.choice.source === 'windows',
  );
  await controller.select(row);
  assert.equal(await controller.apply(row), true);
  assert.equal(commands.length, 1);
  assert.equal(commands[0].payload.source, null);
});
