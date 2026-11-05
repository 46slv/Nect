// Minimal declaration resolver; no DOM, Document, filesystem, or evaluator imports.
function copyData(value, seen = new Set(), depth = 0, budget = { count: 0 }) {
  if (++budget.count > 4096 || depth > 32) throw Error('Declaration exceeds limits');
  if (typeof value === 'string') {
    if (value.length > 65536) throw Error('Declaration exceeds limits');
    return value;
  }
  if (value === null || typeof value === 'boolean') return value;
  if (typeof value === 'number' && Number.isFinite(value) && !Object.is(value, -0)) return value;
  if (typeof value !== 'object' || seen.has(value)) throw Error('Declaration must be data only');
  const array = Array.isArray(value),
    proto = Object.getPrototypeOf(value);
  if (array ? proto !== Array.prototype : proto !== Object.prototype && proto !== null)
    throw Error('Declaration must use plain data');
  seen.add(value);
  const descriptors = Object.getOwnPropertyDescriptors(value),
    keys = Reflect.ownKeys(descriptors);
  if (keys.some((key) => typeof key !== 'string')) throw Error('Declaration has a symbol key');
  if (keys.some((key) => key.length > 1024)) throw Error('Declaration exceeds limits');
  const out = array ? [] : {};
  if (array) {
    if (value.length > 4096) throw Error('Declaration exceeds limits');
    for (let i = 0; i < value.length; i++) {
      const d = descriptors[String(i)];
      if (!d || !Object.hasOwn(d, 'value') || !d.enumerable)
        throw Error('Invalid declaration array');
      out.push(copyData(d.value, seen, depth + 1, budget));
    }
    if (
      keys.some(
        (key) =>
          key !== 'length' && (!/^(0|[1-9][0-9]*)$/.test(key) || Number(key) >= value.length),
      )
    )
      throw Error('Invalid declaration array key');
  } else {
    for (const key of keys) {
      const d = descriptors[key];
      if (!d.enumerable || !Object.hasOwn(d, 'value'))
        throw Error('Declaration has accessor or hidden data');
      Object.defineProperty(out, key, {
        value: copyData(d.value, seen, depth + 1, budget),
        enumerable: true,
      });
    }
  }
  seen.delete(value);
  return Object.freeze(out);
}
const requiredString = (value, label) => {
  if (typeof value !== 'string' || !value.trim()) throw Error(`Invalid ${label}`);
  return value;
};
function uniqueIds(items, label) {
  if (!Array.isArray(items) || !items.length) throw Error(`Invalid ${label}`);
  const ids = new Set();
  for (const item of items) {
    const id = requiredString(item?.id, `${label} id`);
    if (ids.has(id)) throw Error(`Duplicate ${label} id: ${id}`);
    ids.add(id);
  }
  return ids;
}
// External data can be parsed without evaluating caller-provided JavaScript objects.
export function parseNodeDeclaration(text) {
  if (typeof text !== 'string' || text.length > 262144) throw Error('Invalid declaration text');
  return copyData(JSON.parse(text));
}
export function createNodeRegistry(declarations, bindings, host) {
  if (!(host?.types instanceof Map) || typeof host.validateOutputs !== 'function')
    throw Error('Missing trusted host adapters');
  if (!Array.isArray(declarations) || declarations.length > 256)
    throw Error('Invalid declarations');
  const table = new Map();
  for (const input of declarations) {
    const declaration = copyData(input);
    const type = requiredString(declaration.typeId, 'type id');
    if (table.has(type)) throw Error(`Duplicate node type: ${type}`);
    if (!Number.isSafeInteger(declaration.revision) || declaration.revision < 1)
      throw Error('Invalid declaration revision');
    const implementationId = requiredString(declaration.implementationId, 'implementation id');
    if (!Array.isArray(declaration.capabilities) || declaration.capabilities.length)
      throw Error('Unsupported semantic capability');
    const parameterIds = uniqueIds(declaration.parameters, 'parameter');
    for (const parameter of declaration.parameters) {
      const typeId = requiredString(parameter.type, 'parameter type');
      const adapter = host.types.get(typeId);
      if (!adapter || typeof adapter.validateDefault !== 'function')
        throw Error(`Unsupported parameter type: ${typeId}`);
      if (adapter.validateDefault(parameter.default) !== true)
        throw Error(`Invalid parameter default: ${parameter.id}`);
    }
    uniqueIds(declaration.outputs, 'output');
    const operationIds =
      declaration.operations === undefined ? null : uniqueIds(declaration.operations, 'operation');
    if (
      operationIds
        ? !operationIds.has(declaration.defaultOperation)
        : declaration.defaultOperation !== undefined
    )
      throw Error('Invalid operation default');
    if (declaration.bypassInput !== undefined && !parameterIds.has(declaration.bypassInput))
      throw Error('Invalid bypass default');
    const dynamicResult = declaration.resultType === undefined;
    if (dynamicResult && !operationIds) throw Error('Invalid result type');
    if (
      dynamicResult &&
      (declaration.outputs.length !== 1 ||
        declaration.outputs[0].result !== true ||
        declaration.outputs[0].type !== undefined)
    )
      throw Error('Unsupported dynamic output');
    if (!dynamicResult && declaration.outputs.some((output) => output.result !== undefined))
      throw Error('Unsupported dynamic output');
    const interfaces = new Map();
    const variants = operationIds ? declaration.operations : [null];
    for (const operation of variants) {
      if (
        operation &&
        (!Array.isArray(operation.active) ||
          new Set(operation.active).size !== operation.active.length ||
          operation.active.some((id) => !parameterIds.has(id)))
      )
        throw Error('Invalid active input');
      if (!dynamicResult && operation?.resultType !== undefined)
        throw Error('Conflicting result type');
      const resultType = requiredString(
        dynamicResult ? operation.resultType : declaration.resultType,
        'result type',
      );
      if (!host.types.has(resultType)) throw Error(`Unsupported result type: ${resultType}`);
      const outputs = declaration.outputs.map((output) =>
        Object.freeze(output.result === true ? { ...output, type: resultType } : { ...output }),
      );
      for (const output of outputs) {
        const typeId = requiredString(output.type, 'output type');
        const adapter = host.types.get(typeId);
        if (
          !adapter ||
          typeof adapter.validateOutput !== 'function' ||
          adapter.validateOutput(output) !== true
        )
          throw Error(`Unsupported output type: ${typeId}`);
      }
      if (host.validateOutputs(outputs) !== true) throw Error('Unsupported output');
      interfaces.set(
        operation?.id,
        Object.freeze({ resultType, outputs: Object.freeze(outputs), active: operation?.active }),
      );
    }
    if (
      !declaration.catalog ||
      typeof declaration.catalog.categoryId !== 'string' ||
      typeof declaration.catalog.labelJa !== 'string' ||
      typeof declaration.catalog.description !== 'string' ||
      !Array.isArray(declaration.catalog.aliases) ||
      declaration.catalog.aliases.some((alias) => typeof alias !== 'string') ||
      declaration.catalog.creationRoute !== 'add'
    )
      throw Error('Invalid catalog metadata');
    const binding = bindings instanceof Map ? bindings.get(implementationId) : undefined;
    if (!binding || typeof binding.evaluate !== 'function')
      throw Error(`Unavailable implementation: ${implementationId}`);
    const evaluate = binding.evaluate;
    const project = binding.project;
    if (
      (project !== undefined && typeof project !== 'function') ||
      (declaration.outputs.length > 1 && typeof project !== 'function')
    )
      throw Error('Invalid projection');
    const outputIds = new Set(declaration.outputs.map(({ id }) => id));
    const resolved = Object.freeze({
      declaration,
      interface(operation) {
        if (operationIds && !operationIds.has(operation)) throw Error('Invalid operation');
        return interfaces.get(operationIds ? operation : undefined);
      },
      evaluate(operation, read) {
        if (operationIds && !operationIds.has(operation)) throw Error('Invalid operation');
        return evaluate(operation, read);
      },
      project(output, value) {
        if (!outputIds.has(output)) throw Error('Invalid output');
        return project ? project(output, value) : value;
      },
    });
    table.set(type, resolved);
  }
  return Object.freeze({
    get: (type) => table.get(type),
    types: () => Object.freeze([...table.keys()]),
  });
}
