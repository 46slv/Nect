# Canvas wrap width: Y Center/Bottom references

Base: verified PR55 `codex/legacy-numeric-wrap-tests`, remote
`10daa5687b33aa683c5e9e22a5425fa694d20bdd` in `46slv/Nect`.
Current helper rejects Y Center/Bottom; no equivalent implementation exists.

The numeric width command/reflow remains canonical. `text-layout.js` anchors the
visible pre-character-adjustment outline block to Position.y at Top/Center/Bottom,
then applies individual character offsets/transforms and maintained arrangement.
Its relative lineOffsets are unchanged by that anchoring. Do not translate the
glyphs or reuse an already shifted glyph baseline for another pixel correction.

Keep the existing logical metric frame height: source-wide ascent+descent plus
derived relative line offsets, at least the font size. This is the width-budget
guide, not a visible-ink/stroke containment promise. First-baseline behavior is
unchanged. For Y Top/Center/Bottom, frameTop = Position.y - height \* 0/0.5/1.
Thus reflow height changes move the top by zero/half/all of the delta while the
specified reference stays fixed. Width handles remain at the midpoint of the
existing X side: right for X Left/Center, left for X Right. Apply the supported
source-to-artwork affine after computing this node-local frame. No added glyph,
Position or baseline shift; only textWrap.width is authored.

Capture/freshness-check Y reference with existing session/revision/node/X/selection
identity. Keep RAF preview, numeric bounds/no flip, one drag/one Undo, exact
Escape/pointercancel/blur restoration, locks and stale-target rejection. Existing
positive axis-aligned transforms remain supported; other transforms/routes are
hidden. No core/schema/layout rewrite, fixed height, vertical or general resize.
Preserve the PR55 numeric Escape repair and earlier sealed evidence.

Verification budget: small model geometry/numeric-equivalence matrix across all
nine X/Y reference combinations; DOM lifecycle at four boundary combinations;
native synthetic at two representative combinations and two zooms. Compare
numeric JSON/SVG, stable characters/Position/selection, height/reference geometry,
Undo/Redo and cancellation. Rich style/manual-leading and post-character offset
cases distinguish logical frame anchoring from changing ink bounds. No duplicate
nine-case native matrix or unrelated full suites. Physical input/actual OS focus
loss/file dialogs/cold reopen/IME remain separately unverified.
