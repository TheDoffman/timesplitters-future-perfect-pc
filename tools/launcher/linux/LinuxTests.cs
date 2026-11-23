using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.IO.Compression;

namespace TimeSplitters.Launcher
{
    public static class LinuxTests
    {
        sealed class CaptureProgress : IProgress<string> {
            public readonly List<string> Messages=new List<string>();
            public Action<string> OnReport;
            public void Report(string message) {Messages.Add(message);OnReport?.Invoke(message);}
        }
        static void UpdateHelper(string parentRoot) {
            string root=Path.Combine(parentRoot,"update helper Ω");Directory.CreateDirectory(root);
            LauncherPlatform.CopyFile(Environment.ProcessPath,Core.PathIn(root,Core.LauncherName));
            string helper=Core.PathIn(root,".launcher/update-helper");Directory.CreateDirectory(Path.GetDirectoryName(helper));
            LauncherPlatform.CopyFile(Environment.ProcessPath,helper);File.SetUnixFileMode(helper,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
            string saves=Core.PathIn(root,"Saves/keep");Directory.CreateDirectory(Path.GetDirectoryName(saves));File.WriteAllText(saves,"personal save");
            LauncherRestartChecks.Stage(root,"v1.0.0");Core.InstallPending(root);
            LauncherRestartChecks.Stage(root,"v1.1.0");
            var start=new ProcessStartInfo(helper){UseShellExecute=false,WorkingDirectory=root};
            using(var parent=Process.Start(new ProcessStartInfo("/bin/sleep","5"){UseShellExecute=false})) {
                foreach(var arg in new[]{"--test-game-helper",root,parent.Id.ToString()})start.ArgumentList.Add(arg);
                using(var child=Process.Start(start)) {
                    try {
                        var deadline=Stopwatch.StartNew();string capture=Core.PathIn(root,"update-progress-preview.png");
                        while(!File.Exists(capture) && !child.HasExited && deadline.ElapsedMilliseconds<4000)System.Threading.Thread.Sleep(50);
                        Need(File.Exists(capture)&&!child.HasExited&&!parent.HasExited,"Updater did not remain visible while waiting for its parent");
                        Need(child.WaitForExit(30000)&&child.ExitCode==0,"Visible update helper failed or timed out");
                    } finally {if(!child.HasExited){child.Kill(true);child.WaitForExit();}}
                }
            }
            Need(Core.InstalledVersion(root)=="v1.1.0" && !Directory.Exists(Core.PathIn(root,".launcher/pending")),"Visible helper did not install and clean up");
            Need(File.ReadAllText(saves)=="personal save","Visible helper changed saves");
            Need(File.Exists(Core.PathIn(root,"game-update-preview.png")),"Visible helper did not reopen the launcher");
            string log=File.ReadAllText(Core.PathIn(root,".launcher/launcher.log"));
            foreach(string phase in new[]{"Verifying staged files","Backing up saves","Backing up installed files","Installing files","Cleaning up installation files","Reopening the launcher"})Need(log.Contains("Update phase: "+phase),"Visible helper missing phase "+phase);
            string evidence=Environment.GetEnvironmentVariable("TSFP_TEST_UPDATE_CAPTURE");
            Need(File.Exists(Core.PathIn(root,"update-installing-preview.png")),"Installer reports did not reach the visible window");
            if(!String.IsNullOrEmpty(evidence))File.Copy(Core.PathIn(root,"update-installing-preview.png"),evidence,true);
            LauncherRestartChecks.Stage(root,"v1.2.0");
            File.AppendAllText(Core.PathIn(root,".launcher/pending/payload/"+LauncherPlatform.GameBinary),"tampered");
            var rejected=new ProcessStartInfo(helper){UseShellExecute=false,WorkingDirectory=root};
            foreach(var arg in new[]{"--test-game-helper",root,Int32.MaxValue.ToString()})rejected.ArgumentList.Add(arg);
            using(var child=Process.Start(rejected)) {
                try {Need(child.WaitForExit(30000)&&child.ExitCode==1,"Tampered helper update was accepted or hung");}
                finally {if(!child.HasExited){child.Kill(true);child.WaitForExit();}}
            }
            Need(Core.InstalledVersion(root)=="v1.1.0" && File.ReadAllText(saves)=="personal save","Rejected helper update damaged installed files");
            Need(File.ReadAllText(Core.PathIn(root,".launcher/last-result.txt")).StartsWith("Update failed:"),"Visible helper failure was not retained");
            Console.WriteLine("PASS visible Linux game updater: parent wait, progress window, full install, cleanup, preservation and rendered launcher restart");
        }
        static void Need(bool value,string message) { Core.Require(value,message); }
        static void Reject(Action action) { try { action(); } catch(InvalidDataException) { return; } throw new Exception("Expected rejection"); }
        public static int VerifyPackage(string archive,string version)
        {
            string root=Path.Combine(Path.GetTempPath(),"tsfp-linux-package-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
            try {
                string install=Path.Combine(root,"installation with spaces");Directory.CreateDirectory(install);
                Directory.CreateDirectory(Path.Combine(install,"Saves"));File.WriteAllText(Path.Combine(install,"Saves/keep"),"preserved");
                File.WriteAllText(Path.Combine(install,"BUILD.json"),"preserved game metadata");
                var changes=SelfUpdate.Unpack(archive,Path.Combine(root,"payload"),version);
                Core.ApplyLauncher(install,changes,null);Need(SelfUpdate.Complete(install),"Installed package failed verification");
                string app=Core.PathIn(install,Core.AppPath);File.AppendAllText(app,"corruption");Need(!SelfUpdate.Complete(install),"Corrupt launcher accepted");
                string pending=Core.PathIn(install,".launcher/self-pending");Directory.CreateDirectory(pending);
                SelfUpdate.Unpack(archive,Path.Combine(pending,"payload"),version);
                Core.Write(Path.Combine(pending,"release.json"),Json.Encode(new{version=version,repair=true}));
                string helper=Core.PathIn(install,".launcher/linux-self-helper");LauncherPlatform.CopyFile(Environment.ProcessPath,helper);
                File.SetUnixFileMode(helper,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
                using(var parent=Process.Start(new ProcessStartInfo("/bin/sleep","1"){UseShellExecute=false})) {
                    var info=new ProcessStartInfo(helper){UseShellExecute=false};foreach(var arg in new[]{"--test-self-helper",install,parent.Id.ToString()})info.ArgumentList.Add(arg);
                    using(var child=Process.Start(info)) { if(!child.WaitForExit(45000)){child.Kill(true);throw new Exception("Package helper timed out");}Need(child.ExitCode==0,"Package repair/restart failed"); }
                }
                Need(SelfUpdate.Complete(install)&&File.Exists(Path.Combine(install,"self-update-preview.png")),"Repaired launcher did not restart and render");
                Need(File.ReadAllText(Path.Combine(install,"Saves/keep"))=="preserved"&&File.ReadAllText(Path.Combine(install,"BUILD.json"))=="preserved game metadata","Self update changed game/user files");
                Console.WriteLine("Linux package installed, corruption rejected, repaired through copied helper and restarted/rendered; saves and game metadata preserved.");return 0;
            } catch(Exception error) { Console.Error.WriteLine(error);return 1; }
            finally { Directory.Delete(root,true); }
        }
        public static int Run()
        {
            string root=Path.Combine(Path.GetTempPath(),"tsfp-linux-launcher-"+Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try {
                // /dev/shm is a separate filesystem on the Linux test hosts.
                // A symlinked installation must query its target, not root's disk.
                string mount=Environment.GetEnvironmentVariable("TSFP_TEST_SPACE_MOUNT")??"/dev/shm";
                Need(Directory.Exists(mount),"Mounted filesystem fixture missing");
                string linked=Path.Combine(root,"mounted storage");Directory.CreateSymbolicLink(linked,mount);
                try {
                    long expected=new DriveInfo(mount).AvailableFreeSpace;
                    long actual=TsfpDiskSpace.Available(Path.Combine(linked,"not-created","Data"));
                    Need(Math.Abs(actual-expected)<1024*1024,"Space check ignored destination mount/symlink");
                    Reject(()=>Core.CheckSpace(linked,actual+1024*1024));
                    Core.CheckSpace(linked,0);
                    Console.WriteLine("PASS destination filesystem space checks, including nested paths and symlinks");
                } finally {Directory.Delete(linked);}
                LauncherRestartChecks.Run();
                UpdateHelper(root);
                LauncherTextureChecks.Run();
                Need(LauncherPlatform.AssetSuffix=="linux-x64","Wrong release platform");
                Directory.CreateDirectory(Path.Combine(root,".launcher"));
                using(var held=LauncherPlatform.AcquireUpdateLock(root)) {
                    bool blocked=false;try { using(var second=LauncherPlatform.AcquireUpdateLock(root)) {} } catch(IOException) { blocked=true; }
                    Need(blocked,"Concurrent updater lock accepted");
                    using(var nested=LauncherPlatform.AcquireSaveLock(root,true)) {}
                }
                Need(Program.FindRoot(new[]{"--root",root})==root,"Explicit installation root lost");
                Directory.CreateDirectory(Path.Combine(root,"tools/build"));
                File.WriteAllText(Path.Combine(root,"tools/build/build-native-linux.py"),"fixture");
                Need(LauncherPlatform.Checkout(root)&&LauncherPlatform.GamePath(root)=="build/linux/game/futureperfect_recomp","Checkout routing failed");
                Need(Program.FindRoot(Array.Empty<string>(),Path.Combine(root,"build/linux-launcher/package"))==root,"Checkout discovery failed");
                NativeHost.Prepare(root);
                var info=GameSettings.StartInfo(root);
                Need(info.FileName==Path.Combine(root,"build/linux/game/futureperfect_recomp"),"Wrong game target");
                Need(info.Environment["RECOMP_SAVE_DIR"]==Path.Combine(root,"private/saves"),"Wrong save target");
                Need(info.Environment["TSFP_DISPLAY_CONFIG"]==Path.Combine(root,"config/pc-display.ini"),"Wrong display target");
                Need(!info.Environment.ContainsKey("VK_DRIVER_FILES")&&!info.Environment.ContainsKey("DYLD_LIBRARY_PATH"),"Foreign driver override leaked");
                Need(!Directory.Exists(Path.Combine(root,"Settings")),"Created installed settings in source checkout");
                string game=Path.Combine(root,"build/linux/game/futureperfect_recomp");Directory.CreateDirectory(Path.GetDirectoryName(game));
                byte[] elf=new byte[64];elf[0]=127;elf[1]=69;elf[2]=76;elf[3]=70;elf[4]=2;elf[5]=1;elf[18]=62;File.WriteAllBytes(game,elf);
                Action manifest=()=>File.WriteAllText(Path.Combine(root,"build/linux/game/BUILD.json"),Json.Encode(new{executable_sha256=Core.Hash(game),renderer="vulkan"}));
                manifest();NativeHost.CheckBuild(root);
                elf[18]=183;File.WriteAllBytes(game,elf);manifest();Reject(()=>NativeHost.CheckBuild(root));
                elf[18]=62;File.WriteAllBytes(game,elf);manifest();
                File.Delete(Path.Combine(root,"tools/build/build-native-linux.py"));
                NativeHost.Prepare(root);info=GameSettings.StartInfo(root);
                Need(info.FileName==Path.Combine(root,"Bin/futureperfect_recomp")&&info.Environment["RECOMP_SAVE_DIR"]==Path.Combine(root,"Saves"),"Installed routing failed");
                Need(Core.Allowed("Launch.sh")&&Core.Allowed("Bin/libSDL3.so.0")&&!Core.Allowed("Bin/unmanaged.sh"),"Runtime package allowlist failed");
                Reject(()=>Core.PathIn(root,"../escape"));
                string link=Path.Combine(root,"linked");Directory.CreateSymbolicLink(link,Path.GetTempPath());Reject(()=>Core.PathIn(root,"linked/escape"));Directory.Delete(link);
                string copy=Path.Combine(root,"TimeSplitters.Launcher");LauncherPlatform.CopyFile(Environment.ProcessPath,copy);
                Need((File.GetUnixFileMode(copy)&UnixFileMode.UserExecute)!=0,"Executable permission lost");
                var restart=LauncherPlatform.RestartInfo(root);Need(restart.FileName==copy&&restart.ArgumentList.Contains(root),"Restart lost installation path");
                string sleeper=Path.Combine(root,"futureperfect_recomp");LauncherPlatform.CopyFile("/bin/sleep",sleeper);
                using(var running=Process.Start(new ProcessStartInfo(sleeper,"30"){UseShellExecute=false})) {
                    try { Need(Core.GameRunning(),"Running Linux game not detected"); }
                    finally { running.Kill();running.WaitForExit(); }
                }
                // Install an actual hashed runtime ZIP while preserving user files.
                string stage=Path.Combine(root,".launcher/staged"),archive=Path.Combine(root,"runtime.zip");
                var payload=new Dictionary<string,byte[]> {{"Bin/futureperfect_recomp",elf},{"Launch.sh",System.Text.Encoding.UTF8.GetBytes("#!/bin/sh\nexit 0\n")},{"Settings/pc-controls.json",File.ReadAllBytes(Path.Combine(root,"Settings/pc-controls.json"))}};
                var hashes=new Dictionary<string,object>();
                using(var zip=ZipFile.Open(archive,ZipArchiveMode.Create)) {
                    foreach(var pair in payload){var entry=zip.CreateEntry(pair.Key);using(var output=entry.Open())output.Write(pair.Value);using(var sha=System.Security.Cryptography.SHA256.Create())hashes[pair.Key]=Convert.ToHexString(sha.ComputeHash(pair.Value)).ToLowerInvariant();}
                    using(var writer=new StreamWriter(zip.CreateEntry("BUILD.json").Open()))writer.Write(Json.Encode(new{version="v9.0.0",files=hashes}));
                }
                var extraction=new CaptureProgress();
                var changes=Core.Unpack(archive,stage,"v9.0.0",default(System.Threading.CancellationToken),extraction);
                Need(extraction.Messages.First()=="Checking the release archive" && extraction.Messages.Last()=="Extracting and checking files · 100%","Extraction progress did not span preparation");
                using(var cancel=new System.Threading.CancellationTokenSource()) {
                    bool cancelled=false;
                    try { Core.Unpack(archive,stage+"-cancel","v9.0.0",cancel.Token,new CaptureProgress { OnReport=message=>{if(message.StartsWith("Extracting"))cancel.Cancel();} }); }
                    catch(OperationCanceledException) { cancelled=true; }
                    Need(cancelled,"Preparation did not honor cancellation");
                    Need(!File.Exists(Path.Combine(root,"BUILD.json")),"Cancelled extraction changed the installation");
                }
                Need(changes.Any(c=>c.Name=="Launch.sh"),"Linux launch script not staged");
                string personal=File.ReadAllText(Path.Combine(root,"Settings/pc-controls.json"));
                Core.Apply(root,changes,null);
                Need(File.ReadAllText(Path.Combine(root,"Settings/pc-controls.json"))==personal,"Update changed personal controls");
                Need((File.GetUnixFileMode(Path.Combine(root,"Bin/futureperfect_recomp"))&UnixFileMode.UserExecute)!=0,"Installed game is not executable");
                Console.WriteLine("Linux launcher adapters, settings, ELF validation, locks and runtime install passed.");
                return 0;
            } catch(Exception error) { Console.Error.WriteLine(error);return 1; }
            finally { Directory.Delete(root,true); }
        }
    }
}

