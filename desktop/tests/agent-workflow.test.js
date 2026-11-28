import test from 'node:test';
import assert from 'node:assert/strict';
import {
  createAgentWorkflow,
  agentWorkflowTools,
  AgentWorkflowError,
} from '../src/agent-workflow.js';
import { blankDocument, blankLayer, node } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

const imageData =
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';

function imageContents({ visible = true } = {}) {
  const doc = blankDocument(),
    layer = blankLayer('PNG'),
    image = node('Image'),
    out = node('Image Output');
  doc.imageAssets = {
    'image-red': { name: 'red.png', mime: 'image/png', width: 1, height: 1, data: imageData },
  };
  image.asset = 'image-red';
  out.params.path.source = image.id;
  layer.nodes = [image, out];
  layer.output = out.id;
  layer.visible = visible;
  delete layer.allowEmptyOutput;
  doc.layers.push(layer);
  return serializeCompositionProject(migrateLegacyDocumentToCompositionProject(doc));
}

const tool = (workflow, name, args) => workflow.dispatch(`nect_${name}`, args);

async function ok(workflow, name, args) {
  const reply = await tool(workflow, name, args);
  assert.equal(reply.ok, true, JSON.stringify(reply));
  return reply.result;
}

async function error(workflow, name, args, code) {
  const reply = await tool(workflow, name, args);
  assert.equal(reply.ok, false);
  assert.equal(reply.error.code, code, JSON.stringify(reply));
}

test('API is blank at launch and rejects nonfinite, omitted, coercible and non-data New fields', async () => {
  const workflow = createAgentWorkflow({ files: {} });
  const control = await ok(workflow, 'inspect');
  assert.equal(control.target, null);
  assert.equal(control.composition, null);
  assert.equal(control.dirty, false);
  for (const bad of [undefined, null, '12', NaN, Infinity, -Infinity, 0, -1, 100001]) {
    await error(workflow, 'new', { target: null, width: bad, height: 20 }, 'INVALID_ARGUMENT');
    assert.deepEqual(await ok(workflow, 'inspect'), control);
  }
  await error(workflow, 'new', { target: null, height: 20 }, 'INVALID_ARGUMENT');
  await error(
    workflow,
    'new',
    {
      target: null,
      width: 5,
      get height() {
        assert.fail('Getter invoked');
      },
    },
    'INVALID_ARGUMENT',
  );
  await error(workflow, 'dispatch', { type: 'anything' }, 'UNKNOWN_OPERATION');
  const newTool = agentWorkflowTools().find((entry) => entry.name === 'nect_new');
  assert.deepEqual(newTool.inputSchema.required, ['target', 'width', 'height']);
  newTool.inputSchema.required.length = 0;
  assert.deepEqual(
    agentWorkflowTools().find((entry) => entry.name === 'nect_new').inputSchema.required,
    ['target', 'width', 'height'],
  );
  const result = await ok(workflow, 'new', { target: null, width: 100000, height: 10.25 });
  assert.equal(result.dirty, true);
  assert.equal(result.target.revision, 0);
});

test('dispatch serializes ownership across async writes; cancellation and I/O failure cannot admit edits or a binding', async () => {
  const waiting = Promise.withResolvers();
  let attempts = 0;
  const workflow = createAgentWorkflow({
    files: {
      async writeProject({ contents, binding, fileName }) {
        assert.equal(JSON.parse(contents).compositions[0].width, 150);
        assert.equal(fileName, 'a.json');
        assert.equal(binding, null);
        attempts++;
        if (attempts === 1) return waiting.promise;
        throw new AgentWorkflowError('FILE_BUSY', 'File is in use');
      },
    },
  });
  const { target } = await ok(workflow, 'new', { target: null, width: 150, height: 200 });
  const before = await ok(workflow, 'inspect');
  const save = tool(workflow, 'save', { target, fileName: 'a.json' });
  await error(workflow, 'new', { target, discard: true, width: 10, height: 10 }, 'BUSY');
  await error(workflow, 'create_rectangle', { target, x: 30, y: 50 }, 'BUSY');
  waiting.resolve(null);
  assert.equal((await save).error.code, 'SAVE_CANCELLED');
  assert.deepEqual(await ok(workflow, 'inspect'), before);
  await error(workflow, 'save', { target, fileName: 'a.json' }, 'FILE_BUSY');
  assert.deepEqual(await ok(workflow, 'inspect'), before);
});

test('Open needs affirmative image validation and releases only the unused candidate binding', async () => {
  const contents = imageContents();
  for (const capability of [{}, { async validateImages() {} }]) {
    const live = { token: Symbol(), fileName: 'live.json' };
    const candidate = { token: Symbol(), fileName: 'candidate.json' };
    const released = [];
    const workflow = createAgentWorkflow({
      files: {
        ...capability,
        async open() {
          return { contents, binding: candidate };
        },
        async writeProject() {
          return live;
        },
        release(binding) {
          released.push(binding);
        },
      },
    });
    const blank = await ok(workflow, 'new', { target: null, width: 150, height: 200 });
    const { target } = await ok(workflow, 'save', {
      target: blank.target,
      fileName: live.fileName,
    });
    const before = await ok(workflow, 'inspect');
    await error(
      workflow,
      'open',
      { target, fileName: candidate.fileName },
      'IMAGE_VALIDATION_UNAVAILABLE',
    );
    assert.deepEqual(await ok(workflow, 'inspect'), before);
    assert.deepEqual(released, [candidate]);
  }
});

test('SVG waits for image validation; rejection, unavailability and a decoder throw never publish', async () => {
  const contents = imageContents();
  const binding = { token: Symbol(), fileName: 'image.json' };
  let validation,
    entered,
    openValidation = true;
  const published = [];
  const workflow = createAgentWorkflow({
    files: {
      async open() {
        return { contents, binding };
      },
      async assertFresh() {},
      async validateImages({ imageAssets }) {
        if (openValidation) return 'valid';
        entered.resolve(imageAssets);
        return validation.promise;
      },
      async exportArtwork(output) {
        published.push(output);
      },
    },
  });
  const opened = await ok(workflow, 'open', { target: null, fileName: binding.fileName });
  openValidation = false;
  const edit = await ok(workflow, 'create_rectangle', { target: opened.target, x: 30, y: 30 });
  const { target } = edit;
  const before = await ok(workflow, 'inspect');
  const args = { target, fileName: 'out.svg' };
  for (const [outcome, code] of [
    ['rejected', 'IMAGE_REJECTED'],
    ['unavailable', 'IMAGE_VALIDATION_UNAVAILABLE'],
    [null, 'IMAGE_VALIDATION_UNAVAILABLE'],
    ['valid', null],
  ]) {
    validation = Promise.withResolvers();
    entered = Promise.withResolvers();
    const exporting = tool(workflow, 'export_svg', args);
    const images = await Promise.race([entered.promise, exporting.then(() => 'completed early')]);
    assert.deepEqual(Object.values(images), [{ data: imageData, width: 1, height: 1 }]);
    assert.deepEqual(published, []);
    await error(workflow, 'undo', { target }, 'BUSY');
    if (outcome === null) validation.reject(new Error('Could not launch C:/private/decoder.exe'));
    else validation.resolve(outcome);
    const result = await exporting;
    if (code) {
      assert.equal(result.ok, false);
      assert.equal(result.error.code, code, JSON.stringify(result));
      assert.doesNotMatch(result.error.message, /private/);
      assert.deepEqual(published, []);
    } else {
      assert.equal(result.ok, true);
      assert.equal(published.length, 1);
      assert.match(published[0].svg, /<image /);
      assert.equal(published[0].fileName, args.fileName);
    }
    assert.deepEqual(await ok(workflow, 'inspect'), before);
  }
});

test('SVG without an emitted image needs no decoder even when the project has an image asset', async () => {
  let calls = 0;
  const workflow = createAgentWorkflow({
    files: {
      async open() {
        return { contents: imageContents({ visible: false }), binding: null };
      },
      async validateImages() {
        return ++calls === 1 ? 'valid' : 'unavailable';
      },
      async assertFresh() {},
      async exportArtwork({ svg }) {
        assert.doesNotMatch(svg, /<image /);
      },
    },
  });
  const { target } = await ok(workflow, 'open', { target: null, fileName: 'hidden.json' });
  await ok(workflow, 'export_svg', { target, fileName: 'out.svg' });
  assert.equal(calls, 1);
});
