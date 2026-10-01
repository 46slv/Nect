# R12 original CP2 SVG fixture replay / cloud evidence boundary

Status: PARSER/SESSION SUBSET QUALIFIED; FULL HOST CONTRACT STILL FAILS. Base `b3e2b97d46c4dc28ab2997297d435c3d15a90457`.
Owner: dot, sole cloud writer. This recovers an existing acceptance input and exposes
an honest parser/Session subset; it adds no product scope or production path policy.

## Canonical artifact recovery

Source: [CP2 r4 canonical page](https://app.notion.com/p/3e3fd279a6f3818e877bc08effc9e600),
“Worker access patch — exact fixture bytes inline”, last edited2026-09-24T13:49:33.535Z.
The page explicitly declares byte-for-byte delivery mirrors of approved r1/r2,
not new specifications. Both recovered bytes match the original pinned expectations:

| File | Bytes | SHA-256 |
|---|---:|---|
| nect-stroke-cp2-cases-r1.json | 5597 | 4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1 |
| nect-stroke-cp2-fixture-supplement-r2.json | 5877 | fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee |

Root independently checked lengths, SHA, valid JSON, BOM-free UTF-8/LF/final newline,
then copied exact bytes only to ignored `build/d0-core/stroke-cp2-r4-fixtures`.
No JSON reserialization or expected-value edits. Archive bytes were not claimed
verified: inline recovery sufficed. Fresh environments must restore the same exact
artifacts; a missing/mismatched fixture remains a failed obligation, not a pass.

## Replay and test boundary

Untouched `svg_import_contract` advanced beyond missing fixtures and reached Host
negative import, then failed its production reject-code assertion. The Host still
requires an absolute local Windows drive path; no path validation is bypassed or
relaxed. The diagnostic now shows fixture label, expected code and actual code.

An explicit `--parser-session-only` test mode is added and separately named CTest
`svg_parser_session_contract`. It executes the original parser/native/Session/Undo
checks, exact r1/r2 positive and negative cases and command-budget cases, then stops
before the first Host I/O case. Unknown arguments fail. The existing no-argument full
`svg_import_contract` still executes all Host/GUI checks with unchanged expectations.
This separates a proven subset from an unproven Host contract; it is not a substitute
for Windows/hands-on/full aggregate acceptance or whole REQ/R00 closure.

## Fresh-clone reproducibility follow-on

After the initial ignored-build replay established the exact input and Host boundary,
retain immutable JSON source mirrors under `tests/fixtures`, with adjacent provenance
README. No fixture-specific no-vendoring restriction, third-party artwork, fonts,
SDKs, binaries or credentials were found in the canonical inputs; existing release/
distribution restrictions remain. Original historical review metadata is unchanged.

A compile-time source-fixture directory is the first lookup candidate. Length/hash/
encoding assertions remain mandatory; a corrupted source file fails rather than
falling back to another copy. Exact per-file `.gitattributes -text` rules protect the
pinned LF bytes from Windows autocrlf conversion. The subset was verified from a fresh
scratch working directory, without relying on the initial ignored build copies.

## Final receipt — 2026-10-01

- Original fixture recovery, root independent hashes/length/encoding checks and
  source mirrors PASS. Git `core.autocrlf=true` filtering yields the same blob hashes
  as raw bytes for both files; both text attributes are explicitly unset.
- `svg_parser_session_contract` PASS. The same binary/mode also PASS from a fresh
  `/tmp` working directory with no scratch fixture directory, proving source-backed
  lookup. Unknown mode returns1 with usage instead of silently accepting a subset.
- Full `svg_import_contract` remains FAILED, with precise first Host boundary:
  `r1-n00-host`, expected `SVG_UNSUPPORTED`, actual `INVALID_SVG_PATH` (“SVG requires
  an absolute local drive path”). No production path policy or expected assertion changed.
- Final all-target desktop Release build PASS. Full serial70 CTest:43 pass,1 WIC
  skip,26 fail. Failing names are unchanged from69-test Frame checkpoint. The SVG
  failure is now diagnosed as a Windows drive-path boundary, not missing input.
- Current earliest failure categories: DirectWrite10, WIC4, Windows drive-path4,
  local-server startup7, cross-platform SVG golden one-ULP1. These are not claims
  that later stages in those failed tests ran. Full Windows/Host/GUI acceptance stays open.
- Independent read-only review verified hashes, attributes, source-first lookup,
  stage boundary, strict full expectations and no production-code change; no blocker.
- Evidence: ignored `build/d0-evidence/stroke-replay-{source-only,invalid-mode,stages,all-build,all-tests}.log`.
- Next eligible R12 slice: the existing History byte-budget/gesture/Point Edit/
  Group-follow safety functions currently lie after the unsupported Text stage in
  the full suite. Execute those exact independent oracles as a separately named
  subset while retaining the untouched default full contract and its platform failure.
