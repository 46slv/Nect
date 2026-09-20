# Checkpoint — Linked / Embedded image assets complete

## Current scope — 2026-09-20

The user's latest instruction ends this implementation session at the image-asset
slice. That slice is complete. Do not start another feature or resume the earlier
open-ended practical-alpha Mission without a new instruction. Inherited Artboard
content/templates and other future slices have not been started.

Continuity authority is this repository on `codex/practical-alpha`. This file is
part of the coherent asset checkpoint after `7b05709`; obtain its exact commit
with `git log -1`. Earlier checkpoint narratives remain in Git history and
[docs/first-usable.md](docs/first-usable.md), not as active next-step instructions.

## Completed slice

- PNG/JPEG import, stable asset IDs, Linked/Embedded state and shared placements.
- Explicit Check link(s) detects external changes without altering accepted pixels.
  Reload/Relink accepts new bytes in one transaction; Embed retains cached bytes.
  Missing/unreadable/invalid sources preserve artwork and reject unsafe replacement.
- Width/Height are normal linkable/expression properties. Canvas placement, resize,
  transforms, existing geometry masks, opacity and blend modes share core semantics.
- Exact Undo/Redo, native save/reopen, live protection and abnormal-exit recovery.
  Native opening never reads linked files; status starts unchecked.
- GUI, semantic API and formal MCP use the same Session commands and Host file
  operations. SVG embeds normalized oriented sRGB PNGs without changing native
  original bytes, profile/orientation metadata or locators.

Major existing capabilities also include stable-ID Bézier editing, retained
Circle/Rectangle/Polygon/Star sources and Point Edit overrides, ordered local
Fill/Stroke/Repeater/Offset, gradients, editable Text, Named Colors, links and
bounded expressions, multi-selection/batch edits, Anchors/Transform Parents,
geometry masks/Group compositing, ordered Artboards with parent-size inheritance,
History, atomic saves/backups/recovery, SVG export and the live API/MCP.
This checkpoint does not claim completion of the entire practical-alpha Mission.

## Verification

Release build passed (`build/assets-final-build.log`; final desktop-test harness
rebuild `build/assets-desktop-test-build.log`). All 35 CTest entries passed in
51.78 s (`build/assets-final-ctest.log`), including 116 raster, 56 asset semantic/
codec and 70 desktop lifecycle/UI/pixel checks, strict native migrations, storage,
protection, formal MCP save/restart/recovery and existing feature regressions.

`examples/material-study.nect` / `.svg` contains one Linked JPEG shared by four
placements, one Embedded transparent PNG, a geometry mask, Multiply, three Named
Colors and seven editable Text objects. Source artwork is original procedural
fixture data in `examples/assets`; `scripts/create_asset_demo.py` reproduces it.
Actual Windows GUI on an owned working copy resized Width 480 to 520 and undid
exactly; Check detected an external file edit, Reload changed all four placements
without changing their object state, and one Undo restored the accepted source.
Native/recovery readbacks were exact and the example stayed unchanged. Receipts:
`build/assets-study-receipt.json`, `build/assets-manual-receipt.json`.
Owned application and benchmark windows closed normally; no helper/build remains.

Visible performance: 24 placements sharing eight 512x384 assets, 4,121,532 source
bytes / 1,572,864 decoded pixels, three masks/Multiply leaves plus two curves.
Worst p95 interval 18.49 ms, maximum 20.41 ms, maximum gesture release 19.70 ms;
all 30 fps p95/release budgets pass. 60 fps target remains unmet. This measures
pan/zoom and curve point/handle/translation with images present, not import/reload,
image-body translation timing or maximum-sized documents. Full boundaries and
results: [docs/first-usable.md](docs/first-usable.md),
`build/canvas-benchmark-assets.json`.

## Persistence and limits

Current native format is **0.13**, raster/interpretation version 1. Strict readers
migrate 0.1–0.12. Native/API/protection cap is 64 MiB; History retains its separate
1,024 edits / 64 MiB estimate. Assets retain immutable accepted original bytes,
shared by snapshots and History; derived pixels are not authored state.

Supported Windows memory WIC subset: 8-bit PNG/JPEG, alpha, JPEG EXIF 1–8 and
bounded usable RGB/gray ICC-to-sRGB interpretation. Limits: 8 MiB source/image,
8192 per axis, 16,777,216 pixels/image; 24 MiB source and 33,554,432 pixels/document,
128 assets. SVG normalized PNG: 16 MiB/image, 32 MiB aggregate.

Known limitations: link checks are explicit, import/reload/export are synchronous,
absolute local-drive locators only; unsupported color metadata/CMYK/high bit depth/
animation and other raster formats reject. No pixel painting or raster mask source.
The example's locator may require Relink on another checkout; cached pixels remain
usable. Independent browser SVG visual inspection was blocked by local-URL security
policy and is unverified. Normalized SVG pixel equality/structure and actual Nect
rendering passed. No current slice blocker or failing test remains.

## Stop and handoff

This checkpoint is delivered as a local coherent commit with final HEAD/status
reported in the handoff. This Astra context ends here. No publication or next
implementation task is authorized.
Push remains blocked by the previously observed GitHub GH007 private-author-email
rejection; no push was retried, account settings changed or history rewritten.

Next safe action for a new operator: read `START_HERE.md` and this file, confirm
branch/HEAD/status and obtain the new operating scope. Do not automatically start
inherited Artboard content or any other feature. Requirements/decisions remain
owned by Notion; implementation truth remains in this repository.
