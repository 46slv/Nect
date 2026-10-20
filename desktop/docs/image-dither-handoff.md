# REQ303 Image Dither continuation

Goal: retain existing Shape Dither and provide actual derived Image output with
source/settings editable. User requirement and chosen bounds are separated in
[the contract](image-dither-output.md). Notion belongs to the parent lane; do not edit it here.

## Source and ownership

- Draft PR: https://github.com/46slv/Nect/pull/39
- Base: `3922a425429d57ce5b8b4e6dba985554f657ff17`.
- Product/source commit: `a195cd48d3e0b687ef964c14e4ba5fbe9bcc63c4`.
- Branch: `codex/image-dither-output`; PR base `codex/nect-test-candidate-20261020`.
- Worktree: `C:/Users/shiro/Documents/Codex/2026-10-21/task-5/Nect-image-dither`.
  Its independent Git common directory is the sibling `nect-lane.git`.
- This lane is the sole source writer. No successor has been started. Before a
  continuation mutates source, fresh-check branch/HEAD/status, current authority,
  and sole-writer ownership, then record `TAKEOVER_ACK` in the current checkpoint.
  Keep the compact Brief; do not reconstruct authority from the full transcript.
- Preserve the tested `C:/Users/shiro/.codex/worktrees/65e8/Nect` checkout and its
  delivered EXE. `D:/Documents/Nect` remains forbidden; do not follow the original
  checkout's `.git` link. Do not contact the other tester or create duplicate writers.

## Completed evidence

- Two new fixed Image types, common recipe/resolver path and schema 41. No authored
  asset growth during evaluation; source plus settings regenerate output on reopen.
- Focused source/DOM tests **117/117 PASS** and existing motif-bypass probe **1/1 PASS**.
  Pixel/alpha, old Shape fingerprints, consumer typing, Keyer/luminance/viewer,
  save/reopen, presets, Undo/Redo, cache replacement/eviction and atomic limits covered.
- Production web build **PASS**, 138 modules; native config, scoped formatting,
  syntax and diff checks **PASS**.
- Separate custom-protocol native release-profile compile **PASS** in 3m04s.
  [Build provenance](image-dither-build.json) binds all changed product inputs and
  native configuration/lockfile blobs to the source commit. Rust native source is
  unchanged from the base.
- Candidate `C:\Users\shiro\Documents\Codex\2026-10-21\task-5\candidate-image-dither\nect-svg-desktop.exe`:
  9,995,776 bytes; SHA-256
  `9c625fc2bc2a8592b2410921b642c1e45829aa728bf6899d079094f8a5943d3b`.
  It has not been launched or distributed.
- Local logs and an extracted, task-owned baseline probe are in ignored
  `desktop/test-results/image-dither/`. Preserve these and the separate candidate.

The extra whole Luminance Instances test file has an unrelated `24 !== 21` schema
assertion, reproduced on extracted base 3922a425. Its specific motif-bypass test
passes. Full suite is **NOT_RUN**. The stale Bypass unsupported-type list was
corrected with dedicated Image/Mask/motif functional coverage; base reproduces its
old Keyer assertion. No broad history audit is needed.

## Pending UI checkpoint

Status: **LOCAL_WAIT_UI_RELEASE / HANDOFF_READY**. The other lane exclusively owns
WS UI for CPU/held-drag testing. The exact pending actions are launching a production
browser/CSP test and launching/operating this new native candidate. Neither action
has been attempted; this is pending trusted parent release, not an uncertain run.
Do not infer release from elapsed time or the other lane appearing idle.

After explicit release, continue within the already authorized development scope:

1. Verify this source, clean/owned state, candidate SHA-256 and product input blobs.
   A fresh continuation is appropriate for this distinct UI phase; start only one
   and acknowledge takeover before writes or UI ownership.
2. Run a bounded production-bundle/CSP browser check for the new Image types. Exercise
   Image source -> each new Dither -> Image Output, mark-shape/size/spacing changes,
   Undo/Redo and save/reopen. Compare the actual embedded dither PNG and downloaded
   PNG, including transparent gaps, source-alpha behavior and old Shape controls.
   Reuse existing Image/Dither e2e setup; do not add general automation infrastructure.
3. Follow [native acceptance storage](native-acceptance-build.md) with this separate
   EXE and a fresh, nonexistent direct-child root named `nect-svg-acceptance-<run-id>`.
   Set `NECT_SVG_ACCEPTANCE_ROOT` only in the child process. Read the applicable
   computer-use skill, preserve the existing app/profile, and operate only this PID.
   No installer, normal-profile launch, security change or warning bypass is authorized.
4. Load a disposable copy of [the four-panel synthetic example](../examples/image-dither-output.json)
   through the allowed documents folder. Verify the two Image outputs, the two old
   Shape outputs, controls/history, JSON save/reopen and actual SVG/PNG files. Use a
   fresh acceptance root for another process, as the native storage contract requires.
5. Save screenshots, pixel/readback results, exact source/EXE identity and any limits
   actually encountered. Keep browser/native/physical-input acceptance separate from
   source/DOM/build evidence. Repair a proven local defect in this branch, recheck only
   affected contracts, push normally and update the same draft PR/checkpoint.

Do not merge a protected/main branch, release, force-push, alter security or edit
Notion. If all UI rows pass, report the verified result to the parent for its product
acceptance/Notion reconciliation. If blocked, record exact action, target, reason,
pending/denied/uncertain state and the resume trigger, then preserve the candidate.
