// View-only ordering. Move existing controls; keep their handlers and identity.
const sections = new Map();
function disclosure(document, key, title) {
  const details = document.createElement('details');
  details.className = 'text-inspector-details';
  details.dataset.textInspectorSection = key;
  const summary = document.createElement('summary');
  summary.textContent = title;
  details.append(summary);
  details.open = sections.get(key) || false;
  // The global click handler can render while dismissing transient panels.
  // Store the requested state before that render replaces this summary.
  summary.addEventListener('click', (event) => {
    event.preventDefault();
    details.open = !details.open;
    sections.set(key, details.open);
  });
  details.addEventListener('toggle', () => sections.set(key, details.open));
  return details;
}
export function arrangeTextInspector(root, editingCharacters = false) {
  const controls = root.querySelector('.text-controls');
  if (!controls?.querySelector('[data-text]')) return;
  root.classList.add('text-inspector');
  const doc = root.ownerDocument;
  const field = (selector) => controls.querySelector(selector)?.closest('.field');
  const primary = [
    field('[data-text]'),
    controls.querySelector('[data-style-text-range]'),
    controls.querySelector('[data-kern-text-cursor]'),
    field('[data-font]'),
    ...controls.querySelectorAll(':scope > [role="status"]'),
    controls.querySelector('[data-load-font]'),
    root.querySelector('.os-font-outlines [data-os-text-font]'),
    root.querySelector('.field[data-key="size"]'),
    field('[data-writing-mode]'),
    field('[data-text-alignment]'),
    root.querySelector('.field[data-key="letterSpacing"]'),
    root.querySelector('.field[data-key="lineSpacing"]'),
    controls.querySelector('.text-wrap-controls'),
    controls.querySelector('.text-leading-controls'),
  ].filter(Boolean);
  const advanced = disclosure(doc, 'typography', 'More typography');
  for (const child of [...controls.children]) {
    if (!primary.includes(child)) advanced.append(child);
  }
  for (const child of primary) controls.append(child);
  const os = root.querySelector('.os-font-outlines');
  if (os) advanced.append(os);
  controls.append(advanced);
  root.prepend(controls);
  const characters = root.querySelector('.character-panel');
  if (characters) {
    controls.after(characters);
    if (editingCharacters) {
      const typography = characters.querySelector('.character-typography');
      if (typography) characters.querySelector('.character-actions')?.after(typography);
      root.prepend(characters);
    }
  }
  const output = disclosure(doc, 'output', 'Node and output');
  for (const child of root.querySelectorAll(':scope > .viewer-assignment')) output.append(child);
  if (output.children.length > 1) root.append(output);
}
