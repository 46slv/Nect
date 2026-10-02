# Windows verification receipt

Date: 2026-10-02 UTC

Worker request: GPT-6 Luna Max (the runtime model label is not exposed here, so selection was not independently verified).

## Result

| Check | Status | Evidence |
|---|---|---|
| Full Windows x64 Release build | **PASS** | All Release targets compiled and Qt deployment completed. |
| Corrected full Release CTest run | **FAIL** | 72 tests: 71 passed, 0 skipped, 1 failed (`folder_library_contract`). |
| Qt interaction coverage for the new Template, Guide, and Grid controls | **PASS** | `template_ui_interaction`, `template_frame_ui_interaction`, `template_layout_ui_interaction`, `artboard_guide_alignment_ui_interaction`, `inherited_grid_alignment_ui_interaction`, `scoped_guide_drag_ui_interaction`, and `scoped_guide_drag_interaction` passed. |
| Automated host and MCP-named contract tests | **PASS** | `desktop_host_contract` and all six `mcp_*_contract` tests passed. These test results do not establish external formal MCP or host acceptance. |
| Native CUA visual review, high-DPI review, and subjective acceptance | **NOT_RUN** | The execution tool inventory has no `node_repl`/`@oai/sky` entry point or screenshot, mouse, or keyboard controls. Automated Qt interaction tests above did run. |
| Codex project-selector inspection | **NOT_RUN** | No project/thread UI inspection was possible with the available CUA. The absence of project/thread API tools does not establish whether the desktop selector is available. No successor task or thread was created. |
| External formal Host/MCP acceptance | **NOT_RUN** | No external host session was available to verify. The core JSON-lines service was not treated as formal MCP. |
| Product-code repair | **NOT_NEEDED** | No Windows product defect was isolated. The remaining CTest failure is an environment registry-access denial, documented below. |

## Source and branch provenance

- The required remote base was freshly checked before work: `bfa0ffc794bb6562dac12f4de158c1bcfcae7a43`, tree `59e0f1a33db0c172b2669726df7f938914c4b541`, on `dot/d0-cloud-canary`.
- Work began in the isolated task-owned checkout on `dot/windows-lunamax-20261001`, at that exact base. The protected `codex/practical-alpha` checkout was read-only and remained at `01a909097ba222db0340e1c15cdeba6018f445ff`.
- Before configuring, Dot supplied the newer accepted test-only checkpoint `bba27ee8c5b2e7def64310f9b8efdd7aeb48e04b`, tree `c47824d142b28cefe64a4749506d0251a128abd0`. The isolated branch was fast-forwarded cleanly to it and the Windows runs below used this exact source. Relative to the originally specified base, that checkpoint changes `CMakeLists.txt`, `CURRENT_GOAL.md`, adds `docs/r12-duplication-remap-slice.md`, and changes `tests/duplication_tests.cpp`; it adds the Duplication test-only mode.
- No production source was changed by this worker. `CMakeLists.txt`, `tests/duplication_tests.cpp`, `CURRENT_GOAL.md`, `include/nect/core.hpp`, and `src/core.cpp` were not edited. In particular, the new primitive Inspector / gesture-generation proposal remains Dot's source work.
- The candidate branch contains this receipt only beyond the tested checkpoint. The exact receipt commit and resulting tree are reported with the handoff; a Git object cannot contain its own commit ID. No merge was performed.

## Windows toolchain and commands

Runtime observed: Windows build `10.0.26200.9457`; Visual Studio Community 2019 `16.11.6`; MSVC x64 `19.29.30137.0` (toolset `14.29.30133`); MSBuild `16.11.2.50704`; Qt `6.5.3.0`; Boost `1.85.0`; CMake and CTest `3.31.12`; native Windows Python `3.12.11` (`win32`, `nt`). CMake selected Windows SDK `10.0.16299.0` while targeting `10.0.26200`.

CMake was not installed in the existing Windows environment. With explicit user authorization, the official portable Kitware CMake 3.31.12 archive was downloaded into the task-owned workspace, checked against Kitware's SHA-256 manifest, and unpacked there. The archive SHA-256 is `0c4baa40f28b3f8225eb3fdf6946c987b4fe901403b4eaf2fbbd9378100aaa0c` (46,666,397 bytes). No global PATH, application setting, credential, or system installation was changed. The build used the existing read-only dependency roots:

- Boost: `D:\Documents\Nect\build\deps\boost_1_85_0`
- Qt: `D:\Documents\Nect\build\deps\6.5.3\msvc2019_64`

Documented full-path command, run first with the default CMake Python selection and again after configuring the cache to native Windows Python:

```powershell
& .\scripts\build-windows.ps1 -BoostRoot 'D:\Documents\Nect\build\deps\boost_1_85_0' -QtRoot 'D:\Documents\Nect\build\deps\6.5.3\msvc2019_64'
```

This script configures the Visual Studio 16 2019 x64 generator, builds every Release target, runs `windeployqt`, and runs the full Release CTest suite with failure output and a 60-second test timeout. The second run set `Python3_EXECUTABLE=C:\msys64\mingw64\bin\python.exe` in the task-owned build cache and used process-local `MSBUILDDISABLENODEREUSE=1`.

The first complete run built and deployed successfully but selected MSYS/POSIX Python. CTest reported 62 passed, 0 skipped, and 10 failed: the registry test below plus nine Python-backed tests that could not open Windows drive-letter script paths. After selecting native Windows Python, all nine Python-backed tests passed. No test assertion or source script was weakened.

## Corrected aggregate result and remaining failure

The corrected run built every Release target and deployed Qt successfully. CTest ran all 72 tests: **71 PASS, 0 SKIP, 1 FAIL**. The sole failure was test 36, `folder_library_contract`, at `Create an exact owned NativeFormat scratch key`.

A separate diagnostic attempted to create and remove a unique scratch subkey beneath the same `HKCU\Software\Nect\Tests` registry tree. The write returned `Access denied`. The task-owned scratch key and empty parent keys created by the diagnostic were removed. No ACL, registry security setting, or product code was changed. The failed test remains a **FAIL**; it was not reclassified as skipped or passed. This environment evidence points to the current Windows registry permission boundary rather than a demonstrated product defect.

The following relevant tests passed in the corrected run:

- Template: `template_duplicate_contract`, `template_ui_interaction`, `template_frame_ui_interaction`, `template_layout_ui_interaction`, and `template_json_contract`.
- Guide and Grid: `artboard_guide_alignment_contract`, `artboard_guide_alignment_ui_interaction`, `inherited_grid_alignment_contract`, `inherited_grid_alignment_ui_interaction`, `scoped_guide_drag_ui_interaction`, and `scoped_guide_drag_interaction`.
- Desktop and host: `desktop_host_contract`, `window_interaction`, and `canvas_interaction`.
- MCP-named automation tests: `mcp_desktop_contract`, `mcp_preset_contract`, `mcp_macro_contract`, `mcp_definition_contract`, `mcp_template_contract`, and `mcp_collection_contract`.

The delegation's own-cloud bba reference was 45 pass, 1 skip, and 26 fail. That result is included as owner-provided context only; it was not rerun or independently inspected by this worker.

## Fixtures and evidence logs

Both CP2 SVG replay JSON fixtures passed the exact-byte, pinned-hash, strict UTF-8, JSON parse, no-BOM, CR-free, final-LF, and `.gitattributes` newline-protection checks:

- `tests/fixtures/nect-stroke-cp2-cases-r1.json`: 5,597 bytes; SHA-256 `4b23b6f719a335155164786ff3738d277bd9e5ad2908e1b6870a33fa328dd1c1`; Git blob SHA-1 `92c47e1be72d272ce9ffba20dd0dcbe921a90494`.
- `tests/fixtures/nect-stroke-cp2-fixture-supplement-r2.json`: 5,877 bytes; SHA-256 `fc586ecb77076b50d23933bf185ee679f409bea87503017c59ceac166e4e65ee`; Git blob SHA-1 `ecca8778199f0fc2738f94b79c93a1e8809b0c5b`.

Raw logs are in the ignored, task-owned `build/d0-evidence/` directory and are not part of the source diff. SHA-256 values:

| Log | SHA-256 |
|---|---|
| `windows-release-native-python.log` — corrected build, deploy, and CTest | `2047E5801FBF988F59B4F6BAB6A45C6E78963541274BD78EFCA2E59254F96878` |
| `windows-release-all.log` — first full run using MSYS/POSIX Python | `5E6D62637C58B05BFA19414D3EAFD7EA86954FD008F426793BA83E7C1FC217CA` |
| `windows-registry-diagnostic.log` — scratch-key access denial and cleanup | `7E19BD6EA36527AD77BD4512231A1B99152AE5F99D0944926530CC5680A3C42E` |
| `windows-fixtures.log` — CP2 fixture byte and hash checks | `555F91783D281CA746893030D9794A9E237057482D1AA5370059347247E9E212` |
| `windows-tool-setup.log` — portable CMake archive verification | `6C3E7D2306E296EFC1A51D17FF4F2D83491FBC51293D0C2B9D7BF5EE590262BD` |
| `windows-build-attempt.log` — initial toolchain preflight and blocked attempt | `C3AFA0C364D17A847B626FBBE29231DB58F331734410E09167432F92E3304328` |

## Handoff and writer release

Direct `git ls-remote` could not connect to `github.com:443` in this execution environment. The connected GitHub repository API is available and reports push permission; it will be used to create a new non-force candidate branch from the tested checkpoint. Exact remote readback is recorded in the final handoff. No force push, merge, release, or publication occurred. At handoff, no active CMake, CTest, MSBuild, compiler, linker, or Nect build/test process remained. Source and build writers are released to Dot.
