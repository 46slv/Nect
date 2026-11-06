# Sample Field evaluation slice

This internal Phase 2 slice follows the
[Field System Foundation](https://app.notion.com/p/40bfd279a6f3814ea121e16cadd46389).
An expression descriptor, a caller-owned Sample context and the returned runtime
values remain separate. The module is not connected to a node or saved project.

The supported subset is deliberately small:

- A Constant with a finite Scalar or Vector2 scale, or a Boolean selection,
  returns one typed `Uniform` value with the population count. It does not
  allocate one copy per Sample or require Position capability.
- Position returns ordered, copied Vector2 values as `Varying`. The caller
  supplies the population order, explicit frame token, unit and request cap.
  The field's frame token and unit must match the capability exactly; this
  module performs no coordinate conversion.
- Attribute reads the existing materialized `luminance` or `darkness` Scalar
  values as `Varying`. The caller explicitly supplies a Sample-domain source
  whose name, role, type and absent unit match the field. Missing or mismatched
  attributes reject, including for an empty population. This reads values; it
  does not sample paint or images, compute Color, or capture a Field.

The role, unit and token strings used by these internal descriptors are local
to this slice, not a definition of general product Space or unit literals.
Position capability and matching frame/unit are required even for an empty
population. A valid empty Constant has count zero, and a valid empty Position
has an empty value list. Unsupported types, roles, missing capability,
non-finite values and request-budget excess reject without returning partial
runtime values or changing the input.

Each request evaluates the whole supplied population. There is no persistent or
request-level cache here. In particular, a future Position cache must account
for membership and ordering as well as geometry and frame dependencies; a Grid
crop can change membership while its geometry revision stays unchanged.

These tests exercise contexts from the legacy point bridge and the separate
Grid identity fixture. They do not establish generic Field coverage, Color,
Space conversion, ValueSource, project serialization, user-facing nodes,
InstanceSet, or the first product vertical. A future Attribute cache must also
include the relevant source/attribute revision and population/order dependencies.
