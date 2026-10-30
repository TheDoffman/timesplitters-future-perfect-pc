[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$testRoot = Join-Path $projectRoot 'private\network-validation'
$buildRoot = Join-Path $projectRoot 'build\windows'
$exe = Join-Path $buildRoot "$Configuration\futureperfect_recomp.exe"
$python = Join-Path $projectRoot '.venv\Scripts\python.exe'
$processes = @()
$testLock = $null
$environmentBackup = @{}

foreach ($required in @($exe, $python)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing $required. Run setup.ps1 and tsfp.ps1 Build first."
    }
}

# Refuse redirected test paths before either game can write saves there.
foreach ($relative in @('private', 'private\network-validation',
    'private\network-validation\host', 'private\network-validation\host\saves',
    'private\network-validation\client', 'private\network-validation\client\saves')) {
    $folder = Join-Path $projectRoot $relative
    if ((Test-Path -LiteralPath $folder) -and
        ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing redirected test directory: $folder"
    }
}
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

try {
    # Prevent concurrent test launchers from sharing saves or recycling live disks.
    $testLock = [IO.File]::Open((Join-Path $testRoot 'session.lock'),
        [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    & $python (Join-Path $projectRoot 'tools\build\network-identity.py') --verify --executable $exe
    if ($LASTEXITCODE -ne 0) { throw 'Multiplayer build verification failed. Run tsfp.ps1 Build first.' }

    # Clear inherited test replays, save routing, and network overrides. Restore
    # the caller's environment immediately after each pair of launches.
    foreach ($entry in Get-ChildItem Env:) {
        if ($entry.Name -match '^(TSFP_|RECOMP_)') { $environmentBackup[$entry.Name] = $entry.Value }
    }
    try {
        foreach ($name in $environmentBackup.Keys) { [Environment]::SetEnvironmentVariable($name, $null) }
        $env:TSFP_NETWORK = '1'
        $env:RECOMP_GAME_DIR = Join-Path $projectRoot 'private\game'
        $env:RECOMP_INPUT_CONFIG = Join-Path $projectRoot 'tests\fixtures\pc-controls.json'
        $env:RECOMP_TRACE_BUDGET = '0'
        $env:RECOMP_WINDOW_BACKGROUND = '1'

        foreach ($role in @('host', 'client')) {
            $roleRoot = Join-Path $testRoot $role
            New-Item -ItemType Directory -Force -Path (Join-Path $roleRoot 'saves') | Out-Null
            $env:TSFP_NET_BIND = if ($role -eq 'host') { '127.0.0.2' } else { '127.0.0.3' }
            $env:TSFP_NET_PEERS = if ($role -eq 'host') { '127.0.0.3' } else { '127.0.0.2' }
            $env:RECOMP_SAVE_DIR = Join-Path $roleRoot 'saves'
            $env:TSFP_DISPLAY_CONFIG = Join-Path $roleRoot 'display.ini'
            $env:RECOMP_LOG_FILE = Join-Path $roleRoot 'interactive.err'
            $gameProcess = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru `
                -WindowStyle Hidden -RedirectStandardOutput (Join-Path $roleRoot 'interactive.out') `
                -RedirectStandardError (Join-Path $roleRoot 'interactive.err')
            $processes += $gameProcess
            Write-Output "Started $role (PID $($gameProcess.Id), $($env:TSFP_NET_BIND))."
        }
    }
    finally {
        foreach ($entry in Get-ChildItem Env:) {
            if ($entry.Name -match '^(TSFP_|RECOMP_)') { [Environment]::SetEnvironmentVariable($entry.Name, $null) }
        }
        foreach ($name in $environmentBackup.Keys) {
            [Environment]::SetEnvironmentVariable($name, $environmentBackup[$name])
        }
    }

    Write-Output 'In both windows: Xbox Live / System Link > System Link.'
    Write-Output 'Host: Start New Match. Client: select that match, then Ready. Host: Go!'
    Write-Output 'Leave this launcher open. Temporary test disks are recycled after BOTH games close.'
}
finally {
    try {
        # Also wait for a successfully launched host if the second launch fails.
        foreach ($gameProcess in $processes) { $gameProcess.WaitForExit() }
        if ($testLock -and @($processes | Where-Object { !$_.HasExited }).Count -eq 0) {
            & (Join-Path $projectRoot 'tools\network\cleanup-network-tests.ps1')
            Write-Output 'System Link test finished; test profiles and logs retained.'
        }
    }
    finally {
        foreach ($gameProcess in $processes) { $gameProcess.Dispose() }
        if ($testLock) { $testLock.Dispose() }
    }
}
