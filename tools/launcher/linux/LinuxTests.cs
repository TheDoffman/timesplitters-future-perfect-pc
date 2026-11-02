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
                var changes=Core.Unpack(archive,stage,"v9.0.0");
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

