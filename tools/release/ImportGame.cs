// XDVDFS reader for the supported Xbox disc. No external runtime dependencies.
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Security.Cryptography;

public static class TsfpImport
{
    public const string ExpectedHash = "3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc";
    sealed class Entry { public string Name; public long Offset, Size; }
    static void Require(bool ok, string message) { if (!ok) throw new InvalidDataException(message); }
    static void Range(Stream s, long offset, long size) {
        Require(offset >= 0 && size >= 0 && offset <= s.Length && size <= s.Length-offset, "Image is truncated or corrupt.");
    }
    static byte[] Read(Stream s, long offset, int size) {
        Range(s, offset, size); s.Position=offset;
        byte[] b=new byte[size]; int n=0;
        while(n<size) { int got=s.Read(b,n,size-n); if(got==0) throw new EndOfStreamException(); n+=got; }
        return b;
    }
    static string Hash(byte[] bytes) {
        using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }
    static void NameCheck(string name) {
        Require(name.Length>0 && name!="." && name!=".." && name.TrimEnd(' ','.')==name &&
            name.IndexOfAny(Path.GetInvalidFileNameChars())<0, "Unsafe filename in game data.");
        string stem=name.Split('.')[0].ToUpperInvariant();
        Require(stem!="CON" && stem!="PRN" && stem!="AUX" && stem!="NUL" &&
            !(stem.Length==4 && (stem.StartsWith("COM") || stem.StartsWith("LPT")) && Char.IsDigit(stem[3])), "Reserved filename in game data.");
    }
    static void DirectoryEntries(Stream s,long part,uint sector,uint size,string prefix,int depth,
        HashSet<long> dirs,HashSet<string> names,List<Entry> files) {
        Require(depth<=64 && dirs.Count<10000 && size<=64*1024*1024, "Invalid directory structure.");
        long at=part+(long)sector*2048;
        Require(dirs.Add(at), "Repeated directory in image.");
        byte[] data=Read(s,at,(int)size);
        for(int block=0;block<data.Length;block+=2048) {
            int end=Math.Min(block+2048,data.Length);
            for(int off=block;off+14<=end;) {
                int len=data[off+13];
                if(len==0 || (BitConverter.ToUInt16(data,off)==65535 && BitConverter.ToUInt16(data,off+2)==65535)) break;
                Require(off+14+len<=end,"Truncated directory entry.");
                for(int i=0;i<len;i++) Require(data[off+14+i]>=32 && data[off+14+i]<127,"Non-ASCII filename in image.");
                string name=Encoding.ASCII.GetString(data,off+14,len); NameCheck(name);
                string relative=prefix+name;
                Require(names.Add(relative) && names.Count<=100000,"Duplicate or excessive paths in image.");
                uint sec=BitConverter.ToUInt32(data,off+4), bytes=BitConverter.ToUInt32(data,off+8);
                Range(s,part+(long)sec*2048,bytes);
                if((data[off+12]&16)!=0) DirectoryEntries(s,part,sec,bytes,relative+"/",depth+1,dirs,names,files);
                else files.Add(new Entry {Name=relative,Offset=part+(long)sec*2048,Size=bytes});
                off+=(14+len+3)&~3;
            }
        }
    }
    static List<Entry> Catalog(Stream s) {
        long part=-1;
        foreach(long candidate in new long[]{0,0xFD90,0x30600,0xFD90000,0x18300000}) {
            long at=candidate+32*2048;
            if(at+2048<=s.Length && Encoding.ASCII.GetString(Read(s,at,20))=="MICROSOFT*XBOX*MEDIA") { part=candidate; break; }
        }
        Require(part>=0,"Select an extracted Xbox XISO or supported full-disc Xbox ISO.");
        byte[] desc=Read(s,part+32*2048,2048);
        var files=new List<Entry>();
        DirectoryEntries(s,part,BitConverter.ToUInt32(desc,20),BitConverter.ToUInt32(desc,24),"",0,
            new HashSet<long>(),new HashSet<string>(StringComparer.OrdinalIgnoreCase),files);
        Entry xbe=files.Find(e=>e.Name.Equals("default.xbe",StringComparison.OrdinalIgnoreCase));
        Require(xbe!=null && xbe.Size==6270976,"This is not the supported US Xbox release.");
        Require(Hash(Read(s,xbe.Offset,(int)xbe.Size))==ExpectedHash,"Wrong game version: the US Xbox executable is required.");
        Require(files.Exists(e=>e.Name.StartsWith("pak/",StringComparison.OrdinalIgnoreCase)) &&
            files.Exists(e=>e.Name.StartsWith("xmv/",StringComparison.OrdinalIgnoreCase)),"Game data folders are missing.");
        return files;
    }
    public static int ValidateIso(string iso) {
        using(var s=File.OpenRead(iso)) return Catalog(s).Count;
    }
    public static void ImportIso(string iso,string destination) {
        Require(!Directory.Exists(destination) && !File.Exists(destination),"Data already exists; it will not be overwritten.");
        using(var s=File.OpenRead(iso)) {
            var files=Catalog(s); long total=0; foreach(var e in files) total+=e.Size;
            string parent=Path.GetDirectoryName(Path.GetFullPath(destination));
            Require(TsfpDiskSpace.Available(parent)>total+128*1024*1024,"Not enough disk space for extracted game data.");
            string stage=Path.Combine(parent,"Data-import-"+Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(stage);
            try {
                byte[] buffer=new byte[1024*1024]; int count=0;
                foreach(var e in files) {
                    string output=Path.Combine(stage,e.Name.Replace('/',Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(output)); s.Position=e.Offset;
                    using(var target=new FileStream(output,FileMode.CreateNew,FileAccess.Write)) {
                        long remaining=e.Size;
                        while(remaining>0) { int n=s.Read(buffer,0,(int)Math.Min(buffer.Length,remaining));
                            if(n==0) throw new EndOfStreamException(); target.Write(buffer,0,n); remaining-=n; }
                    }
                    count++; if(count%25==0 || count==files.Count) Console.WriteLine("Imported {0}/{1} files",count,files.Count);
                }
                Directory.Move(stage,destination);
            } catch { Console.Error.WriteLine("Incomplete import retained at: "+stage); throw; }
        }
    }
    static void FolderFiles(string root,string relative,int depth,List<string> files) {
        Require(depth<=64,"Folder is too deeply nested.");
        string current=Path.Combine(root,relative);
        Require((File.GetAttributes(current)&FileAttributes.ReparsePoint)==0,"Linked folders are not supported.");
        foreach(string path in Directory.GetFileSystemEntries(current)) {
            var attr=File.GetAttributes(path); Require((attr&FileAttributes.ReparsePoint)==0,"Linked files are not supported.");
            string name=Path.GetFileName(path); NameCheck(name);
            string sub=Path.Combine(relative,name);
            if((attr&FileAttributes.Directory)!=0) FolderFiles(root,sub,depth+1,files);
            else { files.Add(sub); Require(files.Count<=100000,"Too many game files."); }
        }
    }
    public static void ImportFolder(string source,string destination) {
        source=Path.GetFullPath(source); destination=Path.GetFullPath(destination);
        Require(!Directory.Exists(destination) && !File.Exists(destination),"Data already exists; it will not be overwritten.");
        string xbe=Path.Combine(source,"default.xbe");
        Require(File.Exists(xbe) && new FileInfo(xbe).Length==6270976 && Hash(File.ReadAllBytes(xbe))==ExpectedHash,
            "Select the folder containing the supported US Xbox default.xbe.");
        Require(Directory.Exists(Path.Combine(source,"pak")) && Directory.Exists(Path.Combine(source,"xmv")),"Game data folders are missing.");
        var files=new List<string> { "default.xbe" };
        FolderFiles(source,"pak",0,files); FolderFiles(source,"xmv",0,files);
        long total=0; foreach(string p in files) total+=new FileInfo(Path.Combine(source,p)).Length;
        Require(TsfpDiskSpace.Available(destination)>total+128*1024*1024,"Not enough disk space for game data.");
        string stage=Path.Combine(Path.GetDirectoryName(destination),"Data-import-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage);
        try {
            foreach(string p in files) { string output=Path.Combine(stage,p); Directory.CreateDirectory(Path.GetDirectoryName(output)); File.Copy(Path.Combine(source,p),output,false); }
            Directory.Move(stage,destination);
        } catch { Console.Error.WriteLine("Incomplete import retained at: "+stage); throw; }
    }
}

// Query the destination filesystem, not / on Unix (home and SD cards can be
// separate mounts). Walk to an existing parent for not-yet-created staging paths.
public static class TsfpDiskSpace
{
    public static long Available(string destination)
    {
        string path=Path.GetFullPath(destination);
        while(!Directory.Exists(path)) {
            string parent=Path.GetDirectoryName(path);
            if(String.IsNullOrEmpty(parent) || parent==path)
                throw new DirectoryNotFoundException("Cannot find the destination filesystem.");
            path=parent;
        }
        return new DriveInfo(path).AvailableFreeSpace;
    }
}
