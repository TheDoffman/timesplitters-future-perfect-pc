#requires -Version 5.1
<#
.SYNOPSIS
Build, validate, package and publish the VERSION from a clean, pushed source branch.
.DESCRIPTION
Stable uses main by default; pass -Branch for a preview or maintenance branch.
Enhanced textures are optional and omitted from official game packages.
.EXAMPLE
.\Publish-Release.ps1 -PrepareOnly
.EXAMPLE
.\Publish-Release.ps1
#>
[CmdletBinding()]
param([switch]$PrepareOnly, [switch]$Draft, [string]$Branch = 'main', [string]$ChannelName, [string]$NotesFile, [ValidateRange(1,64)][int]$TestWorkers)
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'Run setup.ps1 first.' }
$releaseArguments = @((Join-Path $PSScriptRoot 'tools\release\publish-release.py'))
$releaseArguments += @('--branch', $Branch)
if ($ChannelName) { $releaseArguments += @('--channel-name', $ChannelName) }
if ($NotesFile) { $releaseArguments += @('--notes-file', $NotesFile) }
if ($PSBoundParameters.ContainsKey('TestWorkers')) { $releaseArguments += @('--test-workers', $TestWorkers) }
if ($PrepareOnly) { $releaseArguments += '--prepare-only' }
if ($Draft) { $releaseArguments += '--draft' }
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & $python @releaseArguments
    $releaseExitCode = $LASTEXITCODE
}
finally { $ErrorActionPreference = $previousPreference }
if ($releaseExitCode -ne 0) { throw "Release stopped (exit $releaseExitCode). See the error above; no existing release was overwritten." }
