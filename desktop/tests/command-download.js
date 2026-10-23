import { activateCommand } from './command-menu-actions.js';

// Observe both promises immediately: a blocked command must not leave a download
// timeout unhandled while Playwright is still waiting for its click to complete.
export async function downloadCommand(page, action) {
  const [download] = await Promise.all([
    page.waitForEvent('download'),
    activateCommand(page, action),
  ]);
  return download;
}
