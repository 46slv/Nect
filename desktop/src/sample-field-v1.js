// Internal Sample-domain evaluation slice. It does not define saved Field
// expressions, public nodes, general Space literals or coordinate conversion.
const LABEL = 'Sample Field v1';
const MAX_TOKEN_LENGTH = 256;

function captureData(value, required, optional = []) {
  if (
    value === null ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.getPrototypeOf(value) !== Object.prototype
  )
    return null;
  const present = Reflect.ownKeys(value);
  if (present.some((key) => !required.includes(key) && !optional.includes(key))) return null;
  const result = Object.create(null);
  for (const key of [...required, ...optional.filter((key) => present.includes(key))]) {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return null;
    result[key] = descriptor.value;
  }
  return result;
}

function captureArray(value, expectedLength, maximumLength) {
  if (!Array.isArray(value) || Object.getPrototypeOf(value) !== Array.prototype) return null;
  const lengthDescriptor = Object.getOwnPropertyDescriptor(value, 'length');
  if (!lengthDescriptor || !Object.hasOwn(lengthDescriptor, 'value')) return null;
  const length = lengthDescriptor.value;
  if (
    !Number.isSafeInteger(length) ||
    length < 0 ||
    length !== expectedLength ||
    length > maximumLength
  )
    return null;
  if (Reflect.ownKeys(value).length !== length + 1) return null;
  const result = [];
  for (let index = 0; index < length; index++) {
    const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return null;
    result.push(descriptor.value);
  }
  return result;
}

function vector(value) {
  const result = captureArray(value, 2, 2);
  return result && Number.isFinite(result[0]) && Number.isFinite(result[1]) ? result : null;
}

function token(value) {
  return typeof value === 'string' && value.length > 0 && value.length <= MAX_TOKEN_LENGTH;
}

/** Evaluate the supported pure subset against one caller-owned Sample context. */
export function evaluateSampleFieldV1(fieldInput, contextInput) {
  const field = captureData(
    fieldInput,
    ['kind', 'version', 'valueType', 'role', 'unit'],
    ['value', 'frameToken'],
  );
  if (!field || field.version !== 1) throw Error(`${LABEL} needs a supported descriptor`);
  const context = captureData(contextInput, ['domain', 'count', 'maxSamples'], ['position']);
  if (
    !context ||
    context.domain !== 'Sample' ||
    !Number.isSafeInteger(context.count) ||
    context.count < 0 ||
    !Number.isSafeInteger(context.maxSamples) ||
    context.maxSamples < 0
  )
    throw Error(`${LABEL} needs a valid Sample context`);
  if (context.count > context.maxSamples) throw Error(`${LABEL} exceeds the request budget`);

  if (field.kind === 'Constant') {
    if (!Object.hasOwn(field, 'value') || Object.hasOwn(field, 'frameToken'))
      throw Error(`${LABEL} needs a supported Constant`);
    let value;
    if (field.valueType === 'Scalar' && field.role === 'scale' && field.unit === 'ratio') {
      if (!Number.isFinite(field.value)) throw Error(`${LABEL} needs a finite Scalar`);
      value = field.value;
    } else if (field.valueType === 'Vector2' && field.role === 'scale' && field.unit === 'ratio') {
      value = vector(field.value);
      if (!value) throw Error(`${LABEL} needs a finite Vector2`);
    } else if (field.valueType === 'Boolean' && field.role === 'selection' && field.unit === null) {
      if (typeof field.value !== 'boolean') throw Error(`${LABEL} needs a Boolean`);
      value = field.value;
    } else {
      throw Error(`${LABEL} Constant type, role or unit is unsupported`);
    }
    return {
      kind: 'Uniform',
      version: 1,
      valueType: field.valueType,
      role: field.role,
      unit: field.unit,
      count: context.count,
      value,
    };
  }

  if (
    field.kind !== 'Position' ||
    field.valueType !== 'Vector2' ||
    field.role !== 'position' ||
    !token(field.unit) ||
    !token(field.frameToken) ||
    Object.hasOwn(field, 'value')
  )
    throw Error(`${LABEL} operator, type, role or unit is unsupported`);
  const position = captureData(context.position, ['frameToken', 'unit', 'values']);
  if (!position) throw Error(`${LABEL} requires a Position capability`);
  if (position.frameToken !== field.frameToken) throw Error(`${LABEL} Position frame mismatch`);
  if (position.unit !== field.unit) throw Error(`${LABEL} Position unit mismatch`);
  const inputs = captureArray(position.values, context.count, context.maxSamples);
  if (!inputs) throw Error(`${LABEL} requires ordered Position values for the population`);
  const values = [];
  for (let index = 0; index < inputs.length; index++) {
    const point = vector(inputs[index]);
    if (!point) throw Error(`${LABEL} requires finite Position vectors`);
    values.push(point);
  }
  return {
    kind: 'Varying',
    version: 1,
    valueType: 'Vector2',
    role: 'position',
    unit: field.unit,
    frameToken: field.frameToken,
    count: context.count,
    values,
  };
}
