# Keyer input texel picking

New parent-selected production slice from PR68
`7797e6695ce06e69973c15d1444514d495eb6d0d`. Existing Keyer/Mask evaluator and
export stay intact. Add Pick from input to the selected Key color field.

Design contract: a compact temporary input-pixel preview; primary action is one
pixel click; hover/outside/transparent/error/cancel/stale states; existing Canvas
and Viewer remain untouched behind it. Reuse editor controls, no new panels.

Resolve the selected Keyer's `path` source before this Keyer via existing
evaluate/imageAssetFor/copyImageRgba. Earlier Keyers/Dither remain applied.
Preview intrinsic image pixels in an SVG viewBox, using its screen CTM inverse
to map pointer coordinates. Floor X/Y, half-open [0,width) x [0,height), no edge
clamp. Reject alpha0; partial alpha retains straight RGB, never checkerboard or
composited screen color. This preview supports Image position/nonuniform size
without using artwork coordinates; Canvas pan/zoom/rotation do not enter it.
Nonsingular 2D preview transforms work; unavailable/singular/3D CTMs cannot pick.
PaintedPath/Composition/unsupported Image routes remain subject to existing typing.

One click uses existing set-parameter/set-parameter-base Color editing, retaining
stored alpha and expressions/connections as that editor does. Driven fields say
Pick base from input. Locks and Session/revision/composition/source/selection
identity guard commit. Image replacement, target change or stale input discards
the preview. Start/hover/invalid click/Escape/cancel/blur have no authored effect;
close restores the previous working display. Block other pointer starts while
preview is active. One changed click is one Undo; repeated color is a no-op.

Focused model/DOM: expected RGB and upstream effects, transparent/outside,
floor boundaries and preview inverse transform, cancellation/stale/reload/locks,
normal Color/base behavior, Undo/Redo/no-op and unchanged assets/other parameters.
One native CDP synthetic flow: pick a partial-alpha input texel, see keyed alpha
change, JSON/SVG/PNG consistency. D target/runtime/output/TEMP/TMP; preserve older
candidates. No new evaluator, feather, luminance mask, OS eyedropper, external
images/services, main merge/release or Library upload. Physical/OS input and
native Save/Open remain separate unverified routes.
