# Text wrap: Canvas right-edge width handle

Base: PR52 `codex/graph-cancel-render` at verified remote
`8351a98cc7c00d666b0a5329f40c9b8988add6f9`,
`https://github.com/46slv/Nect.git`. Single main/core writer. Preserve prior
PR40–52 candidates/receipts. No main merge, release, force push or denied paths.

Source: `text-wrap.md`, current `text-wrap.js`, main's numeric wrap controls,
`text-values` / `wrap` Session gesture and existing Canvas overlay/pointer routes.
The old brief excludes Canvas handles; current source also supports vertical
basic wrapping. This new slice adds only horizontal right-edge adjustment and
keeps the existing numeric and vertical features unchanged.

First outcome: one screen-sized right handle on a logical wrap-width frame.
Width is the existing advance budget, not ink/stroke containment. Keep authored
Text position and the logical frame's left/top fixed; height derives from reflow.
Use source-wide font ascent for baseline-origin text so frame top does not follow
the changing soft lines. Start/top reference uses its existing fixed position.
Long indivisible spans keep overflowing and retain existing warning/export rules.

Supported initial scope: one graph-selected regular Text source (existing
pre-downstream source overlay) or one Canvas artwork with Text → optional static
axis-aligned positive scale/translation Transform → solid Fill → Output.
Horizontal start alignment with origin/start cross reference only. Hide for
rotation/skew/reflection/collapsed transforms, locked/hidden/bypassed nodes/routes,
non-wrap/vertical/attached Text, OS/variable fonts, driven non-path inputs,
multiple selection, character/in-place/anchor/Pen editing and unsupported routes.
Other references/layouts keep existing numeric controls. No bake/rebind or model
rewrite. A disabled Canvas route never changes selection or authored data.

Map the pointer through Canvas SVG CTM inverse (zoom/pan) and the existing affine
inverse into source-local width. Reuse numeric range0.01–100000 and preserve wrap
policy. Reuse the existing Session text-values/wrap gesture and final commit;
one drag is one Undo, click/no change no-op. Escape/pointercancel/blur discard
the preview, preserving source text/character IDs/position/selection and exact
committed JSON/SVG/dirty/history. Changed committed wrap/SVG is intentional.

Coalesce both preview updates and Canvas drawing on RAF; update only the target
width controls. Do not rebuild or evaluate unrelated Graph panels on each move.
Flush the final pending pointer before commit; cancel pending callbacks and reject
stale session/revision/selection/target atomically. Dedicated hit targets take
priority over marquee/artwork movement; viewport pan/other tools keep precedence.

Focused model/geometry and actual-main DOM checks cover guards, transformed width,
coalescing, bounds, source/ID stability, cancel/Undo/Redo, pending release and stale
targets. Build plus actual native renderer at two or more Canvas zooms verifies
drag/cancel/Undo/export. Physical input/OS dialogs/IME are separate unverified
coverage; no broad unrelated audit or panel redesign.
