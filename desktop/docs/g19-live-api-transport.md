# G19: Normal desktop API and MCP, first transport slice

Status: design candidate. No listener, profile change, or release in this checkpoint.
The [1.0 scope and G19 requirement](https://app.notion.com/p/3e3fd279a6f381b4ba9dd2d1bf066e0f)
remain the product source. The long-term goal is the same authoring operations
from GUI and non-GUI clients, not remote mouse control.

## What the proof does and does not establish

The Windows acceptance instance proves document identity, authored object
discovery, selected Blend and Text edits, history, save receipts and export
on an isolated editor. The installed-face test uses one exact Meiryo Regular
and compares Inspector and MCP edits, retained Text, and the same export
route after both edits. It does not prove an installed production connection,
font selection over MCP, IME, native chooser behavior, or an active Hermes
Desktop session. Keep fixed paths, fixture injection and test commands out
of ordinary builds. Legacy Qt IPC is precedent, not this Tauri backend.

## Authoring coverage checkpoints

Keep command behavior and connection availability separate. A checked fixture
does not put that operation into the ordinary app. Expand the catalog through
shared Session commands and compare GUI outcomes on the same starting state.

| Workflow                 | Verified in the Windows fixture                                                                             | Still needed for the 1.0 goal                                                                          |
| ------------------------ | ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Inspect and discover     | Existing editor identity, bounded layer/node enumeration, stale-target rejection                            | Ordinary-app Inspect with approved exact client, then bounded listing through that same grant          |
| Basic creation           | Rectangle and Text; other supported shape types are listed by the narrow creation contract                  | Verified create/edit/delete and positioning coverage across the 1.0 shape set and layer hierarchy      |
| Text                     | Content, undo/redo, and persistence for the bundled Japanese sample and Meiryo Regular                      | Font/style selection, Typography settings and composition-sensitive editing with the same rules as GUI |
| Blend and graph          | Distance and Steps edits against the shared editor command, with stale/locked/inactive guards where defined | Mode selection, remaining properties, explicit node/port connections and topology edits                |
| Appearance and reuse     | Existing GUI Session commands are the starting point, not proof of API parity                               | Color, effects, macros and shared assets must be usable from the same document model                   |
| History, save and export | Undo/redo, a fixed-path Save As receipt replay, exact copy reopen, SVG/PNG                                  | Ordinary UI destination consent, scoped write permission and disconnection/outcome checks              |

Only the first read-only shipping route is designed below. A mutation grant
would be a separate explicit capability and never implicit in Inspect.

## First product route

- Keep one editor-owned Session and run the typed handlers on its owning
  renderer. GUI and external operations use the same command/history layer.
  No direct renderer eval, blind active-tab edit, or second project copy.
- Windows candidate: a native local named pipe, created only after explicit
  enablement, with an ACL limited to the owner and remote clients rejected.
  Scope authorization to the selected editor instance and client grant. Being
  on localhost or under the same account is not a substitute for the grant.
  Disable and app exit revoke the session grant and close the owned endpoint.
  A contained first implementation should exercise mocked native ownership and
  revocation before enabling a connection to a normal UI.
- A stdio MCP client adapter translates a small stable tool catalog to that
  native endpoint. It does not silently spawn a document or monitor a network
  port. Endpoint or grant material does not belong in prompts, process command
  lines, logs, or project files. Keep access unavailable until the native
  connection and renderer are both ready.
- Start with editor inspection and bounded authored-object listing. They return
  a captured target and necessary metadata only. Confirm target generation,
  composition and revisions again on the edit path. Return a named reason for
  a stale target or disabled connection. A read does not touch selection,
  undo or authored data. Layer positions are cursors within one revision,
  never object IDs.
- Add one edit with the existing shared command, then typed persistence
  operations. Distinguish not dispatched from unknown completion; reconcile
  a write receipt before replay. Preserve GUI destination authority and avoid
  accepting arbitrary output paths in this first slice.

The read-admission prototype checks an owner-issued grant both before
dispatch and when accepting a result for publication. The production
transport must serialize that final check with its connection writer and
purge unpublished results on revocation; returned test data is not proof of
socket or OS authorization.

## Normal renderer handshake (first candidate)

Use a scoped Tauri channel owned by the main editor WebView. Register only
while an explicitly enabled read session exists; an external WebView or a
global event cannot publish a reply. The Rust session owns one in-flight
inspection ID. The renderer captures the existing editor target, then
consumes that capture once only if document generation and revisions still
match. WebView replacement or closing the read session invalidates that ID.
Rust validates the reply shape and the current grant again before delivery.

The isolated Windows acceptance build exercises a real Tauri Channel and the
Rust reply shape. It does not wire the normal channel, exercise interactive
tab changes during publication, or prove client grants on this path. Native
rebind must not revive old requests or let an old renderer close the current
binding. For the normal app, native WebView replacement and explicit disable
must be authoritative; renderer pagehide is only a best-effort signal. A
renderer reply already in transit is not proof that the target is still current.
An edit must independently validate its requested generation and revisions at
dispatch. Keep the first normal-channel verification in-process with a
synthetic document: denied when disabled; accepted when enabled; stale on
switch, reload, or revocation; exact unchanged document and history. An
external transport or Hermes registration is a separate change.

## In-app Windows preview

For development only, build with Vite `VITE_NECT_LIVE_INSPECT_PREVIEW=1`,
acceptance flags unset, and Rust features `custom-protocol,live-inspect-preview`.
In an editor with a disposable project, open Local inspection preview, choose
Enable, then Check document. Expected: Active document confirmed, with no
target metadata returned to the UI and no authored state or history change.
Disable should cancel or hide any pending result. Reload and close revoke
the binding from the native WebView lifecycle. Rerun the check after a fresh
Enable. The preview registers no OS or MCP listener and is off at startup;
the ordinary build omits the preview UI and denies these commands.
This does not demonstrate permission to attach a separate client to an already
running editor.

A hash-pinned disposable preview may additionally opt into
`NECT_LIVE_INSPECT_PREVIEW_STDIO=1`. It requires isolated storage and rejects
the separate live-editor acceptance mode. The launching parent owns the
inherited stdin/stdout handles; no named or discoverable listener is opened.
The host processes one bounded `editor.inspect` frame only, while the local
preview is enabled for the current WebView. Disable or reload invalidates
that binding, including results waiting for publication. Test this with
synthetic data only. This is not a daemon, a general MCP server, or attach
authority for an already running installation. For a local, independent
MCP client check, run the existing acceptance adapter with
`NECT_MCP_PREVIEW_READ_ONLY=1`, the reviewed preview executable and its SHA-256.
The adapter lists only the inspection tool, refuses edits and nonempty
inspection arguments without consuming the proof, and reports a consumed
proof after the first valid request. Enable/disable still happens in the app.
The adapter owns and closes only the disposable process it starts.

## Local attachment development gate

The Windows pipe prototype is compiled in tests and in the explicitly
opt-in Windows inspection preview. Its host-provided proof settings are
accepted only with disposable storage and cannot be combined with either
stdio proof. The preview does not open a pipe at startup. Its chooser
defaults to In-app only. That option can run a local check without opening
the pipe. The user must explicitly select the test client, shown with the
image name and PID of the retained Windows process, and then click Enable.
The native side only accepts its already-retained selection and rechecks
liveness. The selected route binds to the current WebView, then closes
after one request or on timeout. Disable and reload
request cancellation before dropping the renderer binding. It creates one local
pipe restricted to the current logon SID and verifies the expected client PID
before reading bytes. This tests a transport boundary, not a product grant.
It retains a client process handle during authorization and uses overlapped
connect/read/write with a cancellation flag and time budget. The focused
Windows test exercises connect/read timeout, read cancellation, and a fresh
synthetic round trip. The deadline requests OS cancellation; draining the
completion can outlast the budget. This does not prove cancellation of a
blocked write or that a racing write did not complete. A second opt-in test
uses separately launched fixture clients. It confirms that a wrong process
gets no bytes, that the selected process can read one synthetic response, and
that the retained identity is no longer live after client exit.
See [Windows overlapped pipe I/O](https://learn.microsoft.com/en-us/windows/win32/ipc/synchronous-and-overlapped-input-and-output).

An independent test MCP process can now use this proof without starting or
restarting the editor. Set NECT_MCP_PREVIEW_READ_ONLY=1 and
NECT_MCP_LOCAL_PIPE_PROOF to the exact disposable identifier before launching
the adapter, then use its PID in the existing native proof settings. Its only
tool is nect_test_inspect, and one valid call consumes the attempt. Windows
acceptance checked an opened synthetic Blend document: the permitted call
returned the active revision without changing editor state, and a separate
run returned app-unavailable after Disable. It also rejected edit, extra
arguments, and a repeat call. The adapter checks the closed inspect reply
shape and the fixture executable's SHA-256 before serving. On connection it
retains the process identified by Windows and compares the process image
path to the exact fixture path before sending inspection data. The Windows
proof correctly refused a different path, even when its bytes were equal.
This assumes a trusted, dedicated fixture location; it is not a signed
product identity, client registration, or normal-editor integration.

The preview retains the document generation, Composition and Session
selected when Enable was clicked. Revision changes within that scope are
allowed, but a mismatched document or Session is refused and stays refused
until explicit reattachment. A discarded request does not renew the scope.
In the Windows proof, replacing the open project with identical authored
bytes changed its identity and the independent MCP client got no target.
The prior proof for Disable closed the pipe; document mismatch currently
refuses publication, not an immediate OS disconnect.

Before enabling attachment to a running editor, the native owner must bind
client selection to an explicit user decision, preserve per-client grant
expiry, and serialize final publication with revocation. Do not promote
the test-only SID lookup into the app. The ordinary editor does not start
this endpoint or discover client programs. No product profile or installed client is modified.

## First ordinary connection candidate

An already-open, initially disabled editor should present one explicit
approval for the selected local adapter and active document. The approval
establishes a short-lived read-only connection, not a global enable flag.
Show the verified client identity, the document in scope, the allowed
operations, and an immediate disconnect action. Approving a replacement
first revokes the previous connection. A tab change, reload, expiry, exit
of the selected process, or explicit disconnect revokes the grant.

A trusted launcher or OS-mediated selection must provide each expected
process identity before document traffic. A user-supplied name or numeric
PID alone is not a credential. Retain the processes throughout the grant,
check the client and server on the pipe handles before sending or exposing
document data, and fail closed if identity or liveness cannot be checked.
The adapter must not connect by discovery to a same-named endpoint. See
[Windows server-process lookup](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeserverprocessid)
and [Windows client-process lookup](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeclientprocessid).

Start with a fixed packaged Windows MCP stdio helper and an already-open
normal editor. A pairing action gives the helper one short-lived local
endpoint and places one minimal offer in an owner-only pairing folder.
The editor does not start a listener or scan the folder. The user picks one
offer, then the native owner checks the peer and presents read-only consent
for that peer and the active document. Decline, expiry or disconnect ends
the connection; no automated reconnect or durable permission is granted.

Only a strict, bounded versioned rendezvous identifier is decoded from
the chosen offer. The offer cannot name a program, include a path or PID,
set a scope, or confer authority. Keep it out of tool output and logs.
A Windows-only feature test exercises the prospective offer lifetime:
read the process's actual logon SID, draw the rendezvous ID from the
system RNG, create a new file with a logon-only DACL in a fresh owned
directory, and mark it for removal when the handles close. The local
same-logon test reads the exact bounded format, promptly closes that read
handle, verifies attempted replacement is refused, and confirms cleanup.
It does not test a different logon or the production file chooser.
These are fixture results only: the ordinary helper still does not issue
offers, create an endpoint, read credentials, or touch user profiles.

The disposable Windows editor can now display a separate Review pairing
offer action when both inspection and pairing development features are
enabled. The chooser and bounded reader are restricted to that run's
documents directory; the reader handles the writer's sharing flags and
checks the opened path before and after reading. The renderer receives a
format pass/cancel/error, never the ticket or file path. This action does
not enable inspection, open the named pipe, or create a grant. Test
coverage here does not establish the ordinary OS chooser or actual helper
rendezvous end to end.

Use the existing duplex connection for both pairing and the authorized
read, with no document traffic before consent. On each end validate the
kernel-reported process, live handle, and trusted installed package or
signed release identity. A basename, writable hash-pin fixture, same
logon, or token by itself is not sufficient for the ordinary route.

The opt-in nect-mcp-local executable currently starts only the
[MCP stdio protocol](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports).
It advertises one no-argument Inspect tool and returns pairing-required.
Ordinary invocation does not issue an offer or create an endpoint. An
explicit developer-only --verify-pairing-window invocation creates an
idle one-instance local endpoint and its temporary offer in a fresh
fixture directory. A ten-second independent deadline or stdio exit
removes both. The Windows helper process kept serving MCP after the
fixture expired, without returning a document. An ordinary Python reader
could not open the delete-on-close file; the native reader must share
read, write and delete explicitly, read bounded bytes and close promptly.
This mode never dispatches a peer connection, launches an editor or
reads document data; it is not available from an MCP tool request. The
ordinary editor remains the default build and does not package this
candidate. The local development executable is
unsigned, so do not accept it as release identity. An installed pairing
pilot needs an approved trust source, e.g. a verified
[Windows PE signature](https://learn.microsoft.com/en-us/windows/win32/seccrypto/example-c-program--verifying-the-signature-of-a-pe-file)
plus an expected publisher and role, before any document traffic.
The development-only verify-pe-publisher.c checks a Windows trust result
and an independently supplied exact signing-certificate SHA-256. Do not
derive the expected value from the peer being accepted. Its Windows proof
distinguished a known signed development dependency, the wrong certificate,
and the unsigned Nect candidate without network revocation fetches. This
validates the check, not any Nect release identity. An absent release trust
anchor, unavailable trust, wrong role, or unverifiable running image still
means no document may be shared.

A separate development-only Windows acceptance script,
`verify-local-mcp-refusal.py`, runs the unsigned build of
`nect-mcp-local` (Cargo feature `live-mcp-pairing`) and the compiled
publisher probe in a disposable scratch directory. Supply the unsigned
helper, a scratch directory, and the probe in that order:

```powershell
python -B desktop/scripts/verify-local-mcp-refusal.py <unsigned-helper.exe> <scratch-directory> <verify-pe-publisher.exe>
```

The script holds the process that Windows attributes to the connected pipe
and verifies its image with the probe. The connection is local and carries
no app payload. Windows must report no signature, and Inspect must remain
unavailable. Any policy for an installed pairing pilot must independently
bind both publisher and component role to an approved distribution. An
offer, same-logon access, a signed file found on disk, and the test probe
are not substitutes for that policy or for attribution of the live peer.
The disposable GUI's offer review remains format-only.

First acceptance is Inspect once: a standard MCP client can initialize
without sharing any document; a specific native consent in the already-open
editor is needed before one exact synthetic-document identity can be read.
Deny, disable, expiry, peer exit, and switching away and back must not
recover the earlier permission. The helper does not launch the editor.
Defer List until its bounded return schema is verified on the same route.
Do not add edits, arbitrary paths, installed-profile registration, or
persistent permission before the ordinary read-only grant works.

## Acceptance before broader exposure

1. A connection with no grant, a disabled connection, another instance, and a
   revoked/expired grant all fail closed. Reconnect does not restore authority.
2. A real normal Windows instance, with a synthetic open document, accepts
   inspect and list only through an authorized client. Compare identity
   and state with GUI; switch tabs and confirm earlier targets are rejected.
3. Pass an authorized edit through the same command as GUI. Verify exactly
   one history step, Undo/Redo, and no change after stale failure. Test safe
   disconnect before dispatch, during dispatch, and after receipt.
4. Add the approved Desktop MCP registration using a dedicated test profile,
   not a live profile reload or an invisible fallback session. Match GUI
   conversation and target instance. Do not infer this result from stdio tests.
5. Saved/reopened editable state, user-selected installed fonts, actual Windows
   chooser, and SVG/PNG are distinct later gates, not this design's results.
