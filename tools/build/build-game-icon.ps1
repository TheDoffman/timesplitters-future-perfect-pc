#requires -Version 5.1
<# Build the game's multi-size Windows icon before compiling its executable. #>
[CmdletBinding()]
param([string]$Output, [string]$LogoPath, [switch]$RequireLogo, [switch]$Fallback)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$Output) { $Output = Join-Path $project 'build\game-icon' }
$Output = [IO.Path]::GetFullPath($Output)
if (!$LogoPath) { $LogoPath = Join-Path $project 'private\launcher-art\timesplitters-future-perfect-logo.png' }
if ($Fallback -and $RequireLogo) { throw '-Fallback and -RequireLogo cannot be combined.' }
if ($Fallback) { $LogoPath = $null }
elseif (!(Test-Path -LiteralPath $LogoPath -PathType Leaf)) {
    if ($RequireLogo) { throw 'Official game logo is required. Restore private artwork or supply -LogoPath.' }
    $LogoPath = $null
}
else {
    $LogoPath = (Resolve-Path -LiteralPath $LogoPath).ProviderPath
    if ((Get-Item -LiteralPath $LogoPath).Length -gt 1MB) { throw 'Game logo must be no larger than 1 MiB.' }
}
New-Item -ItemType Directory -Path $Output -Force | Out-Null
if (!('LauncherIconBuilder' -as [type])) {
    Add-Type -Path @((Join-Path (Split-Path -Parent $PSScriptRoot) 'launcher\IconBuilder.cs'), (Join-Path (Split-Path -Parent $PSScriptRoot) 'launcher\GameMonogram.cs')) -ReferencedAssemblies System.Drawing
}
$icon = Join-Path $Output 'game.ico'
[LauncherIconBuilder]::WriteGame($icon, $LogoPath)
[LauncherIconBuilder]::PreviewGame((Join-Path $Output 'game-icon-preview.png'), $LogoPath)
$encoding = New-Object Text.UTF8Encoding $false
# Forward slashes avoid RC backslash escapes; code page 65001 preserves Unicode paths.
$iconReference = $icon.Replace('\','/')
if ($iconReference.Contains('"') -or $iconReference.Contains("`n") -or $iconReference.Contains("`r")) { throw 'Invalid resource output path.' }
$resource = Join-Path $Output 'game-icon.rc'
[IO.File]::WriteAllText($resource, ("#pragma code_page(65001)`r`n1 ICON " + '"' + $iconReference + '"' + "`r`n"), $encoding)
$record = [ordered]@{ schema=1; icon_sha256=(Get-FileHash -LiteralPath $icon -Algorithm SHA256).Hash.ToLowerInvariant(); sizes=@(16,24,32,48,64,128,256); mode='tsfp-monogram'; monogram='Future TimeSplitters serif outlines'; source_commit=(& git -C $project rev-parse HEAD); source_changes=@(& git -C $project status --porcelain) }
if ($LogoPath) {
    $record.mode = 'official-logo-and-small-monogram'
    $record.logo_sha256 = (Get-FileHash -LiteralPath $LogoPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $Output 'game-icon-build.json'), (($record | ConvertTo-Json -Depth 4) + "`n"), $encoding)
Write-Output $resource
