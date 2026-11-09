#if MACOS
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace TimeSplitters.Launcher
{
    // macOS installation migration. Publish the verified installation with one
    // directory rename on the destination volume; never modify source data or
    // merge into an existing destination. Interrupted copies remain resumable.
    public static class MacStorageMigration
    {
        const string Owner="tsfp-mac-storage-v1",Receipt=".launcher/storage-migration.json";
        static readonly HashSet<string> Roots=new HashSet<string>(StringComparer.OrdinalIgnoreCase) {
            "Data","Saves","Settings","Textures","Logs","Backups","Bin","Support","Licenses","Source",
            "BUILD.json","README.txt","Launch.command"
        };
        static readonly string[] Metadata={"preferences.json","texture-opt-in.json","texture-pack.json","launcher.log"};
        static readonly string[] Pending={"transaction","self-transaction","repair-transaction","pending","self-pending",
            "repair-pending","save-restore","save-restore-location.txt","texture-install"};
        public sealed class Item { public string Name,Hash; public long Size; public bool Directory; }
        static string Work(string destination) => Core.PathIn(Path.GetDirectoryName(destination),"."+Path.GetFileName(destination)+"-migration");
        public static string PendingSource(string destination)
        {
            string path=Core.PathIn(Work(destination),"owner.json");
            if(!File.Exists(path))return null;
            Core.Require(new FileInfo(path).Length<16384,"Invalid migration recovery record; files were kept.");
            var record=Json.Object(File.ReadAllText(path));
            Core.Require(Json.String(record,"owner")==Owner && MacStorage.Same(Json.String(record,"destination"),destination),
                "A different migration is pending; its files were kept.");
            string source=SaveLocations.Validate(Json.String(record,"source"));
            return CompletedFrom(destination,source)?null:source;
        }
        public static bool CompletedFrom(string destination,string source)
        {
            string receipt=Core.PathIn(destination,Receipt);
            if(!File.Exists(receipt))return false;
            Core.Require(new FileInfo(receipt).Length<16384,"The migration record is invalid. Existing files were kept.");
            var record=Json.Object(File.ReadAllText(receipt));
            return Json.String(record,"owner")==Owner && MacStorage.Same(Json.String(record,"source"),source);
        }
        public static bool Automatic(string[] args,string source,string selected,string support=null)
            => AutomaticSource(args,source,selected,support)!=null;
        public static string AutomaticSource(string[] args,string source,string selected,string support=null)
        {
            support=support??MacStorage.DefaultRoot;
            if(MacStorage.Portable(args,source) || args.Contains("--choose-storage") || LauncherPlatform.Checkout(source) ||
                MacStorage.Same(source,selected) || !MacStorage.Same(selected,support))return null;
            if(File.Exists(Core.PathIn(support,"storage-root.txt")))return null; // Respect a folder the user explicitly selected.
            string pending=PendingSource(selected);if(pending!=null)return pending;
            return !NativeInstallation.Translocated(source) && !File.Exists(Core.PathIn(support,"storage-root.txt")) &&
                MacStorage.HasInstallation(source) && !CompletedFrom(selected,source)?source:null;
        }
        public static void Migrate(string source,string destination,string launcherSource=null,IProgress<string> progress=null,CancellationToken token=default(CancellationToken))
        {
            string parent=Path.GetDirectoryName(SaveLocations.Validate(destination));
            Migrate(source,destination,launcherSource??source,progress,token,bytes=>Core.CheckSpace(parent,bytes),null);
        }
        internal static void Migrate(string source,string destination,string launcherSource,IProgress<string> progress,
            CancellationToken token,Action<long> checkSpace,Action<int> afterCopy)
        {
            source=SaveLocations.Validate(source);destination=SaveLocations.Validate(destination);
            Core.Require(!MacStorage.Same(source,destination) && !LauncherPlatform.Checkout(source) && !LauncherPlatform.Checkout(destination),
                "Choose an installed game folder outside the destination and source checkout.");
            Core.Require(!NativeInstallation.Translocated(source) && !NativeInstallation.Translocated(destination),"Choose the original game folder outside the temporary macOS app location.");
            for(var dir=new DirectoryInfo(destination);dir!=null;dir=dir.Parent)
                Core.Require(!dir.Name.EndsWith(".app",StringComparison.OrdinalIgnoreCase),"Choose a game folder outside application bundles.");
            token.ThrowIfCancellationRequested();
            Core.Require(!Core.GameRunning(),"Close the game before moving its files to Application Support.");
            if(CompletedFrom(destination,source))return;
            Core.Require(Available(destination),"Application Support already contains an installation. Nothing was overwritten. Choose which existing game folder to use.");
            string parent=Path.GetDirectoryName(destination);
            string work=Work(destination);
            Directory.CreateDirectory(work);
            using(var migrationLock=new FileStream(Core.PathIn(work,"lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
                string owner=Core.PathIn(work,"owner.json");
                if(File.Exists(owner)) {
                    Core.Require(new FileInfo(owner).Length<16384,"Invalid migration recovery record; files were kept.");
                    var record=Json.Object(File.ReadAllText(owner));
                    Core.Require(Json.String(record,"owner")==Owner && MacStorage.Same(Json.String(record,"source"),source) &&
                        MacStorage.Same(Json.String(record,"destination"),destination),"A different migration is pending. Its files were kept; finish that migration first.");
                } else {
                    Core.Require(Directory.EnumerateFileSystemEntries(work).All(path=>Path.GetFileName(path)=="lock"),
                        "The migration recovery folder contains unrelated files. They were kept.");
                    Core.Write(owner,Json.Encode(new {owner=Owner,source=source,destination=destination}));
                }
                if(CompletedFrom(destination,source))return;
                Core.Require(Available(destination),
                    "Application Support already contains an installation. Nothing was overwritten. Choose which existing game folder to use.");
                Core.Require(Directory.Exists(source) && MacStorage.HasInstallation(source),"Choose the existing folder containing Data, Saves and Settings.");
                Directory.CreateDirectory(Core.PathIn(source,".launcher"));
                using(LauncherPlatform.AcquireUpdateLock(source)) {
                    Core.Require(!Pending.Any(name=>File.Exists(Core.PathIn(source,".launcher/"+name)) || Directory.Exists(Core.PathIn(source,".launcher/"+name))),
                        "Your old installation needs update or save recovery. Open it in portable mode to finish recovery before migrating. All files were kept.");
                    progress?.Report("Checking your existing game, saves, settings and textures…");
                    var items=Snapshot(source,token);
                    string plan=Json.Encode(items),planPath=Core.PathIn(work,"plan.json");
                    const string payloadName=".launcher/payload";
                    string payload=Core.PathIn(work,payloadName);
                    if(File.Exists(planPath) && File.ReadAllText(planPath)!=plan) {
                        progress?.Report("The source changed; restarting the verified copy…");
                        Core.RemoveOwned(work,payloadName);
                    }
                    Core.Write(planPath,plan);
                    Directory.CreateDirectory(payload);
                    string partial=Core.PathIn(work,"copy-part");
                    if(File.Exists(partial))File.Delete(partial);
                    long remaining=items.Where(item=>!Matches(payload,item)).Sum(item=>item.Size);
                    // Reserve regular allocation even when sparse files are available,
                    // plus space for the managed launcher and its transaction.
                    checkSpace(checked(remaining+2L*LauncherPlatform.LauncherExpandedLimit));
                    int copied=0;
                    foreach(var item in items) {
                        token.ThrowIfCancellationRequested();
                        copied++;
                        progress?.Report("Copying "+copied+" of "+items.Count+": "+item.Name);
                        if(!Matches(payload,item))Copy(source,payload,partial,item,token);
                        afterCopy?.Invoke(copied);
                    }
                    Core.Require(items.All(item=>Matches(payload,item)),"The copied files changed before verification completed. Nothing was switched; retry the migration.");
                    // An absolute save path inside the old install follows the copy;
                    // external/custom-volume locations remain byte-for-byte unchanged.
                    string preference=SaveLocations.Preference(payload);
                    if(File.Exists(preference)) {
                        Core.Require(new FileInfo(preference).Length<4096,"Invalid save-folder preference. Originals were kept.");
                        string old=SaveLocations.Validate(File.ReadAllText(preference).TrimEnd('\r','\n'));
                        if(MacStorage.Same(old,SaveLocations.Default(source)))File.Delete(preference);
                        else if(Inside(source,old))Core.Write(preference,Path.Combine(destination,Path.GetRelativePath(source,old))+"\n");
                    }
                    progress?.Report("Verifying the completed copy…");
                    Core.Require(Json.Encode(Snapshot(source,token))==plan,"The original files changed during migration. Nothing was switched; retry to copy the latest files.");
                    token.ThrowIfCancellationRequested();
                    MacStorage.Prepare(launcherSource,payload);
                    Core.Write(Core.PathIn(payload,Receipt),Json.Encode(new {owner=Owner,source=source,completed_utc=DateTime.UtcNow.ToString("o")}));
                    Core.Write(Core.PathIn(payload,".launcher/last-result.txt"),"Your existing game was copied to Application Support. Your original folder was kept as a backup: "+source);
                    token.ThrowIfCancellationRequested();
                    Core.Require(Available(destination),"The destination changed during migration. Nothing was overwritten; retry after choosing the correct folder.");
                    if(Directory.Exists(destination)) {
                        if(Directory.EnumerateFileSystemEntries(destination).Any())
                            Directory.Move(destination,Core.PathIn(work,".launcher/previous-root")); // Preserve the location preference during the atomic switch.
                        else Directory.Delete(destination,false);
                    }
                    Directory.Move(payload,destination); // Atomic publication on the destination volume.
                    progress?.Report("Migration complete. Your original folder was kept as a backup.");
                }
            }
        }
        static bool Inside(string root,string path) => Path.GetFullPath(path).StartsWith(
            Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,SaveLocations.Comparison);
        static bool Available(string destination) => !File.Exists(destination) && (!Directory.Exists(destination) ||
            Directory.EnumerateFileSystemEntries(destination).All(path=>Path.GetFileName(path)=="storage-root.txt" && File.Exists(path)));
        static List<Item> Snapshot(string root,CancellationToken token)
        {
            var result=new List<Item>();
            var roots=new HashSet<string>(Roots,StringComparer.OrdinalIgnoreCase);
            string preference=SaveLocations.Preference(root);
            if(File.Exists(preference)) {
                Core.Require(new FileInfo(preference).Length<4096,"Invalid save-folder preference. Originals were kept.");
                string custom=SaveLocations.Validate(File.ReadAllText(preference).TrimEnd('\r','\n'));
                if(Inside(root,custom)) {
                    string top=Path.GetRelativePath(root,custom).Split(Path.DirectorySeparatorChar)[0];
                    Core.Require(top!=".launcher" && !top.EndsWith(".app",StringComparison.OrdinalIgnoreCase),"Choose a dedicated save folder outside launcher-managed files before migrating.");
                    roots.Add(top);
                }
            }
            foreach(string path in Directory.EnumerateFileSystemEntries(root).OrderBy(p=>p,StringComparer.Ordinal))
                if(roots.Contains(Path.GetFileName(path)))Collect(root,path,result,token);
            foreach(string name in Metadata) {
                string path=Core.PathIn(root,".launcher/"+name);
                if(File.Exists(path))Collect(root,path,result,token);
            }
            Core.Require(result.Select(item=>item.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count()==result.Count,
                "Case-conflicting files cannot be migrated safely. Originals were kept.");
            return result.OrderBy(item=>item.Name,StringComparer.Ordinal).ToList();
        }
        static void Collect(string root,string path,List<Item> result,CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            string name=Path.GetRelativePath(root,path).Replace('\\','/');Core.PathIn(root,name);
            if(name.StartsWith("Support/Launcher/",StringComparison.OrdinalIgnoreCase) || name.Equals("Support/Launcher",StringComparison.OrdinalIgnoreCase))return;
            if(Directory.Exists(path)) {
                Core.Require(result.Count<100000,"Too many installation files to migrate safely.");
                result.Add(new Item {Name=name,Directory=true,Hash=""});
                foreach(string child in Directory.EnumerateFileSystemEntries(path).OrderBy(p=>p,StringComparer.Ordinal))Collect(root,child,result,token);
            } else {
                Core.Require(result.Count<100000,"Too many installation files to migrate safely.");
                result.Add(new Item {Name=name,Size=new FileInfo(path).Length,Hash=Core.Hash(path)});
            }
        }
        static bool Matches(string root,Item item)
        {
            string file=Core.PathIn(root,item.Name);
            return item.Directory?Directory.Exists(file):File.Exists(file) && new FileInfo(file).Length==item.Size && Core.Hash(file)==item.Hash;
        }
        static void Copy(string source,string destination,string partial,Item item,CancellationToken token)
        {
            string inputPath=Core.PathIn(source,item.Name),target=Core.PathIn(destination,item.Name);
            if(item.Directory){Directory.CreateDirectory(target);return;}
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            using(var input=File.OpenRead(inputPath))using(var output=new FileStream(partial,FileMode.Create,FileAccess.Write,FileShare.None)) {
                LauncherPlatform.PrepareSparse(output);output.SetLength(item.Size);
                var buffer=new byte[1024*1024];int count;
                while((count=input.Read(buffer,0,buffer.Length))>0) {
                    token.ThrowIfCancellationRequested();bool nonzero=false;
                    for(int i=0;i<count;i++)if(buffer[i]!=0){nonzero=true;break;}
                    if(nonzero)output.Write(buffer,0,count);else output.Position+=count;
                }
                output.Flush(true);
            }
            Core.Require(new FileInfo(partial).Length==item.Size && Core.Hash(partial)==item.Hash,"Copy verification failed. Your original installation was kept.");
            if(!OperatingSystem.IsWindows())File.SetUnixFileMode(partial,File.GetUnixFileMode(inputPath));
            File.Move(partial,target,true);
        }
    }
}
#endif
