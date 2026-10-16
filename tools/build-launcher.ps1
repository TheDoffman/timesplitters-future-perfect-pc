#requires -Version 5.1
<#
.SYNOPSIS
Build the standalone Windows launcher using the .NET Framework compiler in Windows.
.DESCRIPTION
Users need no Python, Git, .NET SDK or C++ toolchain. WPF/.NET Framework 4.8 is
included in supported Windows 10/11 installations. -Test runs isolated updater
regressions, without downloading a game or accessing personal saves/settings.
#>
[CmdletBinding()]
param([string]$Output, [switch]$Test, [string]$BannerPath, [string]$LogoPath)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (!$Output) { $Output = Join-Path $project 'build\launcher' }
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $Output -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $Output 'Bin'),(Join-Path $Output 'Support\Launcher') -Force | Out-Null
$framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$compiler = Join-Path $framework 'csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x x64 compiler is missing.' }
$source = Join-Path $PSScriptRoot 'launcher'
if (!$BannerPath) { $BannerPath = Join-Path $project 'private\launcher-art\future-perfect-banner-v1.png' }
$bannerArguments = @()
if (Test-Path -LiteralPath $BannerPath -PathType Leaf) {
    $BannerPath = (Resolve-Path -LiteralPath $BannerPath).ProviderPath
    if ((Get-Item -LiteralPath $BannerPath).Length -gt 8MB) { throw 'Launcher banner must be no larger than 8 MiB.' }
    $bannerArguments = @(('/resource:' + $BannerPath + ',launcher-banner.png'))
} else {
    Write-Host 'Private launcher banner not present; using the built-in vector theme.'
}

# Wide title artwork is sidebar-only; the EXEs use a dedicated compact portal mark.
if (!$LogoPath) { $LogoPath = Join-Path $project 'private\launcher-art\timesplitters-future-perfect-logo.png' }
$logoArguments = @()
Add-Type -AssemblyName System.Drawing
if (Test-Path -LiteralPath $LogoPath -PathType Leaf) {
    $LogoPath = (Resolve-Path -LiteralPath $LogoPath).ProviderPath
    if ((Get-Item -LiteralPath $LogoPath).Length -gt 1MB) { throw 'Launcher logo must be no larger than 1 MiB.' }
    $logo = [Drawing.Image]::FromFile($LogoPath)
    try {
        if ($logo.Width -ne 500 -or $logo.Height -ne 187) { throw 'Logo must be the 500x187 title lockup; sidebar subtitle coordinates depend on it.' }
        $logoArguments = @(('/resource:' + $LogoPath + ',launcher-logo.png'))
    } finally { $logo.Dispose() }
} else {
    Write-Host 'Private title logo not present; using the sidebar text fallback.'
}
if (!('LauncherIconBuilder' -as [type])) {
    Add-Type -TypeDefinition ([IO.File]::ReadAllText((Join-Path $source 'IconBuilder.cs'))) -ReferencedAssemblies System.Drawing
}
[LauncherIconBuilder]::Write((Join-Path $Output 'launcher.ico'))
[LauncherIconBuilder]::Preview((Join-Path $Output 'icon-preview.png'))
$references = @('System.dll','System.Core.dll','System.Web.Extensions.dll','System.Net.Http.dll',
    'System.IO.Compression.dll','System.IO.Compression.FileSystem.dll','System.Xaml.dll',
    'System.Windows.Forms.dll')
$commonArguments = @('/nologo','/target:winexe','/platform:x64','/optimize+','/utf8output',
    ('/win32manifest:' + (Join-Path $source 'launcher.manifest')),
    ('/win32icon:' + (Join-Path $Output 'launcher.ico')),
    ('/resource:' + (Join-Path $Output 'launcher.ico') + ',launcher.ico'))
$commonArguments += $logoArguments
foreach ($reference in $references) { $commonArguments += '/reference:' + (Join-Path $framework $reference) }
foreach ($reference in @('PresentationCore.dll','PresentationFramework.dll','WindowsBase.dll')) {
    $commonArguments += '/reference:' + (Join-Path $framework "WPF\$reference")
}
$commonSources = @((Join-Path $source 'Core.cs'), (Join-Path $source 'ReleaseClient.cs'),
    (Join-Path $source 'SelfUpdate.cs'), (Join-Path $source 'LauncherVersion.cs'))
$arguments = $commonArguments + @('/main:TimeSplitters.Launcher.Bootstrap',
    ('/out:' + (Join-Path $Output 'TimeSplitters Launcher.exe'))) + $commonSources + @((Join-Path $source 'Bootstrap.cs'))
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw 'Bootstrap compilation failed.' }
$arguments = $commonArguments + @('/main:TimeSplitters.Launcher.Program',
    ('/out:' + (Join-Path $Output 'Bin\TimeSplitters.App.exe')),
    ('/resource:' + (Join-Path $source 'Launcher.xaml') + ',Launcher.xaml')) + $commonSources + @(
    (Join-Path $source 'Launcher.cs'), (Join-Path $PSScriptRoot 'release\ImportGame.cs'))
$arguments += $bannerArguments
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
# Publishable launcher-only assets. This never includes a game binary or data.
$version = [regex]::Match([IO.File]::ReadAllText((Join-Path $source 'LauncherVersion.cs')), 'Value = "([^"]+)"').Groups[1].Value
if (!$version) { throw 'Launcher version was not found.' }
$files = [ordered]@{}
foreach ($name in @('TimeSplitters Launcher.exe','Bin/TimeSplitters.App.exe')) {
    $files[$name] = (Get-FileHash -LiteralPath (Join-Path $Output $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest = [ordered]@{ schema = 1; version = $version; files = $files }
$encoding = New-Object Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $Output 'Support\Launcher\manifest.json'), (($manifest | ConvertTo-Json -Depth 4) + "`n"), $encoding)
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$assetName = "TimeSplitters-Launcher-v$version-windows-x64.zip"
$archivePath = Join-Path $Output $assetName
$archiveStream = New-Object IO.FileStream $archivePath,([IO.FileMode]::Create)
$archive = New-Object IO.Compression.ZipArchive $archiveStream,([IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($name in @('TimeSplitters Launcher.exe','Bin/TimeSplitters.App.exe','Support/Launcher/manifest.json')) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Join-Path $Output $name), $name, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose(); $archiveStream.Dispose() }
$hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($archivePath + '.sha256'), "$hash  $assetName`n", $encoding)
$metadata = [ordered]@{ schema = 1; version = $version; asset_name = $assetName; size = (Get-Item -LiteralPath $archivePath).Length; sha256 = $hash }
[IO.File]::WriteAllText((Join-Path $Output 'launcher-update.json'), (($metadata | ConvertTo-Json) + "`n"), $encoding)
Copy-Item -LiteralPath $archivePath -Destination (Join-Path $Output 'TimeSplitters-Launcher-windows-x64.zip') -Force
[IO.File]::WriteAllText((Join-Path $Output 'TimeSplitters-Launcher-windows-x64.zip.sha256'), ($hash + '  TimeSplitters-Launcher-windows-x64.zip' + "`n"), $encoding)
if ($Test) {
    $testArgs = @('/nologo','/target:exe','/platform:x64','/optimize+','/utf8output','/main:LauncherTests',
        ('/out:' + (Join-Path $Output 'launcher-tests.exe')))
    foreach ($reference in $references) { $testArgs += '/reference:' + (Join-Path $framework $reference) }
    foreach ($reference in @('PresentationCore.dll','PresentationFramework.dll','WindowsBase.dll')) {
        $testArgs += '/reference:' + (Join-Path $framework "WPF\$reference")
    }
    $testArgs += $commonSources + @((Join-Path $source 'Bootstrap.cs'),
        (Join-Path $source 'Launcher.cs'), (Join-Path $PSScriptRoot 'release\ImportGame.cs'),
        (Join-Path $project 'tests\launcher_updates.cs'))
    & $compiler @testArgs
    if ($LASTEXITCODE -ne 0) { throw 'Launcher test compilation failed.' }
    & (Join-Path $Output 'launcher-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Launcher regression tests failed.' }
}
Get-Item -LiteralPath (Join-Path $Output 'TimeSplitters Launcher.exe') | Select-Object FullName,Length
