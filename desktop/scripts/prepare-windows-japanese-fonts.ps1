# Diagnostics are read-only by default. CI opts into the single official FOD below.
# This is not proof of the DirectWrite face: the Rust tests keep their exact selector.
[CmdletBinding()]
param(
    [switch]$InstallIfMissing,
    [string]$EvidenceDirectory = (Join-Path $PSScriptRoot '../test-results')
)

$ErrorActionPreference = 'Stop'
$capabilityName = 'Language.Fonts.Jpan~~~und-JPAN~0.0.1.0'
New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
$evidencePath = Join-Path $EvidenceDirectory 'windows-japanese-font-capability.json'
$dismLogPath = Join-Path $EvidenceDirectory 'windows-japanese-font-capability-dism.log'
$evidence = [ordered]@{
    schemaVersion = 1
    commit = $env:GITHUB_SHA
    runId = $env:GITHUB_RUN_ID
    runAttempt = $env:GITHUB_RUN_ATTEMPT
    imageOS = $env:ImageOS
    imageVersion = $env:ImageVersion
    runnerEnvironment = $env:RUNNER_ENVIRONMENT
    osVersion = [System.Environment]::OSVersion.VersionString
    powershellVersion = $PSVersionTable.PSVersion.ToString()
    requiredFace = [ordered]@{ family = 'Meiryo'; weight = 400; style = 0 }
    capabilityName = $capabilityName
    installIfMissing = [bool]$InstallIfMissing
    elevated = $null
    before = @()
    installResult = $null
    after = @()
    status = 'diagnostics-started'
    error = $null
    faceAcceptance = 'NOT RUN; exact DirectWrite selection and shaping remain Rust test obligations'
}

function Write-CapabilityEvidence {
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $evidencePath -Encoding utf8
}

function Read-JapaneseFontCapability {
    # Retain every matching identity/state, not just the first wildcard result.
    @(Get-WindowsCapability -Online -Name 'Language.Fonts.Jpan*' -ErrorAction Stop |
        ForEach-Object { [ordered]@{ name = $_.Name; state = $_.State.ToString() } })
}

Write-CapabilityEvidence
try {
    if (-not $IsWindows) { throw 'Windows capability diagnostics require Windows.' }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $evidence.elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Write-Host 'Querying Get-WindowsCapability -Online -Name Language.Fonts.Jpan*'
    $evidence.before = @(Read-JapaneseFontCapability)
    Write-CapabilityEvidence
    $evidence.before | ConvertTo-Json -Depth 3 | Write-Host
    $target = @($evidence.before | Where-Object { $_.name -ceq $capabilityName })
    if ($target.Count -ne 1) {
        throw "Expected exactly one capability named $capabilityName; found $($target.Count). No installation attempted."
    }
    if ($target[0].state -eq 'Installed') {
        $evidence.after = $evidence.before
        $evidence.status = 'already-installed'
        Write-Host 'Japanese font capability is already Installed; not reinstalling. Exact Meiryo selection still runs in Rust.'
    } elseif ($target[0].state -eq 'NotPresent') {
        if (-not $InstallIfMissing) {
            throw 'Japanese font capability is NotPresent. Diagnostic-only mode does not install it.'
        }
        if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
            throw 'Capability installation is restricted to an ephemeral GitHub-hosted Windows runner.'
        }
        if (-not $evidence.elevated) { throw 'Capability installation requires an elevated runner. No elevation attempted.' }
        # Use the OS-configured official source/Windows Update. No alternate source,
        # policy, registry, credentials, font downloads, or reboot is attempted.
        # The workflow bounds this step to ten minutes and retains failure evidence.
        $evidence.status = 'installation-started'
        Write-CapabilityEvidence
        Write-Host "Installing official capability $capabilityName"
        $result = Add-WindowsCapability -Online -Name $capabilityName -LogPath $dismLogPath -ErrorAction Stop
        $evidence.installResult = [ordered]@{
            online = $result.Online
            restartNeeded = $result.RestartNeeded
        }
        # Check the state even if DISM returns without throwing on source failure.
        $evidence.after = @(Read-JapaneseFontCapability)
        Write-CapabilityEvidence
        $evidence.after | ConvertTo-Json -Depth 3 | Write-Host
        if ($result.RestartNeeded) {
            throw 'Japanese font capability reports RestartNeeded. This job will not reboot the runner.'
        }
        $installed = @($evidence.after | Where-Object { $_.name -ceq $capabilityName -and $_.state -eq 'Installed' })
        if ($installed.Count -ne 1) {
            throw 'Japanese font capability did not become Installed. Inspect the capability JSON and DISM log; no alternate source or retry attempted.'
        }
        $evidence.status = 'installed'
        Write-Host 'Japanese font capability is Installed; exact Meiryo selection and substantive Rust assertions still must pass.'
    } else {
        throw "Japanese font capability state is $($target[0].state), not Installed or NotPresent. No installation or reboot attempted."
    }
} catch {
    $evidence.status = 'failed'
    $evidence.error = $_.ToString()
    throw
} finally {
    Write-CapabilityEvidence
}
