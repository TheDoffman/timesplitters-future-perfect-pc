[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testRoot = [IO.Path]::GetFullPath((Join-Path $workspace 'private\network-validation'))
if (!(Test-Path -LiteralPath $testRoot)) { return }
# Check each ancestor, not just the final file: never follow a test junction
# into game data or personal saves. A denied recycle operation remains an error.
foreach ($part in @($workspace, (Join-Path $workspace 'private'), $testRoot)) {
    if ((Get-Item -LiteralPath $part).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing cleanup through a reparse point: $part"
    }
}
Add-Type -AssemblyName Microsoft.VisualBasic
foreach ($role in @('host', 'client')) {
    $roleRoot = Join-Path $testRoot $role
    $saveRoot = Join-Path $roleRoot 'saves'
    foreach ($folder in @($roleRoot, $saveRoot)) {
        if ((Test-Path -LiteralPath $folder) -and
            ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing cleanup through a reparse point: $folder"
        }
    }
    if (!(Test-Path -LiteralPath $saveRoot)) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $saveRoot -File -Filter 'Partition*.img') {
        $target = (Resolve-Path -LiteralPath $file.FullName).Path
        if (!$target.StartsWith($saveRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
            $file.Name -notmatch '^Partition[0-5]\.img$' -or
            ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing unexpected test cleanup target: $target"
        }
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile(
            $target, 'OnlyErrorDialogs', 'SendToRecycleBin', 'ThrowException')
        Write-Output "Recycled $target"
    }
}
