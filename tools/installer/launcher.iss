; The installer owns only the three launcher files. No recursive install/delete rules.
#ifndef LauncherDir
  #error LauncherDir is required. Use tools/build-launcher-installer.ps1.
#endif
#ifndef LauncherVersion
  #error LauncherVersion is required.
#endif
#ifndef InstallerAppId
  #define InstallerAppId "TimeSplitters.FuturePerfect.Launcher"
#endif
#ifndef ShortcutName
  #define ShortcutName "TimeSplitters Launcher"
#endif
#ifndef InstallerGroup
  #define InstallerGroup "TimeSplitters Future Perfect"
#endif
#define AppFileVersion GetFileVersion(LauncherDir + "\TimeSplitters Launcher.exe")

[Setup]
AppId={#InstallerAppId}
AppName=TimeSplitters Launcher
AppVersion={#LauncherVersion}
AppVerName=TimeSplitters Launcher
AppPublisher=TimeSplitters Future Perfect PC community
AppPublisherURL=https://github.com/TheDoffman/timesplitters-future-perfect-pc
AppSupportURL=https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues
AppUpdatesURL=https://github.com/TheDoffman/timesplitters-future-perfect-pc/releases
DefaultDirName={localappdata}\Programs\TimeSplitters Future Perfect
DefaultGroupName={#InstallerGroup}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableDirPage=no
DisableProgramGroupPage=yes
DisableWelcomePage=no
WizardStyle=modern
SetupIconFile={#LauncherDir}\launcher.ico
UninstallDisplayIcon={app}\TimeSplitters Launcher.exe
UninstallFilesDir={app}\Support\Installer
OutputBaseFilename=TimeSplitters-Launcher-Setup-v{#LauncherVersion}-windows-x64
VersionInfoVersion={#AppFileVersion}
Compression=lzma2
SolidCompression=yes
CloseApplications=no
RestartApplications=no
AllowNetworkDrive=no
AllowUNCPath=no
SetupMutex=TSFP-Launcher-Setup-{#InstallerAppId}

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#LauncherDir}\TimeSplitters Launcher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LauncherDir}\Bin\TimeSplitters.App.exe"; DestDir: "{app}\Bin"; Flags: ignoreversion
Source: "{#LauncherDir}\Support\Launcher\manifest.json"; DestDir: "{app}\Support\Launcher"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#InstallerGroup}\{#ShortcutName}"; Filename: "{app}\TimeSplitters Launcher.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\TimeSplitters Launcher.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\TimeSplitters Launcher.exe"; Description: "Open TimeSplitters Launcher"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=This installs the TimeSplitters Launcher for your Windows account.%n%nThe launcher downloads published game releases and imports your supported US Xbox ISO. Existing game files, saves, settings and imported data are preserved.%n%nChoose Next to continue.
FinishedLabel=The launcher is installed. Use the Start menu or your desktop shortcut to open it.%n%nUninstall removes the launcher and its shortcuts. Downloaded game files, imported data, your ISO, saves and settings are kept.
ConfirmUninstall=Uninstall the TimeSplitters Launcher?%n%nDownloaded game files, imported data, your ISO, saves and settings will be kept.

[Code]
var
  InstallMutex: THandle;

function NativeCreateMutex(Security, InitialOwner: LongWord; Name: String): THandle;
  external 'CreateMutexW@kernel32.dll stdcall';
function NativeWait(Handle: THandle; Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function NativeReleaseMutex(Handle: THandle): Boolean;
  external 'ReleaseMutex@kernel32.dll stdcall';
function NativeCloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function NativeFileAttributes(Name: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

procedure ReleaseInstallMutex;
begin
  if InstallMutex <> 0 then begin
    NativeReleaseMutex(InstallMutex);
    NativeCloseHandle(InstallMutex);
    InstallMutex := 0;
  end;
end;

function AcquireInstallMutex: Boolean;
var
  Name: String;
  State: LongWord;
begin
  Result := True;
  if InstallMutex <> 0 then exit;
  { Identical UTF-8 path hash to Core.MutexName, shared by app/bootstrap/helpers. }
  Name := 'Local\TSFP-Launcher-' + UpperCase(GetSHA256OfString(Utf8Encode(UpperCase(RemoveBackslashUnlessRoot(ExpandFileName(ExpandConstant('{app}')))))));
  InstallMutex := NativeCreateMutex(0, 0, Name);
  if InstallMutex = 0 then begin Result := False; exit; end;
  State := NativeWait(InstallMutex, 0);
  Result := (State = 0) or (State = $80);
  if not Result then begin NativeCloseHandle(InstallMutex); InstallMutex := 0; end;
end;

function PathIsSafe(Path: String): Boolean;
var
  Attributes: LongWord;
  Parent: String;
begin
  Result := False;
  Path := RemoveBackslashUnlessRoot(ExpandFileName(Path));
  while Path <> '' do begin
    Attributes := NativeFileAttributes(Path);
    if (Attributes <> $FFFFFFFF) and ((Attributes and $400) <> 0) then exit;
    Parent := ExtractFileDir(Path);
    if Parent = Path then break;
    Path := Parent;
  end;
  Result := True;
end;

function CheckInstallation: String;
var
  Root: String;
  Locator, Service, Processes: Variant;
begin
  Result := '';
  Root := ExpandConstant('{app}');
  if not PathIsSafe(Root + '\TimeSplitters Launcher.exe') or
     not PathIsSafe(Root + '\Bin\TimeSplitters.App.exe') or
     not PathIsSafe(Root + '\Support\Launcher\manifest.json') or
     not PathIsSafe(Root + '\Support\Installer') or
     not PathIsSafe(Root + '\.launcher') then begin
    Result := 'Choose a local installation folder without symbolic links or redirected folders.'; exit;
  end;
  if not AcquireInstallMutex then begin
    Result := 'Close the launcher before installing or uninstalling it.'; exit;
  end;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('', 'root\CIMV2');
    Processes := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name=''futureperfect_recomp.exe''');
    if Processes.Count > 0 then Result := 'Close the game before installing or uninstalling the launcher.';
  except
    Result := 'Windows process discovery is unavailable. Setup cannot safely check whether the game is running.';
  end;
  if Result <> '' then exit;
  if DirExists(Root + '\.launcher\self-transaction') or DirExists(Root + '\.launcher\transaction') or
     DirExists(Root + '\.launcher\repair-transaction') or FileExists(Root + '\.launcher\pending\release.json') or
     FileExists(Root + '\.launcher\self-pending\release.json') then
    Result := 'An update needs recovery. Open the installed launcher to finish recovery, then run Setup again.';
end;

function IsNewer(const Filename: String): Boolean;
var
  MS, LS: Cardinal;
begin
  Result := False;
  if GetVersionNumbers(Filename, MS, LS) then
    Result := (MS > {#VersionMS}) or ((MS = {#VersionMS}) and (LS > {#VersionLS}));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := CheckInstallation;
  if (Result = '') and not IsDotNetInstalled(net48, 0) then
    Result := 'Install Microsoft .NET Framework 4.8, then run Setup again.';
  if (Result = '') and (IsNewer(ExpandConstant('{app}\TimeSplitters Launcher.exe')) or
      IsNewer(ExpandConstant('{app}\Bin\TimeSplitters.App.exe'))) then
    Result := 'A newer launcher is already installed. Open it or download the latest installer; your files were kept.';
  if Result <> '' then ReleaseInstallMutex;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then ReleaseInstallMutex;
end;

procedure DeinitializeSetup;
begin
  ReleaseInstallMutex;
end;

function InitializeUninstall: Boolean;
var
  Error: String;
begin
  Error := CheckInstallation;
  Result := Error = '';
  if not Result then begin
    ReleaseInstallMutex;
    SuppressibleMsgBox(Error, mbError, MB_OK, IDOK);
  end;
end;

procedure DeinitializeUninstall;
begin
  ReleaseInstallMutex;
end;
