import assert from 'node:assert/strict';
import test from 'node:test';
import { Window } from 'happy-dom';
import { deserialize } from '../src/core.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import {
  observeProjectRejection,
  projectFileRejections,
} from './helpers/project-file-rejections.js';

const validate = (text) => {
  const project = deserializeCompositionProject(text, deserialize);
  for (const composition of project.compositions)
    deserialize(JSON.stringify(activeCompositionDocument(project, composition.id)));
  return project;
};

test('rejection fixtures each isolate the intended invalid condition', () => {
  for (const [name, text, diagnostic] of projectFileRejections()) {
    assert.throws(() => validate(text), diagnostic, name);
    if (name === 'malformed') continue;
    const repaired = JSON.parse(text);
    if (name === 'dimensions') repaired.compositions[0].width = 1000;
    else {
      const invalid = repaired.compositions
        .flatMap((composition) => composition.layers)
        .flatMap((layer) => layer.nodes)
        .filter((node) => node.type === 'Not a Nect node');
      assert.equal(invalid.length, 1);
      invalid[0].type = 'Rectangle';
    }
    validate(JSON.stringify(repaired));
    for (const composition of repaired.compositions)
      for (const layer of composition.layers)
        assert.equal(layer.nodes.filter((node) => node.type === 'Output').length, 1);
  }
});

const fixture = () => {
  const window = new Window();
  const document = window.document;
  document.body.innerHTML =
    '<input type="file"><div class="status"><span class="message">Open rejected; Invalid node</span></div>';
  const choose = (name) => {
    const input = document.querySelector('input');
    Object.defineProperty(input, 'files', { value: [{ name }], configurable: true });
    input.dispatchEvent(new window.Event('change', { bubbles: true }));
  };
  return { window, document, choose, flush: () => window.happyDOM.whenAsyncComplete() };
};

test('a stale rejection and menu rerender cannot complete the next file case', async () => {
  const f = fixture();
  const observed = observeProjectRejection(
    { name: 'dimensions.json', prefix: 'Open rejected;' },
    f.document,
  );
  try {
    assert.ok(f.document.querySelector('.message').textContent.startsWith('Open rejected;'));
    await f.flush();
    assert.equal(observed.text, null);
    f.choose('dimensions.json');
    f.document.querySelector('.status').innerHTML =
      '<span class="message">Open rejected; Invalid node</span>';
    await f.flush();
    assert.equal(observed.text, null);
    f.document.querySelector('.message').textContent = 'Open rejected; Invalid Composition width';
    await f.flush();
    assert.equal(observed.text, 'Open rejected; Invalid Composition width');
  } finally {
    observed.dispose();
    await f.window.happyDOM.close();
  }
});

test('only the chosen file arms observation; an identical fresh diagnostic still completes it', async () => {
  const f = fixture();
  const observed = observeProjectRejection(
    { name: 'inactive.json', prefix: 'Open rejected;' },
    f.document,
  );
  try {
    f.choose('other.json');
    f.document.querySelector('.message').textContent = 'Open rejected; Invalid node';
    await f.flush();
    assert.equal(observed.text, null);
    f.choose('inactive.json');
    f.document.querySelector('.message').textContent = 'Open rejected; Invalid node';
    await f.flush();
    assert.equal(observed.text, 'Open rejected; Invalid node');
  } finally {
    observed.dispose();
    await f.window.happyDOM.close();
  }
});

test('a fresh wrong diagnostic is observed, not hidden by expected-message filtering', async () => {
  const f = fixture();
  const observed = observeProjectRejection(
    { name: 'dimensions.json', prefix: 'Open rejected;' },
    f.document,
  );
  try {
    f.choose('dimensions.json');
    f.document.querySelector('.message').textContent = 'Open rejected; JSON parse error';
    await f.flush();
    assert.equal(observed.text, 'Open rejected; JSON parse error');
    assert.throws(() => assert.match(observed.text, /Invalid Composition width/));
  } finally {
    observed.dispose();
    await f.window.happyDOM.close();
  }
});
