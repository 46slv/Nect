import test from 'node:test';
import assert from 'node:assert/strict';
import { workspaceIcon } from '../src/ui-icons.js';

test('workspace symbols are unique, font-independent, decorative SVGs', () => {
  const names = [
    'direct',
    'select',
    'pen',
    'Circle',
    'Rectangle',
    'Polygon',
    'Star',
    'Text',
    'assets',
    'undo',
    'redo',
    'expression',
    'colors',
    'layers',
    'align',
    'inspector',
  ];
  const icons = names.map(workspaceIcon);
  assert.equal(new Set(icons).size, names.length);
  for (const icon of icons) {
    assert.match(icon, /viewBox="0 0 24 24"/);
    assert.match(icon, /aria-hidden="true"/);
    assert.match(icon, /stroke="currentColor"/);
    assert.doesNotMatch(icon, /<text/);
  }
  assert.equal((workspaceIcon('colors').match(/<circle/g) || []).length, 2);
  assert.equal((workspaceIcon('colors').match(/fill="#/g) || []).length, 6);
  assert.doesNotMatch(workspaceIcon('colors'), /gradient/i);
  assert.match(workspaceIcon('direct'), /x="1" y="1" width="5.5"/);
  assert.match(workspaceIcon('select'), /fill="currentColor"/);
  assert.match(workspaceIcon('assets'), /cx="17.5"/);
  assert.notEqual(workspaceIcon('eye'), workspaceIcon('eye-off'));
  assert.match(workspaceIcon('eye-off'), /m2 2 20 20/);
  assert.throws(() => workspaceIcon('unknown'), /Unknown workspace icon/);
});
