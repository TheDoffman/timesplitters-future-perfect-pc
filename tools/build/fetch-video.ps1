# The runtime compiles against these headers and loads the DLLs at runtime.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$destination = Join-Path $root 'external\ts3FPrecomp\third_party\ffmpeg'
if (Test-Path -LiteralPath (Join-Path $destination 'include\libavcodec\avcodec.h')) { return }
if (Test-Path -LiteralPath $destination) { throw 'FFmpeg destination exists but is incomplete; inspect it first.' }
$downloadRoot = Join-Path $root 'private\downloads'
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
$archive = Join-Path $downloadRoot 'ffmpeg.zip'
$url = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip'
$expected = '1689DA96399F372CFEE985B3F2B9D74685892078059BC37764388F49E3118BDB'
if (!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $url -OutFile $archive }
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw 'FFmpeg archive differs from the validated build. Review the updated release before changing the pinned checksum.'
}
$unpackRoot = Join-Path $downloadRoot 'ffmpeg'
Expand-Archive -LiteralPath $archive -DestinationPath $unpackRoot -Force
$unpacked = @(Get-ChildItem -LiteralPath $unpackRoot -Directory)
if ($unpacked.Count -ne 1) { throw 'Unexpected FFmpeg archive layout.' }
$sourcePath = [IO.Path]::GetFullPath($unpacked[0].FullName)
$allowedRoot = [IO.Path]::GetFullPath($unpackRoot) + [IO.Path]::DirectorySeparatorChar
if (!$sourcePath.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'FFmpeg source is outside the download directory.' }
Move-Item -LiteralPath $sourcePath -Destination $destination
Write-Output 'FFmpeg 8.1 headers and shared libraries are ready.'
