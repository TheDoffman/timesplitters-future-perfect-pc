using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading;
using System.Security.Cryptography;
using System.Text;
using TimeSplitters.Launcher;

public static class SaveBackupChecks
{
    static void Need(bool value,string why){if(!value)throw new Exception(why);}
    static void Reject(Action action){bool rejected=false;try{action();}catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException))throw;rejected=true;}Need(rejected,"Unsafe save operation was accepted");}
    static void Put(string root,string name,string value){string path=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,value);}
    static string Saves(string root){return Core.PathIn(root,LauncherPlatform.Saves(root));}
    static void Rewrite(string archive,Action<SaveBackups.Manifest> edit)
    {
        using(var zip=ZipFile.Open(archive,ZipArchiveMode.Update)) {
            SaveBackups.Manifest m;using(var reader=new StreamReader(zip.GetEntry("backup.json").Open()))m=Json.Serializer().Deserialize<SaveBackups.Manifest>(reader.ReadToEnd());
            edit(m);byte[] bytes=Encoding.UTF8.GetBytes(Json.Encode(m));zip.GetEntry("backup.json").Delete();zip.GetEntry("backup.sha256").Delete();
            using(var target=zip.CreateEntry("backup.json").Open())target.Write(bytes,0,bytes.Length);
            using(var sha=SHA256.Create())using(var writer=new StreamWriter(zip.CreateEntry("backup.sha256").Open()))writer.Write(BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant());
        }
    }
    public static void Run(string parent,Action<string,Action> test)
    {
        test("custom save locations copy without deleting originals and drive backup/restore",()=>{
            string root=Path.Combine(parent,"save-locations"),custom=Path.Combine(parent,"custom-saves");Directory.CreateDirectory(root);
            string original=Saves(root);Put(original,"UserData/45410066/profile","progress");
            SaveLocations.Choose(root,custom,true,CancellationToken.None);
            Need(SaveLocations.Resolve(root)==custom,"Selection did not persist");
            Need(File.ReadAllText(Core.PathIn(original,"UserData/45410066/profile"))=="progress","Copy changed original");
            var backup=SaveBackups.Create(root,"Manual",null,CancellationToken.None);
            Put(custom,"UserData/45410066/profile","changed");SaveBackups.Restore(root,backup.Id,null,CancellationToken.None);
            Need(File.ReadAllText(Core.PathIn(custom,"UserData/45410066/profile"))=="progress","Restore used wrong location");
            Put(custom,"UserData/45410066/profile","new progress");
            Reject(()=>SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,i=>{if(i==0)throw new IOException("simulated interrupted custom swap");}));
            Need(File.ReadAllText(Core.PathIn(custom,"UserData/45410066/profile"))=="new progress","Custom rollback lost progress");
            SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,i=>{if(i==2)throw new IOException("deferred custom cleanup");});
            Need(SaveBackups.Recover(root) && File.ReadAllText(Core.PathIn(custom,"UserData/45410066/profile"))=="progress","Custom recovery failed");
            SaveLocations.Choose(root,original,false,CancellationToken.None);Need(!File.Exists(SaveLocations.Preference(root)),"Default is not portable");
            Reject(()=>SaveLocations.Choose(root,root,false,CancellationToken.None));
            Reject(()=>SaveLocations.Choose(root,custom,true,CancellationToken.None));
            Need(SaveLocations.Resolve(root)==original,"Rejected copy changed preference");
            Core.Write(SaveLocations.Preference(root),Path.Combine(parent,"missing-save-drive"));
            Reject(()=>SaveLocations.Resolve(root));
            SaveLocations.Choose(root,original,false,CancellationToken.None);
        });
        test("save backups round-trip sparse disks, preserve other data and make restores undoable",()=>{
            string root=Path.Combine(parent,"save-roundtrip");Directory.CreateDirectory(root);Put(root,"BUILD.json","{\"version\":\"v1.0.0\"}");
            Directory.CreateDirectory(Core.PathIn(root,".launcher"));
            using(LauncherPlatform.AcquireUpdateLock(root)) {
#if MACOS
            Reject(()=>SaveBackups.Create(root,"Manual",null,CancellationToken.None));
#endif
            string saves=Saves(root);Directory.CreateDirectory(saves);string partition=Core.PathIn(saves,"Partition1.img");
            using(var file=new FileStream(partition,FileMode.CreateNew,FileAccess.Write)){LauncherPlatform.PrepareSparse(file);file.SetLength(32L*1024*1024);file.Position=7;file.WriteByte(42);file.Position=file.Length-1;file.WriteByte(99);}
            Put(saves,"nested/profile","original progress");Put(root,"Settings/keep.ini","settings");Put(root,"ISO/keep.iso","ISO");Put(root,"Textures/custom.dds","custom texture");
            var backup=SaveBackups.Create(root,"Manual",null,CancellationToken.None,installationLockHeld:true);
            Need(backup.Files==2 && backup.Game=="v1.0.0" && backup.StoredBytes<65536 && backup.LogicalBytes>32L*1024*1024,"Sparse backup became a full disk copy or lost identity");
            File.WriteAllText(partition,"new progress");Put(saves,"later-file","later progress");
            SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,installationLockHeld:true);
            using(var file=File.OpenRead(partition)){Need(file.Length==32L*1024*1024,"Disk length changed");file.Position=7;Need(file.ReadByte()==42,"First saved block differs");file.Position=file.Length-1;Need(file.ReadByte()==99,"Last saved block differs");file.Position=1024*1024;Need(file.ReadByte()==0,"Sparse hole not restored");}
            Need(File.ReadAllText(Core.PathIn(saves,"nested/profile"))=="original progress" && !File.Exists(Core.PathIn(saves,"later-file")),"Restore did not replace save state");
            var before=SaveBackups.List(root).Single(i=>i.Reason=="Before restore");
            SaveBackups.Restore(root,before.Id,null,CancellationToken.None,installationLockHeld:true);
            Need(File.ReadAllText(partition)=="new progress" && File.ReadAllText(Core.PathIn(saves,"later-file"))=="later progress","Pre-restore backup cannot undo restore");
            Need(File.ReadAllText(Core.PathIn(root,"Settings/keep.ini"))=="settings" && File.ReadAllText(Core.PathIn(root,"ISO/keep.iso"))=="ISO" && File.ReadAllText(Core.PathIn(root,"Textures/custom.dds"))=="custom texture","Save operation touched protected non-save data");
            }
        });
        test("save restore failures, cancellation and damaged archives keep current saves",()=>{
            string root=Path.Combine(parent,"save-failures");Directory.CreateDirectory(root);string saves=Saves(root);Directory.CreateDirectory(saves);Put(saves,"profile","original");
            var backup=SaveBackups.Create(root,"Manual",null,CancellationToken.None);Put(saves,"profile","current");Put(saves,"keep","current extra");
            foreach(int step in new[]{0,1}) {
                Reject(()=>SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,i=>{if(i==step)throw new IOException("injected restore failure");}));
                Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="current" && File.ReadAllText(Core.PathIn(saves,"keep"))=="current extra","Failed restore lost current progress");
            }
            using(var stop=new CancellationTokenSource()){stop.Cancel();bool cancelled=false;try{SaveBackups.Create(root,"Manual",null,stop.Token);}catch(OperationCanceledException){cancelled=true;}Need(cancelled,"Backup cancellation ignored");}
            Need(!Directory.GetFiles(Core.PathIn(root,SaveBackups.Folder),"*.partial").Any(),"Cancelled backup was published or left partial data");
            string archive=Core.PathIn(root,SaveBackups.Folder+"/"+backup.Id+".tsfpsave");
            using(var zip=ZipFile.Open(archive,ZipArchiveMode.Update)){var entry=zip.Entries.First(e=>e.FullName.StartsWith("data/"));string name=entry.FullName;int length=(int)entry.Length;entry.Delete();using(var output=zip.CreateEntry(name).Open())output.Write(new byte[length],0,length);}
            Reject(()=>SaveBackups.Restore(root,backup.Id,null,CancellationToken.None));
            Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="current" && !Directory.Exists(Core.PathIn(root,".launcher/save-restore")),"Damaged backup changed saves or left a transaction");
            Reject(()=>SaveBackups.Restore(root,"../outside",null,CancellationToken.None));
        });
        test("interrupted save swaps recover without overwriting newer progress",()=>{
            string root=Path.Combine(parent,"save-recovery");Directory.CreateDirectory(root);string saves=Saves(root);Directory.CreateDirectory(saves);Put(saves,"profile","original");
            var backup=SaveBackups.Create(root,"Manual",null,CancellationToken.None);
            string previous=Core.PathIn(root,".launcher/save-restore/previous"),next=Core.PathIn(root,".launcher/save-restore/new");
            Directory.CreateDirectory(next);Put(next,"profile","restored");Core.Write(Core.PathIn(root,".launcher/save-restore/journal.json"),Json.Encode(new{Owner="tsfp-save-restore",Backup=backup.Id,HadSaves=true}));
            Directory.Move(saves,previous);Need(SaveBackups.Recover(root),"Interrupted swap not recovered");
            Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="original","Incomplete swap lost originals");
            Directory.CreateDirectory(next);Put(next,"profile","restored then played");Core.Write(Core.PathIn(root,".launcher/save-restore/journal.json"),Json.Encode(new{Owner="tsfp-save-restore",Backup=backup.Id,HadSaves=true}));
            Directory.Move(saves,previous);Directory.Move(next,saves);SaveBackups.Recover(root);
            Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="restored then played","Recovery overwrote newer progress");
            SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,i=>{if(i==2)throw new IOException("defer cleanup after commit");});
            Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="original" && Directory.Exists(Core.PathIn(root,".launcher/save-restore")),"Cleanup failure rolled back a completed restore");
            SaveBackups.Recover(root);Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="original" && !Directory.Exists(Core.PathIn(root,".launcher/save-restore")),"Deferred cleanup recovery changed restored saves");
            Put(saves,"profile","newer progress");
            SaveBackups.Restore(root,backup.Id,null,CancellationToken.None,i=>{if(i==2)throw new IOException("defer cleanup");});
            string undo=Json.String(Json.Object(File.ReadAllText(Core.PathIn(root,".launcher/save-restore/journal.json"))),"Backup");
            using(var zip=ZipFile.Open(Core.PathIn(root,SaveBackups.Folder+"/"+undo+".tsfpsave"),ZipArchiveMode.Update)){zip.GetEntry("backup.sha256").Delete();using(var writer=new StreamWriter(zip.CreateEntry("backup.sha256").Open()))writer.Write(new string('0',64));}
            Reject(()=>SaveBackups.Recover(root));Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="original" && File.ReadAllText(Core.PathIn(previous,"profile"))=="newer progress","Recovery discarded originals without a valid undo backup");
        });
        test("unowned save recovery files are retained",()=>{
            string root=Path.Combine(parent,"unowned-recovery");Directory.CreateDirectory(root);Put(root,".launcher/save-restore/keep","unknown recovery data");
            Reject(()=>SaveBackups.Recover(root));Need(File.ReadAllText(Core.PathIn(root,".launcher/save-restore/keep"))=="unknown recovery data","Unowned recovery data was removed");
        });
        test("save archive traversal, duplicate paths, missing owner and impossible ranges are rejected",()=>{
            string root=Path.Combine(parent,"save-metadata");Directory.CreateDirectory(root);string saves=Saves(root);Directory.CreateDirectory(saves);Put(saves,"profile","keep progress");
            foreach(var mutate in new Action<SaveBackups.Manifest>[] {
                m=>m.Files[0].Name="../escaped",m=>m.Files.Add(m.Files[0]),m=>m.Owner=null,
                m=>m.Files[0].Blocks[0].Offset=-1,m=>m.Files[0].Length=65L*1024*1024*1024}) {
                var backup=SaveBackups.Create(root,"Manual",null,CancellationToken.None);
                Rewrite(Core.PathIn(root,SaveBackups.Folder+"/"+backup.Id+".tsfpsave"),mutate);
                Reject(()=>SaveBackups.Restore(root,backup.Id,null,CancellationToken.None));
                Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="keep progress","Malformed archive changed active progress");
            }
            var damaged=SaveBackups.Create(root,"Manual",null,CancellationToken.None);
            using(var zip=ZipFile.Open(Core.PathIn(root,SaveBackups.Folder+"/"+damaged.Id+".tsfpsave"),ZipArchiveMode.Update)){zip.GetEntry("backup.json").Delete();using(var writer=new StreamWriter(zip.CreateEntry("backup.json").Open()))writer.Write("{}");}
            Reject(()=>SaveBackups.Restore(root,damaged.Id,null,CancellationToken.None));
            Need(SaveBackups.List(root).Length==0 && Directory.GetFiles(Core.PathIn(root,SaveBackups.Folder),"*.tsfpsave").Length==6,"Invalid archives were listed or deleted");
        });
        test("game updates and rollback automatically back up saves before replacing runtime",()=>{
            string root=Path.Combine(parent,"save-updates");Directory.CreateDirectory(root);string saves=Saves(root);Directory.CreateDirectory(saves);Put(saves,"profile","player progress");Put(root,"BUILD.json","{\"version\":\"v1.0.0\"}");
            string source=Path.Combine(parent,"new-save-test-build.json");File.WriteAllText(source,"{\"version\":\"v1.1.0\"}");
            Core.Apply(root,new List<Change>{new Change{Name="BUILD.json",Source=source}},null);
            Need(SaveBackups.List(root).Any(i=>i.Reason=="Before game update" && i.Game=="v1.0.0"),"Update skipped save backup");
            Core.Rollback(root);Need(SaveBackups.List(root).Any(i=>i.Reason=="Before game update" && i.Game=="v1.1.0"),"Rollback skipped backup");
            Need(File.ReadAllText(Core.PathIn(saves,"profile"))=="player progress","Runtime update modified saves");
#if !MACOS
            using(var locked=new FileStream(Core.PathIn(saves,"profile"),FileMode.Open,FileAccess.ReadWrite,FileShare.None))
                Reject(()=>Core.Apply(root,new List<Change>{new Change{Name="BUILD.json",Source=source}},null));
            Need(Core.InstalledVersion(root)=="v1.0.0","Failed automatic backup still replaced the runtime");
#endif
            Put(root,"BUILD.json","previous game metadata");
            var unknown=SaveBackups.Create(root,"Manual",null,CancellationToken.None);
            Need(unknown.Game=="Unknown game version" && File.ReadAllText(Core.PathIn(root,"BUILD.json"))=="previous game metadata" && File.ReadAllText(Core.PathIn(saves,"profile"))=="player progress","Malformed version metadata blocked backup or changed local data");
            string empty=Path.Combine(parent,"no-saves");Directory.CreateDirectory(empty);Need(SaveBackups.Create(empty,"Manual",null,CancellationToken.None)==null,"Empty save folder produced a backup");
        });
    }
}
