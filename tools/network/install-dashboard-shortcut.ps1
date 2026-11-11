# Install only client-side dashboard connection files, outside the checkout.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_][A-Za-z0-9_.@:-]*$')]
    [string]$SshAlias,
    [string]$Python
)
$ErrorActionPreference = 'Stop'
if (-not $Python) {
    $Python = Join-Path $PSScriptRoot '../../.venv/Scripts/python.exe'
}
$pythonBase = & $Python -c 'import sys; print(sys._base_executable)'
if ($LASTEXITCODE -ne 0) { throw 'Could not locate the Python interpreter.' }
$pythonWindowless = Join-Path (Split-Path $pythonBase) 'pythonw.exe'
if (-not (Test-Path -LiteralPath $pythonWindowless)) {
    throw 'This Windows shortcut needs pythonw.exe alongside python.exe.'
}
Get-Command ssh -ErrorAction Stop | Out-Null
$installDir = Join-Path $env:LOCALAPPDATA 'TimeSplitters Online/dashboard-shortcut'
New-Item -ItemType Directory -Path $installDir -Force | Out-Null
foreach ($name in @('view_online_dashboard.py', 'dashboard_shortcut.py')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $installDir $name)
}
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'Mac Mini Dashboard.lnk'
$shortcutShell = New-Object -ComObject WScript.Shell
$shortcut = $shortcutShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $pythonWindowless
$shortcut.Arguments = '"' + (Join-Path $installDir 'dashboard_shortcut.py') + '" --ssh "' + $SshAlias + '"'
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = 'Connect to the Mac mini and open the TimeSplitters online dashboard'
$shortcut.IconLocation = "$env:SystemRoot\System32\shell32.dll,17"
$shortcut.Save()
Write-Output "Installed: $shortcutPath"
