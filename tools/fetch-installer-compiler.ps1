#requires -Version 5.1
<# Download the pinned official Inno Setup compiler into ignored build tools. #>
[CmdletBinding()]
param([string]$Destination)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$pin = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installer\compiler.json') -Raw | ConvertFrom-Json
$version = $pin.version
$expected = $pin.sha256
if (!$Destination) { $Destination = Join-Path $project "build\tools\innosetup-$version" }
$Destination = [IO.Path]::GetFullPath($Destination)
$compiler = Join-Path $Destination 'ISCC.exe'
if (Test-Path -LiteralPath $compiler -PathType Leaf) {
    foreach ($file in $pin.files.PSObject.Properties) {
        if ((Get-FileHash -LiteralPath (Join-Path $Destination $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Value) { throw "Installer compiler pin mismatch: $($file.Name)" }
    }
    Write-Output $compiler
    return
}
if ((Get-PSDrive -Name ([IO.Path]::GetPathRoot($Destination).Substring(0,1))).Free -lt 256MB) { throw 'Need 256 MiB free for installer build tools.' }
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$download = Join-Path $Destination "innosetup-$version.exe"
if (!(Test-Path -LiteralPath $download -PathType Leaf)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri $pin.url -OutFile $download
}
if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Official Inno Setup download checksum mismatch. Nothing was executed.' }
$arguments = @('/PORTABLE=1','/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR="' + $Destination + '"'),('/LOG="' + (Join-Path $Destination 'compiler-setup.log') + '"'))
$process = Start-Process -FilePath $download -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $compiler)) { throw 'Portable installer compiler setup failed.' }
foreach ($file in $pin.files.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $Destination $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Value) { throw "Installed compiler pin mismatch: $($file.Name)" }
}
# Retain one small compiler tree and its verified download for repeat builds.
Write-Output $compiler
