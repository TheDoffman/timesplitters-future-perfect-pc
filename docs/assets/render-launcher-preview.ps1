#requires -Version 5.1
<#
Render the documented v1.8.0 Windows launcher with isolated Stable sample data.
Run with Windows PowerShell -STA. Supply the published application recorded
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
$expectedHash = 'eb9e3aad6531842b8b5ebb61415f7b5e8afa78f156eec18e92c2ee6d5addecb9'
if ((Get-FileHash -LiteralPath $ApplicationPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'Application does not match the documented v1.8.0 preview build.'
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
    $ui.Window.FindName('InstalledLabel').Text = 'v0.7.2 (sample)'
    $ui.Window.FindName('ChannelBadge').Text = [char]0x25cf + '  STABLE CHANNEL'
    # The released redesign is a presentation adapter over the existing controls.
    # Supply only in-memory sample state; no real installation or game is used.
    $flags = [Reflection.BindingFlags]'Instance,NonPublic'
    $ui.GetType().GetField('installed', $flags).SetValue($ui, 'v0.7.2 (sample)')
    $ui.GetType().GetField('gameInstalled', $flags).SetValue($ui, $true)
    $ui.GetType().GetField('validData', $flags).SetValue($ui, $true)
    $appearance = New-Object TimeSplitters.Launcher.LauncherAppearance($ui)
    $visual = $ui.Window.Content
    # Off-screen windows have no ActualWidth; use the normal desktop layout size.
    $visual.Child.Width = 1184
    $visual.Child.Height = 684
    $visual.Measure([Windows.Size]::new(1184, 684))
    $visual.Arrange([Windows.Rect]::new(0, 0, 1184, 684))
    $visual.UpdateLayout()
    $bitmap = [Windows.Media.Imaging.RenderTargetBitmap]::new(1184, 684, 96, 96, [Windows.Media.PixelFormats]::Pbgra32)
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
