# Key-color recipe core — bounded prototype, snapshot v2

Status: **experimental pure generator, not an active editor palette or default**.
`desktop/src/key-color-recipe.js` also validates optional historical snapshots in
the [schema58 retained-only adapter](../desktop/docs/key-color-recipe-retention.md).
That adapter stores historical receipts and permits a separate explicit one-shot
application of active HEX/alpha values to unlocked existing slots. Loading does
not generate/apply colors. No generator UI binding, Field evaluator, automatic
legacy conversion or replacement is introduced; the pure solver is unchanged.
The separate [developer/test-fixture initialization API](../desktop/docs/key-color-recipe-initialization.md)
allocates only fully explicit fresh IDs and a revision-zero historical receipt;
it introduces no product preset/default or ordinary-user creation flow.

## Source and decision boundary

Direction comes from the user's 2026-11-01 key-color decision, explicitly confirmed
at 07:58 UTC: 「キーカラー方針は本人の指示です。その方針で進めてください。」
The source document is `nect-color-palette-spec-v2-2026-11-01(1).md`, candidate
v2.1, Library `libfile_8b4f7bf7ab2881919fcb7a5a50c512b0`, read in full (165 lines).
Sections 1, 3–6 and 10–11 ground the contracts below. Section 4's remaining `kC`
spelling is interpreted as additive **Δr**, consistently with sections 1 and 3;
this prototype does not implement a chroma multiplier for user adjustments.

Approved direction is C1 + preset-derived hues + per-derived-slot lightness and
chroma-ratio deviations. The source's section 13 leaves concrete TonePlan /
ChromaPlan coefficients, role sets, generated counts and schema migration open.
**No aesthetically approved coefficients or final named presets are claimed.**
The `experimental-affine-v1` solver requires explicit caller-supplied mappings:

- `L_base = key.L * lightnessScale + lightnessOffset`
- `r_base = key.ratioCmax * chromaScale + chromaOffset`
- `L_effective = clamp(L_base + deltaL, 0, 1)`
- `r_effective = clamp(r_base + deltaRatioCmax, 0, 1)`
- `C = r_effective * Cmax(L_effective, derivedHue)`, then the existing
  `proposeAutoChroma` gamut limiter preserves hue and reduces C if necessary.

These affine coefficients are a versioned experimental evaluation mechanism,
not a final implementation of the designer's full procedure. Existing Triadic,
Complementary (including its repeated opposite), Analogous and Split relations
are reused through `harmonyCells`; `hueSteps` remains detail, not generated count.

## Internal prototype contract

`createKeyColorRecipe` accepts `paletteId`, `keyColor`, an explicit versioned
`preset`, and the derived `slots`. IDs follow the existing stable-slot character
and length bound; the total budget is 256 including C1. Role labels are explicit
(nonempty, at most 128 characters); identity does not imply largest area.

```js
{
  paletteId: 'brand_palette',
  keyColor: { id: 'brand', role: 'Identity', value: { hex: '#aBcD09', alpha: 0.4 } },
  preset: {
    id: 'experiment', version: 1, harmony: 'triadic', hueSteps: 12,
    slots: [{
      id: 'surface', role: 'Base', position: 1,
      lightnessScale: 0.7, lightnessOffset: 0.15,
      chromaScale: 0.8, chromaOffset: 0,
    }],
  },
  slots: [{ id: 'surface', role: 'Base', deltaL: 0, deltaRatioCmax: 0 }],
}
```

- C1's exact six-digit encoded-sRGB literal spelling and alpha survive. OKLCH is
  analysis only; C1 is never regenerated or assigned an offset. Its stored
  `hueHint` is initially zero unless explicitly supplied. A chromatic C1 updates
  the hint to its actual hue; exact black/white/gray retain the last hint, avoiding
  matrix-roundoff hue changes. Neutral C1 chroma/ratio are zero. Endpoint Cmax=0
  produces `zero-cmax`, never division by zero. This hue-hint policy is provisional.
- Derived alpha defaults to **1**, not C1 alpha; an explicit preset `alpha` is
  preserved, including zero. Chroma's additive deviation can color a zero base.
- Both deviations are finite, unrestricted stored intent. Only effective L and
  ratio clamp; `lightness-clamped` / `ratio-clamped` describe that limitation.
  `gamut-limited` describes chroma reduction; it never rewrites the deviations.
- A snapshot contains model/solver/color-semantics versions, palette identity,
  revision, intent, last committed resolved colors, full-precision linear samples
  and diagnostics. `restoreKeyColorRecipe` validates/copies that saved snapshot;
  it does **not** solve again. It validates shape/identity/ranges and C1's exact
  source-literal/decoded-linear agreement, plus the ratio relationships below.
  It does not prove provenance or full solver equivalence of arbitrary externally
  fabricated derived metadata.
- `editKeyColorRecipe` requires the exact `expectedRevision` and returns a detached
  candidate with one revision increment only on success. The caller must replace
  the committed snapshot atomically; exceptions never modify the input. This is
  ready for one future Session/Undo transaction, **not** an implemented Undo path.
- Explicit `key-color`, `preset` and `recompute` edits solve all active mapped
  derived slots and retain their deviations. `slot-delta` edits solve only the
  selected active stable ID and preserve every other slot's resolved data.
- Preset changes map known IDs explicitly, never by C2/C3 number or array index.
  Roles cannot silently retarget. All omitted slots are retained, even if no
  reference inventory was supplied: inactive slots keep their last committed
  value, role, alpha and deviations. They remain readable by stable ID, do not
  follow key edits while inactive, and rejoin generation when explicitly mapped
  again. Adding/removing IDs or editing inactive slots is outside this prototype.
- The saved global `areaAware` setting defaults true. No coverage input is accepted
  yet, so the solver is unweighted; true yields `coverage-unavailable-unweighted`.
  This is not a synthetic 0% measurement, stale coverage or readability pass.
  Tone hierarchy, contrast/readability (including alpha/background), area-based
  reinspection and required-constraint mode are **not evaluated** here.

## Explicit ratio values and prototype snapshot compatibility

Snapshot format **2** adds `rTarget` and `rRealized` to every resolved slot.
The solver and color-semantics versions are unchanged: no coefficients, gamut
profile or resolved colors change merely because these names are made explicit.

| Value                | Meaning                                                                                                                                                     |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Raw derived ratio    | `baseRatioCmax + intentSlot.deltaRatioCmax`, before any clamp. It is not a new saved field; the existing base metadata and authored delta retain it.        |
| `rTarget`            | `clamp(rawRatio, 0, 1)`, after the ratio clamp but **before** gamut fallback.                                                                               |
| `rRealized`          | Actual stored `C / cmax`, after gamut fallback; exactly `0` when `cmax === 0`. Never divide by zero or substitute the requested ratio.                      |
| `effectiveRatioCmax` | **Deprecated compatibility alias for `rTarget`**, preserving its original meaning. It does not mean realized output and must not be silently reinterpreted. |

C1 has no raw base-plus-delta intent: its ratio metadata only analyzes the exact
input. `rRealized` is its analyzed `C / cmax`; `rTarget` and the old alias hold the
clamped analyzed ratio. None of these numbers regenerates or changes C1.
At zero capacity, both C and `rRealized` are zero, and `zero-cmax` is required.
A derived slot can still retain a positive `rTarget` and authored delta while
its zero-capacity output has `rRealized=0`.

Restore validates the alias/target relationship, target versus the saved raw
ratio, realized ratio versus stored C/Cmax, zero-capacity diagnostics and the
requested upper bound for derived C. Version-2 snapshots require both new fields;
missing, nonfinite or conflicting values reject atomically. This validation
uses saved values only, not a new solve.

Valid **prototype version-1** snapshots remain readable: restore returns a
version-2 detached copy, adding only the ratio metadata. Intent, every committed
color/linear sample, diagnostics, stable IDs and revision remain unchanged.
This metadata-only upgrade does not count as an edit; a subsequent explicit edit
increments the original revision once. Mixed v1/v2 field shapes and unknown
versions reject. This is an internal prototype-format compatibility path, **not**
a native document/schema migration or conversion of legacy ColorDesign data.

### Disconnected-blue acceptance

Existing `auto-chroma.js` enumerates cubic-channel boundary intervals; it does
not assume that gamut feasibility is monotonic in chroma. The final fallback
selects the largest feasible C no greater than `rTarget*cmax`. The current
profile remains `C <= .45`, linear-RGB tolerance `1e-7`, with the existing
8-ulp reconstruction guard; exact-zero-tolerance endpoints are not substituted.

| Coordinates                              |       Cmax | C requested at rTarget=.9 | Resolved C |  rRealized |
| ---------------------------------------- | ---------: | ------------------------: | ---------: | ---------: |
| L=.25, h=264.1°                          | .173150554 |                .155835498 | .149001937 | .860533990 |
| Pure-blue C1 L=.452013718, h≈264.052021° | .313214409 |                .281892968 | .265593354 | .847960205 |

Both cases return `gamut-limited` while `rTarget`/the deprecated alias remain .9
and stored deviations remain unchanged. A request within the second feasible
island remains reachable. These are formal focused regressions; pure-blue C1
still retains its exact literal/alpha and original linear sample.

### Muse relay policy note — not implemented by this patch

Muse relayed a Neutral/Text ΔL-only direction and opaque derived-alpha direction.
They are recorded here for root policy review, **not enforced as defaults** by
this ratio-metadata change. The prototype still allows additive ratio deviations
for any explicitly mapped role and preserves explicit preset alpha, including
zero; the existing recovery/alpha tests remain in force. No product UI/schema,
Neutral/Text restriction or alpha policy is changed here.

## Field and compatibility boundaries

`keyColorSlotToLinear(snapshot, slotId)` reads committed output only. It returns
`ColorF32V1`, linear-sRGB float32 channels, **straight alpha**, plus palette ID,
palette revision, stable slot ID and color-semantics version. Derived float
channels come from resolved OKLCH, **not** from 8-bit display HEX; C1 channels
come from decoding its exact source literal. This is a boundary sample, not Field
interpolation, compositing, Capture, paint-target assignment or an exposed API.

Legacy recipe inputs reject rather than convert. No legacy schema, Absolute
chroma override, Lock, Neutral lightness, alpha, role or reference is removed or
rewritten. No legacy conversion runs on load and no existing document is automatically
linked to this model. Schema58 can explicitly retain an independently validated
receipt as historical metadata; it does not make the recipe a live color owner.
Future active integration still needs an explicit conversion operation and its
own save/reopen/reference acceptance.

## Verification

Run from the repository root:

```sh
node --test desktop/tests/key-color-recipe.test.js
```

Focused tests cover exact C1/alpha, determinism and nonmutation, all four hue
relations, key propagation, isolated individual edits, zero-base recovery,
effective clamps without intent loss, invalid/stale/overflow atomic failure,
achromatic hue/zero-Cmax behavior, preset ID retention/reordering, saved resolved
round trips, linear precision, explicit target/realized ratios, prototype-v1
metadata-only restore and unchanged legacy inputs. Pure tests are not
visual, native, UI, final-preset, generic Field or migration acceptance.
