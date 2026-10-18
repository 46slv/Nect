# Grapheme editing probe — handed off, no implementation

Font/grapheme production source was unchanged when the user's 20:03 JST request
for per-operation recovery took priority. Base:9e4cbf9040c6e080710ca509765c3340fec53989.
The parent now owns assignment to another worktree/branch; this writer handles
only per-operation recovery. Do not merge or duplicate the other font/Dither work.

Disposable Windows Edge keyboard probe: task/probe-native-graphemes.mjs and
task/native-grapheme-before-probe.json. For A + grapheme + B, native left/right
and deletion preserved surrogate pairs, family ZWJ, regional-indicator flags and
airplane variation sequences. For x + U+0301, Backspace removed only the accent.
For Devanagari KA/VIRAMA/ZWJ/SSA, native arrows stopped inside the grapheme and
Backspace/Delete removed partial components. Node Intl.Segmenter grouped each as
one extended grapheme. Browser Intl.Segmenter and actual application controls
still need matching probes before implementation.

Reuse native-safe behavior. Graphemes are not shaping clusters. Keep native input,
Undo ownership and IME composition. This records evidence, not a shipped fix.
