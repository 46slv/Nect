# Isolated native acceptance build

This prepares a disposable Windows run. It does not establish native WebView2, dialog, IME or editor
acceptance. CI never launches the application or installer for these storage tests.

## Build provenance

The Windows workflow builds with `--no-bundle` and uploads one standalone executable, Cargo.lock,
logs and `test-results/native-acceptance-build.json` in the existing verification artifact. The receipt
records the actual checkout commit/tree, run ID/attempt and executable size/SHA-256. For pull requests,
the checkout can be a synthetic merge commit: verify its actual tree separately from the PR head tree.

Use `scripts/verify-native-artifact.ps1` with PowerShell 7 on Windows before any separately authorized
native run. Supply every expected value independently from the trusted **successful** run and artifact
API, including the ZIP digest/size. Values copied only from inside the ZIP cannot authenticate it.
The SHA-256 argument is 64 lowercase hexadecimal characters without the API's `sha256:` prefix.
From `desktop/`:

```powershell
pwsh -NoProfile -NonInteractive -File scripts/verify-native-artifact.ps1 `
  -ArchivePath $localZip -ExpectedArchiveSHA256 $apiArchiveSha256 `
  -ExpectedArchiveBytes $apiArchiveBytes -ExpectedCheckoutCommit $actualCheckoutCommit `
  -ExpectedCheckoutTree $actualCheckoutTree -ExpectedRunId $runId -ExpectedRunAttempt $runAttempt
```

The workflow's common upload root is `desktop/`, so the exact ZIP entries are
`test-results/native-acceptance-build.json` and `src-tauri/target/release/nect-svg-desktop.exe`, without
a `desktop/` prefix. The verifier checks the whole archive, strict receipt fields and EXE stream
length/hash. It rejects duplicate/missing entries, unsafe paths, malformed JSON and oversized input
(512 MiB archive, 128 MiB per entry, 64 KiB receipt, 4096 entries). Before constructing `ZipArchive`,
it inspects the same open stream's EOCD and central directory, caps directory bytes at 4 MiB, counts
actual records and checks offset/length ranges. ZIP64, multi-disk and inconsistent directory records
are unsupported and rejected. Ordinary single-disk ZIP32 CI archives remain supported.

Run only in a trusted local working directory where other processes do not replace the archive or
its ancestors between checking and opening it. Fixed/RAM drive, UNC, reparse-point and offline-file
checks are best-effort preflight. They do not prevent a filesystem race from redirecting OS path
resolution to a network location. No race-proof network isolation is provided. The output's
`pathSafety: BEST_EFFORT_REQUIRES_STABLE_TRUSTED_LOCAL_ANCESTORS` records this prerequisite.

The verifier reads streams only, without extraction, EXE writes/execution or download, and does not
call communication APIs itself; this differs from the OS path-resolution race above. Its small JSON
result and exit code (0 matched, 1 rejected) always report native UI `NOT_RUN` and signature
verification `NOT_PERFORMED`. A match is neither signature verification nor native acceptance. The
existing launch script does not check CI receipts; a later authorized launch must also bind the
extracted EXE's size/hash to the verified stream measurements. A rebuild needs new provenance.

Focused tests use synthetic non-executable payloads and standard PowerShell/.NET only:

```powershell
pwsh -NoProfile -NonInteractive -File tests/verify-native-artifact.test.ps1
```

## Process-local contract

Normal launches with `NECT_SVG_ACCEPTANCE_ROOT` absent retain the ordinary window and file adapter.
An empty value is an invalid opt-in, not a request for normal mode.

For an authorized future test, copy the verified standalone EXE into a new directory within the task
workspace. Supply `NECT_SVG_ACCEPTANCE_ROOT` only in that child process's environment. Its value must
be an absolute local drive path to a **nonexistent direct child of the EXE directory**, with a leaf
name beginning `nect-svg-acceptance-` followed by a run ID. For example:

```text
EXE:  C:\task\native-bin\nect-svg-desktop.exe
ROOT: C:\task\native-bin\nect-svg-acceptance-20261016-001
```

The EXE directory and all ancestors must already exist as regular directories without reparse points.
UNC, device/verbatim, drive-relative, slash-separated, dot/traversal, duplicate-separator, reserved
device, alternate-stream and trailing-dot/space spellings are rejected. Existing roots, including empty
ones, are rejected. No ancestor is created. Invalid input, an unsupported platform, an ambiguous window
configuration or pre-Tauri storage preparation failure exits with code 2 before `Builder::run`. A
partially created fresh run directory may remain for inspection; it is never replaced with the normal
profile. Later Tauri/window creation failures terminate startup without a normal-profile fallback;
their exit status follows Tauri's existing panic/error behavior.
Directory identity uses exact canonical paths, without case folding, so distinct case-sensitive NTFS
directories cannot compare equal. Ambiguous aliases can conservatively reject.

The application creates only these two children of the fresh run root:

```text
webview2\   WebView2 profile, localStorage/cache and other browser data
documents\  disposable JSON, SVG and PNG files
```

The acceptance startup disables automatic window creation before `Builder::run`. It then creates the
single configured window manually with an explicit absolute `data_directory`. This avoids Tauri
2.9.0's earlier default-directory creation. Before starting Tauri threads, it also assigns
`WEBVIEW2_USER_DATA_FOLDER` to the same directory in this process, replacing any inherited value and
taking precedence over a WebView2 registry UDF override. It does not alter user/machine environment
variables, registry values, permissions, CSP, browser arguments or security features.

When launching later, a .NET `ProcessStartInfo` with `UseShellExecute = false` and an entry in its
`Environment` dictionary scopes the opt-in to the new process and its children. Do not use `setx`,
change Windows environment settings, or launch an installed copy. Record the launched PID and close
only that instance. Screen tools must be working before this native acceptance run is authorized.

## Application write audit and limits

| Code path                       | Acceptance behavior                                                                                                                                                                                                                     |
| ------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `RunStorage::prepare`           | Exclusively creates a new root and its two children beside the isolated EXE.                                                                                                                                                            |
| Tauri window preparation        | Automatic creation is disabled; the manual builder receives the validated directory before it prepares the window.                                                                                                                      |
| WebView2                        | Process-local UDF override and builder directory agree; each fresh root keeps a separate browser session.                                                                                                                               |
| Native JSON/SVG/PNG Save        | Dialog starts in `documents`. Both staging and atomic replacement are confined to regular files directly in that directory, with checks before staging and before persistence. Outside paths, nested folders and reparse points reject. |
| Native JSON Open                | Limited to regular files directly in this run's `documents`; the existing 50 MB bound remains.                                                                                                                                          |
| Dialog cancellation             | Returns before reading, staging or writing a document. Actual native cancellation behavior still needs screen-based acceptance.                                                                                                         |
| Frontend `src/storage.js`       | Tauri JSON/SVG/PNG exports use the guarded native commands; browser download fallback is unchanged. No frontend localStorage/sessionStorage/IndexedDB write calls exist in this source.                                                 |
| `src-tauri/src/system_fonts.rs` | Production code only reads bounded installed font files; the catalog and extracted bytes stay in memory. Its temporary/extracted-font writes are under `cfg(test)` only.                                                                |
| Other production storage        | No logger, store, updater or shell plugin is configured; no additional application-owned disk writer exists in this source.                                                                                                             |

This is storage confinement for a cooperative disposable test, not an OS sandbox. The installed
WebView2 runtime, system font reads and Windows-managed dialog/runtime state remain outside the run
folder. Other software must not rename or replace run directories concurrently: reparse checks do not
provide handle-based protection against a hostile filesystem race. Native file inputs for manual font,
image or SVG import remain read-only selection paths; use only disposable fixtures. Do not claim that
Windows itself makes no external cache, history, log or temporary writes.

Keep the generated profile and exports as evidence. A later process must use a different fresh root;
copy only disposable project fixtures into its `documents` directory to test reopening. Do not reuse a
prior run root or copy a user's normal WebView2 profile. No cleanup, installation, signing, association
change, remote debugging or security-warning bypass is included.

## Verification and resume gate

Rust tests cover normal-mode preservation, ambiguous startup configuration, invalid Windows paths,
fresh-root creation, existing-root/wrong-parent rejection, actual transactional native-adapter
roundtrips, outside/traversal/stream/subdirectory refusal and junction replacement without changing
the outside sentinel. These tests do not create a Tauri window or invoke an OS file dialog. The full
existing JavaScript/Chrome/CSP/font/build suite remains separate evidence.

Resume the [native checklist](windows-acceptance.md) only with working screen/app tools, a verified
standalone artifact, the already-installed WebView2 runtime and an authorized disposable test folder.
Confirm the actual WebView2 profile location and preserve screenshots and JSON/SVG/PNG results.
Stop at any permission/security prompt. Native interaction, IME and dialog durability remain unverified
until that run occurs.

References: [Tauri 2.9.0 default-directory preparation](https://github.com/tauri-apps/tauri/blob/tauri-v2.9.0/crates/tauri/src/manager/webview.rs#L505-L523),
[explicit builder directory](https://github.com/tauri-apps/tauri/blob/tauri-v2.9.0/crates/tauri/src/webview/webview_window.rs#L1020-L1024),
[Microsoft WebView2 environment overrides](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/webview2-idl#CreateCoreWebView2EnvironmentWithOptions),
[process environment scope](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.processstartinfo.environment).
