// Research comparison only: these are not product defaults or creation plans.
const rows = (assort, accent) => [
  { id: 'assort', role: 'Assort', position: 1, ...assort, chromaOffset: 0, alpha: 1 },
  { id: 'accent', role: 'Accent', position: 2, ...accent, chromaOffset: 0, alpha: 1 },
  {
    id: 'neutral',
    role: 'Neutral',
    position: 0,
    lightnessScale: 0,
    lightnessOffset: 0.94,
    chromaScale: 0,
    chromaOffset: 0,
    alpha: 1,
  },
  {
    id: 'text',
    role: 'Text',
    position: 0,
    lightnessScale: 0,
    lightnessOffset: 0.14,
    chromaScale: 0,
    chromaOffset: 0,
    alpha: 1,
  },
];

export const researchPlans = [
  {
    id: 'research-triadic-offset',
    version: 1,
    harmony: 'triadic',
    hueSteps: 12,
    slots: rows(
      { lightnessScale: 1, lightnessOffset: -0.07, chromaScale: 0.7 },
      { lightnessScale: 1, lightnessOffset: -0.16, chromaScale: 0.9 },
    ),
  },
  {
    id: 'research-neutral-start',
    version: 1,
    harmony: 'triadic',
    hueSteps: 12,
    slots: rows(
      { lightnessScale: 0.6, lightnessOffset: 0.2, chromaScale: 0 },
      { lightnessScale: 0.4, lightnessOffset: 0.45, chromaScale: 0 },
    ),
  },
];
