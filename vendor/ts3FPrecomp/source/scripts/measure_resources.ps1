<#
.SYNOPSIS
    Sample Windows performance counters for one process over a fixed window.

.DESCRIPTION
    Written to compare a statically recompiled title against an emulator running
    the same game, but there is nothing title-specific in it: give it a process
    name and a window and it returns CPU, memory and GPU for that process.

    It waits for the process to appear, waits out -DelaySeconds (use this to skip
    start-up and menus and land the window in gameplay), then samples once a
    second for -Seconds seconds.

    Counters, all per-process:
      \Process(<name>)\% Processor Time        -- 100 = one logical core saturated
      \Process(<name>)\Working Set             -- resident, shared pages included
      \Process(<name>)\Working Set - Private   -- resident and not shareable
      \Process(<name>)\Private Bytes           -- committed and not shareable
      \GPU Engine(pid_<pid>_*)\Utilization Percentage
      \GPU Process Memory(pid_<pid>_*)\Local Usage and \Dedicated Usage

    GPU Engine has one instance per engine (3D, Copy, VideoDecode, ...) per
    adapter, each a percentage of that engine. They are reported three ways
    because none of them is "the" GPU figure: the sum over engines, the largest
    single engine (what Task Manager's GPU column shows), and the 3D engine on
    its own.

    The first sample of a rate counter is meaningless -- it has no previous raw
    value to difference against -- so one extra sample is taken and discarded.

.PARAMETER ProcessName
    Executable name without .exe, as the Process counter set spells it. When two
    copies of the same title are running -- easily done on a shared machine --
    this picks whichever the OS lists first, so prefer -ProcessId.

.PARAMETER ProcessId
    Measure this pid exactly, instead of searching by name. Use it when the
    caller launched the process and knows which one it means.

.PARAMETER Seconds
    Length of the measurement window in seconds; one sample per second.

.PARAMETER DelaySeconds
    Delay between the process appearing and the window opening. Default 0.

.PARAMETER OutCsv
    Optional path for the per-sample rows. The summary always goes to stdout.

.PARAMETER WaitTimeoutSeconds
    How long to wait for the process to appear. Default 120.

.EXAMPLE
    powershell -File scripts\measure_resources.ps1 -ProcessName timesplitters2_recomp `
        -DelaySeconds 80 -Seconds 50 -OutCsv runs\ts2-capped.csv
#>
param(
    [string] $ProcessName = "",
    [int]    $ProcessId = 0,
    [Parameter(Mandatory = $true)][int] $Seconds,
    [int]    $DelaySeconds = 0,
    [string] $OutCsv = "",
    [int]    $WaitTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
if ($ProcessName -eq "" -and $ProcessId -eq 0) {
    Write-Error "give -ProcessName or -ProcessId"; exit 1
}
$name = $ProcessName -replace '\.exe$', ''

# --- find the process -------------------------------------------------------
$proc = $null
if ($ProcessId -ne 0) {
    $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($null -eq $proc) { Write-Error "no process with pid $ProcessId"; exit 1 }
    $name = $proc.ProcessName
} else {
    $deadline = (Get-Date).AddSeconds($WaitTimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $found = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
        if ($found.Count -gt 1) {
            Write-Warning "$($found.Count) processes named '$name'; measuring pid $($found[0].Id). Use -ProcessId."
        }
        if ($found.Count -gt 0) { $proc = $found[0]; break }
        Start-Sleep -Milliseconds 250
    }
    if ($null -eq $proc) { Write-Error "process '$name' did not appear within $WaitTimeoutSeconds s"; exit 1 }
}
$pidNum = $proc.Id
Write-Host "[measure] $name pid $pidNum found; waiting $DelaySeconds s before the window"

if ($DelaySeconds -gt 0) { Start-Sleep -Seconds $DelaySeconds }
if ($proc.HasExited) { Write-Error "process exited before the window opened"; exit 1 }

# --- resolve the Process counter instance ----------------------------------
# Several processes can share a name; the instance that belongs to this pid is
# the one whose "ID Process" counter matches.
$instance = $name
$idSamples = (Get-Counter "\Process(*)\ID Process" -ErrorAction SilentlyContinue).CounterSamples
foreach ($s in $idSamples) {
    if ([int]$s.CookedValue -eq $pidNum) { $instance = $s.InstanceName; break }
}
Write-Host "[measure] counter instance '$instance'; sampling $Seconds s"

$paths = @(
    "\Process($instance)\% Processor Time",
    "\Process($instance)\Working Set",
    "\Process($instance)\Working Set - Private",
    "\Process($instance)\Private Bytes",
    "\GPU Engine(*)\Utilization Percentage",
    "\GPU Process Memory(*)\Local Usage",
    "\GPU Process Memory(*)\Dedicated Usage"
)

$cores = [Environment]::ProcessorCount
$rows = New-Object System.Collections.ArrayList
$pidTag = "pid_${pidNum}_"
# Which adapter and which engines the process actually used. On a laptop with
# two GPUs this is the difference between measuring the discrete card and
# measuring the integrated one, and the numbers alone do not say which.
$engines = New-Object System.Collections.Generic.HashSet[string]

# One call so the rate counters difference correctly; the first sample is dropped.
$sets = Get-Counter -Counter $paths -SampleInterval 1 -MaxSamples ($Seconds + 1) -ErrorAction Continue
$first = $true
foreach ($set in $sets) {
    if ($first) { $first = $false; continue }
    $cpu = 0.0; $ws = 0.0; $wsp = 0.0; $pb = 0.0
    $gpuSum = 0.0; $gpuMax = 0.0; $gpu3d = 0.0
    $gpuLocal = 0.0; $gpuDedicated = 0.0
    foreach ($s in $set.CounterSamples) {
        $p = $s.Path
        if ($p -like "*\% processor time") { $cpu = $s.CookedValue }
        elseif ($p -like "*\working set - private") { $wsp = $s.CookedValue }
        elseif ($p -like "*\private bytes") { $pb = $s.CookedValue }
        elseif ($p -like "*\working set") { $ws = $s.CookedValue }
        elseif ($p -like "*gpu engine*") {
            if ($s.InstanceName -like "*$pidTag*") {
                $gpuSum += $s.CookedValue
                if ($s.CookedValue -gt $gpuMax) { $gpuMax = $s.CookedValue }
                if ($s.InstanceName -like "*engtype_3D*") { $gpu3d += $s.CookedValue }
                if ($s.CookedValue -gt 0) { [void]$engines.Add($s.InstanceName) }
            }
        }
        elseif ($p -like "*gpu process memory*local usage") {
            if ($s.InstanceName -like "*$pidTag*") { $gpuLocal += $s.CookedValue }
        }
        elseif ($p -like "*gpu process memory*dedicated usage") {
            if ($s.InstanceName -like "*$pidTag*") { $gpuDedicated += $s.CookedValue }
        }
    }
    [void]$rows.Add([pscustomobject]@{
        Timestamp        = $set.Timestamp
        CpuPercentOfCore = [math]::Round($cpu, 2)
        CpuPercentOfAll  = [math]::Round($cpu / $cores, 2)
        WorkingSetMB     = [math]::Round($ws / 1MB, 1)
        PrivateWSMB      = [math]::Round($wsp / 1MB, 1)
        PrivateBytesMB   = [math]::Round($pb / 1MB, 1)
        GpuSumPct        = [math]::Round($gpuSum, 2)
        GpuMaxEnginePct  = [math]::Round($gpuMax, 2)
        Gpu3DPct         = [math]::Round($gpu3d, 2)
        GpuLocalMB       = [math]::Round($gpuLocal / 1MB, 1)
        GpuDedicatedMB   = [math]::Round($gpuDedicated / 1MB, 1)
    })
}

if ($rows.Count -eq 0) { Write-Error "no samples collected"; exit 1 }
if ($OutCsv -ne "") {
    $dir = Split-Path -Parent $OutCsv
    if ($dir -ne "" -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $rows | Export-Csv -Path $OutCsv -NoTypeInformation -Encoding utf8
    Write-Host "[measure] $($rows.Count) samples -> $OutCsv"
}

function Stat($field) {
    $vals = $rows | ForEach-Object { $_.$field }
    $m = $vals | Measure-Object -Average -Maximum -Minimum
    "{0,-18} mean {1,9:N2}  max {2,9:N2}  min {3,9:N2}" -f $field, $m.Average, $m.Maximum, $m.Minimum
}

Write-Host ""
Write-Host "=== $name (pid $pidNum), $($rows.Count) samples at 1 s, $cores logical cores ==="
foreach ($f in @("CpuPercentOfCore","CpuPercentOfAll","WorkingSetMB","PrivateWSMB",
                 "PrivateBytesMB","GpuSumPct","GpuMaxEnginePct","Gpu3DPct","GpuLocalMB","GpuDedicatedMB")) {
    Write-Host (Stat $f)
}
if ($engines.Count -gt 0) {
    Write-Host "GPU engine instances with work:"
    foreach ($e in ($engines | Sort-Object)) { Write-Host "  $e" }
}
