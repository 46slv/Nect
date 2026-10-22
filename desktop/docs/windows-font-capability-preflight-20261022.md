# Windows Japanese acceptance font prerequisite

## Scope and evidence boundary

Candidate base: `6e3f765a57125141ee82d8f6c127dc2604754d0b`.
The reported Windows CI failure reaches four strict DirectWrite selectors for
`family == "Meiryo" && weight == 400 && style == 0` before their substantive
wrapping, Japanese-boundary, vertical-orientation and font-identity assertions.
Other Japanese outlines do not establish this exact prerequisite.

This candidate changes CI setup and test failure diagnostics only. It does not
change production font enumeration, selection, shaping, layout, test assertions,
dependencies, runner security policy or the accepted font. No font is downloaded
from an arbitrary site or redistributed.

## Prepared CI behavior

`scripts/prepare-windows-japanese-fonts.ps1` is diagnostic-only by default. The
Windows verification workflow opts into installation with `-InstallIfMissing`:

1. Record runner image, OS, PowerShell and elevation details, then query
   `Get-WindowsCapability -Online -Name 'Language.Fonts.Jpan*'`. Persist every
   matching capability name/state before attempting a change.
2. Require exactly one
   `Language.Fonts.Jpan~~~und-JPAN~0.0.1.0` identity. If already `Installed`, do not
   reinstall. If any other state except `NotPresent`, fail visibly without a
   servicing attempt.
3. Only on an elevated, GitHub-hosted Windows runner, install a `NotPresent`
   capability with the official `Add-WindowsCapability -Online -Name ...` path.
   No alternative source, ISO acquisition, policy, registry, credentials,
   elevation or reboot is attempted. The workflow step has a ten-minute timeout
   and no `continue-on-error`.
4. Record the DISM result and re-query capability state. Fail for restart-needed
   or non-Installed results, including acquisition failures that do not throw.
   Capability installation is not reported as exact-face acceptance.
5. Keep every pre-existing compile/test guard unchanged so independent checks
   remain eligible after a preflight failure. All four strict Rust tests still
   run with their original assertions and exact Meiryo prerequisite. On selector
   failure, report every DirectWrite face's family, name, weight, style, stretch,
   format and color fields, including localized family labels as returned.

The always-run artifact step includes
`test-results/windows-japanese-font-capability.json`,
`test-results/windows-japanese-font-capability-dism.log*` and the existing Rust
test log. Evidence is written before a potentially slow installation, so a
timeout can leave `installation-started` rather than a completed result. A
timeout/cancellation is not proof that provisioning completed or rolled back.

## Source basis

- [Microsoft Windows font list](https://learn.microsoft.com/en-us/typography/fonts/windows_10_font_list):
  lists Meiryo in Japanese Supplemental Fonts, rather than establishing that any
  Japanese font or a particular hosted image supplies it.
- [Language and region FOD](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/features-on-demand-language-fod?view=windows-11):
  gives the Japanese font capability identity used above.
- [Features on Demand](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/features-on-demand-v2--capabilities?view=windows-11):
  documents online Windows Update acquisition and matching Server 2022 media if
  offline media were needed. This candidate does not acquire such media.
- [Add-WindowsCapability](https://learn.microsoft.com/en-us/powershell/module/dism/add-windowscapability?view=windowsserver2022-ps):
  documents supported parameters, official source resolution and possible silent
  acquisition failure. It does not document a `-NoRestart` parameter; none is
  supplied.
- [Runner image manifest](https://github.com/actions/runner-images/blob/win22/20261012.333/images/windows/Windows2022-Readme.md):
  identifies Server 2022 image `20261012.333.1`; it does not prove this capability
  is present or installable in the next job.
- [GitHub runner variables](https://docs.github.com/en/actions/reference/workflows-and-actions/variables):
  identifies `RUNNER_ENVIRONMENT=github-hosted` for the installation guard.

## Local validation receipt

- `actionlint .github/workflows/verify-windows.yml`: passed.
- PowerShell tree-sitter syntax parse: passed with no error or missing nodes.
- Complete Rust file tree-sitter syntax parse: passed with no errors.
- Focused source-preservation checks: passed. Production Rust bytes are
  unchanged; only the shared diagnostic helper and four exact-selector call
  sites changed. Existing substantive test bodies/assertions and every
  pre-existing workflow step/guard remain unchanged, apart from adding the two
  evidence artifact paths.
- Script command/branch checks: passed. The only servicing command is in the
  `NotPresent` branch, after opt-in, hosted-Windows and elevation checks, followed
  by post-install state/restart checks. No reboot, policy or download command is
  present. Eight focused static checks passed in total.
- Receipt Markdown Prettier check: passed.
- `git diff --check`: passed.

PowerShell/DISM execution, Rust compilation, rustfmt and Windows font selection
were **not run locally** because this Linux executor has no PowerShell or Rust
toolchain. The syntax parse is not a PowerShell runtime test. Hosted-runner
privileges, official source availability, timeout/restart behavior and all
substantive native tests remain **unverified until the next actual Windows CI**.
This is an isolated review candidate, not a pushed change or a Windows pass.
