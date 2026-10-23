import test from 'node:test';
import assert from 'node:assert/strict';
import {
  evaluate,
  node,
  connect,
  serialize,
  deserialize,
  exportSVG,
  renderElement,
  CommandSession,
  parameterReferenceExpression,
} from '../src/core.js';
import { paintedGroup } from '../src/composition-reference.js';
import { formatSvgNumber } from '../src/svg-number-format.js';
import { strokeFixture } from './fixtures/repeater-alternate-stroke.js';
import { fillFixture } from './fixtures/repeater-alternate-fill.js';
const paths = (svg) => [...svg.matchAll(/<g opacity="([^"]+)"><path([^>]+)\/><\/g>/g)];
const strip = ({ repeaterStroke, ...c }) => c;
const send = (s, f, type, payload = {}) =>
  s.dispatch({
    ...s.state,
    type,
    payload: { layerId: f.layer.id, nodeId: f.repeat.id, ...payload },
  });
const style =
  'stroke-width="3" stroke-linecap="round" stroke-linejoin="bevel" stroke-miterlimit="7" stroke-dasharray="4 2" stroke-dashoffset="1.5"';

test('Scale/Fill/Stroke independently combine without moving geometry/IDs or changing source/style/opacity', () => {
  for (const kind of ['linear', 'radial'])
    for (const scale of [100, 60])
      for (const fill of [false, true]) {
        const f = strokeFixture(kind);
        f.repeat.params.alternateScale.value = scale;
        f.repeat.params.alternateFill.value = fill;
        const source = structuredClone(f.source),
          before = evaluate(f.layer).path;
        for (const enabled of [false, true]) {
          f.repeat.params.alternateStroke.value = enabled;
          const after = evaluate(f.layer).path;
          assert.deepEqual(after.contours.map(strip), before.contours);
          assert.deepEqual(after.fill, before.fill);
          assert.deepEqual(after.stroke, before.stroke);
          after.contours.forEach((c, i) =>
            assert.deepEqual(
              c.repeaterStroke,
              enabled && i % 2 ? { hex: '#7828a3', alpha: 0.5 } : undefined,
            ),
          );
          const p = paths(exportSVG(f.doc));
          assert.equal(p.length, 24);
          p.forEach(([, opacity, attrs], i) => {
            assert.equal(opacity, formatSvgNumber(before.contours[i].repeaterOpacity));
            assert.ok(attrs.includes(`fill="${fill && i % 2 ? '#c43d27' : '#285d8f'}"`));
            assert.ok(
              attrs.includes(
                `stroke="${enabled && i % 2 ? '#7828a3' : '#17232d'}" stroke-opacity="${enabled && i % 2 ? '0.5' : '0.4'}" ${style}`,
              ),
            );
          });
          assert.deepEqual(f.source, source);
        }
        const actual = evaluate(f.layer).path.contours;
        f.repeat.params.copiesAbove.value = false;
        assert.deepEqual(evaluate(f.layer).path.contours, actual.toReversed());
        assert.equal(exportSVG(deserialize(serialize(f.doc))), exportSVG(f.doc));
      }
});

test('transparent stroke and partial odd tail keep stroke alpha separate from group opacity', () => {
  const f = strokeFixture();
  f.repeat.params.alternateStroke.value = true;
  f.repeat.params.alternateStrokeColor.value = { hex: '#ff00aa', alpha: 0 };
  assert.equal(
    paths(exportSVG(f.doc)).filter(([, , a]) => a.includes('stroke-opacity="0"')).length,
    12,
  );
  f.repeat.params.alternateStrokeColor.value.alpha = 0.5;
  f.repeat.params.count.value = 1.5;
  f.repeat.params.startOpacity.value = 20;
  f.repeat.params.endOpacity.value = 80;
  const p = paths(exportSVG(f.doc));
  assert.deepEqual(
    p.map(([, o]) => o),
    ['0.2', '0.4'],
  );
  assert.ok(p[1][2].includes('stroke-opacity="0.5"'));
  assert.ok(p[1][2].includes(style));
});

test('none stays absent, fill gradients/mixed leaves stay intact; gradient/pattern strokes reject clearly', () => {
  const f = strokeFixture();
  connect(f.layer, f.fill.id, f.out.id, 'path');
  const none = exportSVG(f.doc);
  f.repeat.params.alternateStroke.value = true;
  assert.equal(exportSVG(f.doc), none);
  assert.ok(!none.includes(' stroke="'));
  connect(f.layer, f.stroke.id, f.out.id, 'path');
  connect(f.layer, f.repeat.id, f.stroke.id, 'path');
  const lineOnly = exportSVG(f.doc);
  assert.equal(evaluate(f.layer).path.fill, undefined);
  assert.equal(
    paths(lineOnly).filter(([, , a]) => a.includes('fill="none"') && a.includes(style)).length,
    24,
  );
  assert.equal(paths(lineOnly).filter(([, , a]) => a.includes('stroke="#7828a3"')).length, 12);
  connect(f.layer, f.fill.id, f.stroke.id, 'path');
  const solid = evaluate(f.layer).path;
  assert.equal(
    renderElement({ ...solid, stroke: { ...solid.stroke, kind: 'solid' } }, f.layer),
    renderElement(solid, f.layer),
  );
  const gradient = node('Linear Gradient Fill');
  gradient.id = f.fill.id;
  gradient.params.path.source = f.repeat.id;
  f.layer.nodes[f.layer.nodes.findIndex((n) => n.id === f.fill.id)] = gradient;
  const value = evaluate(f.layer).path,
    svg = exportSVG(f.doc);
  assert.equal((svg.match(/fill="url\(/g) || []).length, 24);
  const noStroke = { ...value };
  delete noStroke.stroke;
  const mixed = renderElement(paintedGroup([solid, value, noStroke]), f.layer);
  assert.equal((mixed.match(/stroke="#7828a3"/g) || []).length, 24);
  assert.equal((mixed.match(/fill="url\(/g) || []).length, 48);
  for (const kind of ['linear-gradient', 'radial-gradient', 'pattern'])
    for (const path of [solid, value])
      assert.throws(
        () => renderElement({ ...path, stroke: { ...path.stroke, kind } }, f.layer),
        /solid strokes.*gradient\/pattern/,
      );
});

test('normal and painted Transform preserve stroke metadata and existing geometry', () => {
  for (const painted of [false, true]) {
    const f = strokeFixture('radial'),
      t = node('Transform');
    f.layer.nodes.push(t);
    t.params.offset.value = [17, 29];
    t.params.rotation.value = -35;
    connect(f.layer, painted ? f.stroke.id : f.repeat.id, t.id, 'path');
    connect(f.layer, t.id, painted ? f.out.id : f.fill.id, 'path');
    const before = evaluate(f.layer).path;
    f.repeat.params.alternateStroke.value = true;
    const after = evaluate(f.layer).path;
    assert.deepEqual(after.contours.map(strip), before.contours.map(strip));
    assert.equal(
      paths(exportSVG(f.doc)).filter(([, , a]) => a.includes('stroke="#7828a3"')).length,
      12,
    );
  }
});

test('common Color references/expressions/errors, Undo/Redo, locks and alpha gesture cancellation remain atomic', () => {
  const f = strokeFixture(),
    c = node('Color'),
    v = node('Value');
  f.layer.nodes.push(c, v);
  const s = new CommandSession(f.doc),
    before = serialize(s.snapshot());
  send(s, f, 'set-parameter', { key: 'alternateStroke', value: true });
  const changed = serialize(s.snapshot());
  send(s, f, 'undo');
  assert.equal(serialize(s.snapshot()), before);
  assert.equal(s.historyView.past.length, 0);
  send(s, f, 'redo');
  assert.equal(serialize(s.snapshot()), changed);
  send(s, f, 'set-parameter-expression', {
    key: 'alternateStrokeColor',
    expression: 'alpha(value, 0.25)',
  });
  assert.deepEqual(evaluate(s.snapshot().layers[0]).path.contours[1].repeaterStroke, {
    hex: '#7828a3',
    alpha: 0.25,
  });
  send(s, f, 'set-parameter-base', {
    key: 'alternateStrokeColor',
    value: { hex: '#abcdef', alpha: 0.8 },
  });
  assert.equal(
    s.snapshot().layers[0].nodes[1].params.alternateStrokeColor.expression,
    'alpha(value, 0.25)',
  );
  send(s, f, 'undo');
  send(s, f, 'set-parameter-expression', {
    key: 'alternateStrokeColor',
    expression: parameterReferenceExpression(c.id, 'value'),
  });
  send(s, f, 'set-parameter', {
    nodeId: c.id,
    key: 'value',
    value: { hex: '#204060', alpha: 0.75 },
  });
  assert.deepEqual(evaluate(s.snapshot().layers[0]).path.contours[1].repeaterStroke, {
    hex: '#204060',
    alpha: 0.75,
  });
  for (const expression of [
    parameterReferenceExpression(v.id, 'value'),
    parameterReferenceExpression('missing-color', 'value'),
    parameterReferenceExpression(f.repeat.id, 'alternateStrokeColor'),
  ]) {
    const snapshot = serialize(s.snapshot()),
      state = s.state;
    assert.throws(
      () => send(s, f, 'set-parameter-expression', { key: 'alternateStrokeColor', expression }),
      /Color|reference|cycle|missing/i,
    );
    assert.equal(serialize(s.snapshot()), snapshot);
    assert.deepEqual(s.state, state);
  }
  for (const value of [
    { hex: '#bad', alpha: 1 },
    { hex: '#123456', alpha: 2 },
  ]) {
    const snapshot = serialize(s.snapshot());
    assert.throws(
      () => send(s, f, 'set-parameter-base', { key: 'alternateStrokeColor', value }),
      /Color/,
    );
    assert.equal(serialize(s.snapshot()), snapshot);
  }
  send(s, f, 'set-parameter-expression', { key: 'alternateStrokeColor', expression: '' });
  const snapshot = serialize(s.snapshot()),
    past = s.historyView.past.length;
  let g = send(s, f, 'begin-gesture', {
    target: {
      kind: 'parameter',
      layerId: f.layer.id,
      nodeId: f.repeat.id,
      key: 'alternateStrokeColor',
      alpha: true,
    },
  });
  g = send(s, f, 'update-gesture', { ...g, update: { value: 0.8 } });
  send(s, f, 'cancel-gesture', g);
  assert.equal(serialize(s.snapshot()), snapshot);
  assert.equal(s.historyView.past.length, past);
  send(s, f, 'set-parameter-lock', { key: 'alternateStrokeColor', locked: true });
  const locked = serialize(s.snapshot());
  assert.throws(
    () =>
      send(s, f, 'set-parameter', {
        key: 'alternateStrokeColor',
        value: { hex: '#fff000', alpha: 1 },
      }),
    /locked/,
  );
  assert.equal(serialize(s.snapshot()), locked);
  assert.equal(exportSVG(deserialize(serialize(s.snapshot()))), exportSVG(s.snapshot()));
});

test('defaultOFF schema46 migration preserves Fill/Scale; authorship promotes47 and retags/missing fields reject', () => {
  const f = fillFixture();
  f.repeat.params.alternateFill.value = true;
  const svg = exportSVG(f.doc);
  delete f.repeat.params.alternateStroke;
  delete f.repeat.params.alternateStrokeColor;
  const d = deserialize(serialize(f.doc));
  assert.equal(d.schemaVersion, 46);
  assert.equal(exportSVG(d), svg);
  const s = new CommandSession(d),
    before = serialize(s.snapshot());
  send(s, f, 'set-parameter', { key: 'alternateStroke', value: true });
  const changed = s.snapshot();
  assert.equal(changed.schemaVersion, 47);
  changed.schemaVersion = 46;
  assert.throws(() => deserialize(serialize(changed)), /schemaVersion 47/);
  changed.schemaVersion = 47;
  delete changed.layers[0].nodes[1].params.alternateStrokeColor;
  assert.throws(() => deserialize(serialize(changed)), /parameter|params/i);
  send(s, f, 'undo');
  assert.equal(serialize(s.snapshot()), before);
  send(s, f, 'set-parameter', {
    key: 'alternateStrokeColor',
    value: { hex: '#ff0000', alpha: 0.5 },
  });
  assert.equal(s.snapshot().schemaVersion, 47);
  assert.equal(exportSVG(s.snapshot()), svg);
});
