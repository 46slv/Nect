# Nect practical-alpha — P02 implementation handoff

## Mission Brief

**Final Goal:** Qualify current R1–R8 behavior and establish a reviewable P02 Grid / Guide / Margin / Smart Snap plus P02-D daily UI design contract. Stop this Task before P02 implementation.

**Completion conditions:** P01 R1–R8 have evidence-backed final classes; product decisions and requirement status are fresh-read from Notion; the design gate and exact implementation entry contract are frozen and synchronized to `codex/practical-alpha`; P02 implementation is handed to a separate Task without a second writer.

**Constraints / authority:** The 2026-09-25 user instruction authorizes Notion/repo reconciliation, documentation-only packet freeze, a coherent commit and non-force push with remote SHA readback. [DEC-70 Accepted](https://app.notion.com/p/3e6fd279a6f3811c9448f8bfb8543bde) resolves product choices but explicitly does not authorize P02 implementation in this Task. No main merge, release, distribution, force push/shared-history rewrite, account/policy change, credential disclosure, unrelated mutation or original-project modification. Notion owns product decisions; Git owns implementation truth.

**Coarse checkpoint map:** P01 qualification/remote closure → P02/P02-D design closure and implementation packet → separately authorized P02-A native definitions/commands/migration → P02-B overlay/settings UI → P02-C Smart Snap and align/distribute → P02-D first-slice adapters. One semantic checkpoint per Task; only the next checkpoint is detailed.

## ACTIVE CHECKPOINT

**Goal:** P02-A native Guide/Grid/Margin definitions, commands and migration under a fresh implementation Task, using [`P02-ENTRY-01` revision 1](docs/p02-implementation-entry.md) as its bounded entry contract.

**Current phase:** `WAIT_SUCCESSOR_TASK / IMPLEMENTATION_NOT_STARTED`. The current design Task stops after documentation synchronization. The future implementation Task has not recorded `TAKEOVER_ACK`.

**Proven:** [P01 qualification](docs/p01-qualification.md) is `CLOSED / QUALIFIED_WITH_ENV_BLOCKERS`: R2/R3/R4/R6/R7 PASS; R1/R5/R8 BLOCKED_ENV, with no accepted P01 product defect. On 2026-09-25 live Notion readback, DEC-70 was Accepted, REQ-95 Confirmed / Should, and the Snapping priority question Closed. [`P02-DG-01` revision 3](docs/p02-design-gate.md) reconciles the five owner decisions; `P02-ENTRY-01` revision 1 fixes exact pre-design baseline `a60d519a6a63ec339944887693fb3aadde2596a1`, schema/commands/errors/fixtures and GUI acceptance. Documentation-only design closure commit `03eb84c5b845252a3cee0ce71f2060fdf767cd18` was non-force pushed; a fresh `ls-remote` returned the same SHA and the tree was clean. No P02 product code, schema or UI has been edited in this Task.

**Next task:** Fresh successor verifies project `D:\Documents\Nect`, actual Sol-high Coordinator/Reviewer and fresh disposable Luna-Max Worker profile, effective Full access/no routine approvals, branch/HEAD/remote, dirty work and sole writer of worktree/live Session. It records `TAKEOVER_ACK` before any implementation edit, then implements only P02-A within the packet. A fresh top-level Task does not inherit authority by assumption.

**Approach:** Read `AGENTS.md`, this Brief, `ARCHITECTURE.md`, `docs/model-v0.md`, current native schema and `P02-ENTRY-01`; compare live code/runtime to the frozen baseline and keep the Document/Session/IO ownership. Make P02-A native 0.14 definitions/commands/migration and focused real acceptance coherent before advancing. Do not start P02-B/C/D in the design Task.

**Done for next:** P02-A native 0.14 shape, migration, stable identities, exact validation, atomic Session commands/errors/Undo/recovery and focused core/IO/API acceptance are implemented and independently verified; any GUI claim has actual runtime evidence. Commit, non-force push and exact local/remote SHA equality close that implementation checkpoint before a new Task owns P02-B.

**State:** Design Task began clean at `D:\Documents\Nect`, `codex/practical-alpha@a60d519a6a63ec339944887693fb3aadde2596a1`, equal to remote. Design closure `03eb84c5b845252a3cee0ce71f2060fdf767cd18` matched remote at readback. This receipt edit becomes a follow-up metadata commit; the successor must fresh-read its final local/remote HEAD. Native writer remains 0.13 until P02-A implementation. R1 Alt+drag, R5 Linked Image activation and R8 Unlink/recovery-open retain their separate GUI requalification routes.

**Authority:** DEC-70 and Confirmed requirements provide product scope; the 2026-09-25 user instruction authorizes this design closure and push only. The successor must establish its own effective implementation authority and `TAKEOVER_ACK`; this Brief and packet do not grant it automatically.

**Handoff:** `NEW_TASK`. Single-writer transfer occurs only after this design Task ends and the successor verifies live state.
