# Mask feather verification

Product: `b91ea40ea6f2284d0e4cabfc343914301889e485`.
Base PR70: `bbd814056afa677dfdf2fecc32a62707c845dc38`.

- 44 focused tests passed, no skipped tests: Mask feather model/DOM,
  Image effects, Composition Reference, numeric scrub and Color value expressions.
- PR70's two JSON and SVG files remain byte-identical after load/export;
  absent feather is virtual zero. First zero and repeated-value edits do not
  dirty the document, clear Redo or add history. One positive edit is one Undo.
- Finite/range and resolved Scalar inputs, expressions, stale commands,
  parameter/Composition locks and filter-region budgets reject atomically.
  Undo/Redo and JSON/composition-project reopen preserve authored settings.
- Production Web bundle with the configured CSP, headless Edge: nine fixture
  documents reopen to exact JSON/SVG; actual Web Export PNG downloads match the
  corresponding SVG's full RGBA. This is not a native PNG save-dialog test.
- Sigma 4 at the edge x=20 gives alpha 7/48/140/222/251/255 at
  x=12/16/20/24/28/32. Normal plus Invert alpha matches source alpha with zero
  8-bit error over the fixture. Green/red interior detail, partial source alpha,
  referenced stroke and its bounds remain intact. A supplementary headless
  zero-area shape probe preserved transparent/fully inverted results.
- Before-Mask shape rotation/nonuniform scaling and after-Mask appearance
  rotation/nonuniform scaling were tested. Sigma is in Mask input document
  coordinates; the later appearance transform moves/scales source and mask
  together. No CSS screen-space adjustment is involved.
- Frontend build and native build-config guard passed. D-target native offline
  release build passed in 42.20 seconds with custom-protocol enabled. The native
  executable was compiled, not launched. No new native GUI or OS dialog opened.
- Before building, all 27 prior executable hashes and 361 sealed files matched.
  PR70 samples, old candidates and the pending native test instance/profile were
  not changed. Child TEMP/TMP and native target/output are on D; the existing
  fixed frontendDist on C remains a build dependency.

Working evidence:
`D:\Nect-codex-output-20261022-task13-f336c94f2b714cd5bc73d6ca646c8f2e\mask-feather-mvj23age`.
The sealed candidate is a separate `mask-feather-artifact-b91ea40` directory
under the same output root. It includes the executable, bundle, fixtures,
PNG/SVG/JSON comparisons, logs, source delta and SHA-256 manifest.

Not verified: native GUI interaction for feather, physical input, normal OS
Save/Open/PNG dialogs and normal OS cold reopen. No broad performance claim,
installer/release, protected-main merge or Library upload is included.
