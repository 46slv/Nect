export function ensureCommandMenus(app, document) {
  const header = app.querySelector('header');
  if (!header || header.querySelector('.command-menu')) return;
  const makeMenu = (label, actions) => {
    const menu = document.createElement('details');
    menu.className = 'command-menu';
    menu.addEventListener('toggle', () => {
      if (!menu.open) return;
      header.querySelectorAll('.command-menu[open]').forEach((other) => {
        if (other !== menu) other.open = false;
      });
    });
    const summary = document.createElement('summary');
    summary.textContent = label;
    const items = document.createElement('div');
    items.className = 'command-menu-items';
    for (const action of actions) {
      const button = header.querySelector(`[data-action="${action}"]`);
      if (button) items.append(button);
    }
    menu.append(summary, items);
    return menu;
  };
  const brand = header.querySelector('.brand');
  const project = makeMenu('Project', ['new', 'open', 'save', 'save-as']);
  const importMenu = makeMenu('Import', ['import-svg', 'import-image', 'add-svg-part']);
  const composition = document.createElement('button');
  composition.type = 'button';
  composition.dataset.action = 'import-composition';
  composition.textContent = 'Project composition';
  composition.title = 'Add a Composition from another project';
  importMenu.querySelector('.command-menu-items').append(composition);
  const exportMenu = makeMenu('Export', ['export', 'export-png']);
  const outputSettings = makeMenu('Output settings', []);
  outputSettings.dataset.outputSettings = '';
  const outputItems = outputSettings.querySelector('.command-menu-items');
  outputItems.classList.add('output-settings-items');
  for (const [label, selector] of [
    ['SVG sizing', '#svg-export-sizing'],
    ['SVG units', '#svg-export-unit'],
    ['SVG precision', '#svg-export-precision'],
    ['PNG size mode', '#png-export-mode'],
    ['PNG scale', '#png-scale'],
  ]) {
    const control = header.querySelector(selector);
    if (!control) continue;
    const wrapper = document.createElement('label');
    wrapper.className = 'header-setting';
    wrapper.append(label, control);
    outputItems.append(wrapper);
  }
  for (const element of [
    header.querySelector('#png-ppi')?.closest('label'),
    header.querySelector('[data-png-dimensions]'),
    header.querySelector('#export-presets'),
  ]) if (element) outputItems.append(element);
  brand?.after(project, importMenu, exportMenu, outputSettings);
  header.querySelectorAll('[data-recovery-status], [data-recovery-open]').forEach((button) => {
    project.querySelector('.command-menu-items').append(button);
  });
  // Preserve the real controls and focus order while making the major jobs
  // legible as groups. No commands are duplicated or deferred by these wrappers.
  const group = (label, nodes) => {
    nodes = nodes.filter((node) => node?.parentElement === header);
    if (!nodes.length) return;
    const container = document.createElement('div');
    container.className = 'header-group';
    container.setAttribute('role', 'group');
    container.setAttribute('aria-label', label);
    nodes[0].before(container);
    nodes.forEach((node) => container.append(node));
  };
  group('File', [project, importMenu, exportMenu, outputSettings]);
  group('Composition and assets', [
    header.querySelector('[data-active-composition]')?.closest('label'),
    header.querySelector('[data-action="add-composition"]'),
    header.querySelector('[data-action="asset-library"]'),
  ]);
  group('History', [
    header.querySelector('[data-action="undo"]'),
    header.querySelector('[data-action="redo"]'),
  ]);
}
