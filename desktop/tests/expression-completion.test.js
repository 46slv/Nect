import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  expressionCompletionContext,
  expressionCompletions,
  attachExpressionCompletion,
} from '../src/expression-completion.js';
import {
  expression,
  expressionReferenceTargets,
  node,
  document as makeDocument,
} from '../src/core.js';
const target = {
  name: 'Main Controller',
  nodeId: 'n-controller',
  key: 'position',
  component: 'x',
  label: 'Main Controller · position.x',
  expression: 'ref("n-controller", "position", "x")',
};
const suggest = (value, targets = [target]) =>
  expressionCompletions(value, value.length, value.length, targets);

test('prefixes use supported scalar grammar and component-safe stable reference tokens', () => {
  assert.equal(suggest('sq')[0].text, 'sqrt()');
  for (const name of ['sin', 'cos', 'tan', 'abs', 'sqrt', 'min', 'max', 'round']) {
    const item = suggest(name, [])[0];
    assert.equal(item.text, `${name}()`);
    assert.ok(Number.isFinite(expression(item.text.replace('()', '(1)'))));
  }
  assert.deepEqual(
    suggest('p', []).map((x) => x.text),
    ['pi'],
  );
  assert.equal(suggest('main')[0].text, target.expression);
  assert.equal(suggest('contr')[0].text, target.expression);
  assert.equal(suggest('pos')[0].text, target.expression);
  assert.match(suggest('pos')[0].detail, /Vector2.X → Scalar/);
  assert.equal(suggest('1; pos')[0].from, 3);
  assert.equal(suggest('1-pos')[0].from, 2);
  assert.equal(suggest('2sin').length, 0);
  assert.equal(suggest('').length, 0);
  assert.equal(suggest('unknown').length, 0);
  assert.equal(expressionCompletionContext('sin', 1, 3), null);
  assert.equal(suggest('ref("pos').length, 0);
  assert.equal(suggest('ref("a\\"pos').length, 0);
  assert.equal(suggest('ref("a\\\\" + si')[0].text, 'sin()');
  assert.equal(expressionCompletionContext('sqrt', 2).to, 4);
  assert.equal(
    suggest(
      'ref',
      Array.from({ length: 30 }, (_, i) => ({ ...target, nodeId: `n-${i}` })),
    ).length,
    12,
  );
});

test('live composition targets exclude nonnumeric operands, self and downstream nodes', () => {
  const doc = makeDocument(),
    layer = doc.layers[0];
  const source = node('Vector'),
    destination = node('Value'),
    downstream = node('Value');
  source.name = 'Controller';
  downstream.params.value.expression = `ref("${destination.id}", "value")`;
  layer.nodes = [source, destination, downstream];
  const targets = expressionReferenceTargets(layer, destination.id);
  assert.deepEqual(
    targets.filter((x) => x.nodeId === source.id).map((x) => x.component),
    ['x', 'y'],
  );
  assert.ok(!targets.some((x) => x.nodeId === destination.id || x.nodeId === downstream.id));
  assert.equal(suggest('cont', targets).length, 2);
  source.name = 'Renamed';
  assert.equal(suggest('cont', expressionReferenceTargets(layer, destination.id)).length, 0);
});

function editor() {
  const win = new Window();
  win.document.body.innerHTML = '<dialog open><input><button>Apply</button></dialog>';
  const input = win.document.querySelector('input');
  let targets = [target],
    applied = 0;
  const completion = attachExpressionCompletion(input, () => targets);
  input.parentElement.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.defaultPrevented && !e.isComposing) applied++;
  });
  input.focus();
  return {
    win,
    input,
    completion,
    get applied() {
      return applied;
    },
    setTargets(value) {
      targets = value;
    },
    type(value, start = value.length, end = start) {
      input.value = value;
      input.setSelectionRange(start, end);
      input.dispatchEvent(new win.Event('input'));
    },
    key(key, options = {}) {
      const event = new win.KeyboardEvent('keydown', {
        key,
        bubbles: true,
        cancelable: true,
        ...options,
      });
      input.dispatchEvent(event);
      return event;
    },
    list() {
      return win.document.querySelector('[role=listbox]');
    },
  };
}

test('keyboard completion changes draft only, leaves caret inside calls, and releases Tab/Enter', () => {
  const e = editor();
  e.type('1 + sqrt + 4', 6);
  assert.equal(e.input.getAttribute('aria-expanded'), 'true');
  assert.equal(e.list().children.length, 1);
  assert.ok(e.key('Enter').defaultPrevented);
  assert.equal(e.input.value, '1 + sqrt() + 4');
  assert.equal(e.input.selectionStart, 9);
  assert.equal(e.applied, 0);
  assert.ok(!e.key('Tab').defaultPrevented);
  e.key('Enter');
  assert.equal(e.applied, 1);
  e.type('pos');
  assert.ok(!e.key('Tab', { shiftKey: true }).defaultPrevented);
  assert.ok(e.key('Tab').defaultPrevented);
  assert.equal(e.input.value, target.expression);
  assert.equal(e.input.selectionStart, target.expression.length);
  e.type('nonsense');
  assert.ok(!e.key('Enter').defaultPrevented);
  assert.equal(e.applied, 2);
});

test('list navigation bounds, Escape, selection, click, focus and IME stay safe', () => {
  const e = editor();
  e.type('s');
  e.key('ArrowUp');
  assert.match(e.list().querySelector('[aria-selected=true]').textContent, /sqrt/);
  e.key('ArrowDown');
  assert.match(e.list().querySelector('[aria-selected=true]').textContent, /sin/);
  assert.ok(e.key('Escape').defaultPrevented);
  assert.ok(e.list().hidden);
  assert.ok(!e.key('Escape').defaultPrevented);
  e.type('sin', 0, 3);
  assert.ok(e.list().hidden);
  e.type('pos');
  e.list().firstElementChild.click();
  assert.equal(e.input.value, target.expression);
  e.type('s');
  e.input.dispatchEvent(new e.win.CompositionEvent('compositionstart'));
  assert.ok(e.list().hidden);
  e.type('sq');
  assert.ok(e.list().hidden);
  e.key('Enter', { isComposing: true });
  assert.equal(e.applied, 0);
  e.input.dispatchEvent(new e.win.CompositionEvent('compositionend'));
  assert.equal(e.list().children.length, 1);
  e.input.blur();
  assert.ok(e.list().hidden);
});

test('deleted or changed targets cannot be accepted; long replacements preserve draft', () => {
  const e = editor();
  e.type('pos');
  e.setTargets([]);
  assert.ok(!e.key('Tab').defaultPrevented);
  assert.equal(e.input.value, 'pos');
  assert.ok(e.list().hidden);
  e.type('s');
  e.input.setSelectionRange(0, 1);
  e.key('Tab');
  assert.equal(e.input.value, 's');
  e.setTargets([target]);
  e.type('1+'.repeat(2040) + 'pos');
  e.key('Tab');
  assert.equal(e.input.value.length, 4083);
});

test('value completion reports scalar or X/Y authored base without entering quoted strings', () => {
  assert.equal(suggest('va')[0].text, 'value');
  assert.match(suggest('va')[0].detail, /Authored base → Scalar/);
  assert.match(expressionCompletions('va; 1', 2, 2, [], 'Vector2')[0].detail, /base X component/);
  assert.match(expressionCompletions('1; va', 5, 5, [], 'Vector2')[0].detail, /base Y component/);
  assert.deepEqual(suggest('ref("va'), []);
});
