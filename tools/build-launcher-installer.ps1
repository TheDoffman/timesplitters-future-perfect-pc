#requires -Version 5.1
<#
.SYNOPSIS
Build a per-user Windows installer and the portable/self-update ZIP.
.DESCRIPTION
Run tools/fetch-installer-compiler.ps1 once, or pass -FetchCompiler. No game data
is included. -Test also runs isolated installer lifecycle and preservation checks.
#>
[CmdletBinding()]
param([string]$Output, [string]$LauncherDirectory, [string]$CompilerPath,
      [switch]$FetchCompiler, [switch]$RequireArtwork, [switch]$Test,
      [ValidatePattern('^installer-tests(?:-[A-Za-z0-9]+)?$')][string]$TestWorkName='installer-tests')
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (!$Output) { $Output = Join-Path $project 'build\launcher' }
$Output = [IO.Path]::GetFullPath($Output)
$pin = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installer\compiler.json') -Raw | ConvertFrom-Json
if (!$CompilerPath) { $CompilerPath = Join-Path $project ("build\tools\innosetup-" + $pin.version + '\ISCC.exe') }
if (!(Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    if (!$FetchCompiler) { throw 'Installer compiler missing. Run tools/fetch-installer-compiler.ps1 or pass -FetchCompiler.' }
    $CompilerPath = & (Join-Path $PSScriptRoot 'fetch-installer-compiler.ps1')
}
$CompilerPath = (Resolve-Path -LiteralPath $CompilerPath).ProviderPath
$compilerRoot = Split-Path -Parent $CompilerPath
foreach ($file in $pin.files.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $compilerRoot $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Value) { throw "Installer compiler differs from the official pin: $($file.Name)" }
}
New-Item -ItemType Directory -Path $Output -Force | Out-Null
if (!$LauncherDirectory) {
    & (Join-Path $PSScriptRoot 'build-launcher.ps1') -Output $Output -RequireArtwork:$RequireArtwork -Test:$Test
    $LauncherDirectory = $Output
}
$LauncherDirectory = (Resolve-Path -LiteralPath $LauncherDirectory).ProviderPath
$manifestFile = Join-Path $LauncherDirectory 'Support\Launcher\manifest.json'
$manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
$sourceVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'launcher\LauncherVersion.cs')), 'Value = "([^"]+)"').Groups[1].Value
if ($manifest.schema -ne 1 -or $manifest.version -ne $sourceVersion) { throw 'Launcher payload version differs from the current source.' }
$expectedFiles = @('TimeSplitters Launcher.exe','Bin/TimeSplitters.App.exe')
if (@($manifest.files.PSObject.Properties).Count -ne 2) { throw 'Unexpected launcher payload manifest.' }
foreach ($name in $expectedFiles) {
    $path = Join-Path $LauncherDirectory $name
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($manifest.files.$name -ne $hash) { throw "Launcher payload checksum mismatch: $name" }
}
$expectedFileVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'launcher\LauncherVersion.cs')), 'AssemblyFileVersion\("([^"]+)"').Groups[1].Value
foreach ($name in $expectedFiles) {
    $fileInfo = (Get-Item -LiteralPath (Join-Path $LauncherDirectory $name)).VersionInfo
    if ($fileInfo.FileVersion -ne $expectedFileVersion -or $fileInfo.ProductVersion -ne $sourceVersion) { throw "Executable version differs from the launcher manifest: $name" }
}
if ($RequireArtwork) {
    $assembly = [Reflection.Assembly]::ReflectionOnlyLoad([IO.File]::ReadAllBytes((Join-Path $LauncherDirectory 'Bin\TimeSplitters.App.exe')))
    $resources = $assembly.GetManifestResourceNames()
    foreach ($name in @('launcher-banner.png','launcher-logo.png')) {
        if ($resources -notcontains $name) { throw "Release launcher artwork is required: $name missing from supplied application." }
    }
}
$info = (Get-Item -LiteralPath (Join-Path $LauncherDirectory 'TimeSplitters Launcher.exe')).VersionInfo
$versionMS = [uint32]($info.FileMajorPart * 65536 + $info.FileMinorPart)
$versionLS = [uint32]($info.FileBuildPart * 65536 + $info.FilePrivatePart)
$arguments = @('/Qp',('/O' + $Output),('/DLauncherDir=' + $LauncherDirectory),('/DLauncherVersion=' + $sourceVersion),('/DVersionMS=' + $versionMS),('/DVersionLS=' + $versionLS),(Join-Path $PSScriptRoot 'installer\launcher.iss'))
& $CompilerPath @arguments
if ($LASTEXITCODE -ne 0) { throw 'Launcher installer compilation failed.' }
$setup = Join-Path $Output ("TimeSplitters-Launcher-Setup-v$sourceVersion-windows-x64.exe")
$hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
$encoding = New-Object Text.UTF8Encoding $false
[IO.File]::WriteAllText(($setup + '.sha256'), ($hash + '  ' + [IO.Path]::GetFileName($setup) + "`n"), $encoding)
$alias = Join-Path $Output 'TimeSplitters-Launcher-Setup-windows-x64.exe'
Copy-Item -LiteralPath $setup -Destination $alias -Force
[IO.File]::WriteAllText(($alias + '.sha256'), ($hash + '  ' + [IO.Path]::GetFileName($alias) + "`n"), $encoding)
$record = [ordered]@{ schema=1; version=$sourceVersion; asset_name=[IO.Path]::GetFileName($setup); sha256=$hash; size=(Get-Item -LiteralPath $setup).Length; compiler_version=$pin.version; compiler_download_sha256=$pin.sha256; source_commit=(& git -C $project rev-parse HEAD); source_changes=@(& git -C $project status --porcelain); files=$manifest.files; launcher_manifest_sha256=(Get-FileHash -LiteralPath $manifestFile -Algorithm SHA256).Hash.ToLowerInvariant() }
[IO.File]::WriteAllText((Join-Path $Output 'launcher-installer-build.json'), (($record | ConvertTo-Json -Depth 5) + "`n"), $encoding)
if ($Test) {
    & (Join-Path $PSScriptRoot 'test-launcher-installer.ps1') -Output $Output -LauncherDirectory $LauncherDirectory -CompilerPath $CompilerPath -WorkName $TestWorkName
}
Get-Item -LiteralPath $setup | Select-Object FullName,Length
