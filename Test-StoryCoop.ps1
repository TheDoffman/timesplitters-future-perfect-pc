[CmdletBinding()]
param(
    [ValidateSet(2, 4)]
    [int]$Players = 2
)

$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    throw 'Python environment not found. Run setup.ps1 first.'
}
& $python (Join-Path $PSScriptRoot 'tools\story_coop_local.py') --players $Players
exit $LASTEXITCODE
