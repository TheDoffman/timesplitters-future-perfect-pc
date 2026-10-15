$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$packRoot = Join-Path $projectRoot 'private\texture-remaster\pack'
if (!(Test-Path -LiteralPath $packRoot) -or !(Get-ChildItem -LiteralPath $packRoot -Filter '*.dds')) {
    throw 'No authored texture pack is installed yet.'
}
$oldPack = $env:TSFP_TEXTURE_PACK_DIR
$oldMode = $env:TSFP_TEXTURE_MODE
try {
    $env:TSFP_TEXTURE_PACK_DIR = $packRoot
    $env:TSFP_TEXTURE_MODE = '4k'
    & (Join-Path $projectRoot 'tsfp.ps1') -Action Run
}
finally { $env:TSFP_TEXTURE_PACK_DIR = $oldPack; $env:TSFP_TEXTURE_MODE = $oldMode }
