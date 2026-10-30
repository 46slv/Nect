# Font choice core foundation

This is a bounded Session/API foundation, not a unified font picker UI. Existing
pickers, saved schemas and Windows preparation restrictions remain unchanged.

## Atomic OS Text replacement

Dispatch `replace-os-text-font` with the normal Session/revision envelope and
`{layerId, nodeId, bytes, fileName}` or `{layerId, nodeId, fontId}`. Exactly one
of `bytes` and `fontId` is required. `fontId` identifies an existing static asset
or `default`; bytes must pass the existing static TTF/OTF parser, embedding
policy, asset budgets and document limits. Variable asset replacement requires
the existing prepared-variable workflow and is not added by this command.

The target must be an unlocked, enabled OS Text in an unlocked composition with
no active Text draft. Character-range replacement is unsupported. The command
prepares the selected asset, removes `osFont` only in a private candidate, checks
the final text/layout and serialization, then commits one history entry. It
never dispatches `clear-os-font-text` followed by `adopt-font`. A missing glyph,
embedding restriction, stale revision or unsupported layout leaves the original
OS snapshot, editable text, IDs, connections, settings and history intact.
Undo restores the exact OS state; Redo restores the exact embedded state.

Saved Windows `ligatures:true` becomes explicit `fontFeatures.liga:true` only
where the existing static feature/layout/font contract supports it; otherwise
the whole replacement refuses. `ligatures:false` uses the normal static shaper
default. The regression compares the bundled licensed Latin font's default
output to explicit liga-on and off: legacy/default is equivalent to off, not on.
Thus modern Japanese/vertical OS Text with ligatures-on currently refuses rather
than silently discarding intent; ligatures-off and legacy snapshots can use
supported Japanese/vertical static replacements.
This is feature intent preservation, not identical cross-renderer outlines.

## Picker integration contract

`fontChoiceCatalog` accepts `embedded`, `installed` and modern `windows` rows.
Every adapter supplies its own opaque `id` and display `label`; native rows also
supply the exact `face` descriptor needed for preparation. The helper namespaces
IDs without merging equal labels or claiming byte/native faces are equivalent.
Adapters must retain IDs, not reconstruct a selection from a display name.

`fontChoiceCapability` selects a command route or gives an explicit reason. It
does not read font bytes or call the OS and is not coverage/preparation proof.
The caller still resolves the selected opaque identity, prepares the exact face,
checks request/selection/revision freshness, and dispatches the chosen command.
Do not fall back by outlining, stripping overrides or switching to a same-name
face. `disabledReason` carries adapter-specific discovery limitations.

Modern Windows multiline, vertical, wrap and native axes remain available.
Asset-variable Text, explicit leading, Type on Path, maintained arrangements,
character styles/offsets/transforms/kerning pairs, driven typography and explicit
`fontFeatures.liga` still block the Windows route. Supporting explicit OpenType
features on that route is a separate bridge change, not a reason to delete them.
