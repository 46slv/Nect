# Saved OS-font availability

A catalog refresh formerly kept the previous DirectWrite face list after a
discovery error. A previously available face could therefore show no warning
after the refresh failed. The legacy-family failure also prevented the modern
face query from running.

The two discovery queries now settle independently. Failed results carry an
error instead of an old list or a successful empty list. The inspector separates
Checking font, Font check failed / Refresh fonts, not checked, and Font unavailable
/ saved outlines retained. Modern references match all seven stored face metadata
fields (family, name, weight, style, stretch, format, color). Only the newest
catalog request updates the displayed inventory. The picker reuses this reader
and can retain successful results when the other discovery source fails.

This is discovery status, not a font or glyph guarantee. The existing native
exact-face/fallback guards still decide every outline preparation. Existing
normal-font asset validation, picker coverage preflight, and explicit Use DejaVu
Sans replacement remain unchanged. Refresh does not shape text, request font
bytes, substitute a face, edit authored data, or add an Undo entry. Saved outlines
remain the display/export authority when the source font is absent.

Evidence: `tests/font-availability.e2e.js` reproduces the pre-fix empty status after
a known face's catalog rejects, then verifies failure/absence distinction,
metadata mismatch, independent queries, recovery, and exact JSON/SVG/revision
preservation in the production bundle/CSP. Its IPC failure is explicitly stubbed.
`tests/os-font-catalog.test.js` plus the existing OS Text/outline/desktop Text tests
pass 15 checks. The old DOM fixture's storage mock was brought up to date with
inert recovery functions; the production recovery behavior is unchanged.

No schema, core editing, native font, storage, shaping, or fallback-chain change.
Cluster-level editing and broader OpenType selection remain separate work.
The bounded local evidence settles this status fix; review gate not-required,
external submissions 0. No universal font-support claim is made.
