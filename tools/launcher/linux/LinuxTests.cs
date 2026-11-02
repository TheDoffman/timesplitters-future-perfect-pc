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
                Need(LauncherPlatform.Checkout(root)&&LauncherPlatform.GamePath(root)=="build/linux/futureperfect_recomp","Checkout routing failed");
                Need(Program.FindRoot(Array.Empty<string>(),Path.Combine(root,"build/linux-launcher/package"))==root,"Checkout discovery failed");
                NativeHost.Prepare(root);
                var info=GameSettings.StartInfo(root);
                Need(info.FileName==Path.Combine(root,"build/linux/futureperfect_recomp"),"Wrong game target");
                Need(info.Environment["RECOMP_SAVE_DIR"]==Path.Combine(root,"private/saves"),"Wrong save target");
                Need(info.Environment["TSFP_DISPLAY_CONFIG"]==Path.Combine(root,"config/pc-display.ini"),"Wrong display target");
                Need(!info.Environment.ContainsKey("VK_DRIVER_FILES")&&!info.Environment.ContainsKey("DYLD_LIBRARY_PATH"),"Foreign driver override leaked");
                Need(!Directory.Exists(Path.Combine(root,"Settings")),"Created installed settings in source checkout");
                string game=Path.Combine(root,"build/linux/futureperfect_recomp");Directory.CreateDirectory(Path.GetDirectoryName(game));
                byte[] elf=new byte[64];elf[0]=127;elf[1]=69;elf[2]=76;elf[3]=70;elf[4]=2;elf[5]=1;elf[18]=62;File.WriteAllBytes(game,elf);
                Action manifest=()=>File.WriteAllText(Path.Combine(root,"build/linux/BUILD.json"),Json.Encode(new{executable_sha256=Core.Hash(game),renderer="vulkan"}));
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
                // Round-trip an actual hashed runtime ZIP through update and rollback.
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

