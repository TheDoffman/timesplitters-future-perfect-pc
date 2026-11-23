#requires -Version 5.1
<#
Render the documented v1.7.0 Windows launcher with isolated Stable sample data.
Run with Windows PowerShell -STA. Supply the source-built application recorded
in README.md beside this script. No game, updater or live network check runs.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ApplicationPath,
    [Parameter(Mandatory = $true)][string]$PreviewRoot,
    [string]$Output
)
$ErrorActionPreference = 'Stop'
if (!$Output) { $Output = Join-Path $PSScriptRoot 'launcher-preview.png' }
if ([Threading.Thread]::CurrentThread.ApartmentState -ne 'STA') {
    throw 'Run this script with Windows PowerShell -STA.'
}
$expectedHash = 'dcb32a5a6f1e673e08afc9c9776815b4d1c4a23f7b8b1eb44fbdc65750c04436'
if ((Get-FileHash -LiteralPath $ApplicationPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'Application does not match the documented v1.7.0 preview build.'
}
$PreviewRoot = [IO.Path]::GetFullPath($PreviewRoot)
if (Test-Path -LiteralPath $PreviewRoot) {
    throw 'PreviewRoot must be a new empty directory, separate from any installation.'
}
New-Item -ItemType Directory -Path $PreviewRoot | Out-Null
Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase
$assembly = [Reflection.Assembly]::LoadFrom((Resolve-Path -LiteralPath $ApplicationPath).Path)
$app = New-Object Windows.Application
$ui = New-Object TimeSplitters.Launcher.LauncherWindow($PreviewRoot, $true)
try {
    # Change only the in-memory preview fixture using the launcher's own picker.
    $releases = [Array]::CreateInstance($assembly.GetType('TimeSplitters.Launcher.Release'), 0)
    $setChannels = $ui.GetType().GetMethod('SetChannels', [Reflection.BindingFlags]'Instance,NonPublic')
    $null = $setChannels.Invoke($ui, [object[]]@($releases, 'stable'))
    $ui.Window.FindName('InstalledLabel').Text = 'v0.7.0 (sample)'
    $ui.Window.FindName('ChannelBadge').Text = [char]0x25cf + '  STABLE CHANNEL'
    $visual = $ui.Window.Content
    $visual.Background = $ui.Window.Background
    $visual.Measure([Windows.Size]::new(1152, 772))
    $visual.Arrange([Windows.Rect]::new(0, 0, 1152, 772))
    $visual.UpdateLayout()
    $bitmap = [Windows.Media.Imaging.RenderTargetBitmap]::new(1152, 772, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $encoder = New-Object Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = [IO.File]::Create([IO.Path]::GetFullPath($Output))
    try { $encoder.Save($stream) } finally { $stream.Dispose() }
    Get-FileHash -LiteralPath $Output -Algorithm SHA256
} finally {
    $ui.Window.Close()
    $app.Shutdown()
}
