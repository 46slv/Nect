# HSL-first standard color picker

Decision: user request, 2026-10-31. All standard editable Color parameters and saved Shared Color literals show H/S/L first: hue 0–360°, saturation 0–100%, lightness 0–100%. Alpha remains a separate 0–1 input. An existing native color swatch remains available as an alternative, not the primary numeric editing surface.

The document model remains validated sRGB `#RRGGBB` plus alpha; editing HSL converts back to the existing hex value through a UI-only adapter. This avoids migration, changing Color expression semantics, altering SVG interchange, or confusing HSL with the OKLCH/Auto Chroma harmony system. HSL is an input representation, not a new palette-generation algorithm.

Requirements:

- Visible H, S and L controls on local and bound Base Color parameters as well as saved shared literals. Bound controls preserve the current disable/base rules.
- Changing one coordinate preserves the other converted coordinates to practical 8-bit sRGB precision, retains existing alpha, and uses the same typed Session command/Undo path.
- Invalid or incomplete values never dispatch authored color. Existing HEX/native swatch remains available for precise entry. Keep stable Shared Color IDs and revision envelopes.
- Browser/DOM tests establish dispatch and conversion; native Windows/WebView2 physical acceptance is separate.

No changes to harmony wheel roles, gamut constraints, automatic chroma, persistence schema or RGB-based rendering math are intended.
