import test from 'node:test';
import assert from 'node:assert/strict';
import { expressionCompletions, expressionReferenceType } from '../src/expression-completion.js';

const color = {
  name: 'Shared Color',
  nodeId: 'n-color',
  key: 'value',
  type: 'Color',
  label: 'Shared Color',
  expression: 'ref("n-color", "value")',
};
const scalar = {
  name: 'Weight',
  nodeId: 'n-weight',
  key: 'value',
  type: 'Scalar',
  label: 'Weight',
  expression: 'ref("n-weight", "value")',
};
const vector = {
  ...scalar,
  name: 'Position',
  nodeId: 'n-vector',
  key: 'position',
  type: 'Vector2',
  component: 'x',
  expression: 'ref("n-vector", "position", "x")',
};
const suggest = (text, type = 'Color') =>
  expressionCompletions(text, text.length, text.length, [color, scalar, vector], type);

test('Color operator completions describe required types and preserve the existing bounded grammar', () => {
  assert.match(suggest('mi')[0].label, /mix\(Color, Color, Scalar\)/);
  assert.match(suggest('mi')[0].detail, /Linear-light/);
  assert.match(suggest('al')[0].label, /alpha\(Color, Scalar\)/);
  assert.match(suggest('al')[0].detail, /Replace alpha/);
  assert.equal(suggest('si').length, 0);
  assert.equal(suggest('wei').length, 0);
  assert.equal(suggest('Sha')[0].text, color.expression);
  assert.equal(suggest('mi', 'Scalar').length, 1); // Scalar min remains available.
  assert.equal(suggest('al', 'Scalar').length, 0);
});
test('Color weights expose Scalar/component references and functions, never a Color base coercion', () => {
  for (const prefix of ['mix(value,#000000,', 'alpha(value,', 'mix(alpha(value,0.5),value,']) {
    assert.equal(suggest(prefix + 'wei')[0].text, scalar.expression);
    assert.equal(suggest(prefix + 'pos')[0].text, vector.expression);
    assert.equal(suggest(prefix + 'si')[0].text, 'sin()');
    assert.ok(
      suggest(prefix + 'va').every((item) => item.id !== 'value' && item.text !== color.expression),
    );
    assert.equal(suggest(prefix + 'Sha').length, 0);
    assert.equal(expressionReferenceType(prefix, prefix.length, 'Color'), 'Scalar');
  }
  const nested = 'mix(value,alpha(Sha';
  assert.equal(suggest(nested)[0].text, color.expression);
  assert.equal(expressionReferenceType(nested, nested.length, 'Color'), 'Color');
  assert.equal(suggest('mix(value,ref("Sha').length, 0);
});
