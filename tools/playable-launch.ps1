$ErrorActionPreference = 'Stop'
$gameRoot = $PSScriptRoot
$settingsPath = Join-Path $gameRoot 'Settings\pc-controls.json'
$controls = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
$sensitivity = [double]$controls.mouse.sensitivity
if ([double]::IsNaN($sensitivity) -or [double]::IsInfinity($sensitivity) -or $sensitivity -lt 0.001 -or $sensitivity -gt 10) {
    throw 'Mouse sensitivity must be between 0.001 and 10.'
}
$logRoot = Join-Path $gameRoot 'Logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$info = New-Object System.Diagnostics.ProcessStartInfo
$info.FileName = Join-Path $gameRoot 'Bin\futureperfect_recomp.exe'
$info.WorkingDirectory = Join-Path $gameRoot 'Bin'
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
foreach ($name in @($info.EnvironmentVariables.Keys)) {
    if ($name -like 'RECOMP_*' -or $name -like 'TSFP_*') { $info.EnvironmentVariables.Remove($name) }
}
# Optional authored pack; installing it later needs no launcher changes.
$info.EnvironmentVariables['TSFP_TEXTURE_PACK_DIR'] = Join-Path $gameRoot 'Textures'
$info.EnvironmentVariables['TSFP_TEXTURE_MODE'] = 'classic'
$info.EnvironmentVariables['RECOMP_GAME_DIR'] = Join-Path $gameRoot 'Data'
$info.EnvironmentVariables['RECOMP_SAVE_DIR'] = Join-Path $gameRoot 'Saves'
$info.EnvironmentVariables['RECOMP_INPUT_CONFIG'] = $settingsPath
$info.EnvironmentVariables['RECOMP_FFMPEG_DIR'] = Join-Path $gameRoot 'Bin'
$info.EnvironmentVariables['RECOMP_TRACE_BUDGET'] = '0'
$info.EnvironmentVariables['RECOMP_PC_MOUSE'] = if ($controls.mouse.enabled) { '1' } else { '0' }
$info.EnvironmentVariables['RECOMP_MOUSE_INVERT_Y'] = if ($controls.mouse.invert_y) { '1' } else { '0' }
$info.EnvironmentVariables['RECOMP_MOUSE_SENSITIVITY'] = $sensitivity.ToString('R', [Globalization.CultureInfo]::InvariantCulture)
$tag = Get-Date -Format 'yyyyMMdd-HHmmss'
$info.EnvironmentVariables['RECOMP_LOG_FILE'] = Join-Path $logRoot "$tag.err"
$outFile = [IO.File]::Create((Join-Path $logRoot "$tag.out"))
# Let the in-game Debug panel tail diagnostics while the launcher writes them.
$errFile = [IO.FileStream]::new((Join-Path $logRoot "$tag.err"),
    [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite, 1, $true)
$game = New-Object System.Diagnostics.Process
$game.StartInfo = $info
try {
    if (!$game.Start()) { throw 'The game could not start.' }
    $outTask = $game.StandardOutput.BaseStream.CopyToAsync($outFile)
    $errTask = $game.StandardError.BaseStream.CopyToAsync($errFile)
    $game.WaitForExit()
    [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($outTask, $errTask))
    $result = $game.ExitCode
} finally {
    $outFile.Dispose()
    $errFile.Dispose()
    $game.Dispose()
}
exit $result
