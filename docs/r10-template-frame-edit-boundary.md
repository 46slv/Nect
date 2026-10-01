# R10 Template Frame-axis explicit-edit repair

Status: QUALIFIED bounded cloud candidate / remote synchronization pending. Base `5b8c752ab7483079a6dd36bdf0d71b783dd91547`.
Owner: dot, sole cloud writer. Existing Confirmed/Should REQ-205 and G5; no new intent.

The ordinary Width/Height controls submit whole `UpdateArtboard`, whose changed-axis
inference compares retained authored fallback values. Explicitly entering that same
fallback can therefore report success and advance History without changing the
inherited/effective size or a pre-existing override. Existing CLI witness: source1200,
target fallback640, assigned target remains1200 after UpdateArtboard(width640), while
canonical SetArtboardTemplateOverride(frame.width,640) correctly evaluates640.

Route explicit assigned Width/Height edits through existing per-axis Template
commands. Preserve independent parent/link/expression guards; plain Artboard and
X/Y crop/rename updates retain their existing path. Do not alter whole UpdateArtboard
inference, which must not promote unedited axes during rename/crop updates. Source
labels should use canonical size readback for template/template_override as well as
independent sources. No new schema, native0.77 change or background rendering choice.

Acceptance: actual public controls reproduce inherited1200→fallback640 and height
counterpart, existing override→fallback, other-axis/source-follow, Reset, exact
Undo/Redo, source/Definition/Instance/Guide/layout identity preservation, invalid and
stale refusal, independent-source protection, plain-Artboard and crop behavior,
Host Save and cold native validation. Baseline/platform failures remain explicit.

## Final cloud receipt — 2026-10-01

- Test-first public control reproduced explicit fallback640 failing to create the
  effective width override. Core whole-object UpdateArtboard was not changed.
- Public Window/Host97 checks PASS, including actual Reset dialog, exact source-kind
  labels, repeated existing width/height override-to-fallback edits, unmodified
  focus-out, missing captured IDs after document replacement, independent parent /
  link / height-expression protection, source/sibling-family/content identity,
  X/Y/rename and plain-Artboard behavior, exact Undo/Redo and native cold validation.
- Focused10/10 contracts PASS; full all-target desktop Release build and real-process
  smoke PASS. Source/test digests matched after final build.
- Full serial69 CTest:42 pass,1 WIC skip,26 fail, same failed-name set as68-test layout
  checkpoint. Full aggregate/Windows/visual/formal MCP obligations remain open.
- Independent read-only review reported no blocking defect. Nonblocking feedback
  strengthened public Reset/label/source/stale/crop coverage and made the assigned
  parent-size tooltip point to available Parent controls or Template Reset.
- Evidence: ignored `build/d0-evidence/template-frame-{red,all-build,all-tests,focused,final,smoke}.log`
  and `template-frame-candidate.sha256`. Native0.77 unchanged.
- Next independent validation: recover original hash-pinned CP2 SVG fixtures from
  their canonical Notion/Library artifacts, stage only ignored build inputs and
  replay the existing acceptance without changing assertions or inventing fixtures.
