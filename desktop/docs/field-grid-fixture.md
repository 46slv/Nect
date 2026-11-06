# Grid identity fixture

This Phase 1 test fixture exercises the identity and revision part of accepted
[DEC-82](https://app.notion.com/p/40bfd279a6f381c59012fd12b01c5d9b). It is not a
public Grid producer or a project serialization format.

The caller supplies a bounded half-open integer lattice window, an opaque frame
token, finite origin and degree angle, and positive finite spacing. A
fixture-only set of integer coordinates can model membership filtering; it is
not geometric clipping. The frame token is retained without interpretation or
conversion. The fixture defines no general Space literals, frame transforms,
world-bounds inversion, shape clipping, or sample-footprint boundary.

Rectangular-v1 sample identities derive from namespace and integer `(i, j)`;
the fixed order is `j` major, then `i`. Position is
`origin + i × basisX + j × basisY`, so `(0, 0)` is the supplied origin. Basis,
cell area and typed grid indices are provided for tests. This differs from the
existing Luminance grid, which samples cell centers at `i + 0.5, j + 0.5`.

For a fixed lattice window, changing origin, spacing, angle or frame token keeps
IDs and membership while changing the geometry descriptor. Window or fixture
mask changes alter membership while preserving surviving IDs, positions, basis
and ordering rule. These claims do not extend to future world-space bounds.
Invalid/reversed windows, unsafe integers, non-finite or unrepresentable
geometry and candidate counts above this fixture's 4096 limit reject before
Samples are materialized. The fixture borrows that limit from the existing
Luminance grid; DEC-82 does not prescribe it as a product default. Zero-span
windows and empty masks are valid.

The tests include JSON readback of a fixture descriptor to check deterministic
regeneration. This does not establish project Save/Open, a full Grid Samples
implementation, or the later Field and Instance workflows.
