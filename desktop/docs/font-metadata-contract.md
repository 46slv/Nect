# Static font metadata prerequisite (F1)

Implemented 2026-10-17. This is derived metadata plumbing, not completed style UI,
variable-font support, shaping, fallback, or native catalog coverage.

## JavaScript integration API

`fontMetadata(font)` in `src/fonts.js` reads an already parsed face and returns a
new frozen descriptor. `parseFont(bytes)` also stores that descriptor at
`font.nectMetadata`, only after the existing validation and parsing succeed.
`makeFontAsset(...).font` and faces from `resolveFonts(...)` carry it as well.

Fields:

- `family`, `subfamily`: preferred typographic names (OpenType IDs 16/17,
  `preferredFamily`/`preferredSubfamily` in opentype.js 1.3.4), falling back
  independently to legacy IDs 1/2.
- `fullName`, `postScriptName`: IDs 4/6; no invented fallback strings.
- `weightClass`: integer OS/2 `usWeightClass` in 1–1000.
- `widthClass`: integer OS/2 `usWidthClass` in 1–9.
- `fsSelection`: raw unsigned 16-bit OS/2 selection bits.
- `italic`, `bold`, `regular`: literal bits 0, 5, 6, respectively. They are not
  inferred from weight or names and may expose contradictory source metadata.
- `oblique`: bit 9 only when OS/2 version is at least 4; otherwise unknown.

Missing/invalid fields are `null`, including unknown flags. Within each naming
record, selection is deterministic: `en`, then lexically sorted `en-*`, then
lexically sorted remaining language keys. This is a stable display default, not
user-locale negotiation. Names must be nonempty strings, at most 256 UTF-16 code
units, without C0/C1 controls; surrounding whitespace is trimmed. Invalid entries
are skipped before trying another language or the legacy record. Metadata
extraction neither validates font bytes nor makes an arbitrary object a trusted
face; untrusted files must still pass `parseFont`.

Consumers may group/display these fields but must retain exact existing asset IDs
and bytes when selecting a face. Metadata is not a resolution authority, path,
CSS synthesis request, or new persisted schema. Identical names can describe
different fonts. Missing fields should remain unknown in UI; do not invent a
Regular/400 face. The existing asset `name` behavior and byte-based deduplication
are unchanged. No outlines, metrics, embedding checks, supported formats, resource
caps, or cache budgets were changed.

## Focused evidence

`tests/font-metadata.test.js` exercises bundled DejaVu Sans (Book, weight 400,
width 5), both existing Japanese fixtures, original in-memory metadata, and
original generated triangle fonts with identical names but distinct bytes and
outlines. The latter prove identity behavior, not real style fidelity. It checks
English/localized fallback, typographic priority, missing/malformed fields,
selection bits, descriptor immutability, exact embedded bytes, existing asset
shape, unchanged outlines/advance, malformed directory bounds, and continued
variable rejection.

Three optional tests inspect already installed DejaVu 2.37 static faces in
`/usr/share/fonts/truetype/dejavu/`; they skip when absent and perform no download,
installation, or copying. All three ran here. Metadata matches Bold (700), Oblique
(400), and Bold Oblique (700), including OS/2 bold/italic flags. Their outlines
match the same pinned parser without Nect metadata and differ from bundled Book.
This is face/geometry evidence, not whole-text/range UI, undo, export or native
Windows acceptance. DejaVu's bundled license is
`public/fonts/LICENSE-DejaVu.txt`; the installed faces retain original names.

Installed face SHA-256 values used for these local checks:

- `a4c5bc453ca281d90ea079e596da7ae0dfeb5777497c29ec254e76d97ff6f890  /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf`
- `e2f09289f4276309a36b9a93e5a0ac64957ef3eb7158151b243d41f667151ee4  /usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf`
- `c35383d5e56add5cc9cdc9f4e4a899f9e540f7c7e49f1901074c2d03af194eff  /usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf`
