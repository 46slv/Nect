export function ensureCommandMenus(app, document) {
  const header = app.querySelector('header');
  if (!header || header.querySelector('.command-menu')) return;
  const makeMenu = (label, actions) => {
    const menu = document.createElement('details');
    menu.className = 'command-menu';
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
  const scale = header.querySelector('#png-scale');
  if (scale) {
    const label = document.createElement('label');
    label.className = 'command-menu-scale';
    label.append('PNG scale', scale);
    exportMenu.querySelector('.command-menu-items').append(label);
  }
  brand?.after(project, importMenu, exportMenu);
  header.querySelectorAll('[data-recovery-status], [data-recovery-open]').forEach((button) => {
    project.querySelector('.command-menu-items').append(button);
  });
}
