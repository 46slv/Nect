// Enter a visible view through its real control; never clear hidden/inert or
// dispatch into a closed panel. Assertions here catch obsolete test setup early.
export function revealDOMBoundary(document, side) {
  const name = side === 'graph' ? 'Toggle node graph controls' : 'Toggle artboard view controls';
  const selector = `[aria-label="${name}"]`;
  const toggle = document.querySelector(selector);
  if (!toggle) throw Error(`Missing boundary control: ${name}`);
  if (toggle.getAttribute('aria-expanded') !== 'true') toggle.click();
  if (document.querySelector(selector)?.getAttribute('aria-expanded') !== 'true')
    throw Error(`Boundary control did not open: ${name}`);
}
export function openDOMPanel(document, id) {
  const selector = `[data-workspace-panel="${id}"]`;
  const toggle = document.querySelector(selector);
  if (!toggle) throw Error(`Missing workspace panel: ${id}`);
  if (toggle.getAttribute('aria-expanded') !== 'true') toggle.click();
  const current = document.querySelector(selector);
  const host = document.getElementById(current?.getAttribute('aria-controls'));
  if (current?.getAttribute('aria-expanded') !== 'true' || !host || host.hidden || host.inert)
    throw Error(`Workspace panel did not become interactive: ${id}`);
}
