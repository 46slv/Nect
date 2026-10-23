# Numeric Escape and wrap schema test maintenance

Base: verified PR54 remote `a6fec92666fa98c6af4674c00d41614aa309beaf`,
`codex/text-wrap-reference-resize` in `46slv/Nect`. Only the two inherited test
failures and directly adjacent contracts are in scope; no Y-reference extension.

`numeric-scrub-dom.test.js:119` is an actual product cancellation bug. A scratch
trace shows connected controls and committed X460/revision0 while preview X480
is active. Escape reaches the earlier numeric-expression handler, restores the
display to460 and returns without cancelling the Session gesture. Late pointerup
then commits X480/revision1; the next drag previews500 instead of480. This is not
a stale input or mock rounding discrepancy. `numeric-scrubbing.md` requires
Escape to cancel the complete gesture and late events to remain neutral.

Minimal product repair: classify an input as a keyboard-expression input only
when no started numeric scrub owns it. Active scrubs then reach the existing
Escape cancellation/key-blocking branches. Ordinary typed arithmetic and its
Escape restoration remain unchanged. Keep all assertions, and add immediate
gesture release, late-pointerup JSON/file/history neutrality and operator-key
ownership checks. A focused native before/after scenario checks the same focused
number-input event route; no broader native audit is required.

`text-wrap-dom.test.js:82` assumes the old minimum8 is the factory's current
schema. Current `document()` starts at24; `set-text-settings` promotes via
max(existing,8) for width-only wrap and max(existing,34) for basic wrap. Loading
does not globally upgrade old versions. Assert preservation of the current
document version separately from a real schema7 fixture promoted to8. Verify
persisted JSON/reload, reject a wrapped document incorrectly tagged7, and exact
Undo/Redo across migration. The same old test also assumes vertical wrapping is
rejected; current writing-mode UI explicitly converts to basic vertical height
and schema34. Check that adopted transition and Undo back to width-only instead
of deleting the assertion or restoring obsolete product behavior.

The numeric suite also reaches its previously blocked local-alignment preference
case. Open the existing Align panel through its toolbar toggle before querying
the field; do not assume it starts open or skip the locked-document assertion.
The new typed-arithmetic neighbor waits for the scheduled Undo button render
before clicking it, and asserts that one Undo actually restores the model.

Run these two suites and focused numeric/character/typed-expression neighbors.
Because a product keyboard guard changes, build and run one native synthetic
Escape case against the sealed base and patched candidate; preserve all earlier
outputs. No main merge, release, Library upload, history rewrite or forced
process termination. Physical input/OS dialogs/IME remain unverified.
