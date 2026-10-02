# P02-D — live batch geometry repair candidate

Base: `e554dbd851cf7ea382c42a19965f82b47f142e93`.
Status: CLOUD AUTOMATED QUALIFIED; exact patched Windows and styled live-phase qualification remain pending.

## Observed failure and causal boundary

Windows e554dbd compiled all targets, including the repaired Path/Text branch, but full91 was84pass/7fail. Six primitive/Repeater normal/high/fractional geometry cases failed; the seventh was the known registry denial. Compact feedback149 checks and storage/protection/formal MCP Template passed; no source or ACL change was made.

The knob local rectangle stayed(0,18)44x44, while its global center moved. Primitive Mixed+29.74488 had row(174,139)148x80/caption98x80/globalcenter(877,870); at+60.25512, row(114,157)208x80/caption158x80/globalcenter(817,888). Repeater moved from row(174,251)148x80/globalcenter(877,1426) to row(12,269)310x80/globalcenter(715,1444). Caption heightForWidth98 was80 throughout, but preferred/minimum widths were144/120. Ancestor rectangles were not captured, so those logs alone did not prove the layout negotiation cause. The geometry assertion threw before the adjacent live-fit assertion at the failing phase.

A new local red on unchanged production source independently exposed the content-to-row hint dependency: row preferred117x80→126x80 and minimum101x80→115x80 on a live fractional update. Linux had enough actual width to avoid movement; this is not falsely labeled a Linux reproduction of the Windows pointer displacement.

## Narrow repair and test oracle

The sole production behavior change is the shared caption's horizontal QSizePolicy from Expanding to Ignored. HBox stretch1 still allocates remaining width; the80px row,44px dial, caption text, numeric/accessible precision and gesture behavior stay unchanged. Caption-derived minimum/preferred widths no longer influence the wrapping form row; patched row hints are stable50x80 in the focused tests.

A shared test helper preserves old assertions and checks local/global row/dial/caption allocation plus row hints. Fit is measured with an independent unconstrained QLabel in both axes (minimum width and height-for-width); an oversized-token negative control guards against a height-only false pass. All three batch families retain measured98x80 sample coverage. Arbitrary counts/fonts, multi-monitor changes and unobserved held-live styled phases are not covered by these samples.

## Qualification

- All-target Release PASS
- Focused angle/utility/compact feedback19/19 PASS
- Full91=64pass/1skip/26fail, exactly the unchanged baseline failed-name set
- Fresh-process native/SVG smoke PASS; GUI/MCP flags false
- Independent read-only review: no remaining findings; root reconciled every source and evidence hash
- No native-format, model, storage, Template, authored-value or gesture semantic change

Exact patched Windows is mandatory before calling this platform repair accepted. Prior4811296 actual GUI idle/quarter evidence does not automatically qualify the new policy. The native drag API cannot capture held phases; no unsupported GUI route or synthetic result is substituted.

## Frozen hashes

- `src/desktop/window.cpp`: `1172350683b08e0c5435d54e39d294e1957079831bc636653480d3a79ffecfd9`
- `src/desktop/window.hpp`: `3dcfabc249f3f501ea83f9c18e8dbeb57d5e4ea8b2978ba21fd38176b7a47dec`
- `tests/batch_angle_geometry.hpp`: `705aa94569918a590d2c88bb0b0515c4d78f1d988635316ab3b68c0b6c1d9789`
- `tests/batch_point_angle_ui_tests.cpp`: `3321fbe3159205efd4caced89f5e50c5ba2e3eebdf2f96612e752e345e747466`
- `tests/primitive_batch_angle_ui_tests.cpp`: `6b2ce300fc631c399d9d3c601ca85e81fd43162110aae333bfc2bebbcb2c8b34`
- `tests/repeater_batch_angle_ui_tests.cpp`: `2a23b17c44c88f12c29accae62965a65a9b025f3461ffe99aa6fa57732a5c5a4`
- `CMakeLists.txt`: `613591ca51dbc75768ce4333ab1b7b7381fa03b14746230aaa180e692d5dfe0d`

Evidence manifest `build/d0-evidence/p02-d-packet-c-live-geometry-receipt.json` SHA256 `9bfd538721f87ab49fbbdd84edffc69bd48e504a51fee0f9be039e86accf579c`.
- `build/d0-evidence/p02-d-packet-c-live-geometry-final-all-build.log`: `38066a70611da06472f9b9d1aea98ccca6205bf429e8922eeda97af1edb44709`
- `build/d0-evidence/p02-d-packet-c-live-geometry-final-focused.log`: `c097bb57ec0c1196ba6158973138a842fb6806813cb6341dc98449b9c0973f4b`
- `build/d0-evidence/p02-d-packet-c-live-geometry-final-full.log`: `b8093a325a56773b17cffbd8bb1b2fc783f47f358c1422c367e00ae6322bf45c`
- `build/d0-evidence/p02-d-packet-c-live-geometry-final-smoke.log`: `7cd237678e4001117718f685f4c612cbbbcb217b79273c1ad43af6dfc8986cb4`
- `build/d0-evidence/p02-d-packet-c-live-geometry-independent-review.txt`: `8de7ec5f35d40d619e7a6a33eac7867bba335c2aeb037950b91f514eb03ec6eb`
- `build/d0-evidence/p02-d-packet-c-live-geometry-red-primitive.log`: `06aec691f34b4659a2789d2972f74af7e385186ef6e65cf019029c0ebe5443f7`
- `build/d0-evidence/p02-d-packet-c-live-geometry-red-repeater.log`: `eb877863b4424e958b02bcf591da63d8afd517a227ccff3eb53688a2a4170294`
- `build/d0-evidence/p02-d-packet-c-live-geometry-red-batch_point.log`: `bfc7f85a901a56fd63f3bc3c731dd9c98888a77275c014622f65267dce24cce5`
- `build/d0-evidence/p02-d-packet-c-live-geometry-green-primitive_batch_angle_ui_tests.log`: `e0968fa01fad38d3f3ce3ffc447339b3ff65b1f6c5f3aff674239dadee442d39`
- `build/d0-evidence/p02-d-packet-c-live-geometry-green-repeater_batch_angle_ui_tests.log`: `04083b1f626c58a9ecc0adc568fdb7ff5c3b7a464d006f5f34eceb92afa9ad5b`
- `build/d0-evidence/p02-d-packet-c-live-geometry-green-batch_point_angle_ui_tests.log`: `a26b79e7e28508b44dbb27b0c013846019f437337df5d64f378c37a0ca2b0d11`
