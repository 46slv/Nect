// Fixed independent reference for the 64x48 opaque-center acceptance fixture.
// No production sampler, renderer, geometry, provenance or coverage helper imports.
// RGB encodes the first three stop masses; alpha minus RGB is the fourth.
export function orderedAngularReferenceSVG() {
  const triangles = [];
  const radius = Math.hypot(28.5, 20.5) * 1.0001;
  for (let wedge = 0; wedge < 720; wedge++) {
    const turn = (wedge + 0.5) / 720;
    let red = 0,
      green = 0,
      blue = 0;
    if (turn < 0.17) {
      green = turn / 0.17;
      red = 1 - green;
    } else if (turn < 0.73) {
      blue = (turn - 0.17) / 0.56;
      green = 1 - blue;
    } else blue = (1 - turn) / 0.27;
    const color =
      '#' +
      [red, green, blue]
        .map((v) =>
          Math.round(v * 255)
            .toString(16)
            .padStart(2, '0'),
        )
        .join('');
    const a = (wedge * 0.5 * Math.PI) / 180,
      b = ((wedge + 1) * 0.5 * Math.PI) / 180;
    triangles.push(
      `<path d="M32.5 24.5L${32.5 + radius * Math.cos(a)} ${24.5 + radius * Math.sin(a)}L${32.5 + radius * Math.cos(b)} ${24.5 + radius * Math.sin(b)}Z" fill="${color}"/>`,
    );
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" width="64" height="48" viewBox="0 0 64 48" overflow="hidden"><defs><pattern id="independent-angular-reference" patternUnits="userSpaceOnUse" x="4" y="4" width="56" height="40" viewBox="4 4 56 40" preserveAspectRatio="none"><g shape-rendering="crispEdges">${triangles.join('')}</g></pattern></defs><rect x="4" y="4" width="56" height="40" fill="url(#independent-angular-reference)"/></svg>`;
}

// The ideal-angle formula is undefined/ambiguous on this positive shared ray.
// Retain the same strict analytic checks everywhere else, and use the ordered
// primitive reference for this ray, the center, and whole-fixture totals.
export const isAngularReferenceInterior = (x, y) => !(y === 24 && x >= 32 && x < 60);
