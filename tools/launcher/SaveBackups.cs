using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading;

namespace TimeSplitters.Launcher
{
    public sealed class SaveBackup
    {
        public string Id, Created, Game, Reason;
        public long LogicalBytes, StoredBytes;
        public int Files;
        public override string ToString() { return DateTime.Parse(Created,CultureInfo.InvariantCulture,DateTimeStyles.RoundtripKind).ToLocalTime().ToString("yyyy-MM-dd HH:mm:ss")+" · "+Game+" · "+Reason; }
    }
    // One shared archive format. Empty virtual-disk blocks do not enter backups.
    public static class SaveBackups
    {
        public const string Folder="Backups/Saves";
        const string Work=".launcher/save-restore";
        const int BlockSize=65536, MaxFiles=1024, MaxBlocks=8192;
        const long DataLimit=512L*1024*1024, LogicalLimit=64L*1024*1024*1024;
        public sealed class Block { public long Offset; public int Size; public string Entry, Hash; }
        public sealed class SavedFile { public string Name; public long Length; public List<Block> Blocks=new List<Block>(); }
        public sealed class Manifest
        {
            public int Schema; public string Owner, Id, Created, Game, Reason;
            public List<SavedFile> Files=new List<SavedFile>();
        }
        public sealed class RestoreJournal { public string Owner, Backup; public bool HadSaves; }
        static bool ValidId(string id) { return Regex.IsMatch(id??"",@"\A[0-9a-f]{32}\z"); }
        static string ArchivePath(string root,string id) { Core.Require(ValidId(id),"Invalid save backup selection.");return Core.PathIn(root,Folder+"/"+id+".tsfpsave"); }
        static string Hash(byte[] bytes,int count) { using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes,0,count)).Replace("-","").ToLowerInvariant(); }
        static IEnumerable<string> Tree(string root,string dir)
        {
            foreach(string path in Directory.EnumerateFileSystemEntries(dir)) {
                string name=path.Substring(root.Length+1).Replace('\\','/');Core.PathIn(root,name);
                Core.Require(name.Length<=180,"Save path is too long for backup.");
                if(Directory.Exists(path))foreach(string child in Tree(root,path))yield return child;
                else yield return name;
            }
        }
        static SaveBackup Info(Manifest m,long size) { return new SaveBackup {Id=m.Id,Created=m.Created,Game=m.Game,Reason=m.Reason,LogicalBytes=m.Files.Sum(f=>f.Length),StoredBytes=size,Files=m.Files.Count}; }
        static int ReadBlock(ZipArchive zip,Block block,byte[] buffer)
        {
            int n=0;using(var input=zip.GetEntry(block.Entry).Open()) {
                while(n<block.Size){int read=input.Read(buffer,n,block.Size-n);Core.Require(read>0,"Truncated save data.");n+=read;}
                Core.Require(input.ReadByte()==-1,"Oversized save data.");
            }
            Core.Require(Hash(buffer,n)==block.Hash,"Save backup checksum failed. Your current saves are unchanged.");return n;
        }
        static void ValidateArchive(string path,string id,CancellationToken token)
        {
            Core.Require(new FileInfo(path).Length<=DataLimit+8*1024*1024,"Oversized save archive.");
            using(var zip=ZipFile.OpenRead(path)) {var m=Read(zip,id);var buffer=new byte[BlockSize];
                foreach(var file in m.Files)foreach(var block in file.Blocks){token.ThrowIfCancellationRequested();ReadBlock(zip,block,buffer);}}
        }
        static byte[] SmallEntry(ZipArchive zip,string name,int limit)
        {
            var entry=zip.GetEntry(name);Core.Require(entry!=null && entry.Length<=limit,"Invalid save backup metadata.");
            using(var input=entry.Open())using(var bytes=new MemoryStream()) {
                var buffer=new byte[8192];int n;while((n=input.Read(buffer,0,buffer.Length))>0){Core.Require(bytes.Length+n<=limit,"Oversized save metadata.");bytes.Write(buffer,0,n);}return bytes.ToArray();
            }
        }
        static Manifest Read(ZipArchive zip,string id)
        {
            byte[] dataBytes=SmallEntry(zip,"backup.json",4*1024*1024);
            Core.Require(System.Text.Encoding.ASCII.GetString(SmallEntry(zip,"backup.sha256",64))==Hash(dataBytes,dataBytes.Length),"Save backup metadata checksum failed.");
            Manifest m=Json.Serializer().Deserialize<Manifest>(System.Text.Encoding.UTF8.GetString(dataBytes));
            DateTime created;
            Core.Require(m!=null && m.Schema==1 && m.Owner=="tsfp-save-backup" && m.Id==id &&
                DateTime.TryParseExact(m.Created,"o",CultureInfo.InvariantCulture,DateTimeStyles.RoundtripKind,out created) &&
                new[]{"Manual","Before game update","Before restore"}.Contains(m.Reason) && m.Game!=null && m.Game.Length<=128 &&
                !m.Game.Any(Char.IsControl) && m.Files!=null && m.Files.Count>0 && m.Files.Count<=MaxFiles,"Unsupported save backup.");
            var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var entries=new HashSet<string>{"backup.json","backup.sha256"};long logical=0,data=0;int blocks=0;
            foreach(var file in m.Files) {
                Core.Require(file!=null && file.Name!=null && file.Name.Length<=180,"Invalid saved file.");
                Core.Name(file.Name);Core.Require(names.Add(file.Name) && file.Length>=0 && file.Length<=LogicalLimit && file.Blocks!=null,"Invalid or duplicate saved file.");
                logical+=file.Length;Core.Require(logical<=LogicalLimit,"Save backup exceeds the logical size limit.");long end=0;
                foreach(var block in file.Blocks) {
                    Core.Require(block!=null && block.Offset>=end && block.Size>0 && block.Size<=BlockSize && block.Offset<=file.Length-block.Size &&
                        Regex.IsMatch(block.Entry??"",@"\Adata/[0-9]{6}\z") && Regex.IsMatch(block.Hash??"",@"\A[0-9a-f]{64}\z") && entries.Add(block.Entry),"Invalid saved block.");
                    end=block.Offset+block.Size;data+=block.Size;blocks++;
                    var payload=zip.GetEntry(block.Entry);Core.Require(payload!=null && payload.Length==block.Size && blocks<=MaxBlocks && data<=DataLimit,"Missing or oversized save data.");
                }
            }
            Core.Require(zip.Entries.Count==entries.Count && zip.Entries.All(e=>entries.Contains(e.FullName)),"Unexpected files in save backup.");return m;
        }
        public static SaveBackup[] List(string root,Action<string> notice=null)
        {
            string folder=Core.PathIn(root,Folder);if(!Directory.Exists(folder))return new SaveBackup[0];
            var result=new List<SaveBackup>();
            foreach(string file in Directory.GetFiles(folder,"*.tsfpsave").OrderByDescending(File.GetLastWriteTimeUtc).Take(200)) {
                try { string id=Path.GetFileNameWithoutExtension(file);string path=ArchivePath(root,id);
                    Core.Require(new FileInfo(path).Length<=DataLimit+8*1024*1024,"Oversized save archive.");
                    using(var zip=ZipFile.OpenRead(path))result.Add(Info(Read(zip,id),new FileInfo(path).Length)); }
                catch(Exception e) { if(!(e is IOException || e is InvalidDataException || e is ArgumentException || e is InvalidOperationException))throw;if(notice!=null)notice("Some backup records need attention. Their files were kept; use Open backups folder to inspect them."); }
            }
            return result.OrderByDescending(i=>i.Created,StringComparer.Ordinal).ToArray();
        }
        public static SaveBackup Create(string root,string reason,IProgress<string> progress,CancellationToken token,bool installationLockHeld=false)
        {
            Directory.CreateDirectory(Core.PathIn(root,".launcher"));
            using(LauncherPlatform.AcquireSaveLock(root,installationLockHeld)) { Recover(root);return CreateLocked(root,reason,progress,token); }
        }
        static SaveBackup CreateLocked(string root,string reason,IProgress<string> progress,CancellationToken token)
        {
            Core.Require(!Core.GameRunning(),"Close the game before backing up saves.");
            Core.Require(new[]{"Manual","Before game update","Before restore"}.Contains(reason),"Invalid backup reason.");
            string saves=Core.PathIn(root,LauncherPlatform.Saves(root));
            if(!Directory.Exists(saves))return null;
            var names=Tree(saves,saves).Take(MaxFiles+1).ToArray();if(names.Length==0)return null;
            Core.Require(names.Length<=MaxFiles,"Too many files in the save folder; no backup was created.");
            Core.CheckSpace(root,DataLimit+16*1024*1024);
            var m=new Manifest {Schema=1,Owner="tsfp-save-backup",Id=Guid.NewGuid().ToString("N"),Created=DateTime.UtcNow.ToString("o"),Game=GameVersionLabel(root),Reason=reason};
            string output=ArchivePath(root,m.Id),temporary=output+".partial";Directory.CreateDirectory(Path.GetDirectoryName(output));
            var sources=new List<FileStream>();long logical=0,stored=0;int number=0;
            try {
                foreach(string name in names) {
                    Core.Require(name.Length<=180,"Save path is too long for backup.");
                    var source=new FileStream(Core.PathIn(saves,name),FileMode.Open,FileAccess.Read,FileShare.Read);sources.Add(source);
                    logical+=source.Length;Core.Require(logical<=LogicalLimit,"Save folder exceeds the logical size limit; no files were changed.");
                    m.Files.Add(new SavedFile {Name=name,Length=source.Length});
                }
                using(var stream=new FileStream(temporary,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.None))using(var zip=new ZipArchive(stream,ZipArchiveMode.Create)) {
                    var buffer=new byte[BlockSize];long completed=0,lastReport=-1;
                    for(int i=0;i<sources.Count;i++) {
                        var source=sources[i];var file=m.Files[i];long offset=0;
                        while(offset<file.Length) {
                            token.ThrowIfCancellationRequested();int n=(int)Math.Min(BlockSize,file.Length-offset),read=0;
                            while(read<n){int got=source.Read(buffer,read,n-read);Core.Require(got>0,"Save changed while reading; retry with the game closed.");read+=got;}
                            bool nonzero=false;for(int j=0;j<n;j++)if(buffer[j]!=0){nonzero=true;break;}
                            if(nonzero) {
                                stored+=n;Core.Require(stored<=DataLimit && number<MaxBlocks,"Save backup exceeds 512 MiB of nonempty data; original saves were not changed.");
                                var block=new Block {Offset=offset,Size=n,Hash=Hash(buffer,n),Entry="data/"+(number++).ToString("D6")};
                                using(var target=zip.CreateEntry(block.Entry,CompressionLevel.Optimal).Open())target.Write(buffer,0,n);file.Blocks.Add(block);
                            }
                            offset+=n;completed+=n;
                            if(completed/(16*1024*1024)!=lastReport){lastReport=completed/(16*1024*1024);if(progress!=null)progress.Report("Backing up "+file.Name+" · "+(logical==0?100:completed*100/logical)+"%");}
                        }
                    }
                    byte[] manifest=System.Text.Encoding.UTF8.GetBytes(Json.Encode(m));
                    using(var outputEntry=zip.CreateEntry("backup.json").Open())outputEntry.Write(manifest,0,manifest.Length);
                    using(var writer=new StreamWriter(zip.CreateEntry("backup.sha256").Open()))writer.Write(Hash(manifest,manifest.Length));
                }
                ValidateArchive(temporary,m.Id,token);token.ThrowIfCancellationRequested();File.Move(temporary,output);return Info(m,new FileInfo(output).Length);
            } finally { foreach(var source in sources)source.Dispose();if(File.Exists(temporary))File.Delete(temporary); }
        }
        public static void BeforeGameChange(string root)
        { Recover(root);CreateLocked(root,"Before game update",null,CancellationToken.None);Core.Require(!Core.GameRunning(),"Close the game before changing the installed release."); }
        static string GameVersionLabel(string root)
        {
            try {string path=Core.PathIn(root,LauncherPlatform.ManifestPath(root));if(!File.Exists(path))return "Unversioned game";
                Core.Require(new FileInfo(path).Length<=8*1024*1024,"Oversized build record.");string version=Core.InstalledVersion(root);
                return version.Length>0 && version.Length<=128 && !version.Any(Char.IsControl)?version:"Unknown game version";}
            catch(Exception ex){if(!(ex is IOException || ex is InvalidDataException || ex is ArgumentException || ex is InvalidOperationException || ex is UnauthorizedAccessException))throw;return "Unknown game version";}
        }
        public static void Restore(string root,string id,IProgress<string> progress,CancellationToken token,Action<int> checkpoint=null,bool installationLockHeld=false)
        {
            Directory.CreateDirectory(Core.PathIn(root,".launcher"));
            using(LauncherPlatform.AcquireSaveLock(root,installationLockHeld)) {
                Core.Require(!Core.GameRunning(),"Close the game before restoring saves.");Recover(root);
                string archive=ArchivePath(root,id),work=Core.PathIn(root,Work),next=Core.PathIn(root,Work+"/new"),previous=Core.PathIn(root,Work+"/previous");
                string saves=Core.PathIn(root,LauncherPlatform.Saves(root));Directory.CreateDirectory(work);
                Core.Write(Core.PathIn(root,Work+"/owner"),"tsfp-save-restore-v1");Directory.CreateDirectory(next);
                bool committed=false;
                try {
                    using(var zip=ZipFile.OpenRead(archive)) {
                        var m=Read(zip,id);var buffer=new byte[BlockSize];long stored=m.Files.Sum(f=>f.Blocks.Sum(b=>(long)b.Size));Core.CheckSpace(root,stored+16*1024*1024);
                        foreach(var file in m.Files) {
                            token.ThrowIfCancellationRequested();if(progress!=null)progress.Report("Verifying and preparing "+file.Name);
                            string target=Core.PathIn(next,file.Name);Directory.CreateDirectory(Path.GetDirectoryName(target));
                            using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                                if(file.Blocks.Sum(b=>(long)b.Size)<file.Length)LauncherPlatform.PrepareSparse(output);
                                output.SetLength(file.Length);
                                foreach(var block in file.Blocks) {
                                    token.ThrowIfCancellationRequested();int n=ReadBlock(zip,block,buffer);output.Position=block.Offset;output.Write(buffer,0,n);
                                }
                            }
                        }
                    }
                    var backup=CreateLocked(root,"Before restore",progress,token);token.ThrowIfCancellationRequested();
                    Core.Require(!Core.GameRunning(),"Close the game before restoring saves.");
                    var journal=new RestoreJournal {Owner="tsfp-save-restore",HadSaves=Directory.Exists(saves),Backup=backup==null?null:backup.Id};
                    Core.Write(Core.PathIn(root,Work+"/journal.json"),Json.Encode(journal));
                    if(journal.HadSaves)Directory.Move(saves,previous);
                    if(checkpoint!=null)checkpoint(0);
                    Directory.CreateDirectory(Path.GetDirectoryName(saves));Directory.Move(next,saves);
                    if(checkpoint!=null)checkpoint(1);
                    Core.Write(Core.PathIn(root,Work+"/committed"),"complete");committed=true;
                    if(checkpoint!=null)checkpoint(2);
                    Core.RemoveOwned(root,Work);
                } catch(Exception ex) {
                    if(committed){try{LauncherLog.Write(root,"Saves restored; temporary cleanup deferred: "+ex.GetType().Name);}catch{}return;}
                    // Roll back only the saves installed by this transaction. The
                    // pre-restore archive remains available as another restore point.
                    if(Directory.Exists(previous)) {
                        if(Directory.Exists(saves) && !Directory.Exists(next))Directory.Move(saves,next);
                        if(!Directory.Exists(saves))Directory.Move(previous,saves);
                    }
                    Core.RemoveOwned(root,Work);throw;
                }
            }
        }
        public static bool Recover(string root)
        {
            string work=Core.PathIn(root,Work);if(!Directory.Exists(work))return false;
            Core.Require(!Core.GameRunning(),"Close the game before recovering a save restore.");
            var allowed=new[]{"owner","new","previous","journal.json","journal.json.new","committed","committed.new"};
            Core.Require(Directory.GetFileSystemEntries(work).All(p=>allowed.Contains(Path.GetFileName(p))),"Unknown files in save recovery folder; everything was retained.");
            string journalPath=Core.PathIn(root,Work+"/journal.json");
            if(!File.Exists(journalPath)) {
                string owner=Core.PathIn(root,Work+"/owner");
                Core.Require(File.Exists(owner) && new FileInfo(owner).Length<=64 && File.ReadAllText(owner)=="tsfp-save-restore-v1" && !Directory.Exists(Core.PathIn(root,Work+"/previous")),"Save recovery record is missing; files were retained.");
                Core.RemoveOwned(root,Work);return true;
            }
            Core.Require(new FileInfo(journalPath).Length<4096,"Invalid save restore recovery record; files were retained.");
            var j=Json.Serializer().Deserialize<RestoreJournal>(File.ReadAllText(journalPath));
            Core.Require(j!=null && j.Owner=="tsfp-save-restore" && (j.Backup==null || ValidId(j.Backup)),"Invalid save restore recovery record; files were retained.");
            string saves=Core.PathIn(root,LauncherPlatform.Saves(root)),old=Core.PathIn(root,Work+"/previous"),next=Core.PathIn(root,Work+"/new");
            if(Directory.Exists(old)) {
                Core.Require(j.HadSaves,"Inconsistent save restore record; files were retained.");
                if(!Directory.Exists(saves)){Directory.CreateDirectory(Path.GetDirectoryName(saves));Directory.Move(old,saves);}
                else {
                    Core.Require(!Directory.Exists(next) && ((j.Backup!=null && File.Exists(ArchivePath(root,j.Backup))) || !Tree(old,old).Any()),"Save restore needs manual recovery; both save folders were retained.");
                    if(j.Backup!=null)ValidateArchive(ArchivePath(root,j.Backup),j.Backup,CancellationToken.None);
                }
            }
            Core.RemoveOwned(root,Work);return true;
        }
    }
}
