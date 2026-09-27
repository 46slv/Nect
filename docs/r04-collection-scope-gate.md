# R04 Collection target and membership scope — local DESIGN_GATE

Status: local design gate, 2026-09-27. Baseline `codex/practical-alpha@dbd171092fe6fb8fb50fafad3a59e4c0537d78a6` (clean local/tracking/fresh remote at intake). This gate applies only to the Collection target edge of R04/P04-D; it does not stop independent Route work.

## Authority and observed boundary

Confirmed [REQ-36](https://app.notion.com/p/3e0fd279a6f3815bbbaed3bddf04d3f5) asks for reusable batch rename, sort and group/folder organization commands callable beyond manual UI. [P04-D](https://app.notion.com/p/3e3fd279a6f381ee8ab0ea5ea614c006) asks that a batch target be explicit as selection, Folder or Collection and that old/new names, order and references be previewed before one atomic application. Existing selection batch rename, sibling paint-order sort and previewed Folder creation satisfy bounded parts of that contract; they do not establish Collection scope.

The current `Document::collections` model is an ordered, non-owning ID set. Validation rejects missing or duplicate members; native 0.23 reads/writes `collections`; History can retain changed Collection vectors. The Session `Command` variant has no create/rename/delete Collection or membership-edit command. Desktop exposes no Collection target picker. Existing JSON-lines commands can batch `Rename` and reorder siblings but cannot author Collection membership. Tests using pre-authored Collection fixtures establish preservation during other edits, not membership authoring.

[REQ-56](https://app.notion.com/p/3e0fd279a6f381c1ad16c2f697578397), [REQ-57](https://app.notion.com/p/3e0fd279a6f38139800cee233803ebfe), [REQ-58](https://app.notion.com/p/3e0fd279a6f3813a92b8ccf9f8d8e650) and [REQ-59](https://app.notion.com/p/3e0fd279a6f381f9ae05cc429e7c8675) remain Candidate, even where their Level is Must. [Collection operation scope](https://app.notion.com/p/3e0fd279a6f38107a094f4a50b7115c6) is Open, and the unified named-set/graph-source [DEC-29](https://app.notion.com/p/3e0fd279a6f381f0afa3ef27be017f98) is Proposed. Neither P04-D's mention of Collection nor the persisted model adopts those requirements.

## Gate and resume trigger

The undecided contract is whether a Collection is a reusable target for rename only, paint-order sort, grouping, or broader edits; whether members may cross parents or Compositions for each operation; and how a user creates/changes the target set. Sort/group would change structural paint order or ownership and require additional appearance and coordinate rules. A Collection target cannot be equated to contiguous siblings or silently flattened into selection.

Resume this edge when the accepted product scope explicitly selects the supported Collection operations and authoring path, or when a narrower Confirmed packet is established using only a pre-existing Collection as a read-only source of IDs. For that narrower packet, freeze exact membership/revision, reject absent/duplicate/stale IDs atomically, specify same-parent/Composition limits per operation, preview stable IDs and before/after names/order, apply one Session batch, and verify one Undo/Redo, native cold reopen, JSON-lines/API and Desktop target choice. Do not claim REQ-56–59 acceptance from a REQ-36 batch tool.

The general appearance-changing reparent gate and R04 collapse/expand hands-on `LOCAL_WAIT` remain separate. No Collection product code or native schema changed in this checkpoint.
