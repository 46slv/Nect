// Browser harness commands must use the visible Project/Import/Export surfaces, not
// programmatic clicks on buttons hidden in closed <details>. Keyboard activation
// deliberately avoids releasing a pressed canvas pointer during interruption tests.
export async function revealCommandMenu(page, label, { keyboard = false } = {}) {
  const menu = page.locator('header details.command-menu').filter({
    has: page.locator('summary', { hasText: new RegExp(`^${label}$`) }),
  });
  if ((await menu.getAttribute('open')) === null) {
    const summary = menu.locator('summary');
    // Keep Space as this helper's native toggle route. The Pen regression
    // separately exercises summary Enter without releasing the canvas pointer.
    if (keyboard) await summary.press('Space');
    else await summary.click();
  }
  return menu;
}

export async function activateCommand(page, action, { keyboard = false } = {}) {
  const label = ['new', 'open', 'save', 'save-as'].includes(action)
    ? 'Project'
    : ['export', 'export-png'].includes(action)
      ? 'Export'
      : ['import-svg', 'import-image', 'add-svg-part', 'import-composition'].includes(action)
        ? 'Import'
        : null;
  let button = page.locator(`[data-action="${action}"]`);
  if (label) {
    const menu = await revealCommandMenu(page, label, { keyboard });
    button = menu.locator(`[data-action="${action}"]`);
  }
  if (keyboard) await button.press('Enter');
  else await button.click();
}
