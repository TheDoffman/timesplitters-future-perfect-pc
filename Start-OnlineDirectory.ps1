#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$PublicAddress,
    [ValidateRange(1024,65535)]
    [int]$Port = 30820
)
$ErrorActionPreference = 'Stop'
function Invoke-OpenSsl {
    param([string[]]$Arguments)
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $opensslPath @Arguments
        $result = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($result -ne 0) { throw "OpenSSL failed (exit $result)." }
    $output
}
$root = $PSScriptRoot
$directory = Join-Path $root 'private\online-directory'
$python = Join-Path $root '.venv\Scripts\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'Run setup.ps1 first.' }
if ($PublicAddress) {
    $parsed = $null
    if (![Net.IPAddress]::TryParse($PublicAddress, [ref]$parsed) -or
        $parsed.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) {
        throw '-PublicAddress must be the IPv4 address friends use to reach your router.'
    }
}
$openssl = Get-Command openssl -CommandType Application -ErrorAction SilentlyContinue
if ($openssl) { $opensslPath = $openssl.Source }
else {
    $git = Get-Command git -CommandType Application -ErrorAction Stop
    $opensslPath = Join-Path (Split-Path (Split-Path $git.Source)) 'usr\bin\openssl.exe'
    if (!(Test-Path -LiteralPath $opensslPath)) { throw 'OpenSSL is required to create the local certificate; Git for Windows includes it.' }
}
foreach ($folder in @((Join-Path $root 'private'), $directory)) {
    if ((Test-Path -LiteralPath $folder) -and
        ((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing redirected directory configuration: $folder"
    }
}
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$certificate = Join-Path $directory 'certificate.pem'
$key = Join-Path $directory 'private-key.pem'
if (!(Test-Path -LiteralPath $certificate) -and !(Test-Path -LiteralPath $key)) {
    Invoke-OpenSsl -Arguments @('req', '-x509', '-newkey', 'rsa:2048', '-sha256', '-nodes',
        '-days', '730', '-subj', '/CN=TimeSplitters Player Directory', '-keyout', $key, '-out', $certificate)
}
if (!(Test-Path -LiteralPath $certificate) -or !(Test-Path -LiteralPath $key)) {
    throw 'The directory certificate or key is missing. Restore the matching pair before starting.'
}
Write-Output "Directory port: TCP $Port. Forward it to this PC. Keep this window open."
Write-Output 'Share the HTTPS address and this certificate fingerprint with your friends:'
$fingerprintLine = Invoke-OpenSsl -Arguments @('x509', '-in', $certificate, '-noout', '-fingerprint', '-sha256')
Write-Output (($fingerprintLine -split '=')[-1] -replace ':', '')
if ($PublicAddress) { Write-Output "Friends use https://${PublicAddress}:$Port" }
else { Write-Output 'Supply -PublicAddress for Internet hosting from this PC; without it, local hosts advertise their LAN address.' }
Write-Output "On this PC use https://127.0.0.1:$Port and the same fingerprint."
Write-Output 'Game hosts also forward their selected UDP port (default 30821); guests need no forwarding.'
$arguments = @((Join-Path $root 'tools\lobby_server.py'), '--bind', '0.0.0.0', '--port', "$Port", '--cert', $certificate, '--key', $key)
if ($PublicAddress) { $arguments += @('--public-address', $PublicAddress) }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw "Directory stopped with exit code $LASTEXITCODE." }
