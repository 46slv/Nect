import { angleDialIndicator } from './repeater-angle-dial.js';

const EDIT_MODES = new Set(['live', 'staged', 'async']);

function requiredText(value, name) {
  if (typeof value !== 'string' || !value.trim()) throw Error(`${name} must be a non-empty string`);
  return value;
}

function optionalFinite(value, name) {
  if (value === undefined) return undefined;
  if (!Number.isFinite(value)) throw Error(`${name} must be finite`);
  return value;
}

function freezeRange(range = {}, name) {
  const min = optionalFinite(range.min, `${name}.min`),
    max = optionalFinite(range.max, `${name}.max`);
  if (min !== undefined && max !== undefined && min > max)
    throw Error(`${name}.min must not exceed ${name}.max`);
  return Object.freeze({
    ...(min === undefined ? {} : { min }),
    ...(max === undefined ? {} : { max }),
  });
}

function freezePort(port, direction, index) {
  if (!port || typeof port !== 'object') throw Error(`${direction}[${index}] must be an object`);
  return Object.freeze({
    id: requiredText(port.id, `${direction}[${index}].id`),
    type: requiredText(port.type, `${direction}[${index}].type`),
    ...(port.owner ? { owner: requiredText(port.owner, `${direction}[${index}].owner`) } : {}),
  });
}

function freezePresentation(presentation, index) {
  if (!presentation || typeof presentation !== 'object')
    throw Error(`presentations[${index}] must be an object`);
  const min = optionalFinite(presentation.min, `presentations[${index}].min`),
    max = optionalFinite(presentation.max, `presentations[${index}].max`),
    step = optionalFinite(presentation.step, `presentations[${index}].step`);
  if (min !== undefined && max !== undefined && min > max)
    throw Error(`presentations[${index}].min must not exceed max`);
  if (step !== undefined && step <= 0) throw Error(`presentations[${index}].step must be positive`);
  if (presentation.project !== undefined && typeof presentation.project !== 'function')
    throw Error(`presentations[${index}].project must be a function`);
  return Object.freeze({
    id: requiredText(presentation.id, `presentations[${index}].id`),
    type: requiredText(presentation.type, `presentations[${index}].type`),
    ...(presentation.unit
      ? { unit: requiredText(presentation.unit, `presentations[${index}].unit`) }
      : {}),
    ...(min === undefined ? {} : { min }),
    ...(max === undefined ? {} : { max }),
    ...(step === undefined ? {} : { step }),
    ...(presentation.project ? { project: presentation.project } : {}),
  });
}

// A ParameterSurface is disposable UI/edit metadata. It never stores an authored value,
// graph edge, document schema or Undo state; those remain with the named product owners.
export function defineParameterSurface({
  id,
  value,
  edit = {},
  presentations = [],
  connection = {},
  owner,
}) {
  if (!value || typeof value !== 'object') throw Error('value must be an object');
  if (!owner || typeof owner !== 'object') throw Error('owner must be an object');
  const mode = edit.mode || 'live';
  if (!EDIT_MODES.has(mode)) throw Error(`Unsupported edit mode: ${mode}`);
  const step = optionalFinite(edit.step, 'edit.step'),
    fineStep = optionalFinite(edit.fineStep, 'edit.fineStep'),
    coarseStep = optionalFinite(edit.coarseStep, 'edit.coarseStep'),
    soft = freezeRange({ min: edit.softMin, max: edit.softMax }, 'edit.soft');
  for (const [name, amount] of [
    ['edit.step', step],
    ['edit.fineStep', fineStep],
    ['edit.coarseStep', coarseStep],
  ])
    if (amount !== undefined && amount <= 0) throw Error(`${name} must be positive`);
  const views = presentations.map(freezePresentation),
    ids = new Set();
  for (const view of views) {
    if (ids.has(view.id)) throw Error(`Duplicate presentation id: ${view.id}`);
    ids.add(view.id);
  }
  if (connection.receive !== undefined && !Array.isArray(connection.receive))
    throw Error('connection.receive must be an array');
  if (connection.send !== undefined && !Array.isArray(connection.send))
    throw Error('connection.send must be an array');
  const receive = (connection.receive || []).map((port, index) =>
      freezePort(port, 'receive', index),
    ),
    send = (connection.send || []).map((port, index) => freezePort(port, 'send', index));
  return Object.freeze({
    type: 'ParameterSurface',
    id: requiredText(id, 'id'),
    value: Object.freeze({
      kind: requiredText(value.kind, 'value.kind'),
      ...(value.unit ? { unit: requiredText(value.unit, 'value.unit') } : {}),
      hard: freezeRange(value.hard, 'value.hard'),
      ...(value.multiTurn === undefined ? {} : { multiTurn: !!value.multiTurn }),
    }),
    edit: Object.freeze({
      mode,
      ...(step === undefined ? {} : { step }),
      ...(fineStep === undefined ? {} : { fineStep }),
      ...(coarseStep === undefined ? {} : { coarseStep }),
      ...(soft.min === undefined ? {} : { softMin: soft.min }),
      ...(soft.max === undefined ? {} : { softMax: soft.max }),
    }),
    presentations: Object.freeze(views),
    connection: Object.freeze({ receive: Object.freeze(receive), send: Object.freeze(send) }),
    owner: Object.freeze({
      value: requiredText(owner.value, 'owner.value'),
      transaction: requiredText(owner.transaction, 'owner.transaction'),
    }),
  });
}

export function parameterPresentation(surface, id) {
  if (surface?.type !== 'ParameterSurface') throw Error('Expected a ParameterSurface');
  const presentation = surface.presentations.find((candidate) => candidate.id === id);
  if (!presentation) throw Error(`Unknown presentation ${id} for ${surface.id}`);
  return presentation;
}

export function projectParameterPresentation(surface, id, snapshot) {
  if (!snapshot || typeof snapshot !== 'object' || !Object.hasOwn(snapshot, 'authored'))
    throw Error('Parameter snapshot requires authored');
  const presentation = parameterPresentation(surface, id);
  return presentation.project ? presentation.project(snapshot) : snapshot.authored;
}

// All projections and adapters are validated before any presentation mutates the view.
// An edit source therefore refreshes peer controls from one binding snapshot, not by
// dispatching synthetic events between controls.
export function renderParameterPresentations(surface, snapshot, adapters) {
  if (surface?.type !== 'ParameterSurface') throw Error('Expected a ParameterSurface');
  if (!snapshot || typeof snapshot !== 'object' || !Object.hasOwn(snapshot, 'authored'))
    throw Error('Parameter snapshot requires authored');
  if (!adapters || typeof adapters !== 'object') throw Error('Presentation adapters are required');
  const state = Object.freeze({
      authored: snapshot.authored,
      effective: Object.hasOwn(snapshot, 'effective') ? snapshot.effective : snapshot.authored,
      revision: snapshot.revision ?? null,
      readOnly: !!snapshot.readOnly,
      mixed: !!snapshot.mixed,
      source: snapshot.source ?? null,
      expression: snapshot.expression ?? null,
    }),
    plan = surface.presentations.map((presentation) => {
      const render = adapters[presentation.id];
      if (typeof render !== 'function')
        throw Error(`Missing presentation adapter: ${surface.id}.${presentation.id}`);
      return {
        presentation,
        render,
        value: presentation.project ? presentation.project(state) : state.authored,
      };
    });
  for (const item of plan)
    item.render(
      Object.freeze({
        surface,
        presentation: item.presentation,
        snapshot: state,
        value: item.value,
      }),
    );
  return state;
}

export const GRID_OPACITY_SURFACE = defineParameterSurface({
  id: 'document.gridOpacity',
  value: { kind: 'scalar', unit: 'ratio', hard: { min: 0, max: 1 } },
  edit: { mode: 'live', step: 0.01, softMin: 0, softMax: 1 },
  presentations: [
    { id: 'overlay', type: 'readout' },
    {
      id: 'slider',
      type: 'slider',
      unit: 'percent',
      min: 0,
      max: 100,
      step: 1,
      project: ({ authored }) => authored * 100,
    },
    {
      id: 'numeric',
      type: 'numeric',
      unit: 'percent',
      min: 0,
      max: 100,
      step: 1,
      project: ({ authored }) => authored * 100,
    },
  ],
  owner: { value: 'Document guide settings', transaction: 'Session set-guide-setting' },
});

export const GEOMETRY_REPEATER_ROTATION_SURFACE = defineParameterSurface({
  id: 'Geometry Repeater.rotation',
  value: {
    kind: 'angle',
    unit: 'degree',
    hard: { min: -Number.MAX_VALUE, max: Number.MAX_VALUE },
    multiTurn: true,
  },
  edit: { mode: 'live', step: 1, fineStep: 0.1, coarseStep: 10 },
  presentations: [
    { id: 'numeric', type: 'numeric', unit: 'degree' },
    {
      id: 'angle-dial',
      type: 'angle-dial',
      unit: 'degree',
      project: ({ authored }) => angleDialIndicator(authored),
    },
  ],
  connection: {
    receive: [{ id: 'source', type: 'Scalar', owner: 'Parameter.source' }],
  },
  owner: { value: 'Parameter value/expression/source', transaction: 'Session parameter gesture' },
});

export function nodeParameterSurface(type, key) {
  return type === 'Geometry Repeater' && key === 'rotation'
    ? GEOMETRY_REPEATER_ROTATION_SURFACE
    : null;
}
