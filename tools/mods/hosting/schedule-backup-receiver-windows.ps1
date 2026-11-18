param(
    [Parameter(Mandatory=$true)][ValidateSet('Identity','Install','Run','Status','Remove')][string]$Operation,
    [string]$Spec,
    [string]$Sha256
)
$ErrorActionPreference = 'Stop'
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
if ($Operation -eq 'Identity') {
    if ($Spec -or $Sha256) { throw 'Unexpected identity arguments' }
    @{schema=1;sid=$sid} | ConvertTo-Json -Compress
    exit 0
}
if ($Sha256 -notmatch '\A[0-9a-f]{64}\z') { throw 'Supply the schedule fingerprint' }
$bytes = [IO.File]::ReadAllBytes($Spec)
if ($bytes.Length -gt 65536) { throw 'Schedule exceeds budget' }
$hash = [Security.Cryptography.SHA256]::Create()
try { $actual = ([BitConverter]::ToString($hash.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() }
finally { $hash.Dispose() }
if ($actual -ne $Sha256) { throw 'Schedule fingerprint differs' }
$value = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
if ($value.schema -ne 1 -or $value.type -ne 'tsfp-backup-schedule-v1' -or
    $value.job_id -notmatch '\A[0-9a-f]{32}\z' -or $value.sid -ne $sid) { throw 'Schedule authority differs' }
$name = 'TSFP Workshop Backup ' + $value.job_id
$description = 'TSFP backup receiver v1:' + $value.job_id + ':' + $Sha256
$service = New-Object -ComObject Schedule.Service
$service.Connect()
$folder = $service.GetFolder('\')

function Assert-Definition($definition) {
    if ($definition.RegistrationInfo.Description -ne $description -or
        $definition.Principal.UserId -ne $sid -or $definition.Principal.LogonType -ne 3 -or
        $definition.Principal.RunLevel -ne 0 -or $definition.Actions.Count -ne 1 -or
        $definition.Triggers.Count -ne 2) { throw 'Preserving foreign task definition' }
    $action = $definition.Actions.Item(1)
    if ($action.Type -ne 0 -or $action.Path -ne $value.executable -or
        $action.Arguments -ne $value.arguments -or $action.WorkingDirectory -ne $value.runtime) {
        throw 'Preserving changed task action'
    }
    $settings = $definition.Settings
    if ($settings.MultipleInstances -ne 2 -or $settings.ExecutionTimeLimit -ne 'PT35M' -or
        -not $settings.StartWhenAvailable -or $settings.WakeToRun -or -not $settings.Hidden -or
        -not $settings.AllowDemandStart -or -not $settings.AllowHardTerminate -or
        $settings.DisallowStartIfOnBatteries -or $settings.StopIfGoingOnBatteries -or
        $settings.RunOnlyIfNetworkAvailable -or $settings.RunOnlyIfIdle -or
        $settings.RestartCount -ne 0) { throw 'Preserving changed task settings' }
    $daily = $definition.Triggers.Item(1)
    $logon = $definition.Triggers.Item(2)
    if ($daily.Type -ne 2 -or $daily.DaysInterval -ne 1 -or $daily.StartBoundary -ne $value.start_boundary -or
        $daily.Repetition.Interval -ne 'PT1H' -or $daily.Repetition.Duration -ne 'P1D' -or
        $daily.Repetition.StopAtDurationEnd -or -not $daily.Enabled -or
        $daily.EndBoundary -or $logon.StartBoundary -or $logon.EndBoundary -or $logon.Repetition.Interval -or
        $logon.Repetition.Duration -or $logon.Type -ne 9 -or $logon.UserId -ne $sid -or
        $logon.Delay -ne 'PT1M' -or -not $logon.Enabled) {
        throw 'Preserving changed task triggers'
    }
}

function Assert-Task($task) {
    Assert-Definition $task.Definition
    $security = [Security.AccessControl.RawSecurityDescriptor]::new($task.GetSecurityDescriptor(5))
    if ($security.Owner.Value -ne $sid -or $security.DiscretionaryAcl.Count -ne 2) {
        throw 'Preserving foreign task security'
    }
    $seen = @{}
    foreach ($ace in $security.DiscretionaryAcl) {
        $principal = $ace.SecurityIdentifier.Value
        if (($principal -ne $sid -and $principal -ne 'S-1-5-18') -or $seen.ContainsKey($principal) -or
            $ace.AceQualifier -ne [Security.AccessControl.AceQualifier]::AccessAllowed -or
            $ace.AccessMask -ne 0x1f01ff -or $ace.AceFlags -ne [Security.AccessControl.AceFlags]::None) {
            throw 'Preserving changed task security'
        }
        $seen[$principal] = $true
    }
}

if ($Operation -eq 'Install') {
    # TASK_CREATE only. No overwrite, update or registration trigger. The first
    # timer is deliberately in the future; manual native acceptance is explicit.
    $definition = $service.NewTask(0)
    $definition.RegistrationInfo.Description = $description
    $definition.Principal.UserId = $sid
    $definition.Principal.LogonType = 3
    $definition.Principal.RunLevel = 0
    $settings = $definition.Settings
    $settings.MultipleInstances = 2
    $settings.ExecutionTimeLimit = 'PT35M'
    $settings.StartWhenAvailable = $true
    $settings.WakeToRun = $false
    $settings.Hidden = $true
    $settings.AllowDemandStart = $true
    $settings.AllowHardTerminate = $true
    $settings.DisallowStartIfOnBatteries = $false
    $settings.StopIfGoingOnBatteries = $false
    $settings.RunOnlyIfNetworkAvailable = $false
    $settings.RunOnlyIfIdle = $false
    $settings.RestartCount = 0
    $action = $definition.Actions.Create(0)
    $action.Path = $value.executable
    $action.Arguments = $value.arguments
    $action.WorkingDirectory = $value.runtime
    $daily = $definition.Triggers.Create(2)
    $daily.StartBoundary = $value.start_boundary
    $daily.DaysInterval = 1
    $daily.Repetition.Interval = 'PT1H'
    $daily.Repetition.Duration = 'P1D'
    $daily.Repetition.StopAtDurationEnd = $false
    $daily.Enabled = $true
    $logon = $definition.Triggers.Create(9)
    $logon.UserId = $sid
    $logon.Delay = 'PT1M'
    $logon.Enabled = $true
    Assert-Definition $definition
    $sddl = 'O:' + $sid + 'D:P(A;;FA;;;SY)(A;;FA;;;' + $sid + ')'
    $task = $folder.RegisterTaskDefinition($name,$definition,18,$sid,$null,3,$sddl)
    Assert-Task $task
} else {
    $task = $folder.GetTask($name)
    Assert-Task $task
    if ($Operation -eq 'Run') { $null = $task.Run($null) }
    if ($Operation -eq 'Remove') {
        $task.Stop(0)
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $task = $folder.GetTask($name)
            Assert-Task $task
            if ($task.State -ne 4) { break }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        if ($task.State -eq 4) { throw 'Own task has not stopped; preserving registration' }
        $folder.DeleteTask($name,0)
        @{schema=1;phase='backup-schedule-removed';job_id=$value.job_id} | ConvertTo-Json -Compress
        exit 0
    }
}
@{schema=1;phase='backup-schedule';job_id=$value.job_id;state=[int]$task.State;
  last_result=[long]$task.LastTaskResult;enabled=[bool]$task.Enabled;
  last_run=$task.LastRunTime.ToUniversalTime().ToString('o');
  next_run=$task.NextRunTime.ToUniversalTime().ToString('o')} | ConvertTo-Json -Compress
