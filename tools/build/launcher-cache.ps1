# Content-based receipts for local, successful launcher builds and regressions.
# No timestamp-only cache keys and no dependency on Python or the .NET SDK.
function Get-LauncherFingerprint {
    param([string[]]$Paths, [string]$Options)
    $rows = @($Options)
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
        $rows += $path + ':' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n")))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Test-LauncherReceipt {
    param([string]$Receipt, [string]$Fingerprint, [string]$Output, [string[]]$Files)
    if (!$Fingerprint -or !(Test-Path -LiteralPath $Receipt -PathType Leaf)) { return $false }
    try {
        $saved = Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json
        if ($saved.schema -ne 1 -or $saved.fingerprint -ne $Fingerprint) { return $false }
        if (@($saved.files.PSObject.Properties).Count -ne $Files.Count) { return $false }
        foreach ($name in $Files) {
            $path = Join-Path $Output $name
            if (!(Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
            if ($saved.files.$name -ne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) { return $false }
        }
        return $true
    } catch { return $false }
}

function Save-LauncherReceipt {
    param([string]$Receipt, [string]$Fingerprint, [string]$Output, [string[]]$Files)
    if (!$Fingerprint) { throw 'Cannot record a launcher build with missing inputs.' }
    $hashes = [ordered]@{}
    foreach ($name in $Files) {
        $hashes[$name] = (Get-FileHash -LiteralPath (Join-Path $Output $name) -Algorithm SHA256).Hash
    }
    $value = [ordered]@{ schema = 1; fingerprint = $Fingerprint; files = $hashes }
    $temporary = $Receipt + '.tmp'
    [IO.File]::WriteAllText($temporary, ($value | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding $false))
    Move-Item -LiteralPath $temporary -Destination $Receipt -Force
}
