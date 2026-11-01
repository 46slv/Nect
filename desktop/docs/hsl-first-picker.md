# HSL-first Color swatch picker

Decision clarified by the user on 2026-11-01: the standard **popup opened by clicking a Color node's colored swatch** must prioritize HSL. The prior interpretation (three always-visible H/S/L numeric controls inside nodes and saved Shared Colors) was incorrect and has been reverted.

## Interaction

- Click the colored swatch of a local editable Color parameter (including Fill, Stroke, Color, gradient stops and Text/Keyer Color where supported). Open a compact anchored Nect-owned HSL picker, not the WebView/OS-native `input[type=color]` dialog.
- Primary controls in that popup are a 2D Saturation/Lightness surface at fixed Hue, a Hue slider, and explicit H 0–360°, S 0–100%, L 0–100% numeric inputs. HEX is an available secondary precision/interchange input.
- Current color is shown by the swatch/preview. Opacity remains a separate Color Alpha input and is not modified by a hue/saturation/lightness edit.
- A finished numeric/slider edit or plane pointer gesture authors one typed Session edit and one Undo step; an unfinished drag or Escape never authors the pending value. Invalid values and stale revisions must not mutate the document.
- Connected/expressed parameters retain their existing editable Base semantics. Locked/Global Color-bound parameters remain disabled. Color slot identity, artwork references and palette recipes are unchanged.
- The popup lives outside the rerendered Graph/Inspector DOM; it should not leak tool shortcuts, steal graph drags or detach while editing.

## Boundaries

This change modifies the **picker UI**, not the stored Color model: the sRGB six-digit HEX plus Alpha contract remains. Neither the Color Harmony wheel nor the OKLCH/Auto Chroma palette engine is replaced by HSL. Shared Color inline editing stays as previously implemented.

Evidence: [Draft PR #133](https://github.com/46slv/Nect/pull/133), separate stacked work on #130. Exact CI and Windows/WebView2 acceptance must be checked independently; a passing browser simulation is not native GUI acceptance.
