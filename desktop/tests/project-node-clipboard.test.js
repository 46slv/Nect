import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import {
  blankDocument,
  blankLayer,
  node,
  connect,
  createCharacterPositions,
  CommandSession,
  bindProjectDocumentContext,
} from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { makeFontAsset } from '../src/fonts.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

function fixture({ setup = () => {}, ...options } = {}) {
  const doc = blankDocument(),
    target = blankDocument(),
    sourceLayer = doc.layers[0],
    targetLayer = target.layers[0],
    value = node('Value', 40, 70),
    math = node('Math', 340, 110),
    alternate = node('Value', 40, 300);
  value.params.value.value = 7;
  alternate.params.value.value = 42;
  connect({ nodes: [value, math] }, value.id, math.id, 'a');
  sourceLayer.nodes.push(value, math, alternate);
  setup(doc, target);
  const project = migrateLegacyDocumentToCompositionProject(doc);
  project.compositions[0].id = 'source-composition';
  project.compositions.push({
    id: 'target-composition',
    name: 'Target',
    width: target.width,
    height: target.height,
    layers: target.layers,
    editor: target.editor,
  });
  const owner = createCompositionSessionOwner(project, options),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    copy = (nodeIds = [value.id, math.id], layerId = sourceLayer.id) =>
      send('copy-nodes', { layerId, nodeIds }),
    paste = (layerId = targetLayer.id) => send('paste-nodes', { layerId }),
    state = () => JSON.stringify(owner.projectSnapshot());
  return {
    doc,
    target,
    project,
    sourceLayer,
    targetLayer,
    value,
    math,
    alternate,
    owner,
    send,
    copy,
    paste,
    state,
  };
}

function bridge() {
  return {
    async prepareDocument(document) {
      return { document: structuredClone(document) };
    },
    assertFresh(ticket) {
      assert.ok(ticket.document);
    },
    preparedFonts: () => ({}),
    activate() {},
    cancel() {},
  };
}

for (const prepared of [false, true]) {
  test(`${prepared ? 'prepared' : 'synchronous'} outer-tab switch retains a captured graph without History or dirty changes`, async () => {
    const notifications = [],
      f = fixture({ onCommitted: (event) => notifications.push(event) });
    const initial = f.state(),
      revision = f.owner.projectRevision;
    f.copy();
    assert.equal(f.state(), initial);
    assert.equal(f.owner.historyView.past.length, 0);
    f.send('set-parameter', {
      layerId: f.sourceLayer.id,
      nodeId: f.value.id,
      key: 'value',
      value: 99,
    });
    const beforeSwitch = f.state(),
      history = f.owner.historyView.past.length,
      projectRevision = f.owner.projectRevision,
      notificationsBefore = notifications.length;
    if (prepared) {
      const token = await f.owner.prepareCompositionSwitch('target-composition', bridge());
      f.owner.commitPreparedCompositionSwitch(token);
    } else f.owner.switchComposition('target-composition');
    assert.equal(f.state(), beforeSwitch);
    assert.equal(f.owner.projectRevision, projectRevision);
    assert.equal(f.owner.historyView.past.length, history);
    assert.equal(notifications.length, notificationsBefore);
    const ids = f.paste().nodeIds,
      saved = f.owner.snapshot(),
      layer = saved.layers.find((item) => item.id === f.targetLayer.id),
      copies = ids.map((id) => layer.nodes.find((item) => item.id === id));
    assert.equal(copies[0].params.value.value, 7);
    assert.equal(copies[1].params.a.source, copies[0].id);
    assert.ok(ids.every((id) => ![f.value.id, f.math.id].includes(id)));
    assert.deepEqual(
      copies.map((item) => item.position),
      [
        [70, 100],
        [370, 140],
      ],
    );
    assert.equal(layer.output, f.targetLayer.output);
    assert.equal(f.owner.historyView.past.length, history + 1);
    const pasted = f.state();
    f.send('undo');
    assert.equal(f.state(), beforeSwitch);
    f.send('redo');
    assert.equal(f.state(), pasted);
    f.owner.switchComposition('source-composition');
    f.owner.switchComposition('target-composition');
    const again = f.paste().nodeIds;
    assert.ok(again.every((id) => !ids.includes(id)));
    assert.deepEqual(
      f.owner.snapshot().layers[0].nodes.find((item) => item.id === again[0]).position,
      [100, 130],
    );
    assert.ok(f.owner.projectRevision > revision);
  });
}

for (const prepared of [false, true]) {
  test(`${prepared ? 'prepared' : 'synchronous'} switch cannot overwrite a newer Copy in beforeCommit`, async () => {
    const f = fixture();
    f.copy();
    const before = f.state();
    const options = { beforeCommit: () => f.copy([f.alternate.id]) };
    if (prepared) {
      const token = await f.owner.prepareCompositionSwitch('target-composition', bridge());
      assert.throws(
        () => f.owner.commitPreparedCompositionSwitch(token, options),
        /STALE.*clipboard/i,
      );
    } else
      assert.throws(
        () => f.owner.switchComposition('target-composition', options),
        /STALE.*clipboard/i,
      );
    assert.equal(f.owner.activeCompositionId, 'source-composition');
    assert.equal(f.state(), before);
    const id = f.paste(f.sourceLayer.id).nodeIds[0];
    assert.equal(
      f.owner.snapshot().layers[0].nodes.find((item) => item.id === id).params.value.value,
      42,
    );
  });
}

test('a new successful Copy replaces the project clipboard across return trips', () => {
  const f = fixture();
  f.copy();
  f.owner.switchComposition('target-composition');
  const id = f.paste().nodeIds[0];
  f.send('set-parameter', { layerId: f.targetLayer.id, nodeId: id, key: 'value', value: 123 });
  f.copy([id], f.targetLayer.id);
  f.owner.switchComposition('source-composition');
  const copy = f.paste(f.sourceLayer.id).nodeIds[0];
  assert.equal(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === copy).params.value.value,
    123,
  );
});

test('failed and canceled switches leave the current Copy usable and produce no History', async () => {
  const f = fixture();
  f.copy();
  const before = f.state(),
    state = f.owner.state;
  assert.throws(() => f.owner.switchComposition('missing'), /Unknown/);
  assert.throws(
    () =>
      f.owner.switchComposition('target-composition', {
        beforeCommit: () => {
          throw Error('cancel switch');
        },
      }),
    /cancel switch/,
  );
  const b = bridge(),
    token = await f.owner.prepareCompositionSwitch('target-composition', b);
  f.owner.cancelPreparedCompositionSwitch(b);
  assert.throws(() => f.owner.commitPreparedCompositionSwitch(token), /STALE/);
  assert.equal(f.state(), before);
  assert.deepEqual(f.owner.state, state);
  assert.equal(f.paste(f.sourceLayer.id).nodeIds.length, 2);
});

test('Copy during asynchronous preparation uses the latest snapshot at commit time', async () => {
  const f = fixture();
  f.copy();
  const token = await f.owner.prepareCompositionSwitch('target-composition', bridge());
  f.copy([f.alternate.id]);
  f.owner.commitPreparedCompositionSwitch(token);
  const id = f.paste().nodeIds[0];
  assert.equal(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === id).params.value.value,
    42,
  );
});

test('unsupported external inputs and Layer-graph references clear on an outer-tab crossing', () => {
  for (const kind of ['input', 'layer reference']) {
    const f = fixture({
      setup: (doc) => {
        if (kind !== 'layer reference') return;
        const ref = node('Composition Reference'),
          svg = blankLayer('SVG source');
        doc.schemaVersion = 31;
        doc.layers.push(svg);
        ref.compositionReference = { compositionId: svg.id };
        doc.layers[0].nodes.push(ref);
      },
    });
    const selected = kind === 'input' ? f.math : f.doc.layers[0].nodes.at(-1);
    f.copy([selected.id]);
    f.owner.switchComposition('target-composition');
    const before = f.state();
    assert.throws(() => f.paste(), /clipboard is empty/i);
    assert.equal(f.state(), before);
    f.owner.switchComposition('source-composition');
    assert.throws(() => f.paste(f.sourceLayer.id), /clipboard is empty/i);
  }
});

test('shared font/image/Color-slot identities cross tabs without registry duplication or literal substitution', async () => {
  const bytes = await readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = makeFontAsset(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
      'DejaVuSans.ttf',
    ),
    text = node('Text'),
    image = node('Image'),
    color = node('Color');
  text.font = font.id;
  text.text = 'Portable';
  text.characterPositions = createCharacterPositions(text.text);
  image.asset = 'image-shared';
  color.params.value.colorSlotId = 'shared-color';
  const f = fixture({
    setup: (doc) => {
      doc.schemaVersion = 64;
      doc.fontAssets = { [font.id]: font.asset };
      doc.imageAssets = {
        'image-shared': {
          name: 'pixel.png',
          mime: 'image/png',
          width: 1,
          height: 1,
          data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
        },
      };
      doc.colorSlots = [
        { id: 'shared-color', name: 'Shared', value: { hex: '#aBcD09', alpha: 0 } },
      ];
      doc.layers[0].nodes.push(text, image, color);
    },
  });
  f.copy([text.id, image.id, color.id]);
  f.owner.switchComposition('target-composition');
  const before = f.owner.projectSnapshot(),
    ids = f.paste().nodeIds,
    nodes = f.owner.snapshot().layers[0].nodes.filter((item) => ids.includes(item.id));
  assert.equal(nodes[0].font, font.id);
  assert.deepEqual(nodes[0].characterPositions, text.characterPositions);
  assert.equal(nodes[1].asset, image.asset);
  assert.equal(nodes[2].params.value.colorSlotId, 'shared-color');
  const after = f.owner.projectSnapshot();
  assert.deepEqual(after.fontAssets, before.fontAssets);
  assert.deepEqual(after.imageAssets, before.imageAssets);
  assert.deepEqual(after.colorSlots, before.colorSlots);
});

test('ordinary Composition create/resize/delete and History fallback preserve a self-contained Copy', () => {
  const f = fixture();
  f.copy([f.value.id]);
  const added = f.owner.createComposition({ name: 'Created target' }),
    layer = f.owner.snapshot().layers[0];
  f.owner.resizeComposition(added, 1200, 700);
  assert.equal(f.paste(layer.id).nodeIds.length, 1);
  f.owner.deleteComposition(added);
  assert.equal(f.paste(f.owner.snapshot().layers[0].id).nodeIds.length, 1);
  const g = fixture();
  const created = g.owner.createComposition(),
    target = g.owner.snapshot().layers[0],
    id = g.send('create-node', { layerId: target.id, nodeType: 'Value' }).nodeId;
  g.copy([id], target.id);
  g.send('undo');
  g.send('undo');
  assert.equal(g.owner.activeCompositionId, 'source-composition');
  assert.equal(g.paste(g.sourceLayer.id).nodeIds.length, 1);
});

test('a Copy during project validation rejects a stale resize without losing the newer snapshot', () => {
  let f,
    armed = false;
  f = fixture({
    fonts: () => {
      if (armed) {
        armed = false;
        f.copy([f.alternate.id]);
      }
      return {};
    },
  });
  f.copy();
  const before = f.state();
  armed = true;
  assert.throws(
    () => f.owner.resizeComposition('source-composition', 1200, 700),
    /STALE.*clipboard/i,
  );
  assert.equal(f.state(), before);
  const id = f.paste(f.sourceLayer.id).nodeIds[0];
  assert.equal(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === id).params.value.value,
    42,
  );
});

for (const prepared of [false, true]) {
  test(`${prepared ? 'prepared' : 'synchronous'} switch rejects a final busy callback which changes tabs and copies newer nodes`, async () => {
    let f,
      armed = false,
      third,
      thirdLayer,
      thirdValue;
    f = fixture({
      busy: () => {
        if (armed) {
          armed = false;
          f.owner.switchComposition(third);
          f.copy([thirdValue], thirdLayer);
        }
        return false;
      },
    });
    third = f.owner.createComposition({ name: 'Third' });
    thirdLayer = f.owner.snapshot().layers[0].id;
    thirdValue = f.send('create-node', { layerId: thirdLayer, nodeType: 'Value' }).nodeId;
    f.send('set-parameter', { layerId: thirdLayer, nodeId: thirdValue, key: 'value', value: 42 });
    f.owner.switchComposition('source-composition');
    f.copy([f.value.id]);
    const before = f.state(),
      options = {
        beforeCommit: () => {
          armed = true;
        },
      };
    if (prepared) {
      const token = await f.owner.prepareCompositionSwitch('target-composition', bridge());
      assert.throws(() => f.owner.commitPreparedCompositionSwitch(token, options), /STALE/);
    } else assert.throws(() => f.owner.switchComposition('target-composition', options), /STALE/);
    assert.equal(f.owner.activeCompositionId, third);
    assert.equal(f.state(), before);
    const id = f.paste(thirdLayer).nodeIds[0];
    assert.equal(
      f.owner.snapshot().layers[0].nodes.find((item) => item.id === id).params.value.value,
      42,
    );
  });
}

for (const kind of ['numeric', 'Color']) {
  test(`${kind} Artwork references remain confined to their outer Composition`, () => {
    let sink;
    const f = fixture({
      setup: (doc) => {
        const foreign = blankLayer('Foreign Artwork'),
          source = node(kind === 'Color' ? 'Color' : 'Value');
        sink = node(kind === 'Color' ? 'Fill' : 'Value');
        doc.schemaVersion = 64;
        foreign.nodes.push(source);
        doc.layers.push(foreign);
        doc.layers[0].nodes.push(sink);
        const key = kind === 'Color' ? 'color' : 'value',
          qualifier = kind === 'Color' ? 'colorArtwork' : 'artwork';
        sink.params[key].expression = `ref(${qualifier}, "${foreign.id}", "${source.id}", "value")`;
      },
    });
    f.copy([sink.id]);
    const before = f.state();
    f.owner.switchComposition('target-composition');
    assert.throws(() => f.paste(), /clipboard is empty/i);
    f.owner.switchComposition('source-composition');
    assert.throws(() => f.paste(f.sourceLayer.id), /clipboard is empty/i);
    assert.equal(f.state(), before);
  });
}

test('assets removed by History after switching refuse atomically without consuming Redo or offset', () => {
  const f = fixture(),
    image = f.send('adopt-image', {
      name: 'temporary.png',
      png: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
    });
  f.copy([image.nodeId], image.layerId);
  f.owner.switchComposition('target-composition');
  f.send('undo');
  const before = f.state(),
    state = f.owner.state,
    future = f.owner.historyView.future;
  assert.throws(() => f.paste(), /image is no longer present/i);
  assert.equal(f.state(), before);
  assert.deepEqual(f.owner.state, state);
  assert.deepEqual(f.owner.historyView.future, future);
  f.send('redo');
  const id = f.paste().nodeIds[0];
  assert.deepEqual(
    f.owner.snapshot().layers[0].nodes.find((item) => item.id === id).position,
    [70, 70],
  );
});

test('copied Curve point references remain internal with fresh identities across outer tabs', () => {
  const curve = node('Curve'),
    consumer = node('Value'),
    pointId = curve.points[0].id;
  consumer.params.value.expression = `ref("${curve.id}", "${pointId}.position", "x")`;
  const f = fixture({
    setup: (doc) => {
      doc.schemaVersion = 64;
      doc.layers[0].nodes.push(curve, consumer);
    },
  });
  f.copy([curve.id, consumer.id]);
  f.owner.switchComposition('target-composition');
  const ids = f.paste().nodeIds,
    nodes = f.owner.snapshot().layers[0].nodes,
    copiedCurve = nodes.find((item) => item.id === ids[0]),
    copiedConsumer = nodes.find((item) => item.id === ids[1]);
  assert.ok(copiedCurve.points.every((point, index) => point.id !== curve.points[index].id));
  assert.ok(copiedConsumer.params.value.expression.includes(copiedCurve.id));
  assert.ok(copiedConsumer.params.value.expression.includes(copiedCurve.points[0].id));
  assert.ok(!copiedConsumer.params.value.expression.includes(curve.id));
  assert.ok(!copiedConsumer.params.value.expression.includes(pointId));
});

test('external inputs cannot cross outer contexts even when destination IDs coincide', () => {
  const doc = blankDocument(),
    value = node('Value'),
    math = node('Math'),
    owner = Object.freeze({});
  value.params.value.value = 7;
  connect({ nodes: [value, math] }, value.id, math.id, 'a');
  doc.layers[0].nodes.push(value, math);
  const target = structuredClone(doc);
  target.layers[0].nodes.find((item) => item.id === value.id).params.value.value = 42;
  const project = { compositions: [{ id: 'source' }, { id: 'target' }] };
  bindProjectDocumentContext(doc, project, 'source');
  bindProjectDocumentContext(target, project, 'target');
  const source = new CommandSession(doc, { nodeClipboardOwner: owner });
  source.dispatch({
    ...source.state,
    type: 'copy-nodes',
    payload: { layerId: doc.layers[0].id, nodeIds: [math.id] },
  });
  const destination = new CommandSession(target, {
      nodeClipboardOwner: owner,
      nodeClipboardFrom: source,
    }),
    before = destination.snapshot();
  assert.throws(
    () =>
      destination.dispatch({
        ...destination.state,
        type: 'paste-nodes',
        payload: { layerId: target.layers[0].id },
      }),
    /clipboard is empty/i,
  );
  assert.deepEqual(destination.snapshot(), before);
});
