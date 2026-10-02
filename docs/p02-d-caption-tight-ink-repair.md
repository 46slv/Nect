# P02-D — reject invalid logical font origins in caption reservation

Base: `69e2951afeb2ff8b0a870f0137c019b68546a162`, tree `c8b05da1c2828d2e476acc238093f4f884e519c5`.
Native 0.77 unchanged. This is a bounded allocation repair, not whole Windows qualification.

## Proven cause and independent boundaries

The exact Windows69 actual-widget comparison ran the same product-derived point,
primitive and Repeater suites on offscreen and native `windows` QPA at 1/1.25/2×.
All nine offscreen runs failed the original 98×80 positive fit assertion: Sans
Serif was unresolved, all27 logical bounding boxes started at Qt's invalid
100000 origin, and the product reserved100180px. Native QPA resolved Yu Gothic UI
9.75pt/13px and reserved103px; eight of nine passed. The native Repeater2× run
failed its earlier visible-indicator assertion twice, before caption metrics.
That is a separate test/capture boundary. No manual Windows GUI was run.

The Windows owner's report is
`task10/Nect-qual2/build-win/evidence/qpa-diagnostic-69e2951.md`, SHA256
`0b994525f0574c0ee44a660539b5ff2a8989b0dcf0b83d0c8e8ef0a81c552a1c`;
its manifest SHA256 is
`6cf45c76e1be9d1a7e4fb74598cdf110420be09ef8f96d02681410904792ad72`.
These remote paths identify that owner's evidence; they are not files copied to
this checkout. The Repeater2× failure log SHA256 is
`ed8aa1927b7ba25c034e208b552227f9b12ba73c50e2625cbbc28505c46c5926`.

Linux Qt6.5.3 minimal QPA independently reproduces the same invalid-origin
mechanism on actual product widgets. Old production reserves100240px for a
240px tight/advance requirement. The strongest sample is `-4.940656e-324°`:
logical bounds `(100000,100000,240,16)`, tight bounds `(0,-16,240,16)`, advance240.

## Production change

`Window::add_multi_angle_dial` now measures the union of horizontal advance and
actual tight ink extents: `max(advance, ink.right) - min(0, ink.left)`. Qt's
[tightBoundingRect contract](https://doc.qt.io/qt-6/qfontmetricsf.html#tightBoundingRect)
permits negative left bearings and ink wider than the advance; both remain
included. This is not dropping overflow evidence or capping the width at98.
The independent unclipped raster oracle still determines real painted fit.

Reservation remains once-only, with the98px floor, actual polished font/device,
frozen count, both headers, orientation and all27 signed-g7 candidates. Larger
fonts/counts can enlarge the minimum; wider Inspectors still expand. The80px row,
44px dial, live copy, exact values, Ref vectors, gestures and Undo are unchanged.
Default test QPA remains offscreen.

Nonfinite/negative advance or unusable tight geometry emits a warning and retains
whatever measurement is valid, plus the floor. A nonfinite final span or a span
beyond QWidget's representable size warns and saturates at its documented
`QWIDGETSIZE_MAX`; there is no arbitrary font-width threshold. No new exception
escapes an Inspector event callback. This defensive fallback makes no fit claim
for an unsupported backend; synthetic metric-fault injection is not qualified.

## Regression design

The shared initial-capture assertion now checks actual reservation against all27
actual-widget tight/advance samples. These fixtures keep font/DPI unchanged
between construction and capture. This is not a claim of dynamic font/DPI
re-reservation or universal arbitrary-font fit. Old production fails this new
assertion at100240actual versus240expected, before the raster fit assertion.

The explicit diagnostic runner is separate from the normal positive-fit suite:

```sh
python3 tests/batch_caption_invalid_metrics_tests.py \
  build/d0-core/batch_point_angle_ui_tests --platform minimal
```

On a Windows offscreen backend known to reproduce this condition, explicitly use
`--platform offscreen` and the Windows executable. The runner requires all27
observed invalid logical origins, independently recomputes tight/advance widths,
checks exactly100000 of origin inflation was removed, requires the real
reservation assertion to pass, and then requires the original98×80 assertion to
fail with positive, unclipped raster overflow. It cannot convert the normal
positive suite to a pass. Backend selection is explicit; this backend-specific
regression is not silently registered as an alternative for any normal test.

The separate Repeater capture diagnostics in this candidate were provided by
another owner. They log the original capture and viewport/region/pixel details;
the original predicate, thresholds, capture, waits and outcome remain unchanged.
They do not repair or qualify the Windows Repeater2× failure.

## Verification

- Final all-target Linux Release build PASS; focused9/9 and focused19/19 PASS
- Final normal offscreen point/primitive/Repeater diagnostics at1/1.25/2×:9/9 PASS,
  all actual/replayed minima100px, all27 samples, preserved compact geometry and
  wider Inspector growth
- Explicit minimal-QPA regression:3/3 PASS at1/1.25/2×;100240→240px, with the
  original98×80 positive-fit failure and nontruncated overflow preserved
- Supported-backend negative control: diagnostic runner correctly rejects the
  usable offscreen backend rather than accepting a normal positive-suite pass
- Fresh-process native/SVG smoke PASS; GUI/MCP flags false
- Independent final source review: no blocking findings
- Root independently reran full91:64passed/1skipped/26failed, exact previous failed-name set. Log p02-d-caption-tight-root-full.log SHA2561f08bb906f32bb83b87c8251eb5d1681b6c8b5a8964f690f0ca908c1a00625ca

Exact source, binary, log and review hashes are recorded in
`build/d0-evidence/p02-d-caption-tight-receipt.json`.
Exact repaired Windows replay and manual GUI qualification remain pending.
No commit, push, font installation, global setting or Windows operation was
performed by this repair worker.
