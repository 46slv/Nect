# Recovery retry after document switching

Base: PR73 HEAD 5c9f5f0192f967aa9d5572ede4d49c8af1b60e24.
Current user-selected repair: retain a failed document's latest unsaved recovery
context across document switching, identify it in the UI and explicitly retry
that document. Existing manual project saving and on-disk Recovery formats,
current/previous retention and restore semantics stay unchanged.

The baseline UI reproduction now succeeds after replacing its dropped
requestAnimationFrame callbacks with the real HappyDOM frame scheduler. This
was a fixture problem, not a New-document product regression. With the existing
main.js handlers, A's held write fails after New creates B; the UI reports B's
saved state and cannot retry A. Baseline evidence is outside Git in
recovery-failed-retry-20261022/diagnostic and evidence/ui-baseline-diagnostic.log.

Use the existing serialized RecoveryAutosave queue. Keep failed contexts only
for this running window, with a stable transient retry identity, generation and
failed/latest revisions. Clear them after a successful write covers the failed
revision. Retry sends the latest contents of the explicit context, joins any
already queued/in-flight write and rejects missing/resolved/stale targets.
Never replay an old captured payload. Older generation/revision observations
cannot roll the current context back. No automatic failure retry or second
persistence engine.

UI contract (Operate): existing working surface/Project menu stays in place.
Current recovery status stays separate. A compact failure selector names the
failed document and failed/saved revision; Retry names its selected document.
Pending retries disable the action. State updates preserve a still-valid target.
No modal, explanatory panel or new authored data.

Acceptance: A failure around B switch, both A/B failing, later A edit/success,
multiple switches, absent/stale targets and duplicate retries. Exact latest
contents stay under each original native token; other document/Undo state is
unchanged. Focused scheduler/DOM plus production-CSP UI checks use only synthetic
IPC/storage. Build on D with existing dependencies and isolated TEMP/cache.
Native/physical OS input and real disk-failure acceptance are distinct.
No normal user document/Recovery, main merge, release or Library upload.
