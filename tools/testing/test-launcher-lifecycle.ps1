#requires -Version 5.1
[CmdletBinding()]
param([string]$LauncherRoot)
$project = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (!$LauncherRoot) { $LauncherRoot = Join-Path $project 'build/launcher' }
$LauncherRoot = [IO.Path]::GetFullPath($LauncherRoot)
$fixtureSource = Join-Path $project 'tests/launcher_lifecycle'
$ErrorActionPreference = 'Stop'
$framework = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319'
$csc = Join-Path $framework 'csc.exe'
$root = Join-Path $LauncherRoot 'lifecycle-fixture'
New-Item -ItemType Directory -Path (Join-Path $root 'Bin') -Force | Out-Null
$references = @('/reference:System.dll','/reference:System.Core.dll','/reference:System.Xaml.dll')
foreach ($dll in @('PresentationCore.dll','PresentationFramework.dll','WindowsBase.dll')) {
    $references += '/reference:' + (Join-Path $framework ('WPF/'+$dll))
}
& $csc /nologo /target:winexe /platform:x64 @references ('/out:'+(Join-Path $root 'Bin/futureperfect_recomp.exe')) (Join-Path $fixtureSource 'FakeGame.cs')
if ($LASTEXITCODE -ne 0) { throw 'Fixture compilation failed.' }
& $csc /nologo /target:exe /platform:x64 @references ('/reference:'+(Join-Path $LauncherRoot 'Bin/TimeSplitters.App.exe')) ('/out:'+(Join-Path $LauncherRoot 'Bin/lifecycle-smoke.exe')) (Join-Path $fixtureSource 'LifecycleSmoke.cs')
if ($LASTEXITCODE -ne 0) { throw 'Harness compilation failed.' }
& (Join-Path $LauncherRoot 'Bin/lifecycle-smoke.exe') $root
if ($LASTEXITCODE -ne 0) { throw 'Lifecycle smoke failed.' }
