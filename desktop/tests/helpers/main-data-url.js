const mainUrl = new URL('../../src/main.js', import.meta.url);

export function resolveMainRelativeImports(source) {
  return source.replace(
    /(\bfrom\s*|\bimport\s*\(\s*)(['"])(\.\/[^'"]+)\2/g,
    (_, prefix, quote, specifier) => `${prefix}${quote}${new URL(specifier, mainUrl).href}${quote}`,
  );
}
