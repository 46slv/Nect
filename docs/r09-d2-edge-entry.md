# R09-D2 — structured alpha edge map

Status: implemented and Sol-reviewed bounded packet, 2026-09-27. Baseline `codex/practical-alpha@7ea807552a21150994630fc71d44f1b7caf8550b`, clean and fresh-remote matched before implementation. Authority: DEC-71 Mission, R09/P09 and Confirmed REQ-64/65/66. R09-D1's read-only committed Canvas Artboard output is the input. This packet adds one edge-map primitive; it does not claim outer contour polylines, editable vector tracing or whole REQ-65 acceptance.

## Contract

- Extend the existing `analyze_regions` result with `edge_runs` and `edge_pixel_count`; keep its required identity, revision, Composition, Artboard, scale, threshold, output limits and 4-connected regions unchanged. No new Session command, file operation or native version. The formal MCP tool forwards the same richer result; no second edge-only renderer or mutable analysis state.
- A foreground pixel has alpha byte `>= threshold`. It is an **edge pixel** when any of its four orthogonal neighbors is below threshold or outside the Artboard output. Diagonal-only contact does not remove either edge. A transparent hole contributes an inner boundary. This is a binary alpha edge of the final committed Canvas output, not a color/luma gradient or vector stroke centerline.
- `edge_runs` is a row-major array of `{y,x,width}` covering every edge pixel exactly once with maximal contiguous horizontal spans. Coordinates are integer Artboard-output pixels with top-left origin; width is a count, not an inclusive endpoint. `edge_pixel_count` equals the sum of run widths. The result carries `edge_rule: "foreground-4-neighbor"` and the existing threshold, pixel format, source revision and coordinate metadata. Derived runs have no persistent object IDs.
- At most 100,000 edge runs may be returned. The 100,001st run refuses with `ANALYSIS_LIMIT` and no partial result. Preserve R09-D1's 4,000,000-pixel and 10,000-component limits. Rejecting a result changes no Document, revision, History or file.

## Independent oracles

1. A solid 3×3 rectangle at output pixels x=1..3, y=1..3 has area 9, `edge_pixel_count=8` and runs `{1,1,3}`, `{2,1,1}`, `{2,3,1}`, `{3,1,3}` in that order. The center pixel is not an edge. Move it against the Artboard boundary and the outside-neighbor rule still marks border pixels.
2. Two diagonally touching single pixels remain two 4-connected regions and two edge pixels. At threshold 128 a 127-alpha isolated pixel is absent; at 127 it appears as one edge run. An opaque ring with a transparent hole has both outer and inner edges, while the hole itself contributes no foreground pixel.
3. Compare the small hand-computed run oracle with the pixel helper and with the real Canvas/API result, then compare the nonempty direct API and MCP responses. Invalid identity/revision/gesture/threshold and oversized output retain their existing refusal behavior. Verify analysis does not alter native bytes, Document, revision or History.

## Verification

- Release build: `cmake --build build --config Release --target region_analysis_tests nect_desktop` passed.
- Focused contracts: `ctest --test-dir build -C Release -R "region_analysis_contract|mcp_desktop_contract" --output-on-failure` passed **2/2**. The pixel and real Canvas oracles cover solid/interior, Artboard border, diagonal contact, alpha 127/128, a transparent hole and the 100,001st-run refusal. Direct API and formal MCP results agree on nonempty edge output. Limit refusal preserves native bytes, Document, revision and History.
- Sol reviewed the source and oracle diff. `git diff --check` passed. No hands-on GUI acceptance was run.

Residuals: Color/luma gradients, outer contour ordering, polyline/path candidates, line extraction, mask boolean/morphology, Node reuse, GUI analysis controls and persistent artwork adoption remain open. No GUI/human acceptance is inferred from Qt or MCP tests.
