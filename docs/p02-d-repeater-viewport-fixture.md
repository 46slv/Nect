# P02-D Repeater capture viewport fixture

Base: `630ba88561b9eacb8fa5a84aaa9bb47a87eb25f5`.

The parent-reported exact Windows native DPR2 diagnostic establishes a clipped
fixture: the dial was at viewport y1443 below a 281×944 viewport, its visible
region was empty, and the 2000×2142 whole-window image could not contain a crop
starting at physical y3042. Crop intersection and indicator ink counts were zero.
Source report: task17 `Nect-build/evidence/windows-caption-validation.md`;
manifest SHA256 `970f651cf3c4b6f2cd2ecef8828690aa94cea7ca8992344473b8027865cf9890`;
Repeater log SHA256 `de09adf527db702bc2538bcbc3b172ccbaa0b9eac1b3ff17e99b070f44959d5b`.

The test-only repair uses the existing single Repeater/Primitive ancestor
`QScrollArea::ensureWidgetVisible` pattern, then checks settled full viewport
exposure. It checks that the pixel crop is entirely inside the captured window.
The original cyan predicate, annular bounds, count >5 threshold, centroid and
top-zero direction assertion remain unchanged, as do production code and default
QPA selection. Exposure/crop diagnostics remain available.
Full visible-region coverage uses region subtraction emptiness; Qt's
`QRegion::contains(QRect)` only tests overlap and is not used as a full-coverage
proof. Independent source review identified this distinction before final tests.

Exposure occurs before Common capture and its first Geometry snapshot. A new
fixture changes Session identity, so `rebuild_inspector` resets scroll to zero;
Mixed exposure therefore occurs after its load and before the existing
Common-to-Mixed Geometry comparison and Mixed snapshot. The helper rejects an
active gesture. No scrolling occurs during either measured live arc, and all
existing local/global geometry equality checks remain in place.

## Linux verification

Release target build and focused CTest runs passed: normal interaction 79 checks,
DPR2 interaction 80 checks, and DPR1.25 fixed-global geometry. All three reported
full visible regions and contained crops; original annular ink counts were
34/106/55. Common/Mixed, live geometry, extreme-caption samples and wider-Inspector
checks were reached. These offscreen Linux viewports remain tall enough that
scroll offsets are zero; this is regression evidence, not exact Windows repair
acceptance. Native Windows replay was pending at this Linux checkpoint; the
verified follow-on below closes that bounded replay obligation.

- Test source SHA256: `e2a01e5d38d96762a6b8af336092a0f948311ff9851d44fe8e541634ea66f7ad`
- Binary SHA256: `d6a55df6bc512e439562994ac448121258fc278e215fde4b8ca637959f88f6b7`
- Build log: `build/d0-evidence/p02-d-repeater-viewport-build.log`, SHA256 `b0fa087effd7350d185ba511eb666912d6126638e11e0fbd1dedac9668414ae3`
- Focused log: `build/d0-evidence/p02-d-repeater-viewport-focused.log`, SHA256 `2538ca1a6407ce5f5155fa9a29976b68da74d450fc7f771618c01cc5964de854`

Command: `NECT_GEOMETRY_DIAGNOSTICS=1 ctest --test-dir build/d0-core -V -R '^repeater_batch_angle_ui_(interaction|hidpi|fractional_dpi)$'`.
`git diff --check` passed. No commit, push, Windows operation or font-lane change
was made by this repair worker.

## Native Windows follow-on, 2026-10-02

The parent read and verified the completed result from task
`01a0fcda-c267-7326-b4c5-a09f4ecfd0aa`, final turn
`01a0fd55-6fac-717e-8012-7f1104a8bc9b`. On the base commit above with this exact test patch,
the original full Repeater test executable ran with `NECT_GEOMETRY_ONLY` unset
under native Windows QPA: DPR 1/1.25/2 passed 84/85/85 checks respectively,
each with natural exit code 0. These runs completed the original Repeater suite;
they do not establish a full-project-suite or all-offscreen-green result.

The DPR 2 run took 9.539 seconds and exercised the previously missing exposure path:
scroll offset (0,593); dial rectangle (98,850), 44×44, entirely inside the 281×944
viewport; pixel crop (1558,1856), 88×88, entirely inside the window capture.
Cyan/annular counts were 206/106, with original annular counts 34 at DPR 1 and 55
at DPR 1.25. This establishes
the clipped native DPR2 fixture repair while retaining the later Common/Mixed,
live-gesture and caption assertions in the original executable.

An earlier PowerShell `Start-Process` attempt hung for 90 seconds before QPA diagnostics,
ended by an owned timeout with code 124, and was explicitly terminated. It was
not a natural test exit. A known-good explicit Python `Popen` launch with Qt bin
first in PATH and explicit plugin paths then passed using the same binary,
without source changes or rebuilding. The earlier hang's cause remains unknown;
all owned processes have exited.

- Exact source LF SHA256: `e2a01e5d38d96762a6b8af336092a0f948311ff9851d44fe8e541634ea66f7ad`
- Windows CRLF source SHA256: `36E367B251292C975F4D5D953D226EDAB13BFBD342C7389CE4AF95035681458D`
- Windows binary SHA256: `2A9357D1F22C9E54411ADCEE898984892810B002E530AE185A5040826C79E58C`
- Exact patch SHA256: `FADAAFA06C3B49462F1B4A66C882CB3DF0C15EA3E4C8F78E3588EF417A16FC40`
- Windows report: `C:/Users/shiro/Documents/Codex/2026-10-02/task-17/Nect-build/evidence/windows-caption-validation.md`, SHA256 `59EF334FE635DCE116F564C42C1FE274D4354C7089A62778C88635A4BD114999`
- Windows evidence manifest: 104 entries, SHA256 `A1BC346834BA07E9066393168E17BD4BC910B8F9282495D0BF61616560C2A7AF`

The prior Linux-only receipt is preserved unchanged at
`build/d0-evidence/p02-d-repeater-viewport-fixture-linux-a46895f8.md`, SHA256
`a46895f897d46c117b7dae4b0abb1e6d38408f8d7d44572f3cab7a06c4f6c2b6`.
This follow-on is documentation only; no source, build or Git publication action
was performed for it.
