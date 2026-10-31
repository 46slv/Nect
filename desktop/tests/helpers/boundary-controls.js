// Open the same disclosures a person uses; do not synthesize clicks on hidden
// controls. Keyboard activation avoids releasing a pointer during drag tests.
const graphActions = new Set([
  'find-node',
  'graph-zoom-out',
  'graph-zoom-in',
  'graph-fit',
  'graph-fit-selection',
  'graph-reset',
  'graph-overview',
]);
const canvasActions = new Set([
  'zoom-out',
  'zoom-in',
  'fit',
  'actual-size',
  'snap',
  'align-panel',
  'grid-panel',
  'layout-panel',
  'guides-panel',
]);
export async function revealBoundary(page, side) {
  const label = side === 'graph' ? 'Toggle node graph controls' : 'Toggle artboard view controls';
  const toggle = page.getByRole('button', { name: label, exact: true });
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.press('Enter');
}
export async function activateBoundaryCommand(page, action) {
  if (graphActions.has(action)) await revealBoundary(page, 'graph');
  else if (canvasActions.has(action)) await revealBoundary(page, 'canvas');
  else {
    const label = ['new', 'open', 'save', 'save-as', 'add-composition', 'asset-library'].includes(
      action,
    )
      ? 'Project'
      : ['export', 'export-png'].includes(action)
        ? 'Export'
        : ['import-svg', 'import-image', 'add-svg-part', 'import-composition'].includes(action)
          ? 'Import'
          : null;
    if (label) {
      const menu = page
        .locator('header details.command-menu')
        .filter({ has: page.locator('summary', { hasText: new RegExp(`^${label}$`) }) });
      if ((await menu.getAttribute('open')) === null)
        await menu.locator(':scope > summary').press('Enter');
    }
  }
  const button = page.locator(`[data-action="${action}"]`);
  await button.click();
}
export async function revealNodePresets(page) {
  await revealBoundary(page, 'graph');
  const summary = page.locator('.boundary-popover > summary').filter({ hasText: /^Presets$/ });
  const details = summary.locator('..');
  if ((await details.getAttribute('open')) === null) await summary.press('Enter');
}
export async function revealNodeGroups(page) {
  await revealBoundary(page, 'graph');
  const details = page.locator('[data-node-groups]');
  if ((await details.getAttribute('open')) === null)
    await details.locator('summary').press('Enter');
}
