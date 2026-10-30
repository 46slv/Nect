# Private font files (development candidate)

This route opens a user-selected TTF, OTF, TTC or OTC in the Windows app without
installing it in Windows or embedding its bytes in a project. Use files that you
are permitted to use. A file format or its embedding flags are not proof of a
license. Original files and their flags are never changed.

## User flow

1. Select editable Text and choose **Open font file…** in its normal Font controls.
2. The native file chooser selects one file. Opening it only grants a capability
   to the current process; it does not change the document or add an Undo entry.
3. Font lists the actual member faces of that file under expandable families.
   Choose a member and Apply. The shared Session validates and commits editable
   Text in one Undo step, retaining existing supported typography settings.
4. Cancel or stale selection/preparation leaves the document unchanged.

Font files are immutable in-memory snapshots for that process. Replacing or
removing the original afterwards does not change the snapshot. A new file with
different contents is a different source, even if its family/style names match.
The process limit is eight files and 256 MiB in total, with at most 128 MiB per
file and 256 actual collection members. No arbitrary path can be supplied by JS.

## Persistence and reopening

Document schema 35 adds the optional closed `osFont.source` record:

    { kind: 'file-native-v1', sha256: '<64 lowercase hex characters>', faceIndex: 0 }

The existing geometry/snapshot versions 2–5 retain their meanings. The source
hash identifies the entire original container, and `faceIndex` is its real
zero-based member index, not a family name, display order or extracted font.
Normal installed fonts have no source field.

Only that authored identity and the existing geometry for the document's actual
text/settings are saved. File paths, font bytes, runtime tokens and private glyph
caches are not saved in project JSON, browser preferences or recovery records.
Older readers reject the extra source field rather than resolving a same-named
installed face.

Reopening keeps the saved appearance. The Font controls mark the file as missing
until **Reattach font file…** explicitly selects the identical container again.
Only matching content and member identity unlock that Text's native editing.
Other document edits and saved geometry/export remain available. There is no
automatic disk search, family-name substitution or silent conversion to paths.

Choosing an installed font from a file-native Text is an explicit source
replacement. A file-native edit that accidentally omits source identity is
rejected, rather than relabelling its geometry as an installed font.

## Native boundary

Rust owns selection, bounded reading, SHA-256 identity, random opaque process
capabilities and immutable snapshots. Acceptance mode uses the same disposable
read confinement as its other file operations. Blocking work stays off the UI
thread. DirectWrite opens the snapshot through its process-private in-memory
loader, selects the exact member index and validates the chosen face descriptor.
The common renderer rejects fallback glyph runs. Nothing registers system-wide.

IPC supplies only metadata, opaque capabilities and prepared geometry. The
returned source identity must match the selected file/member before the Session
admits the result. File-native and installed faces remain separate choices even
when every display descriptor is identical.

## Verification limits

Use synthetic or openly licensed fonts for repository and CI tests. Tests cover
collection members with identical descriptors but different geometry, variable
axes, missing-glyph refusal, snapshot immutability, source validation,
serialization, cancellation and Undo/Redo. A mocked browser adapter does not
prove real DirectWrite rendering. Private supplied fonts never enter public CI.

This is not a claim that every commercial font or collection works. Actual
BBB/A1/supplied OTC compatibility and packaged Windows interaction require
separate real-font evidence. Unsupported outlines/typography fail without
replacing the current authored Text.
