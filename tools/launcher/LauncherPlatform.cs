using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
namespace TimeSplitters.Launcher
{
    // Filesystem/package names are host adapters. Validation, transactions,
    // config parsing and download policy stay in the existing shared classes.
    public static class LauncherPlatform
    {
        // Cloud Files markers describe storage, not a redirected pathname.
        // Keep every other reparse tag (including unknown tags) rejected.
        public static bool IsCloudTag(uint tag) { return (tag & 0xFFFF0FFFu) == 0x9000001Au; }
#if MACOS
        public static IDisposable AcquireUpdateLock(string root)
        {
            return new FileStream(Core.PathIn(root, ".launcher/mac.lock"),
                FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
        }
        public static ProcessStartInfo RestartInfo(string root)
        {
            var info = new ProcessStartInfo("/usr/bin/open") { UseShellExecute = false, WorkingDirectory = root };
            foreach (var arg in new[] { "-n", Core.PathIn(root, "TimeSplitters Launcher.app"),
                "--args", "--skip-self-update", "--root", root }) info.ArgumentList.Add(arg);
            return info;
        }
        public static bool IsCloudPlaceholder(string path) { return false; }
        public const string AssetSuffix = "macos-arm64";
        public static string SessionLog(string tag){return "logs/native-play.err";}
        public const int LauncherArchiveLimit = 256*1024*1024, LauncherExpandedLimit = 512*1024*1024, LauncherFileLimit = 4096, LauncherManifestLimit = 1024*1024;
        public const string DisplaySettingsLocation = "System Settings → Displays";
        public const string UpdateManifest = "launcher-update-macos-arm64.json";
        public static bool LauncherResource(string name) { return name == "TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources" || (name.StartsWith("TimeSplitters Launcher.app/Contents/MacOS/",StringComparison.Ordinal) && name.EndsWith(".dylib",StringComparison.Ordinal)) || name.StartsWith("TimeSplitters Launcher.app/Contents/Resources/",StringComparison.Ordinal) && (name.EndsWith(".txt",StringComparison.Ordinal) || name.EndsWith(".icns",StringComparison.Ordinal)); }
        public const string GameBinary = "Bin/futureperfect_recomp";
        public const string LaunchScript = "Launch.command";
        public const string LauncherBinary = "TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher";
        public const string LauncherApplication = "TimeSplitters Launcher.app/Contents/Info.plist";
        public static bool Checkout(string root) { return File.Exists(Path.Combine(root,"tools/play-native-mac.py")); }
        public static string GamePath(string root) { return Checkout(root)?"build/macos/futureperfect_recomp":GameBinary; }
        public static string ManifestPath(string root) { return Checkout(root)?"build/macos/BUILD.json":"BUILD.json"; }
        public static string[] Settings(string root) { return Checkout(root)?new[]{"config/pc-controls.json","config/pc-display.ini","private/native-settings/runtime.conf","config/pc-discord.ini","config/pc-frame-rate.ini"}:new[]{"Settings/pc-controls.json","Settings/pc-display.ini","Settings/runtime.conf","Settings/pc-discord.ini","Settings/pc-frame-rate.ini"}; }
        public static string Data(string root) { return Checkout(root)?"private/game":"Data"; }
        public static string Saves(string root) { return Checkout(root)?"private/saves":"Saves"; }
        public static string Textures(string root) { return Checkout(root)?"private/texture-remaster/pack":"Textures"; }
        public static void CopyFile(string source,string target) {
            string temporary=target+"."+Guid.NewGuid().ToString("N")+".tmp";
            try { File.Copy(source,temporary,false); File.Move(temporary,target,true); Executable(target); }
            finally { if(File.Exists(temporary))File.Delete(temporary); }
        }
        public static void Executable(string path) { if(OperatingSystem.IsMacOS() && File.Exists(path) && (path.EndsWith("/futureperfect_recomp") || path.EndsWith("/TimeSplitters.Launcher") || path.EndsWith(".command"))) File.SetUnixFileMode(path,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute|UnixFileMode.GroupRead|UnixFileMode.GroupExecute|UnixFileMode.OtherRead|UnixFileMode.OtherExecute); }
#else
        sealed class UpdateLock : IDisposable
        {
            readonly Mutex mutex;
            public UpdateLock(string root)
            {
                mutex = new Mutex(false, Core.MutexName(root));
                bool locked;
                try { locked = mutex.WaitOne(30000); } catch (AbandonedMutexException) { locked = true; }
                if (!locked) { mutex.Dispose(); throw new IOException("Another launcher is using this installation. No update was applied."); }
            }
            public void Dispose() { mutex.ReleaseMutex(); mutex.Dispose(); }
        }
        public static IDisposable AcquireUpdateLock(string root) { return new UpdateLock(root); }
        public static ProcessStartInfo RestartInfo(string root)
        {
            return new ProcessStartInfo(Core.PathIn(root, Core.LauncherName), "--after-self-update")
                { WorkingDirectory = root, UseShellExecute = true };
        }
        [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
        struct AttributeTag { public uint Attributes, Tag; }
        [System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode, SetLastError=true)]
        static extern Microsoft.Win32.SafeHandles.SafeFileHandle CreateFileW(string path, uint access, uint share,
            IntPtr security, uint creation, uint flags, IntPtr template);
        [System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError=true)]
        [return: System.Runtime.InteropServices.MarshalAs(System.Runtime.InteropServices.UnmanagedType.Bool)]
        static extern bool GetFileInformationByHandleEx(Microsoft.Win32.SafeHandles.SafeFileHandle handle,
            int infoClass, out AttributeTag info, uint size);
        public static bool IsCloudPlaceholder(string path)
        {
            // OPEN_REPARSE_POINT prevents following a junction/symlink;
            // BACKUP_SEMANTICS permits directory handles. No content access.
            using (var handle = CreateFileW(path, 0, 7, IntPtr.Zero, 3, 0x02200000, IntPtr.Zero))
            {
                AttributeTag info;
                return !handle.IsInvalid && GetFileInformationByHandleEx(handle, 9, out info, 8) &&
                    (info.Attributes & (uint)FileAttributes.ReparsePoint) != 0 && IsCloudTag(info.Tag);
            }
        }
        public const string AssetSuffix = "windows-x64";
        public static string SessionLog(string tag){return "Logs/"+tag+".err";}
        public const int LauncherArchiveLimit = 16*1024*1024, LauncherExpandedLimit = 32*1024*1024, LauncherFileLimit = 12, LauncherManifestLimit = 16384;
        public const string DisplaySettingsLocation = "Windows Settings → System → Display → Advanced display";
        public const string UpdateManifest = "launcher-update.json";
        public static bool LauncherResource(string name) { return false; }
        public const string GameBinary = "Bin/futureperfect_recomp.exe";
        public const string LaunchScript = "Launch.ps1";
        public const string LauncherBinary = "TimeSplitters Launcher.exe";
        public const string LauncherApplication = "Bin/TimeSplitters.App.exe";
        public static bool Checkout(string root) { return false; }
        public static string GamePath(string root) { return GameBinary; }
        public static string ManifestPath(string root) { return "BUILD.json"; }
        public static string[] Settings(string root) { return new[]{"Settings/pc-controls.json","Settings/pc-display.ini","Settings/runtime.conf","Settings/pc-discord.ini","Settings/pc-frame-rate.ini"}; }
        public static string Data(string root) { return "Data"; }
        public static string Saves(string root) { return "Saves"; }
        public static string Textures(string root) { return "Textures"; }
        public static void CopyFile(string source,string target) { File.Copy(source,target,true); }
        public static void Executable(string path) { }
#endif
    }
}
