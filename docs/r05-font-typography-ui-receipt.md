# R05 Advanced Typography UI candidate

Based on isolated e0602dd125740d06211f6d140355b083a04e2f00, native0.78. No main integration or wholeREQ15/G11 acceptance.

The one-whole-Text Inspector (including TextOnPath) exposes ordered whole-text feature rows and lexical additional-axis rows through all five existing Session commands. Existing Weight/Italic retain wght/ital ownership. Exact four-byte case-sensitive padded tags, uint32 maximum and precise finite scientific/double values remain authored. Explicit Apply/Cancel dialogs preserve drafts across Inspector refresh and reject changed session/revision/source/selection/artboard or active gesture; no-op/cancel/error adds no History mutation. Requested/submitted/actual-run values, availability and warnings remain separate.

Portable font-discovery/projection failures are shown honestly rather than aborting the Inspector. A narrow Canvas::select_many fallback after recorded projection failure admits only whole authored Text reachable in the active Composition ownership tree; no derived geometry/world cache, point selection, viewport hit or successful projection is fabricated. Existing hidden-object programmatic Structure selection semantics are retained; this path has no separate lock flag. Missing/non-Text/point/wrong-Composition/deleted identities are negative-tested.

Independent review found and repaired chained QString::arg reinterpreting valid percent-digit tags and arbitrary receipt strings, plus tooltip markup interpretation. Data now remains literal, tooltip markup is escaped with significant whitespace preserved, and normal labels/accessibility are plain text. Regressions include "%3  ", "<br>", arbitrary family/face/locale strings and tooltip QTextDocument presentation; pure formatter tests do not claim real font rendering.

Final source SHA256:
- CMakeLists.txt e2970b1ce5f68e856b0063307625a0556fe33827628f9c824d472bdf5d5255ef
- window.cpp 392ae780aaed54c21069cc19b1cefa2757e59ade1942489cc2aaa42648421208
- window.hpp 0dd354b591fd361df0efcdf1a3497bb2bedb9d52850865fb390099a686d711ec
- canvas.cpp 7a33f7bc4d08b5da8f418cd414503ecc2adfc611f29ba8dcd0184e3787c08a4c
- font_typography_ui_tests.cpp e044a28c18e7b175c77067d4eb00269f4806a110593ebaf056085627054445a1

Release desktop/new Qt target builds passed. Offscreen normal277 and2x278 checks pass; root independently repeated both registered cases. Coverage includes typed-command equivalence, ordering, exact numbers/tags, Undo/Redo/native reopen, cancel/no-op/invalid/duplicate/reserved values, stale contexts/deletion, TextOnPath, keyboard/accessibility and narrow320px dialogs/300px section. Logs copied into build-font-ui/evidence/{reviewed-build.log,reviewed-tests.log,root-repeat.log}.

Independent source-only reviewer approved final exact hashes after repair. Real GUI, Windows receipt display, latest backend oracle, live MCP, aggregate regressions and whole requirements remain separate gates. No font files, credential, global setting or native format change beyond the existing isolated0.78 was added.

## Combined main-caption reconciliation and real GUI, 2026-10-02

Local UI4b65035e20ac68a92a15a5a339cc2f3e3330a0d3 was merged with main630 into isolated local7d1b40d1252e28907353dd93a11e4bc0390e6a5f/tree3f71d59c7820392d9eb06f569628d18d95e288c6. Only CURRENT_GOAL conflicted; the font checkpoint was retained. Window merged across independent scopes. Combined Window SHA2560b315cbff3f736cc3973ea036297e7c6dd6281df5b8d6a33013fadc997150863.

All-target Release build passes with j1 after a j2 compiler process was killed; the cause of that kill was not independently established. Full97=68pass/2skip/27fail: exactly the main26 failed-name set plus font_mcp_parity local-IPC startup denial. Assets and Windows shaping skip explicitly. The merged typography and batch tests pass; fresh-process native/SVG smoke passes and makes no MCP/GUI claim. Logs build-font-ui/evidence/combined-all-build-j1.log SHA256edb1b4fc259bd488ceb74d7d7dac9f6e71b4c30be712a1f82d1adad6ba5e8485, combined-full.log SHA256f60d8de124732a74ba46c877aa9b76ecf9546ecdc84150064d819b906bbdfd52.

Actual own-cloud xcb GUI tested immutable combined desktop SHA256a74149302b4cd2152b27fe756901d41519b14b995d2f5d2c6311bd4031cccbe9. Six real add/edit/remove operations, uint32 overflow rejection with retained draft, Weight ownership rejection, exact padded/scientific input, no-op revision, native Undo and fresh125% reopen/Save As passed. Normal/125%300-logical Inspector and320-logical dialogs remained readable, including literal percent and markup-like tooltips. Unsupported projection/absent actual-run receipt stayed explicit. Both owned sessions closed.

Root independently verified all6 native hashes/JSON, before=Undo and final=reopened byte pairs, and inspected actual125% screenshot. Final native SHA2562f09aa10606f8b535812d081cc950ec0da35e5d53b90b84dc66abd9b9d17e3e4. GUI receipt SHA25659bf1aa5574b05f6b065e7775656ca4ea0de843a853dc83f9b37518a6b9a7da0; native verification SHA2567126c4d84f18392519261e3f70fd73c8e3441f238b6785425fdfd721e8dd31f3, under /workspace/shared/nect-font-ui-gui/evidence. Inspector refresh during open modal draft was not forced in real GUI, and actual TextOnPath interaction/Windows glyph display/live MCP/wholeREQ15 remain separate.

Exact pre-UI e060 Windows backend now passes357 shaping checks against the independent actualFace5 oracle. Its suite0missing count does not clear SourceSans3's separate designated G11 fixture. Desktop compiles/discovery36passes, but liveMCP remains blocked before ready/handshake,0semantic assertions; empty-log startup is under bounded diagnosis. This is not final combined-Windows qualification.
