// Portable XML syntax data. The browser parser supplies tags/attributes/text;
// the Session independently validates the supported SVG subset and authors IDs.
const limits = { bytes: 10000000, elements: 4096, depth: 64 };
export function readSVGSource(text, Parser = globalThis.DOMParser) {
  if (typeof text !== 'string') throw Error('Invalid SVG document');
  if (text.length > limits.bytes || new TextEncoder().encode(text).length > limits.bytes)
    throw Error('SVG exceeds 10 MB');
  if (/<!DOCTYPE|<!ENTITY|<\?(?!xml(?:\s|\?>))/i.test(text))
    throw Error('SVG declarations and external styles are unsupported');
  const xml = new Parser().parseFromString(text, 'image/svg+xml');
  if (!xml.documentElement || xml.querySelector('parsererror')) throw Error('Invalid SVG document');
  let count = 0;
  const lower = (element, depth) => {
    if (++count > limits.elements) throw Error('SVG exceeds element budget');
    if (depth > limits.depth) throw Error('SVG exceeds nesting depth budget');
    return {
      tag: element.tagName,
      namespace: element.namespaceURI,
      attributes: [...element.attributes].map((a) => [a.name, a.value]),
      children: [...element.childNodes].flatMap((child) =>
        child.nodeType === 1
          ? [lower(child, depth + 1)]
          : child.nodeType === 3 || child.nodeType === 4
            ? [child.textContent]
            : [],
      ),
    };
  };
  return lower(xml.documentElement, 1);
}

// This small reader supports only the queries used by the existing SVG subset
// validator. It has no DOM, fetching, script execution or caller callbacks.
export function svgTreeRoot(source) {
  let count = 0,
    bytes = 0;
  const account = (value) => {
    if (typeof value !== 'string') throw Error('Invalid SVG syntax value');
    bytes += new TextEncoder().encode(value).length;
    if (bytes > limits.bytes) throw Error('SVG exceeds 10 MB');
    return value;
  };
  const build = (item, parentElement, depth) => {
    if (++count > limits.elements) throw Error('SVG exceeds element budget');
    if (depth > limits.depth) throw Error('SVG exceeds nesting depth budget');
    if (
      !item ||
      typeof item !== 'object' ||
      Array.isArray(item) ||
      Object.keys(item).some(
        (key) => !['tag', 'namespace', 'attributes', 'children'].includes(key),
      ) ||
      !Array.isArray(item.attributes) ||
      !Array.isArray(item.children)
    )
      throw Error('Invalid SVG syntax tree');
    const tag = account(item.tag);
    if (!/^[A-Za-z_][\w.:-]*$/.test(tag)) throw Error('Invalid SVG tag');
    const attributes = [],
      names = new Set();
    for (const pair of item.attributes) {
      if (!Array.isArray(pair) || pair.length !== 2) throw Error('Invalid SVG attribute');
      const name = account(pair[0]),
        value = account(pair[1]);
      if (!/^[A-Za-z_][\w.:-]*$/.test(name) || names.has(name))
        throw Error('Invalid or duplicate SVG attribute');
      names.add(name);
      attributes.push({ name, value });
    }
    const namespaceURI = item.namespace === null ? null : account(item.namespace);
    const element = {
      nodeType: 1,
      localName: tag.split(':').at(-1),
      prefix: tag.includes(':') ? tag.split(':')[0] : null,
      namespaceURI,
      attributes,
      parentElement,
      getAttribute(name) {
        return attributes.find((a) => a.name === name)?.value ?? null;
      },
      hasAttribute(name) {
        return names.has(name);
      },
      querySelectorAll(selector) {
        const selected = new Set(selector.split(',')),
          result = [];
        const visit = (element) => {
          for (const child of element.children) {
            if (selected.has('*') || selected.has(child.localName)) result.push(child);
            visit(child);
          }
        };
        visit(this);
        return result;
      },
      querySelector(selector) {
        return this.querySelectorAll(selector)[0] ?? null;
      },
      closest(selector) {
        if (selector !== 'g[data-layer]') throw Error('Unsupported SVG syntax query');
        for (let current = this; current; current = current.parentElement)
          if (current.localName === 'g' && current.hasAttribute('data-layer')) return current;
        return null;
      },
      get textContent() {
        return this.childNodes.map((child) => child.textContent).join('');
      },
    };
    element.childNodes = item.children.map((child) =>
      typeof child === 'string'
        ? { nodeType: 3, textContent: account(child) }
        : build(child, element, depth + 1),
    );
    element.children = element.childNodes.filter((child) => child.nodeType === 1);
    return element;
  };
  return build(source, null, 1);
}
