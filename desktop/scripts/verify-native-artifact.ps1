# Required values are checked below, avoiding interactive parameter prompts.
param(
    [object]$ArchivePath,
    [object]$ExpectedArchiveSHA256,
    [object]$ExpectedArchiveBytes,
    [object]$ExpectedCheckoutCommit,
    [object]$ExpectedCheckoutTree,
    [object]$ExpectedRunId,
    [object]$ExpectedRunAttempt
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# verify-windows.yml uploads paths with desktop/ as their common archive root.
$manifestPath = 'test-results/native-acceptance-build.json'
$exePath = 'src-tauri/target/release/nect-svg-desktop.exe'
$maxArchiveBytes = 512MB
$maxEntryBytes = 128MB
$maxManifestBytes = 64KB
$maxEntries = 4096
$maxDirectoryBytes = 4MB

function Reject([string]$Reason) {
    $verificationError = [System.IO.InvalidDataException]::new()
    $verificationError.Data['NativeArtifactReason'] = $Reason
    throw $verificationError
}
function Exact-String([string]$Actual, [string]$Expected) {
    return [string]::Equals($Actual, $Expected, [System.StringComparison]::Ordinal)
}
function Positive-Decimal($Value) {
    if ($Value -is [int] -or $Value -is [long]) {
        $text = $Value.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    } elseif ($Value -is [string]) { $text = $Value } else { Reject 'INVALID_EXPECTATION' }
    [long]$number = 0
    if ($text -cnotmatch '\A[1-9][0-9]*\z' -or -not [long]::TryParse($text, [System.Globalization.NumberStyles]::None, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$number)) {
        Reject 'INVALID_EXPECTATION'
    }
    return $text
}
function Stream-Digest($Stream, [long]$Limit) {
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        $buffer = [byte[]]::new(65536)
        [long]$total = 0
        while (($count = $Stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $total += $count
            if ($total -gt $Limit) { Reject 'ENTRY_TOO_LARGE' }
            [void]$hash.TransformBlock($buffer, 0, $count, $null, 0)
        }
        [void]$hash.TransformFinalBlock([byte[]]::new(0), 0, 0)
        return [pscustomobject]@{ Bytes = $total; SHA256 = [System.BitConverter]::ToString($hash.Hash).Replace('-', '').ToLowerInvariant() }
    } finally { $hash.Dispose() }
}
function Read-Exactly($Stream, [int]$Length) {
    $buffer = [byte[]]::new($Length)
    $offset = 0
    while ($offset -lt $Length) {
        $count = $Stream.Read($buffer, $offset, $Length - $offset)
        if ($count -eq 0) { Reject 'INVALID_ZIP' }
        $offset += $count
    }
    return ,$buffer
}
function Inspect-CentralDirectory($Stream) {
    # Bound allocations before ZipArchive can materialize any entries. ZIP32 only.
    if ($Stream.Length -lt 22) { Reject 'INVALID_ZIP' }
    $tailLength = [int][System.Math]::Min($Stream.Length, 22 + 65535)
    $Stream.Position = $Stream.Length - $tailLength
    $tail = Read-Exactly $Stream $tailLength
    $endIndex = -1
    for ($index = $tail.Length - 4; $index -ge 0; $index--) {
        if ([System.BitConverter]::ToUInt32($tail, $index) -eq 0x06054b50) { $endIndex = $index; break }
    }
    # Reject a later signature in a comment rather than let parsers choose different EOCDs.
    if ($endIndex -lt 0 -or $endIndex + 22 -gt $tail.Length -or $endIndex + 22 + [System.BitConverter]::ToUInt16($tail, $endIndex + 20) -ne $tail.Length) { Reject 'INVALID_ZIP' }
    [long]$endOffset = $Stream.Length - $tailLength + $endIndex
    if ($endOffset -ge 20) {
        $Stream.Position = $endOffset - 20
        $locator = Read-Exactly $Stream 20
        if ([System.BitConverter]::ToUInt32($locator, 0) -eq 0x07064b50) { Reject 'ZIP64_UNSUPPORTED' }
    }
    $disk = [System.BitConverter]::ToUInt16($tail, $endIndex + 4)
    $directoryDisk = [System.BitConverter]::ToUInt16($tail, $endIndex + 6)
    $diskEntries = [System.BitConverter]::ToUInt16($tail, $endIndex + 8)
    $declaredEntries = [System.BitConverter]::ToUInt16($tail, $endIndex + 10)
    [long]$directoryBytes = [System.BitConverter]::ToUInt32($tail, $endIndex + 12)
    [long]$directoryOffset = [System.BitConverter]::ToUInt32($tail, $endIndex + 16)
    if ($disk -eq 65535 -or $directoryDisk -eq 65535 -or $diskEntries -eq 65535 -or $declaredEntries -eq 65535 -or $directoryBytes -eq [uint32]::MaxValue -or $directoryOffset -eq [uint32]::MaxValue) { Reject 'ZIP64_UNSUPPORTED' }
    if ($disk -ne 0 -or $directoryDisk -ne 0 -or $diskEntries -ne $declaredEntries) { Reject 'MULTI_DISK_UNSUPPORTED' }
    if ($declaredEntries -gt $maxEntries) { Reject 'TOO_MANY_ENTRIES' }
    if ($directoryBytes -gt $maxDirectoryBytes) { Reject 'CENTRAL_DIRECTORY_TOO_LARGE' }
    if ($directoryOffset -gt $endOffset -or $directoryBytes -ne $endOffset - $directoryOffset) { Reject 'CENTRAL_DIRECTORY_INVALID' }
    $Stream.Position = $directoryOffset
    $directory = Read-Exactly $Stream ([int]$directoryBytes)
    $cursor = 0
    $actualEntries = 0
    while ($cursor -lt $directory.Length) {
        $actualEntries++
        if ($actualEntries -gt $maxEntries) { Reject 'TOO_MANY_ENTRIES' }
        if ($directory.Length - $cursor -lt 46 -or [System.BitConverter]::ToUInt32($directory, $cursor) -ne 0x02014b50) { Reject 'CENTRAL_DIRECTORY_INVALID' }
        $version = [System.BitConverter]::ToUInt16($directory, $cursor + 6)
        [long]$compressedBytes = [System.BitConverter]::ToUInt32($directory, $cursor + 20)
        [long]$entryBytes = [System.BitConverter]::ToUInt32($directory, $cursor + 24)
        $nameBytes = [System.BitConverter]::ToUInt16($directory, $cursor + 28)
        $extraBytes = [System.BitConverter]::ToUInt16($directory, $cursor + 30)
        $commentBytes = [System.BitConverter]::ToUInt16($directory, $cursor + 32)
        $entryDisk = [System.BitConverter]::ToUInt16($directory, $cursor + 34)
        [long]$localOffset = [System.BitConverter]::ToUInt32($directory, $cursor + 42)
        if ($version -eq 45 -or $compressedBytes -eq [uint32]::MaxValue -or $entryBytes -eq [uint32]::MaxValue -or $entryDisk -eq 65535 -or $localOffset -eq [uint32]::MaxValue) { Reject 'ZIP64_UNSUPPORTED' }
        if ($entryDisk -ne 0) { Reject 'MULTI_DISK_UNSUPPORTED' }
        if ($entryBytes -gt $maxEntryBytes) { Reject 'ENTRY_TOO_LARGE' }
        if ($localOffset + 30 -gt $directoryOffset -or $compressedBytes -gt $directoryOffset - $localOffset - 30) { Reject 'CENTRAL_DIRECTORY_INVALID' }
        $recordBytes = 46 + $nameBytes + $extraBytes + $commentBytes
        if ($recordBytes -gt $directory.Length - $cursor) { Reject 'CENTRAL_DIRECTORY_INVALID' }
        $extraOffset = $cursor + 46 + $nameBytes
        $extraEnd = $extraOffset + $extraBytes
        while ($extraOffset -lt $extraEnd) {
            if ($extraEnd - $extraOffset -lt 4) { Reject 'CENTRAL_DIRECTORY_INVALID' }
            $extraTag = [System.BitConverter]::ToUInt16($directory, $extraOffset)
            $extraLength = [System.BitConverter]::ToUInt16($directory, $extraOffset + 2)
            $extraOffset += 4
            if ($extraTag -eq 1) { Reject 'ZIP64_UNSUPPORTED' }
            if ($extraLength -gt $extraEnd - $extraOffset) { Reject 'CENTRAL_DIRECTORY_INVALID' }
            $extraOffset += $extraLength
        }
        $cursor += $recordBytes
    }
    if ($actualEntries -ne $declaredEntries) { Reject 'CENTRAL_DIRECTORY_INVALID' }
    return $actualEntries
}
function Local-ArchivePath($Value) {
    # Best-effort preflight only; trusted local ancestors must remain unchanged until open.
    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace($Value)) { Reject 'INVALID_ARGUMENT' }
    if ($Value.StartsWith('\') -or $Value.StartsWith('/') -or $Value -match '^[A-Za-z][A-Za-z0-9+.-]*://' -or $Value -match '^[A-Za-z]:[^\\]') { Reject 'LOCAL_PATH_REQUIRED' }
    $location = Get-Location
    if ($location.Provider.Name -ne 'FileSystem') { Reject 'LOCAL_PATH_REQUIRED' }
    $full = [System.IO.Path]::GetFullPath($Value, $location.ProviderPath)
    if ($full -notmatch '^[A-Za-z]:\\' -or $full.Substring(2).Contains(':')) { Reject 'LOCAL_PATH_REQUIRED' }
    $root = [System.IO.Path]::GetPathRoot($full)
    $drive = [System.IO.DriveInfo]::new($root)
    if ($drive.DriveType -notin @([System.IO.DriveType]::Fixed, [System.IO.DriveType]::Ram)) { Reject 'LOCAL_PATH_REQUIRED' }
    $current = $root
    $attributes = [System.IO.File]::GetAttributes($current)
    foreach ($part in ($full.Substring($root.Length) -split '\\')) {
        if (-not $part) { continue }
        $current = [System.IO.Path]::Combine($current, $part)
        $attributes = [System.IO.File]::GetAttributes($current)
        if (($attributes -band ([System.IO.FileAttributes]::ReparsePoint -bor [System.IO.FileAttributes]::Offline)) -ne 0) { Reject 'LOCAL_PATH_REQUIRED' }
    }
    if (($attributes -band [System.IO.FileAttributes]::Directory) -ne 0) { Reject 'INVALID_ARGUMENT' }
    return $full
}
function Assert-Keys($Element, [string[]]$Keys) {
    if ($Element.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) { Reject 'MANIFEST_INVALID' }
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $allowed = [System.Collections.Generic.HashSet[string]]::new($Keys, [System.StringComparer]::Ordinal)
    foreach ($property in $Element.EnumerateObject()) {
        if (-not $seen.Add($property.Name) -or -not $allowed.Contains($property.Name)) { Reject 'MANIFEST_INVALID' }
    }
    if ($seen.Count -ne $Keys.Count) { Reject 'MANIFEST_INVALID' }
}
function Json-String($Element, [string]$Key) {
    $value = $Element.GetProperty($Key)
    if ($value.ValueKind -ne [System.Text.Json.JsonValueKind]::String) { Reject 'MANIFEST_INVALID' }
    return $value.GetString()
}
function Manifest-Text($Entry) {
    if ($Entry.Length -gt $maxManifestBytes) { Reject 'MANIFEST_TOO_LARGE' }
    $stream = $Entry.Open()
    $memory = [System.IO.MemoryStream]::new()
    try {
        $buffer = [byte[]]::new(4096)
        while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            if ($memory.Length + $count -gt $maxManifestBytes) { Reject 'MANIFEST_TOO_LARGE' }
            $memory.Write($buffer, 0, $count)
        }
        if ($memory.Length -ne $Entry.Length) { Reject 'MANIFEST_INVALID' }
        $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($memory.ToArray())
        if ($text.Length -gt 0 -and $text[0] -eq [char]0xfeff) { $text = $text.Substring(1) }
        return $text
    } finally { $memory.Dispose(); $stream.Dispose() }
}

$archive = $null
$zip = $null
$manifest = $null
$phase = 'ARCHIVE_UNREADABLE'
$exitCode = 1
$result = [ordered]@{ ok = $false; reason = 'ARCHIVE_UNREADABLE'; nativeUI = 'NOT_RUN'; signatureVerification = 'NOT_PERFORMED'; pathSafety = 'BEST_EFFORT_REQUIRES_STABLE_TRUSTED_LOCAL_ANCESTORS' }
try {
    if ($PSVersionTable.PSVersion.Major -lt 7) { Reject 'UNSUPPORTED_RUNTIME' }
    if ([System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) { Reject 'UNSUPPORTED_PLATFORM' }
    foreach ($value in @($ExpectedCheckoutCommit, $ExpectedCheckoutTree)) {
        if ($value -isnot [string] -or $value -cnotmatch '\A[0-9a-f]{40}\z') { Reject 'INVALID_EXPECTATION' }
    }
    if ($ExpectedArchiveSHA256 -isnot [string] -or $ExpectedArchiveSHA256 -cnotmatch '\A[0-9a-f]{64}\z') { Reject 'INVALID_EXPECTATION' }
    $expectedBytes = [long](Positive-Decimal $ExpectedArchiveBytes)
    $runId = Positive-Decimal $ExpectedRunId
    $runAttempt = Positive-Decimal $ExpectedRunAttempt
    if ($expectedBytes -gt $maxArchiveBytes) { Reject 'INVALID_EXPECTATION' }
    $localPath = Local-ArchivePath $ArchivePath
    $archive = [System.IO.File]::Open($localPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    if ($archive.Length -gt $maxArchiveBytes) { Reject 'ARCHIVE_TOO_LARGE' }
    if ($archive.Length -ne $expectedBytes) { Reject 'ARCHIVE_LENGTH_MISMATCH' }
    $archiveDigest = Stream-Digest $archive $maxArchiveBytes
    if ($archiveDigest.Bytes -ne $expectedBytes) { Reject 'ARCHIVE_LENGTH_MISMATCH' }
    if (-not (Exact-String $archiveDigest.SHA256 $ExpectedArchiveSHA256)) { Reject 'ARCHIVE_HASH_MISMATCH' }
    $phase = 'INVALID_ZIP'
    $boundedEntryCount = Inspect-CentralDirectory $archive
    $archive.Position = 0
    $zip = [System.IO.Compression.ZipArchive]::new($archive, [System.IO.Compression.ZipArchiveMode]::Read, $true)
    if ($zip.Entries.Count -ne $boundedEntryCount) { Reject 'CENTRAL_DIRECTORY_INVALID' }
    $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $manifestEntry = $null
    $exeEntry = $null
    foreach ($entry in $zip.Entries) {
        $name = $entry.FullName
        if (-not $name -or $name.Length -gt 512 -or $name.StartsWith('/') -or $name.Contains('\') -or $name.Contains(':') -or $name.Contains('//') -or $name -match '[\x00-\x1f\x7f]|(^|/)\.{1,2}(/|$)') { Reject 'ENTRY_PATH_INVALID' }
        if (-not $names.Add($name.TrimEnd([char]'/'))) { Reject 'DUPLICATE_ENTRY' }
        if ($entry.Length -gt $maxEntryBytes -or $entry.Length -lt 0) { Reject 'ENTRY_TOO_LARGE' }
        if ($name.EndsWith('/')) {
            if ($entry.Length -ne 0) { Reject 'ENTRY_PATH_INVALID' }
            continue
        }
        if (Exact-String $name $manifestPath) { $manifestEntry = $entry }
        if (Exact-String $name $exePath) { $exeEntry = $entry }
    }
    if ($null -eq $manifestEntry) { Reject 'MANIFEST_MISSING' }
    if ($null -eq $exeEntry) { Reject 'EXE_MISSING' }
    $phase = 'MANIFEST_INVALID'
    $jsonOptions = [System.Text.Json.JsonDocumentOptions]::new()
    $jsonOptions.MaxDepth = 8
    $manifest = [System.Text.Json.JsonDocument]::Parse((Manifest-Text $manifestEntry), $jsonOptions)
    $root = $manifest.RootElement
    Assert-Keys $root @('commit', 'tree', 'run_id', 'run_attempt', 'native_app_acceptance', 'isolation_tests', 'binaries')
    if (-not (Exact-String (Json-String $root 'commit') $ExpectedCheckoutCommit)) { Reject 'COMMIT_MISMATCH' }
    if (-not (Exact-String (Json-String $root 'tree') $ExpectedCheckoutTree)) { Reject 'TREE_MISMATCH' }
    if (-not (Exact-String (Json-String $root 'run_id') $runId)) { Reject 'RUN_MISMATCH' }
    if (-not (Exact-String (Json-String $root 'run_attempt') $runAttempt)) { Reject 'ATTEMPT_MISMATCH' }
    if (-not (Exact-String (Json-String $root 'native_app_acceptance') 'NOT RUN; compilation only, no installer or release') -or -not (Exact-String (Json-String $root 'isolation_tests') 'native-isolation-rust.log')) { Reject 'MANIFEST_INVALID' }
    $binaries = $root.GetProperty('binaries')
    if ($binaries.ValueKind -ne [System.Text.Json.JsonValueKind]::Array -or $binaries.GetArrayLength() -ne 1) { Reject 'MANIFEST_INVALID' }
    $binary = $binaries[0]
    Assert-Keys $binary @('name', 'bytes', 'sha256')
    if (-not (Exact-String (Json-String $binary 'name') 'nect-svg-desktop.exe')) { Reject 'MANIFEST_INVALID' }
    $declaredHash = Json-String $binary 'sha256'
    $declaredLength = $binary.GetProperty('bytes')
    [long]$exeBytes = 0
    if ($declaredHash -cnotmatch '\A[0-9a-f]{64}\z' -or $declaredLength.ValueKind -ne [System.Text.Json.JsonValueKind]::Number -or $declaredLength.GetRawText() -cnotmatch '\A[1-9][0-9]*\z' -or -not $declaredLength.TryGetInt64([ref]$exeBytes) -or $exeBytes -gt $maxEntryBytes) { Reject 'MANIFEST_INVALID' }
    $phase = 'INVALID_ZIP'
    $exeStream = $exeEntry.Open()
    try { $exeDigest = Stream-Digest $exeStream $maxEntryBytes } finally { $exeStream.Dispose() }
    if ($exeDigest.Bytes -ne $exeEntry.Length -or $exeDigest.Bytes -ne $exeBytes) { Reject 'EXE_LENGTH_MISMATCH' }
    if (-not (Exact-String $exeDigest.SHA256 $declaredHash)) { Reject 'EXE_HASH_MISMATCH' }
    $result = [ordered]@{ ok = $true; reason = 'ARCHIVE_RECEIPT_EXE_MATCH'; archiveBytes = $archiveDigest.Bytes; exeBytes = $exeDigest.Bytes; exeSHA256 = $exeDigest.SHA256; nativeUI = 'NOT_RUN'; signatureVerification = 'NOT_PERFORMED'; pathSafety = 'BEST_EFFORT_REQUIRES_STABLE_TRUSTED_LOCAL_ANCESTORS' }
    $exitCode = 0
} catch {
    $reason = $phase
    if ($_.Exception.Data.Contains('NativeArtifactReason')) { $reason = $_.Exception.Data['NativeArtifactReason'] }
    $result.reason = $reason
} finally {
    foreach ($resource in @($manifest, $zip, $archive)) {
        if ($null -ne $resource) {
            try { $resource.Dispose() } catch { $exitCode = 1; $result.ok = $false; $result.reason = 'ARCHIVE_UNREADABLE' }
        }
    }
}
$result | ConvertTo-Json -Depth 3 -Compress
exit $exitCode
