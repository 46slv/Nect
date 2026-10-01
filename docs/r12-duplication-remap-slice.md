# R12 existing Duplication selection/reference safety slice

Status: PORTABLE SAFETY SUBSET QUALIFIED; FULL WIC/TEXT CONTRACT STILL BLOCKED. Base `bfa0ffc794bb6562dac12f4de158c1bcfcae7a43`.
Owner: dot, sole cloud writer. Existing canonical Duplication/identity safety tests.

The full default suite first constructs a mixed Raster/Text fixture and fails on
Linux WIC before any checks. Four later independent functions construct their own
empty-document/path/primitive fixtures and are not reached in that failed run:

- selection_and_failures: stable sibling paint order, ID/empty/missing/repeated/invalid
  selection rejection, later-batch rollback, cross-Composition refusal, common JSON
  API created-ID readback and atomic HISTORY_LIMIT refusal
- object_visibility_driver_remapping: internal link/expression remapping, distinct
  authored/evaluated values, exact expression text and preserved external/original refs
- point_edit_enabled_driver_remapping: Object and correction IDs remap together,
  original/inbound refs survive, copied-internal and external-original propagation differ
- point_edit_enabled_expression_remapping: copied IDs change without rewriting
  surrounding expression text, original source and target-only external identity stay intact

A strict `--portable-safety-only` mode runs those unchanged functions in their
original relative order. Register `duplication_portable_safety_contract`
separately. Keep the default complete ten-function sequence, mixed fixtures, original
assertions and WIC/Text obligations unchanged. Unknown/additional arguments fail.
No production/schema/native0.77 change or new product requirement is introduced.

Selection/atomicity is included in the same portable safety invocation after source
qualification confirmed it uses only demo_document's ordinary Paths, not the mixed
fixture. This avoids another thin test mode while preserving one explicit bounded
question: whether the existing platform-independent Duplication safety oracles run.
The initial three-function exploratory invocation passed13 remapping checks; the
final named mode additionally runs the unchanged selection/negative/API cases.

## Final cloud receipt — 2026-10-01

- Final `--portable-safety-only` mode PASS23 checks, including10 selection/atomicity/
  API checks and13 visibility/Point Edit remapping checks. Every original source
  byte before main is unchanged; the default full ten-function order stays intact.
- Unknown, empty, repeated and extra arguments all fail with usage before tests run.
- Full all-target desktop build PASS; final serial72 CTest45pass/1skip/26fail,
  unchanged failing-name set from71-test History checkpoint. The default Duplication
  test still fails its initial WIC fixture. No Raster/Text/full-suite promotion.
- Independent read-only review verified fixture independence, original functions and
  order, argument handling, distinct naming, static23-check count and no production diff.
- Evidence: ignored `build/d0-evidence/duplication-remap-{red,final,all-build,all-tests}.log`.
- The user has now explicitly authorized Luna Max Windows validation/repair under
  Dot integration. Finish this exact checkpoint, then release the temporary CMake/
  Duplication-test reservation and reconcile it with the isolated Windows lane.
