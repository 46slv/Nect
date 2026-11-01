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

## Normal Font picker discovery status

The normal **Font** picker also reports native face discovery independently of
installed-file rows and font preparation: **Checking native fonts…**, **Native
font discovery failed**, **No native font faces found**, or the returned native
face count. **Refresh fonts** uses the existing metadata-only refresh path and
is disabled while that request is pending. Search, row selection and preparation
messages do not replace this discovery status. It is shown on the desktop only.

A failed/empty native catalog does not disable installed-file rows or their
existing normal Apply route. No names are mapped between the separate catalogs;
face IDs, route eligibility, defaults and save behavior are unchanged. Opening,
searching and refreshing the picker read no font bytes. Applying an installed
row retains the existing explicit import/validation behavior and one-edit guard.

`tests/os-font-catalog.test.js` covers the distinct discovery results.
`tests/unified-font-picker-dom.test.js` covers rejected discovery, installed Apply
and duplicate-Apply suppression, successful empty results, pending Refresh and
recovery without reopening or document/history edits. The existing unified
picker Chrome scenario additionally checks the discovery line, retained
selection and Apply eligibility across failure/empty/pending/recovery, with
screenshots and `workflow-unified-font-picker.json`. Native catalog and geometry
responses in these UI tests are synthetic; they do not prove real Windows font
availability, aliases, installed/native catalog equivalence, or font fidelity.
