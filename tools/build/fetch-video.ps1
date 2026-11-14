# The runtime compiles against these headers and loads the DLLs at runtime.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$destination = Join-Path $root 'external\ts3FPrecomp\third_party\ffmpeg'
if (Test-Path -LiteralPath (Join-Path $destination 'include\libavcodec\avcodec.h')) { return }
if (Test-Path -LiteralPath $destination) { throw 'FFmpeg destination exists but is incomplete; inspect it first.' }
$downloadRoot = Join-Path $root 'private\downloads'
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
$archive = Join-Path $downloadRoot 'ffmpeg-n8.1.3-35-gf1acb0a266-win64-lgpl-shared-8.1.zip'
# Pin both the dated upstream release and its published SHA-256. The rolling
# latest asset changes in place and cannot reproduce a previously verified build.
$url = 'https://github.com/FFmpeg-Builds/FFmpeg-Builds/releases/download/autobuild-2026-11-13-12-43/ffmpeg-n8.1.3-35-gf1acb0a266-win64-lgpl-shared-8.1.zip'
$expected = '57CB10E4703B88EDF55AA49E94DEE19FD12F555F82ED3493A8CD70C847699996'
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
Write-Output 'FFmpeg n8.1.3-35-gf1acb0a266 headers and shared libraries are ready.'
