# Windows staged trial: 2026-10-18

Base: `3056103ea2168f595441cb7a375f2493c8122664` on `codex/desktop-production-20261016`.
Built on SS-Note-ROG with task-local Rust 1.99.1, MSVC and the committed lockfiles.

Canvas point selection now owns Delete and Backspace before artwork/node deletion.
The existing `delete-point` Session command accepts a batch of stable point IDs,
validates all targets before mutation, preserves Multi Path contour references and
commits one Undo entry. Locked point components and compositions reject deletion.
Generated/Remap point selection refuses deletion without falling through to node
deletion. Input/IME guards remain in force; held delete keys cannot cascade after
selection is pruned.

Verification: 35 focused core/DOM/font checks passed. Final expanded point/outline
DOM checks passed 24/24, including generated Remap and post-delete key repeat.
Production Vite build passed (100 modules, local font Worker emitted). Tauri release
EXE build passed. Isolated copied EXE opened a responding `Nect SVG` window; stdout
and stderr were empty. Close requested but did not complete within three seconds;
test-only process cleanup was required. Native rendered editing, picker interaction,
save/restart, IME and DPI acceptance remain unverified.

Additional older PaintedPath suite: four failures from stale schema expectations
(2/18/21 versus current 27, including centered-grid validation). These were left
untouched. Static family/style DOM suite skipped because optional DejaVu style
fixtures were absent; variable-font UI/named-instance and Text-to-Multi-Path DOM
checks passed. This is a staged trial, not a completed stable release.

Font source includes fontkit Worker, mixed static/variable picker, variable named
instances and Text to Multi Path. Variable support remains limited to supported
whole, horizontal, single-line, simple Latin one-to-one text. Do not infer complete
Japanese/vertical variable-font support.

DIN: Windows GDI installed-family API returned `DIN Alternate`. Reading standard
Windows and user font directories found 405 files / 473 faces, zero parse failures,
and no DIN-named family/face. Nect's native catalog currently searches only these
two directories, so Adobe Fonts activated outside them are not demonstrated in
Nect's picker. DIN format/face origin, native drawing and outline conversion remain
unverified. No Adobe font cache was extracted, copied, embedded or uploaded.

Remaining work: Adobe Fonts discovery/licensing-aware use; direct-selection A,
point nudging, Alt independent handles, corner conversion and endpoint extension.
No main merge, release, force push or authentication/permission changes.
