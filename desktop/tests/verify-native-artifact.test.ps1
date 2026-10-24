$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Synthetic ZIPs only: the payload is text, never a PE file or a file on disk.
$verifier = Join-Path $PSScriptRoot '../scripts/verify-native-artifact.ps1'
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('nect-artifact-verifier-' + [guid]::NewGuid().ToString('N'))
[void][System.IO.Directory]::CreateDirectory($fixtureRoot)
$manifestEntry = 'test-results/native-acceptance-build.json'
$exeEntry = 'src-tauri/target/release/nect-svg-desktop.exe'
$payload = [System.Text.Encoding]::ASCII.GetBytes('synthetic non-executable payload A')
$commit = '1' * 40
$tree = '2' * 40
$passed = 0

function Assert($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Bytes-Hash([byte[]]$Bytes) {
    return [System.Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
function File-Hash([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        return [System.Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant()
    } finally { $stream.Dispose() }
}
function New-Receipt {
    return [ordered]@{
        commit = $commit
        tree = $tree
        run_id = '123'
        run_attempt = '1'
        native_app_acceptance = 'NOT RUN; compilation only, no installer or release'
        isolation_tests = 'native-isolation-rust.log'
        binaries = @([ordered]@{ name = 'nect-svg-desktop.exe'; bytes = $payload.Length; sha256 = (Bytes-Hash $payload) })
    }
}
function New-Fixture([string]$Name, [hashtable]$Options = @{}) {
    $receipt = New-Receipt
    if ($Options.ContainsKey('Change')) { & $Options.Change $receipt }
    $json = $receipt | ConvertTo-Json -Depth 5 -Compress
    if ($Options.ContainsKey('Json')) { $json = $Options.Json }
    $manifestBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json)
    if ($Options.ContainsKey('ManifestBytes')) { $manifestBytes = $Options.ManifestBytes }
    $binaryBytes = $payload
    if ($Options.ContainsKey('Payload')) { $binaryBytes = $Options.Payload }
    $entries = [System.Collections.Generic.List[object]]::new()
    if (-not $Options.ContainsKey('MissingManifest')) { $entries.Add(@($manifestEntry, $manifestBytes)) }
    if (-not $Options.ContainsKey('MissingExe')) { $entries.Add(@($exeEntry, $binaryBytes)) }
    if ($Options.ContainsKey('DuplicateManifest')) { $entries.Add(@($manifestEntry, $manifestBytes)) }
    if ($Options.ContainsKey('DuplicateExe')) { $entries.Add(@($exeEntry, $binaryBytes)) }
    if ($Options.ContainsKey('ExtraName')) { $entries.Add(@($Options.ExtraName, [byte[]]@(65))) }
    if ($Options.ContainsKey('EmptyEntries')) {
        for ($index = 0; $index -lt $Options.EmptyEntries; $index++) { $entries.Add(@("logs/empty-$index", [byte[]]::new(0))) }
    }
    $compression = [System.IO.Compression.CompressionLevel]::NoCompression
    if ($Options.ContainsKey('Deflate')) { $compression = [System.IO.Compression.CompressionLevel]::Optimal }
    $path = Join-Path $fixtureRoot ($Name + '.zip')
    $file = [System.IO.File]::Open($path, [System.IO.FileMode]::CreateNew)
    $zip = [System.IO.Compression.ZipArchive]::new($file, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in $entries) {
            $entry = $zip.CreateEntry($item[0], $compression)
            $entry.LastWriteTime = [System.DateTimeOffset]::new(2026, 1, 1, 0, 0, 0, [System.TimeSpan]::Zero)
            $stream = $entry.Open()
            try { $stream.Write($item[1], 0, $item[1].Length) } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose(); $file.Dispose() }
    return $path
}
function Set-UInt16([byte[]]$Bytes, [int]$Offset, [uint16]$Value) {
    [System.BitConverter]::GetBytes($Value).CopyTo($Bytes, $Offset)
}
function Set-UInt32([byte[]]$Bytes, [int]$Offset, [uint32]$Value) {
    [System.BitConverter]::GetBytes($Value).CopyTo($Bytes, $Offset)
}
function New-PatchedFixture([string]$Name, [scriptblock]$Patch) {
    $path = New-Fixture $Name
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $end = $bytes.Length - 22
    $directory = [int][System.BitConverter]::ToUInt32($bytes, $end + 16)
    & $Patch $bytes $end $directory
    [System.IO.File]::WriteAllBytes($path, $bytes)
    return $path
}
function Expectations([string]$Path) {
    # Simulates values delivered separately by a trusted successful-run/API channel.
    return @{
        ArchivePath = $Path
        ExpectedArchiveSHA256 = (File-Hash $Path)
        ExpectedArchiveBytes = ([System.IO.FileInfo]::new($Path).Length.ToString())
        ExpectedCheckoutCommit = $commit
        ExpectedCheckoutTree = $tree
        ExpectedRunId = '123'
        ExpectedRunAttempt = '1'
    }
}
function Check([string]$Name, [string]$Path, [string]$Reason, [hashtable]$Arguments = @{}) {
    $before = File-Hash $Path
    if ($Arguments.Count -eq 0) { $Arguments = Expectations $Path }
    $output = @(& $verifier @Arguments)
    $code = $LASTEXITCODE
    Assert ($output.Count -eq 1) "$Name emits exactly one result"
    Assert (-not $output[0].Contains($Path) -and -not $output[0].Contains('private-manifest-marker')) "$Name does not disclose paths/manifest"
    $result = $output[0] | ConvertFrom-Json
    $success = $Reason -ceq 'ARCHIVE_RECEIPT_EXE_MATCH'
    Assert ($result.reason -ceq $Reason) "$Name expected $Reason, observed $($result.reason)"
    Assert ($result.ok -eq $success -and $code -eq $(if ($success) { 0 } else { 1 })) "$Name exit/ok ($code)"
    Assert ($result.nativeUI -ceq 'NOT_RUN' -and $result.signatureVerification -ceq 'NOT_PERFORMED') "$Name acceptance limits"
    Assert ($result.pathSafety -ceq 'BEST_EFFORT_REQUIRES_STABLE_TRUSTED_LOCAL_ANCESTORS') "$Name path-race prerequisite"
    Assert ((File-Hash $Path) -ceq $before) "$Name input remains unchanged"
    if ($success) {
        Assert ($result.exeBytes -eq $payload.Length -and $result.exeSHA256 -ceq (Bytes-Hash $payload)) "$Name stream measurements"
    }
    $script:passed++
    Write-Output "PASS $Name"
}

$valid = New-Fixture 'valid'
Check 'valid' $valid 'ARCHIVE_RECEIPT_EXE_MATCH'
$json = New-Receipt | ConvertTo-Json -Depth 5 -Compress
$path = New-Fixture 'utf8-bom' @{ ManifestBytes = [byte[]](@(0xef, 0xbb, 0xbf) + [System.Text.Encoding]::UTF8.GetBytes($json)) }
Check 'utf8-bom' $path 'ARCHIVE_RECEIPT_EXE_MATCH'
$path = New-Fixture 'manifest-limit' @{ Json = ($json + (' ' * (64KB - [System.Text.Encoding]::UTF8.GetByteCount($json)))) }
Check 'manifest-limit' $path 'ARCHIVE_RECEIPT_EXE_MATCH'
$extra = New-Fixture 'diagnostic-entry' @{ ExtraName = 'test-results/diagnostic.log' }
Check 'diagnostic-entry' $extra 'ARCHIVE_RECEIPT_EXE_MATCH'
$path = New-Fixture 'deflate-unicode' @{ Deflate = $true; ExtraName = 'test-results/診断.log' }
Check 'deflate-unicode' $path 'ARCHIVE_RECEIPT_EXE_MATCH'
$path = New-Fixture 'archive-comment'
$bytes = [System.IO.File]::ReadAllBytes($path)
$comment = [System.Text.Encoding]::ASCII.GetBytes('CI artifact comment')
Set-UInt16 $bytes ($bytes.Length - 2) $comment.Length
[System.IO.File]::WriteAllBytes($path, [byte[]]($bytes + $comment))
Check 'archive-comment' $path 'ARCHIVE_RECEIPT_EXE_MATCH'

foreach ($case in @(
    @('eocd-signature', 'INVALID_ZIP', { param($b, $e, $d) Set-UInt32 $b $e 0 }),
    @('eocd-comment-length', 'INVALID_ZIP', { param($b, $e, $d) Set-UInt16 $b ($e + 20) 1 }),
    @('eocd-disk', 'MULTI_DISK_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($e + 4) 1 }),
    @('eocd-directory-disk', 'MULTI_DISK_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($e + 6) 1 }),
    @('eocd-disk-count', 'MULTI_DISK_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($e + 8) 1 }),
    @('declared-entry-limit', 'TOO_MANY_ENTRIES', { param($b, $e, $d) Set-UInt16 $b ($e + 8) 4097; Set-UInt16 $b ($e + 10) 4097 }),
    @('directory-byte-limit', 'CENTRAL_DIRECTORY_TOO_LARGE', { param($b, $e, $d) Set-UInt32 $b ($e + 12) (4MB + 1) }),
    @('directory-offset-range', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt32 $b ($e + 16) ([uint32]::MaxValue - 1) }),
    @('directory-end-range', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt32 $b ($e + 12) ([System.BitConverter]::ToUInt32($b, $e + 12) - 1) }),
    @('directory-count', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt16 $b ($e + 8) 1; Set-UInt16 $b ($e + 10) 1 }),
    @('directory-signature', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt32 $b $d 0 }),
    @('directory-record-range', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt16 $b ($d + 28) 65535 }),
    @('local-offset-range', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt32 $b ($d + 42) ([uint32]::MaxValue - 1) }),
    @('compressed-length-range', 'CENTRAL_DIRECTORY_INVALID', { param($b, $e, $d) Set-UInt32 $b ($d + 20) ([uint32]::MaxValue - 1) }),
    @('entry-disk', 'MULTI_DISK_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($d + 34) 1 }),
    @('zip64-eocd', 'ZIP64_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($e + 10) 65535 }),
    @('zip64-size', 'ZIP64_UNSUPPORTED', { param($b, $e, $d) Set-UInt32 $b ($d + 24) ([uint32]::MaxValue) }),
    @('zip64-offset', 'ZIP64_UNSUPPORTED', { param($b, $e, $d) Set-UInt32 $b ($e + 16) ([uint32]::MaxValue) }),
    @('zip64-version', 'ZIP64_UNSUPPORTED', { param($b, $e, $d) Set-UInt16 $b ($d + 6) 45 })
)) {
    $path = New-PatchedFixture $case[0] $case[2]
    Check $case[0] $path $case[1]
}
$path = New-Fixture 'truncated-eocd'
$bytes = [System.IO.File]::ReadAllBytes($path)
[System.IO.File]::WriteAllBytes($path, [byte[]]$bytes[0..($bytes.Length - 2)])
Check 'truncated-eocd' $path 'INVALID_ZIP'
$path = New-Fixture 'zip64-locator'
$bytes = [System.IO.File]::ReadAllBytes($path)
$end = $bytes.Length - 22
$locator = [byte[]]::new(20)
Set-UInt32 $locator 0 0x07064b50
[System.IO.File]::WriteAllBytes($path, [byte[]]($bytes[0..($end - 1)] + $locator + $bytes[$end..($bytes.Length - 1)]))
Check 'zip64-locator' $path 'ZIP64_UNSUPPORTED'

foreach ($case in @(@('zip64-extra', 1, 0, 'ZIP64_UNSUPPORTED'), @('bad-extra-length', 10, 1, 'CENTRAL_DIRECTORY_INVALID'))) {
    $path = New-Fixture $case[0]
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $end = $bytes.Length - 22
    $directory = [int][System.BitConverter]::ToUInt32($bytes, $end + 16)
    $insert = $directory + 46 + [System.BitConverter]::ToUInt16($bytes, $directory + 28)
    Assert ([System.BitConverter]::ToUInt16($bytes, $directory + 30) -eq 0) 'extra fixture starts with no extra data'
    Set-UInt16 $bytes ($directory + 30) 4
    Set-UInt32 $bytes ($end + 12) ([System.BitConverter]::ToUInt32($bytes, $end + 12) + 4)
    $extraBytes = [byte[]]::new(4)
    Set-UInt16 $extraBytes 0 $case[1]
    Set-UInt16 $extraBytes 2 $case[2]
    [System.IO.File]::WriteAllBytes($path, [byte[]]($bytes[0..($insert - 1)] + $extraBytes + $bytes[$insert..($bytes.Length - 1)]))
    Check $case[0] $path $case[3]
}
$path = New-Fixture 'entry-count-boundary' @{ EmptyEntries = 4094 }
Check 'entry-count-boundary' $path 'ARCHIVE_RECEIPT_EXE_MATCH'
$path = New-Fixture 'actual-entry-overflow' @{ EmptyEntries = 4095 }
$bytes = [System.IO.File]::ReadAllBytes($path)
Set-UInt16 $bytes ($bytes.Length - 14) 2
Set-UInt16 $bytes ($bytes.Length - 12) 2
[System.IO.File]::WriteAllBytes($path, $bytes)
Check 'actual-entry-overflow' $path 'TOO_MANY_ENTRIES'

foreach ($case in @(
    @('commit', 'COMMIT_MISMATCH', { param($r) $r.commit = '3' * 40 }),
    @('tree', 'TREE_MISMATCH', { param($r) $r.tree = '3' * 40 }),
    @('run', 'RUN_MISMATCH', { param($r) $r.run_id = '124' }),
    @('attempt', 'ATTEMPT_MISMATCH', { param($r) $r.run_attempt = '2' }),
    @('run-type', 'MANIFEST_INVALID', { param($r) $r.run_id = 123 }),
    @('commit-nul', 'COMMIT_MISMATCH', { param($r) $r.commit += [char]0 }),
    @('missing-field', 'MANIFEST_INVALID', { param($r) $r.Remove('tree') }),
    @('unknown-field', 'MANIFEST_INVALID', { param($r) $r.extra = 'private-manifest-marker' }),
    @('acceptance-value', 'MANIFEST_INVALID', { param($r) $r.native_app_acceptance = 'PASS' }),
    @('binary-name', 'MANIFEST_INVALID', { param($r) $r.binaries[0].name = 'other.exe' }),
    @('binary-count', 'MANIFEST_INVALID', { param($r) $r.binaries = @() }),
    @('binary-extra-field', 'MANIFEST_INVALID', { param($r) $r.binaries[0].url = 'private-manifest-marker' }),
    @('binary-length-type', 'MANIFEST_INVALID', { param($r) $r.binaries[0].bytes = '33' }),
    @('binary-hash-syntax', 'MANIFEST_INVALID', { param($r) $r.binaries[0].sha256 = 'Z' * 64 }),
    @('binary-hash', 'EXE_HASH_MISMATCH', { param($r) $r.binaries[0].sha256 = '0' * 64 }),
    @('binary-length', 'EXE_LENGTH_MISMATCH', { param($r) $r.binaries[0].bytes++ })
)) {
    $path = New-Fixture $case[0] @{ Change = $case[2] }
    Check $case[0] $path $case[1]
}
foreach ($case in @(
    @('missing-manifest', 'MissingManifest', 'MANIFEST_MISSING'),
    @('missing-exe', 'MissingExe', 'EXE_MISSING'),
    @('duplicate-manifest', 'DuplicateManifest', 'DUPLICATE_ENTRY'),
    @('duplicate-exe', 'DuplicateExe', 'DUPLICATE_ENTRY')
)) {
    $options = @{}; $options[$case[1]] = $true
    $path = New-Fixture $case[0] $options
    Check $case[0] $path $case[2]
}
foreach ($case in @(
    @('bad-json', '{'),
    @('json-root', '[]'),
    @('duplicate-key', ((New-Receipt | ConvertTo-Json -Depth 5 -Compress).Replace('"commit":', '"commit":"private-manifest-marker","commit":'))),
    @('trailing-comma', ((New-Receipt | ConvertTo-Json -Depth 5 -Compress) -replace '}$', ',}'))
)) {
    $path = New-Fixture $case[0] @{ Json = $case[1] }
    Check $case[0] $path 'MANIFEST_INVALID'
}
$path = New-Fixture 'invalid-utf8' @{ ManifestBytes = [byte[]]@(0xc3, 0x28) }
Check 'invalid-utf8' $path 'MANIFEST_INVALID'
$path = New-Fixture 'oversized-manifest' @{ Json = (' ' * (64KB + 1)) }
Check 'oversized-manifest' $path 'MANIFEST_TOO_LARGE'
foreach ($name in @('../outside', 'test-results\alias', '/rooted', 'a:stream')) {
    $path = New-Fixture ('unsafe-' + $passed) @{ ExtraName = $name }
    Check ('unsafe-' + $passed) $path 'ENTRY_PATH_INVALID'
}
$path = New-Fixture 'case-duplicate' @{ ExtraName = $manifestEntry.ToUpperInvariant() }
Check 'case-duplicate' $path 'DUPLICATE_ENTRY'
$path = Join-Path $fixtureRoot 'invalid-zip.zip'
[System.IO.File]::WriteAllBytes($path, $payload)
Check 'invalid-zip' $path 'INVALID_ZIP'

# Advertise an oversized EXE in the ZIP central directory, without allocating it.
$path = New-Fixture 'oversized-entry'
$zipBytes = [System.IO.File]::ReadAllBytes($path)
$patched = $false
for ($offset = 0; $offset -le $zipBytes.Length - 46; $offset++) {
    if ([System.BitConverter]::ToUInt32($zipBytes, $offset) -ne 0x02014b50) { continue }
    $nameLength = [System.BitConverter]::ToUInt16($zipBytes, $offset + 28)
    $name = [System.Text.Encoding]::UTF8.GetString($zipBytes, $offset + 46, $nameLength)
    if ($name -ceq $exeEntry) {
        [System.BitConverter]::GetBytes([uint32](128MB + 1)).CopyTo($zipBytes, $offset + 24)
        $patched = $true
        break
    }
}
Assert $patched 'oversized entry fixture prepared'
[System.IO.File]::WriteAllBytes($path, $zipBytes)
Check 'oversized-entry' $path 'ENTRY_TOO_LARGE'

foreach ($case in @(
    @('archive-hash', 'ExpectedArchiveSHA256', ('0' * 64), 'ARCHIVE_HASH_MISMATCH'),
    @('archive-size', 'ExpectedArchiveBytes', '1', 'ARCHIVE_LENGTH_MISMATCH'),
    @('invalid-expectation', 'ExpectedRunAttempt', '01', 'INVALID_EXPECTATION'),
    @('newline-expectation', 'ExpectedCheckoutCommit', ($commit + "`n"), 'INVALID_EXPECTATION'),
    @('missing-expectation', 'ExpectedCheckoutTree', $null, 'INVALID_EXPECTATION'),
    @('network-path', 'ArchivePath', '\\not-contacted.invalid\artifact.zip', 'LOCAL_PATH_REQUIRED')
)) {
    $arguments = Expectations $valid
    $arguments[$case[1]] = $case[2]
    Check $case[0] $valid $case[3] $arguments
}
foreach ($required in (Expectations $valid).Keys) {
    $arguments = Expectations $valid
    [void]$arguments.Remove($required)
    $reason = if ($required -ceq 'ArchivePath') { 'INVALID_ARGUMENT' } else { 'INVALID_EXPECTATION' }
    Check ('omitted-' + $required) $valid $reason $arguments
}

# Self-consistent changed receipt+payload cannot replace the independent ZIP hash.
$changedPayload = [System.Text.Encoding]::ASCII.GetBytes('synthetic non-executable payload B')
$path = New-Fixture 'self-asserted' @{
    Payload = $changedPayload
    Change = { param($r) $r.binaries[0].sha256 = Bytes-Hash $changedPayload }
}
$arguments = Expectations $valid
$arguments.ArchivePath = $path
Assert ([System.IO.FileInfo]::new($path).Length -eq [long]$arguments.ExpectedArchiveBytes) 'self-asserted fixture size is unchanged'
Check 'self-asserted' $path 'ARCHIVE_HASH_MISMATCH' $arguments

Assert ([System.IO.Directory]::GetFiles($fixtureRoot, '*.exe', [System.IO.SearchOption]::AllDirectories).Length -eq 0) 'no executable was extracted or written'
Write-Output "PASS $passed synthetic ZIP cases; Native UI NOT_RUN; signature verification NOT_PERFORMED"
exit 0
