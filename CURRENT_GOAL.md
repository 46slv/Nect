# Nect practical-alpha — P01 qualification

## Mission Brief

**Final Goal:** Qualify current R1–R8 behavior against the authored operation recipes, reconcile product defects with manual errors, then prepare the P02 Grid / Guide / Margin / Smart Snap and P02-D design packet. Stop before P02 implementation.

**Completion conditions:** Each R1–R8 has a final evidence-backed PASS, FAIL_IMPLEMENTATION, FAIL_UI, FAIL_MANUAL, or BLOCKED_ENV classification. PASS covers the actual runtime, GUI, native and export paths required by its recipe. Failures include reproduction and next owner. Any bounded repair has an exact pushed candidate SHA and Sol review. A durable P01 result and P02 design handoff identify remaining gaps.

**Constraints / authority:** The 2026-09-25 user instruction authorizes P01 read/write, Release build/test, task-owned scratch, Windows runtime/GUI qualification, bounded repairs, coherent commits and non-force push of `codex/practical-alpha`. Sol owns review and reconciliation; fresh disposable Luna Max Workers own bounded execution packets. No P02 implementation, main merge, release, distribution, force push/history rewrite, account or credential changes, paid services, unrelated mutations, or unapproved irreversible action. Preserve user work and original examples.

**Coarse checkpoint map:** P01-A source/runtime reconciliation and R1–R8 qualification; P01-B bounded existing-contract repair and exact-SHA review if needed; P01-C final classification, remote checkpoint and P02/P02-D design packet. P02 implementation belongs to a later Task.

## ACTIVE CHECKPOINT

**Goal:** Execute P01-A qualification of R1–R8 against the current baseline, starting with an evidence-backed R1 result.

**Current phase:** `P01 ACTIVE / QUALIFICATION`; R1 and R5 are `BLOCKED_ENV`, R2–R4 and R6–R7 are `PASS`, and R8 remains unexecuted. CP2 is `CLOSED / REMOTE_SYNCED` and its packet-specific stop condition has been superseded by explicit P01 start authorization.

**Proven:** The repository is `D:\Documents\Nect`, branch `codex/practical-alpha`; configured author is `46slv <254147489+46slv@users.noreply.github.com>`. Notion Worker packet revision 3 states `P00 CLOSED / P01 START AUTHORIZED`; recipe source is `083b53d`. The fresh Sol successor verified Full access, approval `never`, local/remote HEAD `c9913f9e7a78a3338022caa1728e37e28d5c786a`, clean tree and no competing product writer, saved `build/manual-recipes/p01-takeover-ack.json`, and received the predecessor's writer release. R1 qualified real Windows GUI, native reopen and SVG/PNG; only additional Alt+drag is `BLOCKED_ENV` because the GUI driver lacks held-modifier drag. R2 qualified 13/13 and R3 qualified 11/11 actual GUI, native reopen and SVG/PNG checks: `PASS`. R4 qualified mask/Undo/Redo/group/source edits and native reopen/PNG: `PASS`; Sol independently read the native mask/asset and inspected PNG. R5 stopped at blank r0 because the GUI driver could not activate the visible Linked Image menu action: `BLOCKED_ENV`, with downstream steps `NOT_RUN`. R6 qualified 15/15 GUI Artboard inheritance/override/detach/order/Fit, native reopen and per-frame SVG/PNG: `PASS`; Sol independently checked native frame/object state and both exports. R7 qualified GUI SVG intake/Ungroup/shared transform/Undo/Redo, native reopen and SVG/PNG: `PASS`; Sol independently verified the live read-only API document deep-equaled native and post-reopen exports matched byte for byte. Exact receipts, revisions, hashes and owners are in `docs/p01-qualification.md`. No product defect or manual error has been accepted, no repair candidate exists.

**Next task:** Launch a fresh Luna Max R8 Atomicity/Recovery qualification packet in a dedicated owned Window, recovery directory and endpoint. Revisit R1 Alt+drag and R5 Linked Image only if a new actual GUI capability or human manual qualification becomes available; do not turn either `BLOCKED_ENV` into PASS from source or headless tests.

**Approach:** Use the Notion R1–R8 recipe page and current README/manual/owners as acceptance input. Use a new `build/manual-recipes/` run subfolder; never modify source examples. Separate API setup from actual GUI operations. Record HEAD, build/runtime, viewport, before/after revisions, expected/actual, artifacts and exact failure owner. Sol reviews concise worker results against receipts and current code. Qualification does not authorize a new feature.

**Done for next:** R1–R8 each have a final evidence-backed classification and any clear existing-contract repair has passed focused validation, normal commit/push, remote SHA readback and Sol review. Then persist P01 closure and prepare the P02/P02-D DESIGN_GATE packet without implementing it.

**State:** Working branch `codex/practical-alpha`; R7 qualification source baseline `4a89f4a8311ff4e640ce17a03afbddcde3c5ae80`. R1–R7 source, binary and evidence identities are in `docs/p01-qualification.md`; exact current branch HEAD is read from Git after each checkpoint synchronization. R7 Worker completed and was interrupted to mark done; no active child Worker, owned Nect Window or in-flight product mutation remains. The current Sol Task is the single writer for the rest of P01.

**Authority:** Current user P01 instruction plus Notion Worker packet revision 3 and recipe `083b53d`. The CP2 packet-specific stop does not block P01. Exact authority limits are in the Mission Brief above.

**Handoff:** `NEW_TASK` after P01 qualification and closure, in accordance with one semantic checkpoint per Task. The current Sol Task owns P01 and will transfer only after a durable synchronized checkpoint and successor verification.
