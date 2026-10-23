# Japanese basic wrapping

The **Japanese basic** line-composition option applies to static Text and Windows
Text, horizontally with `textWrap.width` or vertically with `textWrap.height`.
Both use the explicit policy `japanese-basic-v1`, requiring schema34. Existing
horizontal `{width}` documents retain their previous geometry and rules. Choosing
a different writing mode explicitly converts a wrapped Text to the new policy.
Ordinary unwrapped documents retain their existing schema and geometry.

The bounded policy prevents a soft line ending after these opening characters:

```text
([{（［｛「『【〔〈《〖〘〚
```

It prevents a soft line beginning with these characters:

```text
)]}）］｝」』】〕〉》〗〙〛、。，．！？!?.,:;：；
ぁぃぅぇぉっゃゅょゎゕゖァィゥェォッャュョヮヵヶーｰ々ゝゞヽヾ〻
```

Consecutive copies of the same `‥`, `…`, `—` or `―` character remain together.
ASCII and ideographic separator-space runs remain in their source spans. A long
word, consecutive prohibited-start/end group or inseparable repeat can exceed
the specified extent. It remains visible, warns and exports intact; there is no
emergency split, clipping, scaling or character deletion. An authored newline
always wins, including a newline adjacent to a prohibited character. Other
punctuation, ruby, hanging punctuation, line justification, script-wide UAX14
conformance and full JLREQ typesetting are outside this named policy.

Static Text keeps its existing supported-script/mapping checks. Vertical fitting
uses the same vertical alternates, advances, physical height, tracking and manual
pair adjustments as rendering. Column leading and source scalar IDs survive
reflow. Rich Text adjustments continue to use their existing source records.
Variable-asset Text and Type on Path keep their earlier restrictions.

Windows Text uses DirectWrite `ja-jp` whole-word wrapping. The new
`directwrite-v4` / OS reference5 snapshot records exact original UTF-16 line spans
as `{start,length,trailingWhitespaceLength,newlineLength}`. JS and Rust validate
contiguous source coverage, scalar boundaries, hard newlines and every named
soft boundary before adoption. A host result violating that policy refuses the
operation and preserves the document. This is a checked bounded contract, not a
claim that every DirectWrite locale/font implements identical general typography.
Vertical layout retains its requested height for start/center/end alignment;
advanceWidth/advanceHeight remain physical dimensions. Saved geometry, spans and
metadata contain no font bytes and re-export without Windows or installed fonts.
OS custom leading/rich/path/character adjustments remain unsupported.

The boundary classes follow the relevant distinction between prohibited line
starts, ends and inseparable characters in [W3C JLREQ](https://www.w3.org/TR/jlreq/),
with the explicit smaller table above. Native metric contracts follow Microsoft's
[line metrics](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_line_metrics),
[vertical trailing-whitespace height](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_2/ns-dwrite_2-dwrite_text_metrics1)
and [whole-word wrapping](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ne-dwrite-dwrite_word_wrapping)
documentation.

Evidence: `tests/japanese-wrap.test.js`, `text-wrap.test.js`, `os-text-wrap.test.js`
and `desktop_font_layout::tests::actual_os_japanese_basic_wrap_vertical_and_boundaries`.
The real Meiryo probe sweeps every table character in both writing modes at8/64px,
long runs, fixed-box alignment, signed tracking, leading, empty/blank/trailing
newlines and fractional extents. Native UI dialog/IME/high-DPI hands-on acceptance
remains separate from browser controls, native geometry tests and EXE startup.
