import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  evaluate,
  node,
  connect,
  serialize,
  deserialize,
  exportSVG,
  renderElement,
  parameterReferenceExpression,
} from '../src/core.js';
import { paintedGroup } from '../src/composition-reference.js';
import { formatSvgNumber } from '../src/svg-number-format.js';
import { alternateRepeaterFixture } from './fixtures/repeater-alternate-scale.js';
import { fillFixture } from './fixtures/repeater-alternate-fill.js';
const send = (s, f, type, payload = {}) =>
  s.dispatch({
    ...s.state,
    type,
    payload: { layerId: f.layer.id, nodeId: f.repeat.id, ...payload },
  });
const paths = (svg) => [...svg.matchAll(/<g opacity="([^"]+)"><path([^>]+)\/><\/g>/g)];
const strip = (c) => {
  const { repeaterFill, ...v } = c;
  return v;
};

test('odd fill is appearance-only, stable with alternate scale/offset/order; alpha wraps once and stroke/source stay unchanged', () => {
  for (const kind of ['linear', 'radial']) {
    const f = fillFixture(kind),
      source = structuredClone(f.source),
      before = evaluate(f.layer).path;
    f.repeat.params.alternateFill.value = true;
    const after = evaluate(f.layer).path;
    assert.deepEqual(after.contours.map(strip), before.contours);
    assert.deepEqual(after.stroke, before.stroke);
    assert.deepEqual(after.fill, before.fill);
    after.contours.forEach((c, i) =>
      assert.deepEqual(c.repeaterFill, i % 2 ? { hex: '#c43d27', alpha: 0.35 } : undefined),
    );
    assert.deepEqual(f.source, source);
    const rendered = paths(exportSVG(f.doc));
    assert.equal(rendered.length, 24);
    rendered.forEach(([_, opacity, attrs], i) => {
      assert.equal(opacity, formatSvgNumber(before.contours[i].repeaterOpacity));
      assert.ok(attrs.includes(` fill="${i % 2 ? '#c43d27' : '#285d8f'}"`));
      assert.ok(attrs.includes(` fill-opacity="${i % 2 ? '0.35' : '0.6'}"`));
      assert.ok(attrs.includes(' stroke="#17232d" stroke-opacity="0.4" stroke-width="2"'));
    });
    f.repeat.params.copiesAbove.value = false;
    assert.deepEqual(evaluate(f.layer).path.contours, after.contours.toReversed());
    assert.equal(exportSVG(deserialize(serialize(f.doc))), exportSVG(f.doc));
    // The final solid base is replaced only for odd copies, including alpha0.
    f.repeat.params.alternateColor.value = { hex: '#ff00aa', alpha: 0 };
    assert.equal(
      paths(exportSVG(f.doc)).filter(([, , a]) => a.includes('fill-opacity="0"')).length,
      12,
    );
    f.repeat.params.alternateColor.value.alpha = 0.35;
    f.repeat.params.count.value = 1.5;
    f.repeat.params.startOpacity.value = 20;
    f.repeat.params.endOpacity.value = 80;
    const partial = paths(exportSVG(f.doc));
    assert.deepEqual(
      partial.map(([, opacity]) => opacity),
      ['0.4', '0.2'],
    );
    assert.ok(
      partial[0][2].includes('fill-opacity="0.35"'),
      'odd tail keeps Color alpha separate from copy alpha',
    );
  }
});

test('none, linear/radial gradients and mixed painted leaves retain their paints; unsupported patterns still reject', () => {
  const f = fillFixture();
  const leaf = (kind) => {
    if (kind === 'none') connect(f.layer, f.repeat.id, f.stroke.id, 'path');
    else {
      if (kind !== 'solid') {
        const gradient = node(kind);
        gradient.id = f.fill.id;
        gradient.params.path.source = f.repeat.id;
        f.layer.nodes[f.layer.nodes.findIndex((n) => n.id === f.fill.id)] = gradient;
      }
      connect(f.layer, f.fill.id, f.stroke.id, 'path');
    }
    f.repeat.params.alternateFill.value = false;
    const before = exportSVG(f.doc);
    f.repeat.params.alternateFill.value = true;
    const after = exportSVG(f.doc),
      value = evaluate(f.layer).path;
    if (kind !== 'solid') assert.equal(after, before);
    return value;
  };
  const solid = leaf('solid'),
    none = leaf('none'),
    linear = leaf('Linear Gradient Fill'),
    radial = leaf('Radial Gradient Fill');
  const mixed = renderElement(paintedGroup([solid, none, linear, radial]), f.layer);
  assert.equal((mixed.match(/fill="#c43d27"/g) || []).length, 12);
  assert.equal((mixed.match(/fill="none"/g) || []).length, 24);
  assert.equal((mixed.match(/fill="url\(/g) || []).length, 48);
  assert.equal((mixed.match(/stroke-opacity="0.4"/g) || []).length, 96);
  assert.throws(() => renderElement({ ...solid, fill: { kind: 'pattern' } }, f.layer), /Color/);
  assert.throws(
    () => connect(f.layer, f.stroke.id, f.repeat.id, 'path'),
    /PaintedPath|Path|cycle/i,
  );
});

test('ordinary and painted Transform routes preserve copy fill metadata and geometry identity', () => {
  for (const painted of [false, true]) {
    const f = fillFixture('radial'),
      transform = node('Transform');
    transform.params.offset.value = [11, 17];
    transform.params.rotation.value = 23;
    f.layer.nodes.push(transform);
    connect(f.layer, painted ? f.stroke.id : f.repeat.id, transform.id, 'path');
    connect(f.layer, transform.id, painted ? f.out.id : f.fill.id, 'path');
    const before = evaluate(f.layer).path;
    f.repeat.params.alternateFill.value = true;
    const after = evaluate(f.layer).path;
    assert.deepEqual(after.contours.map(strip), before.contours.map(strip));
    assert.equal(
      paths(exportSVG(f.doc)).filter(([, , a]) => a.includes('fill="#c43d27"')).length,
      12,
    );
  }
});

test('shared Color expressions/references/dependencies and atomic errors use the existing typed path; edits remain undoable', () => {
  const f = fillFixture(),
    color = node('Color'),
    scalar = node('Value');
  color.params.value.value = { hex: '#336699', alpha: 0.5 };
  f.layer.nodes.push(color, scalar);
  const s = new CommandSession(f.doc),
    before = serialize(s.snapshot());
  send(s, f, 'set-parameter', { key: 'alternateFill', value: true });
  const changed = serialize(s.snapshot());
  send(s, f, 'undo');
  assert.equal(serialize(s.snapshot()), before);
  assert.equal(s.historyView.past.length, 0);
  send(s, f, 'redo');
  assert.equal(serialize(s.snapshot()), changed);
  send(s, f, 'set-parameter-expression', {
    key: 'alternateColor',
    expression: 'alpha(value, 0.25)',
  });
  assert.deepEqual(evaluate(s.snapshot().layers[0]).path.contours[1].repeaterFill, {
    hex: '#c43d27',
    alpha: 0.25,
  });
  send(s, f, 'set-parameter-base', {
    key: 'alternateColor',
    value: { hex: '#abcdef', alpha: 0.8 },
  });
  assert.equal(
    s.snapshot().layers[0].nodes[1].params.alternateColor.expression,
    'alpha(value, 0.25)',
  );
  send(s, f, 'undo');
  send(s, f, 'set-parameter-expression', {
    key: 'alternateColor',
    expression: parameterReferenceExpression(color.id, 'value'),
  });
  send(s, f, 'set-parameter', {
    nodeId: color.id,
    key: 'value',
    value: { hex: '#204060', alpha: 0.75 },
  });
  assert.deepEqual(evaluate(s.snapshot().layers[0]).path.contours[1].repeaterFill, {
    hex: '#204060',
    alpha: 0.75,
  });
  for (const expression of [
    parameterReferenceExpression(scalar.id, 'value'),
    parameterReferenceExpression('missing-color', 'value'),
    parameterReferenceExpression(f.repeat.id, 'alternateColor'),
  ]) {
    const original = serialize(s.snapshot()),
      state = s.state;
    assert.throws(
      () => send(s, f, 'set-parameter-expression', { key: 'alternateColor', expression }),
      /Color|reference|cycle|missing/i,
    );
    assert.equal(serialize(s.snapshot()), original);
    assert.deepEqual(s.state, state);
  }
  for (const value of [
    { hex: '#bad', alpha: 1 },
    { hex: '#123456', alpha: 2 },
  ]) {
    const original = serialize(s.snapshot());
    assert.throws(
      () => send(s, f, 'set-parameter-base', { key: 'alternateColor', value }),
      /Color/,
    );
    assert.equal(serialize(s.snapshot()), original);
  }
  assert.equal(exportSVG(deserialize(serialize(s.snapshot()))), exportSVG(s.snapshot()));
});

test('defaultOFF backfill preserves schema45/SVG; authored flag/color promotes46, Undo restores and retags reject', () => {
  const f = alternateRepeaterFixture();
  f.doc.schemaVersion = 45;
  f.repeat.params.alternateScale.value = 60;
  const svg = exportSVG(f.doc);
  delete f.repeat.params.alternateFill;
  delete f.repeat.params.alternateColor;
  const restored = deserialize(serialize(f.doc));
  assert.equal(restored.schemaVersion, 45);
  assert.equal(exportSVG(restored), svg);
  const s = new CommandSession(restored),
    before = serialize(s.snapshot());
  send(s, f, 'set-parameter', { key: 'alternateFill', value: true });
  const changed = s.snapshot();
  assert.equal(changed.schemaVersion, 46);
  changed.schemaVersion = 45;
  assert.throws(() => deserialize(serialize(changed)), /schemaVersion 46/);
  changed.schemaVersion = 46;
  delete changed.layers[0].nodes[1].params.alternateColor;
  assert.throws(() => deserialize(serialize(changed)), /parameter|params/i);
  send(s, f, 'undo');
  assert.equal(serialize(s.snapshot()), before);
  send(s, f, 'set-parameter', { key: 'alternateColor', value: { hex: '#ff0000', alpha: 0.5 } });
  assert.equal(s.snapshot().schemaVersion, 46);
  assert.equal(
    exportSVG(s.snapshot()),
    svg,
    'OFF keeps the old SVG even with a saved alternate Color',
  );
});
