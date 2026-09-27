# R09-D9 — optional components within exact output-color groups

Status: Sol-frozen and implemented bounded packet, 2026-09-27. Baseline: clean synchronized `codex/practical-alpha@1c7f317a7cce37f45194fe77a8793b94cb05b7b7`; single-writer release `build/manual-recipes/r09-d9-release-20260927.json`. Authority: DEC-71, [Completion Route R09/P09](https://app.notion.com/p/3e3fd279a6f381b4ba9dd2d1bf066e0f), [P09](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006), Confirmed [REQ-66](https://app.notion.com/p/3e0fd279a6f38157bd50d72a2f48654b), `CURRENT_GOAL.md` and [D8](r09-d8-color-groups-entry.md).

## Bounded contract

- Add optional Boolean `include_color_components` to the read-only `analyze_regions` desktop request and formal `nect_analyze_regions` schema. Omitted or `false` preserves D8 response bytes/fields for the same request. `true` requires `include_color_groups:true`; otherwise reject `INVALID_REQUEST`. Non-Boolean values reject `INVALID_REQUEST`. Unknown fields and all existing identity/revision/gesture/scale/threshold checks retain their behavior.
- For each D8 exact straight-sRGB RGB key, split its already computed maximal row runs into 4-connected components. Runs in consecutive rows connect only where their half-open x intervals overlap; corner-only contact does not connect. No second render or conversion, altered alpha rule, authored palette identity, native change or Session command.
- Return a flat `color_components` array ordered by ascending RGB key, then by the component's first row-major pixel. Each item has `component_index` (zero-based ordinal in this result only), `rgb:[r,g,b]`, `area`, `bounds:{x,y,width,height}` and lossless row-major `runs:[{y,x,width}]`. Bounds use top-left output pixels and exclusive width/height. Area equals the sum of run widths. A component index is transient and never an authored Object/region ID or persistent reference. Empty foreground returns an empty array.
- Apply all D1–D8 checks and caps before component construction. Then allow at most 10,000 total color components; the 10,001st refuses `ANALYSIS_LIMIT` with no partial result or Document/revision/History/native-byte change. In particular, D8's 256-group and 20,000-color-run caps keep their precedence. No new dependency, GUI control, file operation or native writer change.

## Independent positive and negative oracles

1. In the D8 4×3 fixture, blue `(2,0),(2,1)` is one 4-connected component (area 2); red `(0,0),(1,0)` is one (area 2), and red `(3,2)` another (area 1). RGB order puts blue first, then red by first pixel. Their runs, bounds, area and result-local indices are exact. A same-color diagonal `(0,0),(1,1)` gives two components; a same-color bridge `(0,0),(2,0),(0,1),(1,1),(2,1)` merges both top runs into one component (area 5), proving union across two predecessors.
2. An all-opaque 100×100 alternating red/blue checkerboard has one alpha region, two D8 color groups and exactly 10,000 color components/runs; accept. A 101×100 checkerboard has 10,100 color components and color runs, below D8's 20,000-run cap, so reject on the D9 cap. Preserve earlier-cap precedence with the existing D8 fixtures.
3. Check pixel helper, actual committed transparent Canvas/API output from an embedded exact-color fixture, and formal MCP equality for a nonempty request. Check omission/false, missing D8 opt-in, invalid type, exact limit/refusal, and unchanged Document/revision/History/native bytes on success and refusal. Do not use automated Qt/API/MCP evidence to claim hands-on GUI acceptance.

## Residuals

This output is a replaceable derived candidate, with no durable component ID or adoption command. Palette identity, approximate/perceptual colors, named group Boolean operands, arbitrary morphology, mask authoring, Node consumers, R09-A alpha/luma mode design and human hands-on rows remain separate. REQ-64/65/66, R09/P09 and L2 stay open until their own residuals are accepted.

## Implementation and verification

`Host::analyze_region_pixels` splits D8's bounded group runs with a row-adjacent interval union pass after D1–D8 success, then emits the result-local ordered components. The live desktop request and formal MCP schema expose the separate Boolean opt-in. The implementation does not change Session commands, native storage or the Canvas renderer.

The Worker completed a Release build and focused `region_analysis_contract|mcp_desktop_contract` CTest **2/2 PASS**. Sol reviewed the exact five-file source/test diff against this packet, including union of multiple predecessor runs, component order, cap precedence and no partial result. Sol independently reran the focused Release CTest **2/2 PASS** (5.84 s, 10.14 s); `git diff --check` passed. Pixel oracles cover diagonal separation, bridge merge, exact 10,000 and 10,001 limits, and D8 cap precedence. Live Canvas/API and formal MCP checks cover a nonempty exact-color result, omitted/false compatibility, option validation, refusal parity and unchanged Document/revision/History/native bytes. This is automated semantic, Qt and MCP evidence, not hands-on GUI or whole REQ-66/R09 acceptance.
