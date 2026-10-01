# Immutable test inputs

`native-v0.1-linked.nect` and `native-v0.2-primitive.nect` retain historical native
reader fixtures. The two CP2 JSON specifications below are exact source mirrors,
not generated runtime receipts:

- `nect-stroke-cp2-cases-r1.json`:5597 bytes,
  SHA-256 `4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1`
- `nect-stroke-cp2-fixture-supplement-r2.json`:5877 bytes,
  SHA-256 `fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee`

Canonical source: [CP2 r4 — exact fixture bytes inline](https://app.notion.com/p/3e3fd279a6f3818e877bc08effc9e600),
last edited2026-09-24T13:49:33.535Z. Original `REVIEW_PENDING`/`NOT_RUN` metadata is
preserved because changing it would alter the historical inputs. Current execution
evidence lives in test results and `docs/r12-svg-fixture-replay.md`, not in these JSONs.

Do not reformat, reserialize, change expected values or convert line endings. Narrow
`.gitattributes -text` rules preserve their BOM-free UTF-8/LF bytes on checkout. Tests
validate length, hash and encoding before using any cases. Generated SVG inputs stay
in memory/task-owned scratch. Source-first test lookup makes fresh-clone validation
independent of Notion/Library credentials; legacy scratch lookup remains available
only when the canonical source path is absent, never after a hash mismatch.

This internal acceptance-data mirror does not grant a new release/distribution
license or promote any Requirement/fixture review status.
