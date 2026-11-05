const manifest = new URL('../src/builtin-node-manifest.js', import.meta.url).href;
const declaration = new URL('./dev-affine-declaration.js', import.meta.url).href;
const binding = new URL('./dev-affine-binding.js', import.meta.url).href;
export async function load(url, context, nextLoad) {
  const loaded = await nextLoad(url, context);
  if (url !== manifest) return loaded;
  const source = String(loaded.source),
    marker = '\n]);',
    at = source.lastIndexOf(marker);
  if (at < 0 || source.indexOf(marker) !== at) throw Error('Unexpected manifest structure');
  const header = `import { fixtureDeclaration } from '${declaration}';\nimport { fixtureEvaluate } from '${binding}';\n`;
  const row =
    "\n  entry(fixtureDeclaration, { evaluate: fixtureEvaluate }, null, null, 'private'),";
  return {
    format: loaded.format,
    source: header + source.slice(0, at) + row + source.slice(at),
    shortCircuit: true,
  };
}
