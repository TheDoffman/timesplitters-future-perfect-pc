using System;
using System.IO;
using Microsoft.Win32;

namespace TimeSplitters.Launcher
{
    // Only existing per-user registrations whose location matches this installation.
    public static class InstallerIntegration
    {
        public const string UninstallKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\TimeSplitters.FuturePerfect.Launcher_is1";
        public static void RefreshVersion(string root) { RefreshVersion(root,UninstallKey); }
        internal static void RefreshVersion(string root,string keyPath)
        {
            try {
                if(!SelfUpdate.Complete(root))return;
                using(var key=Registry.CurrentUser.OpenSubKey(keyPath,true)) {
                    if(key==null)return; // Portable launchers do not register themselves.
                    string location=key.GetValue("InstallLocation") as string;
                    if(String.IsNullOrEmpty(location) || !String.Equals(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar),Path.GetFullPath(location).TrimEnd(Path.DirectorySeparatorChar),StringComparison.OrdinalIgnoreCase))return;
                    string version=SelfUpdate.InstalledVersion(root);
                    if(!String.Equals(key.GetValue("DisplayVersion") as string,version,StringComparison.Ordinal))key.SetValue("DisplayVersion",version,RegistryValueKind.String);
                }
            }catch(Exception ex){LauncherLog.Write(root,"Installer version registration: "+ex.GetType().Name);}
        }
    }
}
