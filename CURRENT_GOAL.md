# Nect practical-alpha — P01 qualification

## Mission Brief

**Final Goal:** Qualify current R1–R8 behavior against the authored operation recipes, reconcile product defects with manual errors, then prepare the P02 Grid / Guide / Margin / Smart Snap and P02-D design packet. Stop before P02 implementation.

**Completion conditions:** Each R1–R8 has a final evidence-backed PASS, FAIL_IMPLEMENTATION, FAIL_UI, FAIL_MANUAL, or BLOCKED_ENV classification. PASS covers the actual runtime, GUI, native and export paths required by its recipe. Failures include reproduction and next owner. Any bounded repair has an exact pushed candidate SHA and Sol review. A durable P01 result and P02 design handoff identify remaining gaps.

**Constraints / authority:** The 2026-09-25 user instruction authorizes P01 read/write, Release build/test, task-owned scratch, Windows runtime/GUI qualification, bounded repairs, coherent commits and non-force push of `codex/practical-alpha`. Sol owns review and reconciliation; fresh disposable Luna Max Workers own bounded execution packets. No P02 implementation, main merge, release, distribution, force push/history rewrite, account or credential changes, paid services, unrelated mutations, or unapproved irreversible action. Preserve user work and original examples.

**Coarse checkpoint map:** P01-A source/runtime reconciliation and R1–R8 qualification; P01-B bounded existing-contract repair and exact-SHA review if needed; P01-C final classification, remote checkpoint and P02/P02-D design packet. P02 implementation belongs to a later Task.

## ACTIVE CHECKPOINT

**Goal:** Execute P01-A qualification of R1–R8 against the current baseline, starting with an evidence-backed R1 result.

**Current phase:** `P01 ACTIVE / QUALIFICATION / HANDOFF_READY`; R1 is recorded, R2–R8 remain unexecuted. CP2 is `CLOSED / REMOTE_SYNCED` and its packet-specific stop condition has been superseded by explicit P01 start authorization.

**Proven:** Fresh start on 2026-09-25: repository `D:\Documents\Nect`, branch `codex/practical-alpha`, clean HEAD and remote branch both `03219d440a540cb9d06e69f8e8d5e434a25bc5b2`; configured author is `46slv <254147489+46slv@users.noreply.github.com>`. Notion Worker packet revision 3 states `P00 CLOSED / P01 START AUTHORIZED`; recipe source is `083b53d`. R1 Luna Max qualified real Windows GUI, native reopen and SVG/PNG; Sol independently read native/exports. R1 final class is `BLOCKED_ENV` solely because the available GUI driver cannot issue the additional held-Alt drag; all other R1 subchecks passed. Exact receipt, revisions, artifact hashes and owner are in `docs/p01-qualification.md`. No product defect or manual error has been accepted, no repair candidate exists.

**Next task:** After successor TAKEOVER_ACK, launch a fresh Luna Max R2 qualification packet. Explicitly edit Repeater Anchor from its preset source center (480,320) to recipe target (360,320). Continue R3–R8 in isolated episodes; R4 and R5 may share an episode because R5 consumes R4.png. Revisit R1 Alt+drag only if an actual held-modifier GUI capability or human manual qualification becomes available; do not turn the current BLOCKED_ENV into PASS from source or headless tests.

**Approach:** Use the Notion R1–R8 recipe page and current README/manual/owners as acceptance input. Use a new `build/manual-recipes/` run subfolder; never modify source examples. Separate API setup from actual GUI operations. Record HEAD, build/runtime, viewport, before/after revisions, expected/actual, artifacts and exact failure owner. Sol reviews concise worker results against receipts and current code. Qualification does not authorize a new feature.

**Done for next:** R1–R8 each have a final evidence-backed classification and any clear existing-contract repair has passed focused validation, normal commit/push, remote SHA readback and Sol review. Then persist P01 closure and prepare the P02/P02-D DESIGN_GATE packet without implementing it.

**State:** Starting baseline `03219d440a540cb9d06e69f8e8d5e434a25bc5b2`. R1 source/binary and local evidence identity are recorded in `docs/p01-qualification.md`. R1 Worker finished and was interrupted to mark done; no active child Worker, owned Nect Window or in-flight product mutation remains. This handoff edit and evidence report are pending a coherent commit/non-force push; exact final local/remote HEAD must be read back and written to `build/manual-recipes/p01-handoff.json` plus successor prompt.

**Authority:** Current user P01 instruction plus Notion Worker packet revision 3 and recipe `083b53d`. The CP2 packet-specific stop does not block P01. Exact authority limits are in the Mission Brief above.

**Handoff:** `NEW_TASK` for P01 continuation from the R1 material boundary. Old Sol holds writer ownership until the fresh Sol successor in the saved Nect Project verifies actual Full-access/no-approval runtime, same branch and exact HEAD, evidence, no active writer, then persists `TAKEOVER_ACK`. If that cannot be done, retain `HANDOFF_READY` and stop safely.
