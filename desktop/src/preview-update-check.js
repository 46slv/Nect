import { createManualUpdateCheck } from './manual-update-check.js';
import { mountManualUpdateCheck } from './manual-update-view.js';
import { renderDiscovery } from './build-info.js';

// Compatibility adapter only; never imported by the production entry graph.
export function createPreviewFixtureCheck({ element, document, readFixture, ...options }) {
  const controller = createManualUpdateCheck({ ...options, readSource: readFixture });
  return mountManualUpdateCheck({ element, document, controller, renderDiscovery });
}
