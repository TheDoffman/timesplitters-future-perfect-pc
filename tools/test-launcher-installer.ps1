#requires -Version 5.1
<# Runs setup/uninstall in one disposable directory and a unique test-only registration. #>
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Output,
      [Parameter(Mandatory=$true)][string]$LauncherDirectory,
      [Parameter(Mandatory=$true)][string]$CompilerPath,
      [ValidatePattern('^installer-tests(?:-[A-Za-z0-9]+)?$')][string]$WorkName='installer-tests')
$ErrorActionPreference = 'Stop'
if (Get-Process futureperfect_recomp -ErrorAction SilentlyContinue) { throw 'Close the game before running installer lifecycle checks. No test installation was changed.' }
$project = Split-Path -Parent $PSScriptRoot
$Output = [IO.Path]::GetFullPath($Output)
$LauncherDirectory = [IO.Path]::GetFullPath($LauncherDirectory)
$work = Join-Path $Output $WorkName
if (Test-Path -LiteralPath $work) { throw 'Previous installer-tests retained. Inspect it before another run.' }
New-Item -ItemType Directory -Path $work -Force | Out-Null
$token = [Guid]::NewGuid().ToString('N')
$id = 'TSFP.Installer.Test.' + $token
$shortcut = 'TimeSplitters Launcher Test ' + $token
$group = 'TSFP Installer Test ' + $token
$keyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\' + $id + '_is1'
$root = Join-Path $work 'installed launcher ü'
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) ($group + '\' + $shortcut + '.lnk')
$desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) ($shortcut + '.lnk')
$passed = 0
$succeeded = $false
function Require([bool]$Value,[string]$Message) { if (!$Value) { throw $Message } }
function Pass([string]$Message) { $script:passed++; Write-Host "PASS $Message" }
function RunSetup([bool]$Success=$true,[string]$Destination=$root) {
    $number = [Guid]::NewGuid().ToString('N')
    $args = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR="' + $Destination + '"'),'/TASKS=desktopicon',('/LOG="' + (Join-Path $work ($number + '.log')) + '"'))
    $p = Start-Process -FilePath $script:testSetup -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
    Require (($p.ExitCode -eq 0) -eq $Success) "Unexpected setup result: $($p.ExitCode). See $number.log"
}
function RunUninstall([bool]$Success=$true) {
    $exe = Join-Path $root 'Support\Installer\unins000.exe'
    $p = Start-Process -FilePath $exe -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -WindowStyle Hidden -Wait -PassThru
    Require (($p.ExitCode -eq 0) -eq $Success) "Unexpected uninstall result: $($p.ExitCode)"
}
function PersonalFiles {
    foreach ($name in $script:personal.Keys) {
        Require ((Get-FileHash -LiteralPath (Join-Path $root $name) -Algorithm SHA256).Hash -eq $script:personal[$name]) "Personal fixture changed: $name"
    }
}
function BusyTest([scriptblock]$Body) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.TrimEnd('\').ToUpperInvariant())))).Replace('-','') } finally { $sha.Dispose() }
    $mutex = New-Object Threading.Mutex $false,('Local\TSFP-Launcher-' + $hash)
    Require ($mutex.WaitOne(0)) 'Test installation unexpectedly busy.'
    try { & $Body } finally { $mutex.ReleaseMutex(); $mutex.Dispose() }
}
try {
    $manifest = Get-Content -LiteralPath (Join-Path $LauncherDirectory 'Support\Launcher\manifest.json') -Raw | ConvertFrom-Json
    $v = (Get-Item -LiteralPath (Join-Path $LauncherDirectory 'TimeSplitters Launcher.exe')).VersionInfo
    $args = @('/Qp',('/O' + $work),('/DLauncherDir=' + $LauncherDirectory),('/DLauncherVersion=' + $manifest.version),('/DVersionMS=' + ($v.FileMajorPart*65536+$v.FileMinorPart)),('/DVersionLS=' + ($v.FileBuildPart*65536+$v.FilePrivatePart)),('/DInstallerAppId=' + $id),('/DShortcutName=' + $shortcut),('/DInstallerGroup=' + $group),(Join-Path $PSScriptRoot 'installer\launcher.iss'))
    & $CompilerPath @args
    Require ($LASTEXITCODE -eq 0) 'Test installer compilation failed.'
    $script:testSetup = Join-Path $work ("TimeSplitters-Launcher-Setup-v$($manifest.version)-windows-x64.exe")
    RunSetup
    foreach ($name in @('TimeSplitters Launcher.exe','Bin/TimeSplitters.App.exe','Support/Launcher/manifest.json')) {
        Require ((Get-FileHash -LiteralPath (Join-Path $root $name)).Hash -eq (Get-FileHash -LiteralPath (Join-Path $LauncherDirectory $name)).Hash) "Installed file mismatch: $name"
    }
    Require (Test-Path -LiteralPath $keyPath) 'Missing per-user Installed apps entry.'
    Require (Test-Path -LiteralPath $menu) 'Missing Start menu shortcut.'
    Require (Test-Path -LiteralPath $desktop) 'Missing selected desktop shortcut.'
    $link = (New-Object -ComObject WScript.Shell).CreateShortcut($menu)
    Require ($link.TargetPath -eq (Join-Path $root 'TimeSplitters Launcher.exe') -and $link.WorkingDirectory -eq $root) 'Shortcut targets the wrong root.'
    Pass 'silent fresh install registers per-user app, correct files and shortcuts'

    $script:personal = @{}
    foreach ($name in @('Saves/profile','Settings/pc-controls.json','Settings/pc-display.ini','Settings/runtime.conf','Data/default.xbe','Textures/pack.dds','disc.iso','Bin/futureperfect_recomp.exe','BUILD.json','.launcher/preferences.json','.launcher/downloads/retained.partial','.launcher/launcher.log')) {
        $path = Join-Path $root $name
        New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
        [IO.File]::WriteAllText($path,('personal fixture: ' + $name))
        $script:personal[$name] = (Get-FileHash -LiteralPath $path).Hash
    }
    [IO.File]::WriteAllText((Join-Path $root 'Bin\TimeSplitters.App.exe'),'damaged launcher')
    RunSetup
    Require ((Get-FileHash -LiteralPath (Join-Path $root 'Bin\TimeSplitters.App.exe')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $LauncherDirectory 'Bin\TimeSplitters.App.exe')).Hash) 'Reinstall did not repair launcher.'
    PersonalFiles
    Pass 'reinstall repairs launcher while preserving game, settings, data, saves, ISO, textures and cache'

    $compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    $fixture = Join-Path $work 'fixture'
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $LauncherDirectory 'Bin\TimeSplitters.App.exe') -Destination (Join-Path $fixture 'TimeSplitters.App.exe')
    $common = @('/nologo','/target:exe','/platform:x64',('/reference:' + (Join-Path $fixture 'TimeSplitters.App.exe')),'/reference:System.dll','/reference:System.Core.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll')
    & $compiler @common ('/out:' + (Join-Path $fixture 'installer-smoke.exe')) (Join-Path $project 'tests\launcher_installer_smoke.cs')
    Require ($LASTEXITCODE -eq 0) 'Installer fixture compilation failed.'
    Copy-Item -LiteralPath (Join-Path $fixture 'installer-smoke.exe') -Destination (Join-Path $fixture 'futureperfect_recomp.exe')
    $fake = Start-Process -FilePath (Join-Path $fixture 'futureperfect_recomp.exe') -ArgumentList @('--game',('"' + $root + '"')) -WindowStyle Hidden -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        while (!(Test-Path -LiteralPath (Join-Path $root 'fake-game-ready')) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 50 }
        Require (Test-Path -LiteralPath (Join-Path $root 'fake-game-ready')) 'Synthetic game fixture not ready.'
        RunSetup $false
        RunUninstall $false
        PersonalFiles
    } finally { [IO.File]::WriteAllText((Join-Path $root 'fake-game-stop'),'stop'); Require ($fake.WaitForExit(10000)) 'Synthetic fixture did not exit.'; $fake.Dispose() }
    Pass 'running-game guard blocks setup and uninstall without terminating the synthetic child'

    BusyTest { RunSetup $false; RunUninstall $false }
    PersonalFiles
    Pass 'shared launcher mutex blocks setup and uninstall, including a Unicode installation path'

    $transaction = Join-Path $root '.launcher\self-transaction'
    New-Item -ItemType Directory -Path $transaction -Force | Out-Null
    RunSetup $false
    RunUninstall $false
    Remove-Item -LiteralPath $transaction
    PersonalFiles
    Pass 'unfinished update transaction is kept and prevents setup and uninstall'

    $newerSource = Join-Path $fixture 'newer.cs'
    [IO.File]::WriteAllText($newerSource,'[assembly:System.Reflection.AssemblyFileVersion("99.0.0.0")] public class Newer { public static void Main(){} }')
    & $compiler /nologo /target:winexe ('/out:' + (Join-Path $root 'TimeSplitters Launcher.exe')) $newerSource
    Require ($LASTEXITCODE -eq 0) 'Newer version fixture compilation failed.'
    $before = (Get-FileHash -LiteralPath (Join-Path $root 'TimeSplitters Launcher.exe')).Hash
    RunSetup $false
    Require ((Get-FileHash -LiteralPath (Join-Path $root 'TimeSplitters Launcher.exe')).Hash -eq $before) 'Setup downgraded a newer launcher.'
    Copy-Item -LiteralPath (Join-Path $LauncherDirectory 'TimeSplitters Launcher.exe') -Destination (Join-Path $root 'TimeSplitters Launcher.exe') -Force
    Pass 'older installer refuses to downgrade a newer launcher'

    $zip = Join-Path $LauncherDirectory ("TimeSplitters-Launcher-v$($manifest.version)-windows-x64.zip")
    & (Join-Path $fixture 'installer-smoke.exe') --update $root $zip
    Require ($LASTEXITCODE -eq 0) 'Transactional self-update fixture failed.'
    PersonalFiles
    Require (Test-Path -LiteralPath (Join-Path $root 'Support\Installer\unins000.exe')) 'Self-update removed uninstaller.'
    Pass 'existing transactional self-updater preserves installer registration and uninstaller'

    RunUninstall
    Require (!(Test-Path -LiteralPath $keyPath)) 'Uninstall registration remained.'
    Require (!(Test-Path -LiteralPath $menu) -and !(Test-Path -LiteralPath $desktop)) 'Owned shortcut remained.'
    Require (!(Test-Path -LiteralPath (Join-Path $root 'TimeSplitters Launcher.exe')) -and !(Test-Path -LiteralPath (Join-Path $root 'Bin\TimeSplitters.App.exe'))) 'Updated launcher files remained after uninstall.'
    PersonalFiles
    Pass 'uninstall removes self-updated launcher files and shortcuts while retaining every personal and game fixture'

    RunSetup
    PersonalFiles
    RunUninstall
    PersonalFiles
    Pass 'installer can reuse the retained game folder without copying or reimporting personal data'

    $redirected = Join-Path $work 'redirected'
    $external = Join-Path $work 'external-preserved'
    New-Item -ItemType Directory -Path $redirected,$external -Force | Out-Null
    $externalApp = Join-Path $external 'TimeSplitters.App.exe'
    [IO.File]::WriteAllText($externalApp,'keep external file')
    $junction = Join-Path $redirected 'Bin'
    New-Item -ItemType Junction -Path $junction -Target $external -ErrorAction Stop | Out-Null
    try { RunSetup $false $redirected; Require ([IO.File]::ReadAllText($externalApp) -eq 'keep external file') 'Redirected target changed.' }
    finally { [IO.Directory]::Delete($junction) }
    Pass 'setup refuses a redirected destination without changing its target'

    Write-Host "PASS: $passed isolated installer lifecycle checks."
    $succeeded = $true
} finally {
    # Preserve a failed install and logs for diagnosis; never delete user installations.
    if (!$succeeded -or (Test-Path -LiteralPath $keyPath)) { Write-Warning "Test installation and logs retained for diagnosis: $work" }
    else {
        $resolved = [IO.Path]::GetFullPath($work)
        Require ($resolved.StartsWith($Output.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase) -and $resolved -ne $Output) 'Unsafe test cleanup root.'
        Require (!@(Get-ChildItem -LiteralPath $resolved -Force -Recurse | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) 'Linked test file retained for inspection.'
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
