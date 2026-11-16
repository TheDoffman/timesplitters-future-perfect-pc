param(
    [Parameter(Mandatory=$true)][string]$Path,
    [Parameter(Mandatory=$true)][ValidateSet('Create','Check')][string]$Mode
)
$ErrorActionPreference = 'Stop'
# Use Windows PowerShell/.NET Framework for atomic directory creation with its
# private ACL already attached. Never relax or replace an existing directory ACL.
$fullPath = [IO.Path]::GetFullPath($Path)
$ancestor = [IO.DirectoryInfo]::new([IO.Path]::GetDirectoryName($fullPath))
while ($null -ne $ancestor) {
    if (-not $ancestor.Exists -or (($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) {
        throw 'Missing or aliased recovery ancestor'
    }
    $ancestor = $ancestor.Parent
}
$userSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
$systemSid = [Security.Principal.SecurityIdentifier]::new('S-1-5-18')
if ($Mode -eq 'Create') {
    if ([IO.File]::Exists($fullPath) -or [IO.Directory]::Exists($fullPath)) {
        throw 'Preserving existing recovery path'
    }
    $security = [Security.AccessControl.DirectorySecurity]::new()
    $security.SetOwner($userSid)
    $security.SetAccessRuleProtection($true,$false)
    foreach ($sid in @($userSid,$systemSid)) {
        $rule = [Security.AccessControl.FileSystemAccessRule]::new(
            $sid,[Security.AccessControl.FileSystemRights]::FullControl,
            ([Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit),
            [Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow)
        $security.AddAccessRule($rule)
    }
    $null = [IO.Directory]::CreateDirectory($fullPath,$security)
}
$item = Get-Item -LiteralPath $fullPath -Force
if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Aliased recovery path refused' }
$acl = $item.GetAccessControl()
if ($acl.GetOwner([Security.Principal.SecurityIdentifier]).Value -ne $userSid.Value) { throw 'Recovery owner differs' }
$rules = @($acl.GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier]))
if ($rules.Count -ne 2) { throw 'Unexpected recovery access rules' }
$seen = @{}
foreach ($rule in $rules) {
    $sid = $rule.IdentityReference.Value
    if (($sid -ne $userSid.Value -and $sid -ne $systemSid.Value) -or $seen.ContainsKey($sid) -or
        $rule.AccessControlType -ne [Security.AccessControl.AccessControlType]::Allow -or
        $rule.FileSystemRights -ne [Security.AccessControl.FileSystemRights]::FullControl) {
        throw 'Recovery access is not private'
    }
    $seen[$sid] = $true
}
if ($Mode -eq 'Create' -and -not $acl.AreAccessRulesProtected) { throw 'Recovery inheritance is not protected' }
@{schema=1;private=$true;owner=$userSid.Value;created=($Mode -eq 'Create')} | ConvertTo-Json -Compress
