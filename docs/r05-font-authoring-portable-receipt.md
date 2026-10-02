# R05-FONT-AUTHORING-01 — Portable model checkpoint

Status: portable Text authoring model/codec/Session command slice implemented;
unintegrated intermediate only. This does not close REQ-15.

## Candidate boundary

- Isolated worktree: `nect-font-authoring`, branch `dot/font-authoring-01`, base
  `e554dbd851cf7ea382c42a19965f82b47f142e93`. No commit or push was made.
- Native writer advances from 0.77 to 0.78. Text source behavior version remains
  1. Optional empty collections are omitted; old files migrate with empty
  collections; presence under native <=0.77 rejects, even for empty values.
- TextSource now owns an ordered whole-text feature vector and a lexical map of
  finite additional-axis doubles. Existing weight/italic fields remain sole
  owners of `wght`/`ital`. Five typed commands flow through the shared Session,
  JSON-lines parser, native codec, Undo/Redo, revision checking and History.
- `CreateText` may include valid initial intent. `UpdateText` preserves those
  collections only when unchanged and otherwise returns `USE_TYPED_COMMAND`.
- No Window, DirectWrite shaping, backend receipt, formal MCP implementation or
  font binary acquisition changed in this checkpoint.

## Verification

- Release Linux build: `font_authoring_contract_tests` and `nect` targets built
  in `build-font-authoring` using the shared Boost dependency directory.
- `font_authoring_contract_tests`: PASS, 52 checks. It covers all five commands,
  padded/case-sensitive exact tags, the uint32 endpoints, duplicate/malformed/
  conflicting/missing/non-Text inputs, precise double round-trip, preservation
  of source fields/drivers, unchanged and rejected `UpdateText`, atomic batches,
  stale revisions, calibrated History growth/admission, Undo/Redo, old-version
  migration/version lies, duplicate JSON keys, malformed native records and the
  non-Windows `CreateText` projection refusal.
- JSON Schema Draft 2020-12 validation ran: schema check passed, one valid source
  passed, eight malformed sources failed, and native 0.77 rejected the new
  fields. Core validation and the existing strict duplicate-key parser remain
  necessary for tag uniqueness and authoring invariants beyond structural schema
  shape.
- The CTest `process_contract` executed the pre-existing process checks plus the
  new native-fixture JSON-lines apply/inspect/fresh-process cold-reopen checks.
  It later exited nonzero at the unrelated historical radial-ornament SVG
  byte-equality assertion. The assigned main worktree's existing `nect` binary
  and this candidate produced identical SVG bytes for that same fixture:
  `d26b3bcaa8ce14456cf47080005a5a713a5160728fb5c7f0c44a43cb018715ea`. The
  committed golden is `007036a75da5de6b23f7b852379d4b33f7dfcc51aa84a6069da8e3c053c6302c`.

## Explicit limit

The portable model and persistence work is not evidence that Windows currently
applies authored feature/axis values or exposes their resolved effects. Linux
still reports `TEXT_PLATFORM_UNSUPPORTED` for projection, and ordinary
`CreateText` remains an anchor-projection path rather than a stub. Keep this
candidate unintegrated until the separate Windows shaping/effective-receipt
checkpoint supplies application or an explicit unsupported result and is
reviewed. G11 fixture qualification remains a later acceptance step.

## Integrator checkpoint

Root verified all11 frozen file hashes and independently repeated the portable contract successfully. The reviewed source was committed locally as b9cdf2f, then latest main0c253b85460eef76e8887275c63e6a8ea03ae95c was merged into this isolated branch without conflicts. That brings the same reviewed shared-caption policy/tests into the feature branch; it does not integrate font behavior into main. Portable model/command/codec source stayed unchanged. Current backend still ignores font intent, so this branch remains explicitly unintegrated pending checkpoint2.
