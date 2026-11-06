#if MACOS
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace TimeSplitters.Launcher
{
    public static class MacStorageTests
    {
        static void Need(bool value,string message) { if(!value)throw new Exception(message); }
        static void Reject(Action action) { try { action(); } catch(Exception ex) when(ex is IOException || ex is InvalidDataException) { return; } throw new Exception("Expected storage rejection"); }
        static void Write(string root,string name,string text) { Core.Write(Core.PathIn(root,name),text); }
        public static int Run()
        {
            // macOS /var is a symlink; exercise the unchanged unlinked-path
            // contract in the owned build output rather than the temp alias.
            string sandbox=Path.Combine(OperatingSystem.IsMacOS()?AppContext.BaseDirectory:Path.GetTempPath(),"tsfp-storage-"+Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(sandbox);
            try {
                string source=Path.Combine(sandbox,"Applications"),support=Path.Combine(sandbox,"Application Support Ω","TimeSplitters");
                Directory.CreateDirectory(source);
                var empty=Array.Empty<string>();
                Need(MacStorage.Resolve(empty,source,support)==support,"Fresh install is not in Application Support");
                Need(!Directory.Exists(support),"Read-only discovery wrote storage");
                string translocated=Path.Combine(sandbox,"AppTranslocation",Guid.NewGuid().ToString(),"d");
                Need(MacStorage.Resolve(empty,translocated,support)==support,"Translocated app did not choose permanent storage");
                Need(!Directory.Exists(translocated),"Discovery wrote a translocated folder");
                Need(MacStorage.Resolve(new[]{"--root",source},source,support)==source,"Explicit root ignored");
                Need(MacStorage.Resolve(new[]{"--portable"},source,support)==source,"Portable override ignored");
                Write(source,".tsfp-portable","portable");
                Need(MacStorage.Resolve(empty,source,support)==source,"Portable marker ignored");
                File.Delete(Core.PathIn(source,".tsfp-portable"));
                string checkout=Path.Combine(sandbox,"checkout");Write(checkout,"tools/play-native-mac.py","fixture");
                Need(MacStorage.Resolve(empty,checkout,support)==checkout,"Developer root changed");
                Write(source,Core.LauncherName,"launcher fixture");Write(source,Core.AppPath,"plist fixture");
                Write(source,"TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources","seal fixture");
                MacStorage.Prepare(source,support);
                Need(SelfUpdate.Complete(support),"Managed launcher is incomplete");
                Need(!File.Exists(Core.PathIn(source,Core.SelfManifest)),"Wrote metadata beside the source app");
                Need(File.ReadAllText(Core.PathIn(support,"TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources"))=="seal fixture","Code seal changed");
                MacStorage.Prepare(source,source);
                Need(SelfUpdate.Complete(source),"Standalone portable bundle cannot create its manifest");
                string existing=Path.Combine(sandbox,"existing game Ω");
                foreach(string name in new[]{"Data/keep","Saves/UserData/keep","Settings/keep","Textures/keep","logs/keep"})Write(existing,name,name);
                var before=Directory.GetFiles(existing,"*",SearchOption.AllDirectories).ToDictionary(p=>p,File.ReadAllBytes);
                Need(MacStorage.ExistingFolder(existing)==existing,"Existing folder rejected");
                Reject(()=>MacStorage.ExistingFolder(Core.PathIn(existing,"Saves")));
                MacStorage.Prepare(source,existing);MacStorage.Remember(existing,support);
                Need(SelfUpdate.Complete(existing),"Existing installation has no complete launcher");
                Need(before.All(pair=>File.ReadAllBytes(pair.Key).SequenceEqual(pair.Value)),"Personal files changed");
                Need(MacStorage.Resolve(empty,source,support)==existing,"Selected folder was forgotten");
                string moved=existing+" moved";Directory.Move(existing,moved);
                Reject(()=>MacStorage.Resolve(empty,source,support));
                MacStorage.Remember(moved,support);
                Need(MacStorage.Resolve(empty,source,support)==moved,"Moved folder cannot be reselected");
                Need(!Directory.Exists(existing),"Missing folder was silently recreated");
                Write(support,"storage-root.txt",new string('x',4096));
                Reject(()=>MacStorage.Resolve(empty,source,support));
                Need(MacStorage.Resolve(new[]{"--root",source},source,support)==source,"Invalid preference overrode --root");
                MacStorage.Remember(moved,support);
                string relocated=source+" moved";Directory.Move(source,relocated);
                Need(MacStorage.Resolve(empty,relocated,support)==moved,"Moving the app changed game storage");
                // Opening an older external app must not downgrade the managed copy.
                string manifest=Core.PathIn(moved,Core.SelfManifest);
                var metadata=Json.Object(File.ReadAllText(manifest));metadata["version"]="v99.0.0";Core.Write(manifest,Json.Encode(metadata));
                MacStorage.Prepare(relocated,moved);
                Need(SelfUpdate.InstalledVersion(moved)=="v99.0.0","Managed launcher was downgraded");
                // Busy storage cannot be rewritten or selected while in use.
                using(LauncherPlatform.AcquireUpdateLock(moved))Reject(()=>MacStorage.Prepare(relocated,moved));
                Write(relocated,"TimeSplitters Launcher.app/Contents/unexpected.bin","reject");
                string rejected=Path.Combine(sandbox,"rejected");
                Reject(()=>MacStorage.Prepare(relocated,rejected));
                Need(!File.Exists(Core.PathIn(rejected,Core.LauncherName)),"Invalid bundle partially installed");
                var restart=MacStorage.StartInfo(moved,empty);
                Need(restart.ArgumentList.Contains(moved) && restart.ArgumentList.Contains("--root"),"Restart lost selected storage");
                Need(!restart.ArgumentList.Contains("--skip-self-update"),"Normal Finder opens suppress launcher updates");
                Reject(()=>MacStorage.Prepare(relocated,Path.Combine(sandbox,"Invalid.app","game")));
                if(!OperatingSystem.IsWindows()) {
                    string linked=Path.Combine(sandbox,"linked");Directory.CreateSymbolicLink(linked,moved);
                    Reject(()=>MacStorage.Prepare(relocated,linked));Directory.Delete(linked);
                }
                MigrationTests(sandbox);
                Console.WriteLine("PASS Mac storage: default/portable/explicit roots, existing files, relocation, missing drive, bundle copy, downgrade and locks");
                return 0;
            } catch(Exception ex) { Console.Error.WriteLine(ex);return 1; }
            finally { Directory.Delete(sandbox,true); }
        }
        static void MigrationTests(string sandbox)
        {
            string source=Path.Combine(sandbox,"legacy install Ω"),target=Path.Combine(sandbox,"auto destination Ω");
            Write(source,Core.LauncherName,"launcher");Write(source,Core.AppPath,"plist");
            foreach(string name in new[]{"Data/pak/keep","Settings/pc-controls.json","Textures/0123456789abcdef.dds","logs/keep","Backups/Saves/keep","Support/Python/bin/python3","Bin/futureperfect_recomp","BUILD.json",".launcher/preferences.json"})Write(source,name,"original "+name);
            Directory.CreateDirectory(Core.PathIn(source,"Saves/UserData/empty folder"));
            string image=Core.PathIn(source,"Saves/UserData/Partition0.img");
            using(var file=new FileStream(image,FileMode.CreateNew,FileAccess.Write)){file.SetLength(4*1024*1024);file.Position=1024;file.WriteByte(73);file.Position=file.Length-1;file.WriteByte(91);}
            var originals=Directory.GetFiles(source,"*",SearchOption.AllDirectories).ToDictionary(p=>p,File.ReadAllBytes);
            Need(MacStorageMigration.Automatic(Array.Empty<string>(),source,target,target),"Legacy install is not migrated automatically");
            Need(!MacStorageMigration.Automatic(new[]{"--root",source},source,target,target),"Explicit root would migrate");
            Need(!MacStorageMigration.Automatic(new[]{"--portable"},source,target,target),"Portable install would migrate");
            Write(source,".tsfp-portable","portable");
            Need(!MacStorageMigration.Automatic(Array.Empty<string>(),source,target,target),"Portable marker would migrate");
            File.Delete(Core.PathIn(source,".tsfp-portable"));
            // Kill the transfer after staging some entries; the destination must
            // remain unavailable until the complete verified installation commits.
            Reject(()=>MacStorageMigration.Migrate(source,target,source,null,default(System.Threading.CancellationToken),bytes=>{},n=>{if(n==3)throw new IOException("interrupted fixture");}));
            Need(!Directory.Exists(target),"Interrupted migration exposed partial saves");
            Need(MacStorageMigration.PendingSource(target)==source,"Interrupted migration lost its source");
            Need(MacStorageMigration.AutomaticSource(Array.Empty<string>(),Path.Combine(sandbox,"moved app"),target,target)==source,"Moved app cannot resume a pending migration");
            string resumedFile=Core.PathIn(Path.Combine(Path.GetDirectoryName(target),"."+Path.GetFileName(target)+"-migration"),".launcher/payload/.launcher/preferences.json");
            File.SetLastWriteTimeUtc(resumedFile,new DateTime(2020,1,1,0,0,0,DateTimeKind.Utc));
            var resumedTime=File.GetLastWriteTimeUtc(resumedFile);
            MacStorageMigration.Migrate(source,target);
            Need(File.GetLastWriteTimeUtc(Core.PathIn(target,".launcher/preferences.json"))==resumedTime,"Resume unnecessarily recopied verified files");
            Need(SelfUpdate.Complete(target) && MacStorageMigration.CompletedFrom(target,source),"Migration did not publish a complete managed installation");
            Need(Directory.Exists(Core.PathIn(target,"Saves/UserData/empty folder")),"Empty save directories disappeared");
            Need(originals.All(pair=>File.ReadAllBytes(pair.Key).SequenceEqual(pair.Value)),"Migration changed original files");
            foreach(var pair in originals.Where(pair=>!pair.Key.Contains("TimeSplitters Launcher.app")))
                Need(File.ReadAllBytes(Core.PathIn(target,Path.GetRelativePath(source,pair.Key).Replace('\\','/'))).SequenceEqual(pair.Value),"Migrated bytes changed: "+pair.Key);
            Write(target,"Saves/UserData/new progress","new progress");
            MacStorageMigration.Migrate(source,target);
            Need(File.ReadAllText(Core.PathIn(target,"Saves/UserData/new progress"))=="new progress","Repeat startup reset progress");
            Need(!MacStorageMigration.Automatic(Array.Empty<string>(),source,target,target),"Completed source would be recopied");
            // A pre-existing destination is never merged or overwritten.
            string conflict=Path.Combine(sandbox,"conflicting destination");Write(conflict,"Saves/keep","newer save");
            Reject(()=>MacStorageMigration.Migrate(source,conflict));
            Need(File.ReadAllText(Core.PathIn(conflict,"Saves/keep"))=="newer save","Destination conflict overwrote saves");
            string noSpace=Path.Combine(sandbox,"no space");
            Reject(()=>MacStorageMigration.Migrate(source,noSpace,source,null,default(System.Threading.CancellationToken),bytes=>{throw new IOException("disk full");},null));
            Need(!Directory.Exists(noSpace) && originals.All(pair=>File.ReadAllBytes(pair.Key).SequenceEqual(pair.Value)),"Disk-full failure changed installations");
            string canceled=Path.Combine(sandbox,"canceled destination");
            using(var cancellation=new System.Threading.CancellationTokenSource()) {
                try {
                    MacStorageMigration.Migrate(source,canceled,source,null,cancellation.Token,bytes=>{},n=>cancellation.Cancel());
                    throw new Exception("Canceled migration completed");
                } catch(OperationCanceledException) { }
            }
            Need(!Directory.Exists(canceled),"Canceled copy exposed partial data");
            MacStorageMigration.Migrate(source,canceled);
            string locked=Path.Combine(sandbox,"locked source destination");
            using(LauncherPlatform.AcquireUpdateLock(source))Reject(()=>MacStorageMigration.Migrate(source,locked));
            Need(!Directory.Exists(locked),"Locked source was migrated");
            string pending=Path.Combine(sandbox,"pending source destination");Write(source,".launcher/pending/keep","unfinished update");
            Reject(()=>MacStorageMigration.Migrate(source,pending));
            Need(!Directory.Exists(pending),"Source with a pending update was migrated");
            File.Delete(Core.PathIn(source,".launcher/pending/keep"));Directory.Delete(Core.PathIn(source,".launcher/pending"));
            string unrelated=Path.Combine(sandbox,"unrelated destination");
            string unrelatedWork=Path.Combine(Path.GetDirectoryName(unrelated),"."+Path.GetFileName(unrelated)+"-migration");
            Write(unrelatedWork,"keep","unrelated file");
            Reject(()=>MacStorageMigration.Migrate(source,unrelated));
            Need(File.ReadAllText(Core.PathIn(unrelatedWork,"keep"))=="unrelated file","Unowned staging was changed");
            if(!OperatingSystem.IsWindows()) {
                string link=Core.PathIn(source,"Data/linked");Directory.CreateSymbolicLink(link,target);
                Reject(()=>MacStorageMigration.Migrate(source,Path.Combine(sandbox,"linked source destination")));
                Directory.Delete(link);
            }
            // A source edit during the copy must prevent switching; a retry
            // restarts the owned staging tree from the new source snapshot.
            string changed=Path.Combine(sandbox,"changed source destination");bool mutated=false;
            Reject(()=>MacStorageMigration.Migrate(source,changed,source,null,default(System.Threading.CancellationToken),bytes=>{},n=>{
                if(!mutated){Write(source,"Settings/pc-controls.json","latest controls");mutated=true;}
            }));
            Need(!Directory.Exists(changed),"Changed source was published");
            MacStorageMigration.Migrate(source,changed);
            Need(File.ReadAllText(Core.PathIn(changed,"Settings/pc-controls.json"))=="latest controls","Retry lost the source edit");
            // Explicit custom saves inside the old installation follow the copy;
            // separately stored saves keep their exact preference and location.
            string internalTarget=Path.Combine(sandbox,"internal custom destination");
            Write(source,"CustomSaves/UserData/save","custom save");Write(source,"Settings/save-location.txt",Core.PathIn(source,"CustomSaves")+"\n");
            MacStorageMigration.Migrate(source,internalTarget);
            Need(SaveLocations.Resolve(internalTarget)==Core.PathIn(internalTarget,"CustomSaves"),"Internal custom save path was not remapped");
            Need(File.ReadAllText(Core.PathIn(internalTarget,"CustomSaves/UserData/save"))=="custom save","Internal custom save was not copied");
            string external=Path.Combine(sandbox,"external saves");Write(external,"UserData/save","external save");
            Write(source,"Settings/save-location.txt",external+"\n");
            string externalTarget=Path.Combine(sandbox,"external custom destination");
            Write(externalTarget,"storage-root.txt",source+"\n");
            MacStorageMigration.Migrate(source,externalTarget);
            Need(SaveLocations.Resolve(externalTarget)==external && File.ReadAllText(Core.PathIn(external,"UserData/save"))=="external save","External custom saves changed");
            Console.WriteLine("PASS automatic Mac migration: original/migrated bytes, sparse saves, resume, conflicts, disk full, source changes, custom paths and repeated startup");
        }
    }
}
#endif
