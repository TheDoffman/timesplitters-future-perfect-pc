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
param([string]$Output, [switch]$Test, [string]$BannerPath, [string]$LogoPath, [switch]$RequireArtwork, [string]$BackdropPath, [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$Output) { $Output = Join-Path $project 'build\launcher' }
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $Output -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $Output 'Bin'),(Join-Path $Output 'Support\Launcher') -Force | Out-Null
$buildTimer = [Diagnostics.Stopwatch]::StartNew()
$buildOutcome = 'failed'
# Hold the output lock through reuse, compilation, tests and receipt publication.
$outputLock = [IO.File]::Open((Join-Path $Output '.build.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
. (Join-Path $PSScriptRoot 'launcher-cache.ps1')
$receipt = Join-Path $Output '.validated-build.json'
$framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$compiler = Join-Path $framework 'csc.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x x64 compiler is missing.' }
$source = Join-Path (Split-Path -Parent $PSScriptRoot) 'launcher'
# Prefer the checksum-pinned artwork used by both release builds. Keep older
# private source artwork intact as a development fallback when no cache exists.
function Resolve-LauncherArtwork([string]$ExplicitPath, [string]$ResourceName, [string]$LegacyName) {
    if ($ExplicitPath) { return $ExplicitPath }
    $cachedArtwork = Join-Path $project ('build\launcher-art\' + $ResourceName)
    if (Test-Path -LiteralPath $cachedArtwork -PathType Leaf) {
        $artworkPin = Get-Content -Raw (Join-Path $project 'config\build\launcher-artwork.json') | ConvertFrom-Json
        $expectedArtworkHash = $artworkPin.resources.$ResourceName
        if (!$expectedArtworkHash -or (Get-FileHash -LiteralPath $cachedArtwork -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedArtworkHash) {
            throw "Cached launcher artwork differs from config/build/launcher-artwork.json: $ResourceName. Recover it with tools/build/fetch-launcher-art.ps1."
        }
        return $cachedArtwork
    }
    return (Join-Path $project ('private\launcher-art\' + $LegacyName))
}

$BannerPath = Resolve-LauncherArtwork $BannerPath 'launcher-banner.png' 'future-perfect-banner-v1.png'
$bannerArguments = @()
if (Test-Path -LiteralPath $BannerPath -PathType Leaf) {
    $BannerPath = (Resolve-Path -LiteralPath $BannerPath).ProviderPath
    if ((Get-Item -LiteralPath $BannerPath).Length -gt 8MB) { throw 'Launcher banner must be no larger than 8 MiB.' }
    $bannerArguments = @(('/resource:' + $BannerPath + ',launcher-banner.png'))
} else {
    if ($RequireArtwork) { throw 'Release launcher artwork is required: banner missing. Supply -BannerPath or restore private/launcher-art.' }
    Write-Host 'Private launcher banner not present; using the built-in vector theme.'
}

# Wide title artwork is sidebar-only; the EXEs use a dedicated compact portal mark.
$LogoPath = Resolve-LauncherArtwork $LogoPath 'launcher-logo.png' 'timesplitters-future-perfect-logo.png'
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
    if ($RequireArtwork) { throw 'Release launcher artwork is required: logo missing. Supply -LogoPath or restore private/launcher-art.' }
    Write-Host 'Private title logo not present; using the sidebar text fallback.'
}
# The generated environment is separate from the untouched original key art.
$BackdropPath = Resolve-LauncherArtwork $BackdropPath 'launcher-backdrop.png' 'future-perfect-backdrop-v1.png'
if (Test-Path -LiteralPath $BackdropPath -PathType Leaf) {
    $BackdropPath = (Resolve-Path -LiteralPath $BackdropPath).ProviderPath
    if ((Get-Item -LiteralPath $BackdropPath).Length -gt 8MB) { throw 'Launcher backdrop must be no larger than 8 MiB.' }
    $bannerArguments += '/resource:' + $BackdropPath + ',launcher-backdrop.png'
} else {
    if ($RequireArtwork) { throw 'Release launcher artwork is required: backdrop missing. Supply -BackdropPath or restore private/launcher-art.' }
    Write-Host 'Private launcher backdrop not present; using the built-in gradient behind the original artwork.'
}
$references = @('System.dll','System.Core.dll','System.Web.Extensions.dll','System.Net.Http.dll',
    'System.IO.Compression.dll','System.IO.Compression.FileSystem.dll','System.Xaml.dll',
    'System.Windows.Forms.dll','System.Management.dll')
$version = [regex]::Match([IO.File]::ReadAllText((Join-Path $source 'LauncherVersion.cs')), 'Value = "([^"]+)"').Groups[1].Value
if (!$version) { throw 'Launcher version was not found.' }
$assetName = "TimeSplitters-Launcher-v$version-windows-x64.zip"
$cachedFiles = @('TimeSplitters Launcher.exe', 'Bin/TimeSplitters.App.exe', 'Support/Launcher/manifest.json',
    'launcher-tests.exe', 'launcher.ico', 'icon-preview.png', 'launcher-update.json', $assetName, ($assetName + '.sha256'))
$cacheInputs = @($PSCommandPath, (Join-Path $PSScriptRoot 'launcher-cache.ps1'),
    (Join-Path $project 'tools/release/ImportGame.cs'), (Join-Path $project 'config/pc-controls.default.json'),
    (Join-Path $project 'src/interface/pc_display_modes.inc'), (Join-Path $project 'config/build/launcher-artwork.json'))
$cacheInputs += @(Get-ChildItem -LiteralPath $source -File | ForEach-Object { $_.FullName })
$cacheInputs += @(Get-ChildItem -LiteralPath (Join-Path $project 'tests/launcher') -Filter '*.cs' -File | ForEach-Object { $_.FullName })
foreach ($name in ($references + @('csc.exe', 'csc.exe.config', 'clr.dll', 'mscorlib.dll', 'System.Drawing.dll', 'System.Xml.dll', 'System.Configuration.dll',
    'WPF/PresentationCore.dll', 'WPF/PresentationFramework.dll', 'WPF/WindowsBase.dll'))) {
    $cacheInputs += Join-Path $framework $name
}
foreach ($path in @($BannerPath, $LogoPath, $BackdropPath)) {
    if (Test-Path -LiteralPath $path -PathType Leaf) { $cacheInputs += $path }
}
$cacheOptions = "launcher-v1|test=$Test|art=$RequireArtwork|banner=$BannerPath|logo=$LogoPath|backdrop=$BackdropPath|os=$([Environment]::OSVersion.VersionString)|ps=$($PSVersionTable.PSVersion)|culture=$([Globalization.CultureInfo]::CurrentCulture.Name)"
$fingerprint = Get-LauncherFingerprint -Paths $cacheInputs -Options $cacheOptions
if ($Test -and !$Rebuild -and (Test-LauncherReceipt -Receipt $receipt -Fingerprint $fingerprint -Output $Output -Files $cachedFiles)) {
    $buildOutcome = 'reused verified build and tests'
    Write-Host 'Launcher inputs and output hashes match the successful build/test receipt.'
    Get-Item -LiteralPath (Join-Path $Output 'TimeSplitters Launcher.exe') | Select-Object FullName,Length
    return
}
# A failed/untested rebuild must never inherit a previous successful receipt.
if (Test-Path -LiteralPath $receipt) { Remove-Item -LiteralPath $receipt }
if (!('LauncherIconBuilder' -as [type])) {
    Add-Type -Path @((Join-Path $source 'IconBuilder.cs'), (Join-Path $source 'GameMonogram.cs')) -ReferencedAssemblies System.Drawing
}
[LauncherIconBuilder]::Write((Join-Path $Output 'launcher.ico'))
[LauncherIconBuilder]::Preview((Join-Path $Output 'icon-preview.png'))
$commonArguments = @('/nologo','/target:winexe','/platform:x64','/optimize+','/utf8output',
    ('/win32manifest:' + (Join-Path $source 'launcher.manifest')),
    ('/win32icon:' + (Join-Path $Output 'launcher.ico')),
    ('/resource:' + (Join-Path $Output 'launcher.ico') + ',launcher.ico'))
$commonArguments += $logoArguments
foreach ($reference in $references) { $commonArguments += '/reference:' + (Join-Path $framework $reference) }
foreach ($reference in @('PresentationCore.dll','PresentationFramework.dll','WindowsBase.dll')) {
    $commonArguments += '/reference:' + (Join-Path $framework "WPF\$reference")
}
$commonSources = @((Join-Path $source 'SaveBackups.cs'), (Join-Path $source 'Core.cs'), (Join-Path $source 'LauncherPlatform.cs'), (Join-Path $source 'DownloadTransfer.cs'), (Join-Path $source 'ReleaseNews.cs'), (Join-Path $source 'ReleaseClient.cs'),
    (Join-Path $source 'SelfUpdate.cs'), (Join-Path $source 'UpdateRestart.cs'), (Join-Path $source 'TexturePack.cs'), (Join-Path $source 'TextureDownload.cs'), (Join-Path $source 'LauncherVersion.cs'), (Join-Path $source 'WindowTheme.cs'))
$arguments = $commonArguments + @('/main:TimeSplitters.Launcher.Bootstrap',
    ('/out:' + (Join-Path $Output 'TimeSplitters Launcher.exe'))) + $commonSources + @((Join-Path $source 'Bootstrap.cs'))
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw 'Bootstrap compilation failed.' }
$arguments = $commonArguments + @('/main:TimeSplitters.Launcher.Program',
    ('/out:' + (Join-Path $Output 'Bin\TimeSplitters.App.exe')),
    ('/resource:' + (Join-Path $source 'Launcher.xaml') + ',Launcher.xaml')) + $commonSources + @(
    (Join-Path $source 'Launcher.cs'), (Join-Path (Split-Path -Parent $PSScriptRoot) 'release\ImportGame.cs'))
$arguments += $bannerArguments
$settingsSources = @((Join-Path $source 'Ui.cs'), (Join-Path $source 'GameSessions.cs'), (Join-Path $source 'CrashReports.cs'), (Join-Path $source 'PageTheme.cs'), (Join-Path $source 'ReleaseNotesMarkdown.cs'), (Join-Path $source 'ReleaseNotesDocument.cs'), (Join-Path $source 'GameSettings.cs'), (Join-Path $source 'DisplayModes.cs'), (Join-Path $source 'InstallationMaintenance.cs'), (Join-Path $source 'DiagnosticIssueDraft.cs'))
# Every shared page is compiled for both hosts; add new pages beside these sources.
$settingsSources += @(Get-ChildItem -LiteralPath $source -Filter '*Page.cs' | ForEach-Object { $_.FullName })
$defaultControls = @(('/resource:' + (Join-Path $source 'PageTheme.xaml') + ',PageTheme.xaml'),
    ('/resource:' + (Join-Path $source 'TabsTheme.xaml') + ',TabsTheme.xaml'), ('/resource:' + (Join-Path $project 'config\pc-controls.default.json') + ',launcher-controls-default.json'),
    ('/resource:' + (Join-Path $project 'src\interface\pc_display_modes.inc') + ',display-modes.inc'))
$arguments += $settingsSources + @($defaultControls)
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
# Publishable launcher-only assets. This never includes a game binary or data.
$files = [ordered]@{}
foreach ($name in @('TimeSplitters Launcher.exe','Bin/TimeSplitters.App.exe')) {
    $files[$name] = (Get-FileHash -LiteralPath (Join-Path $Output $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest = [ordered]@{ schema = 1; version = $version; files = $files }
$encoding = New-Object Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $Output 'Support\Launcher\manifest.json'), (($manifest | ConvertTo-Json -Depth 4) + "`n"), $encoding)
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
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
if ($Test) {
    $testArgs = @('/nologo','/target:exe','/platform:x64','/optimize+','/utf8output','/main:LauncherTests',
        ('/out:' + (Join-Path $Output 'launcher-tests.exe')))
    foreach ($reference in $references) { $testArgs += '/reference:' + (Join-Path $framework $reference) }
    foreach ($reference in @('PresentationCore.dll','PresentationFramework.dll','WindowsBase.dll')) {
        $testArgs += '/reference:' + (Join-Path $framework "WPF\$reference")
    }
    $testArgs += $commonSources + @((Join-Path $source 'Bootstrap.cs'),
        (Join-Path $source 'Launcher.cs'), (Join-Path (Split-Path -Parent $PSScriptRoot) 'release\ImportGame.cs'),
        (Join-Path $project 'tests\launcher\launcher_updates.cs'))
    $testArgs += $settingsSources + @($defaultControls, (Join-Path $project 'tests\launcher\launcher_crashes.cs'), (Join-Path $project 'tests\launcher\launcher_settings.cs'), (Join-Path $project 'tests\launcher\launcher_maintenance.cs'), (Join-Path $project 'tests\launcher\launcher_downloads.cs'), (Join-Path $project 'tests\launcher\launcher_news.cs'), (Join-Path $project 'tests\launcher\launcher_versions.cs'), (Join-Path $project 'tests\launcher\launcher_startup.cs'))
    $testArgs += @((Join-Path $project 'tests\launcher\launcher_sessions.cs'), (Join-Path $project 'tests\launcher\launcher_shared_ui.cs'), (Join-Path $project 'tests\launcher\launcher_saves.cs'), (Join-Path $project 'tests\launcher\launcher_restart.cs'), (Join-Path $project 'tests\launcher\launcher_textures.cs'))
    $testArgs += @(('/resource:' + (Join-Path $source 'Launcher.xaml') + ',Launcher.xaml'),
        ('/resource:' + (Join-Path $Output 'launcher.ico') + ',launcher.ico')) + $bannerArguments + $logoArguments
    & $compiler @testArgs
    if ($LASTEXITCODE -ne 0) { throw 'Launcher test compilation failed.' }
    & (Join-Path $Output 'launcher-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Launcher regression tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --shared-ui
    if ($LASTEXITCODE -ne 0) { throw 'Shared launcher UI tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --textures
    if ($LASTEXITCODE -ne 0) { throw 'Texture installer tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --downloads
    if ($LASTEXITCODE -ne 0) { throw 'Launcher download tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --versions
    if ($LASTEXITCODE -ne 0) { throw 'Launcher game version tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --startup
    if ($LASTEXITCODE -ne 0) { throw 'Launcher startup recovery tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --news
    if ($LASTEXITCODE -ne 0) { throw 'Launcher release news tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --settings
    if ($LASTEXITCODE -ne 0) { throw 'Launcher settings tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --crashes
    if ($LASTEXITCODE -ne 0) { throw 'Launcher crash report tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --sessions
    if ($LASTEXITCODE -ne 0) { throw 'Launcher unfinished-session tests failed.' }
    & (Join-Path $Output 'launcher-tests.exe') --maintenance
    if ($LASTEXITCODE -ne 0) { throw 'Launcher maintenance tests failed.' }
}
Get-Item -LiteralPath (Join-Path $Output 'TimeSplitters Launcher.exe') | Select-Object FullName,Length
if ($Test) {
    if ($fingerprint -ne (Get-LauncherFingerprint -Paths $cacheInputs -Options $cacheOptions)) {
        throw 'Launcher inputs changed during validation; retry the build.'
    }
    Save-LauncherReceipt -Receipt $receipt -Fingerprint $fingerprint -Output $Output -Files $cachedFiles
}
$buildOutcome = 'passed'
} finally {
    $outputLock.Dispose()
    $buildTimer.Stop()
    Write-Host ('[launcher timing] {0}: {1:N2}s' -f $buildOutcome, $buildTimer.Elapsed.TotalSeconds)
}
