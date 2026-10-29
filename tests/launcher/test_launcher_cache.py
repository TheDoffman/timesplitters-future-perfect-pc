"""Exercise actual PowerShell receipt helpers with disposable inputs/outputs."""
from pathlib import Path
import os
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows launcher build cache')
def test_launcher_cache_checks_bytes_options_inventory_and_corrupt_receipts(tmp_path):
    script = tmp_path/'check.ps1'
    script.write_text(r'''
param([string]$Helper, [string]$Root)
$ErrorActionPreference = 'Stop'
. $Helper
$inputFile = Join-Path $Root 'source.cs'
$extra = Join-Path $Root 'new-page.cs'
$receipt = Join-Path $Root 'receipt.json'
$binary = Join-Path $Root 'app.exe'
[IO.File]::WriteAllText($inputFile, 'first')
[IO.File]::WriteAllText($binary, 'built')
$files = @('app.exe')
$key = Get-LauncherFingerprint @($inputFile) 'tested'
if (Test-LauncherReceipt $receipt $key $Root $files) { throw 'Missing receipt reused' }
Save-LauncherReceipt $receipt $key $Root $files
if (!(Test-LauncherReceipt $receipt $key $Root $files)) { throw 'Successful result not reused' }
$stamp = [IO.File]::GetLastWriteTimeUtc($inputFile)
[IO.File]::WriteAllText($inputFile, 'other')
[IO.File]::SetLastWriteTimeUtc($inputFile, $stamp)
$changed = Get-LauncherFingerprint @($inputFile) 'tested'
if (Test-LauncherReceipt $receipt $changed $Root $files) { throw 'Changed source reused' }
[IO.File]::WriteAllText($inputFile, 'first')
$changed = Get-LauncherFingerprint @($inputFile) 'untested'
if (Test-LauncherReceipt $receipt $changed $Root $files) { throw 'Different options reused' }
[IO.File]::WriteAllText($extra, 'page')
$changed = Get-LauncherFingerprint @($inputFile, $extra) 'tested'
if (Test-LauncherReceipt $receipt $changed $Root $files) { throw 'Added input reused' }
$changed = Get-LauncherFingerprint @((Join-Path $Root 'missing.cs')) 'tested'
if (Test-LauncherReceipt $receipt $changed $Root $files) { throw 'Missing input reused' }
$stamp = [IO.File]::GetLastWriteTimeUtc($binary)
[IO.File]::WriteAllText($binary, 'wrong')
[IO.File]::SetLastWriteTimeUtc($binary, $stamp)
if (Test-LauncherReceipt $receipt $key $Root $files) { throw 'Changed output reused' }
Remove-Item -LiteralPath $binary
if (Test-LauncherReceipt $receipt $key $Root $files) { throw 'Missing output reused' }
[IO.File]::WriteAllText($binary, 'built')
if (Test-LauncherReceipt $receipt $key $Root @('app.exe', 'missing.exe')) { throw 'Different output inventory reused' }
[IO.File]::WriteAllText($receipt, '{invalid')
if (Test-LauncherReceipt $receipt $key $Root $files) { throw 'Corrupt receipt reused' }
Save-LauncherReceipt $receipt $key $Root $files
Remove-Item -LiteralPath $receipt
if (Test-LauncherReceipt $receipt $key $Root $files) { throw 'Invalidated receipt reused' }
Write-Output 'All receipt checks passed'
''', encoding='utf-8')
    # A PowerShell 7 test host can export modules incompatible with Windows PS5.
    environment = {key: value for key, value in os.environ.items() if key.upper() != 'PSMODULEPATH'}
    result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                             '-File', str(script), '-Helper', str(ROOT/'tools/build/launcher-cache.ps1'),
                             '-Root', str(tmp_path)], capture_output=True, text=True, timeout=30, env=environment)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'All receipt checks passed' in result.stdout
