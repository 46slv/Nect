# Historical SVG producer compatibility

The native migration process contract compares the complete SVG output of the
historical 0.3 radial ornament and 0.4 gradient ornament with their original
checked-in derivatives. Keep those native and SVG fixtures unchanged. Native
authorship is compared independently, including stable identities, bindings,
Point Edit corrections, operation order and gradient state.

## Linux producer variation

The canonical derivatives were produced on Windows. On Linux, the original
producers already differ in the last bit of `sin(pi/4)`: the recorded Windows
value is `0.70710678118654757`, while the Linux value is
`0.70710678118654746`. The corresponding negative matrix component also differs.
This affects the fill and stroke of petal copy 1 and ray copy 4, exactly four
complete transform strings in each SVG. Their other four matrix components,
including translations, are identical. This is not a migration/export regression
and does not justify changing production trigonometry or authored state.

On 2026-10-22, the original sources were independently built with GCC 14.2,
C++20 and Boost 1.83 on Linux:

- `29ee4c2361eb52dd11476edf4d5d39dfbc13484b` produced the original 0.3 scene.
  Its 36,636-byte Linux SVG has SHA-256
  `d26b3bcaa8ce14456cf47080005a5a713a5160728fb5c7f0c44a43cb018715ea`.
- `f581523dca30bcde99ca3508aa2500e8f6943b8f` produced the original 0.4 gradient scene. Its 38,061-byte Linux SVG
  has SHA-256
  `1194fb771b02f836b7f1b0cd61dfea4768cd1a015774396bf230f262a0b43318`.
- The current producer at `1724d9a61491fddddfe18a7644ee92adc5aaa789` emitted
  byte-identical output to each independently built original Linux producer.

The process test therefore applies one explicitly enumerated expected-string
projection on Linux only. It requires exactly four occurrences of the known
complete Windows transform before substituting the independently reproduced
Linux transform. It still compares the entire output string exactly. Windows
continues to compare the canonical SVG directly. There is no general numeric
tolerance, XML normalization, fixture rewrite, or production-code special case.

Real CLI mutation checks also establish that a `1e-9` generator or authored
transform edit, a name change, and root paint-order changes do not pass that
exact comparison. Any new numeric/platform difference must be investigated,
not added to this exception without equivalent producer evidence.
