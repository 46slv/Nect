import { revealBoundary } from './helpers/boundary-controls.js';
// Browser harness commands must use the visible Project/Import/Export surfaces, not
// programmatic clicks on buttons hidden in closed <details>. Keyboard activation
// deliberately avoids releasing a pressed canvas pointer during interruption tests.
export async function revealCommandMenu(page, label, { keyboard = false } = {}) {
  if (label === 'Output settings') {
    const outer = await revealCommandMenu(page, 'Export', { keyboard });
    const menu = outer.locator('[data-output-settings]');
    if ((await menu.getAttribute('open')) === null) {
      const summary = menu.locator(':scope > summary');
      if (keyboard) await summary.press('Space');
      else await summary.click();
    }
    return menu;
  }
  const menu = page.locator('header details.command-menu').filter({
    has: page.locator('summary', { hasText: new RegExp(`^${label}$`) }),
  });
  if ((await menu.getAttribute('open')) === null) {
    const summary = menu.locator(':scope > summary');
    // Keep Space as this helper's native toggle route. The Pen regression
    // separately exercises summary Enter without releasing the canvas pointer.
    if (keyboard) await summary.press('Space');
    else await summary.click();
  }
  return menu;
}

export async function activateCommand(page, action, { keyboard = false } = {}) {
  if (
    [
      'find-node',
      'graph-zoom-out',
      'graph-zoom-in',
      'graph-fit',
      'graph-fit-selection',
      'graph-reset',
      'graph-overview',
    ].includes(action)
  )
    await revealBoundary(page, 'graph');
  if (
    [
      'zoom-out',
      'zoom-in',
      'fit',
      'actual-size',
      'snap',
      'align-panel',
      'grid-panel',
      'layout-panel',
      'guides-panel',
    ].includes(action)
  )
    await revealBoundary(page, 'canvas');
  const label = ['new', 'open', 'save', 'save-as', 'add-composition', 'asset-library'].includes(
    action,
  )
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
