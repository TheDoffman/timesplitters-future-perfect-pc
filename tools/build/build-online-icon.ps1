#requires -Version 5.1
<# Build the Online companion icon from the shared project icon artwork. #>
[CmdletBinding()]
param([string]$Output)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$Output) { $Output = Join-Path $project 'build\online-icon' }
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $Output -Force | Out-Null
$source = Join-Path (Split-Path -Parent $PSScriptRoot) 'launcher'
if (!('LauncherIconBuilder' -as [type])) {
    Add-Type -Path @((Join-Path $source 'IconBuilder.cs'), (Join-Path $source 'GameMonogram.cs')) -ReferencedAssemblies System.Drawing
}
[LauncherIconBuilder]::WriteMultiplayer((Join-Path $Output 'multiplayer.ico'))
[LauncherIconBuilder]::PreviewMultiplayer((Join-Path $Output 'multiplayer-icon-preview.png'))
Write-Output (Join-Path $Output 'multiplayer.ico')
