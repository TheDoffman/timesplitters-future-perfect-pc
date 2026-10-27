#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$LiteralPath)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build = Join-Path $workspace 'build'
$releases = Join-Path $build 'releases'
$target = [IO.Path]::GetFullPath($LiteralPath)
$testPath = Join-Path $build 'release-tests'
$isStage = [IO.Path]::GetDirectoryName($target) -eq $releases -and
    [IO.Path]::GetFileName($target) -match '^TimeSplitters-Future-Perfect-PC-v[0-9A-Za-z.+-]+-windows-x64$'
if ($target -ne $testPath -and !$isStage) { throw "Refusing unexpected release cleanup target: $target" }
if (!(Test-Path -LiteralPath $target)) { return }
$ancestor = $target
while ($ancestor -and $ancestor.Length -ge $workspace.Length) {
    if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing redirected release cleanup: $ancestor"
    }
    if ($ancestor -eq $workspace) { break }
    $ancestor = [IO.Path]::GetDirectoryName($ancestor)
}
if (@(Get-ChildItem -LiteralPath $target -Force -Recurse | Where-Object {
    $_.Attributes -band [IO.FileAttributes]::ReparsePoint
}).Count) { throw 'Refusing redirected release work files.' }
# Only disposable test/staging files are allowed above. Remove them directly;
# the shell Recycle Bin is unavailable on some drives and noninteractive hosts.
# Packaged archives, checksums, notes and manifests remain outside these targets.
Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
Write-Output "Removed disposable release work directory: $target"
