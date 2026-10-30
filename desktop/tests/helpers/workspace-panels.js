// Tests use the same visible workspace controls as a person; a hidden panel is
// never made actionable by mutating DOM visibility.
export async function ensureWorkspacePanel(page, id) {
  const button = page.locator(`[data-workspace-panel="${id}"]`);
  if ((await button.getAttribute('aria-expanded')) !== 'true') await button.click();
  await page.locator(`#workspace-drawer-${['assets', 'colors'].includes(id) ? 'left' : 'right'}`).waitFor({ state: 'visible' });
}
