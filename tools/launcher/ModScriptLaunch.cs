using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace TimeSplitters.Launcher
{
    // Installation-local user preference. Uploaded packages/catalogs cannot set
    // it. Native code still enforces owner permissions and executable-cache rules.
    public static class ModCacheLocation
    {
        static string Preference(string root){return Core.PathIn(root,Path.GetDirectoryName(LauncherPlatform.Settings(root)[0]).Replace('\\','/')+"/mod-cache-location.txt");}
        static string Validate(string path)
        {
            Core.Require(!String.IsNullOrWhiteSpace(path) && Encoding.UTF8.GetByteCount(path)<4095 && Path.IsPathRooted(path) && !path.Any(Char.IsControl),"Choose an absolute gameplay mod storage folder.");
            string full=Path.GetFullPath(path);
            Core.Require(String.Equals(Path.GetPathRoot(path),Path.GetPathRoot(full),SaveLocations.Comparison) && !full.StartsWith(@"\\",StringComparison.Ordinal) && Directory.Exists(full),"The gameplay mod storage folder is unavailable or is not a local folder.");
            Core.PathIn(full,".tsfp-mod-cache-check");return full;
        }
        public static string Load(string root)
        {
            string file=Preference(root);if(!File.Exists(file))return "";
            Core.Require(new FileInfo(file).Length<4096,"The gameplay mod storage preference is too large.");
            string text=new UTF8Encoding(false,true).GetString(File.ReadAllBytes(file)).TrimEnd('\r','\n');return text.Length==0?"":Validate(text);
        }
        public static void Save(string root,string path)
        {
            Core.Require(!Core.GameRunning(),"Close the game before changing gameplay mod storage.");
            Core.Write(Preference(root),String.IsNullOrWhiteSpace(path)?"":Validate(path));
        }
    }
    internal sealed class ModAudioPin
    {
        public uint Key;
        public string Package,Sha256;
        public ulong Bytes;
    }
    // Data-only handoff after the caller verifies catalog, active profile and
    // package contents. No uploaded paths or executable payloads enter this wire.
    internal sealed class ModAudioCapsule
    {
        public readonly byte[] Bytes;
        public readonly string Sha256;
        ModAudioCapsule(byte[] bytes){Bytes=bytes;Sha256=ModTrust.Hash(bytes);}
        static void Need(bool value){if(!value)throw new InvalidDataException("Invalid mod audio handoff.");}
        static byte[] Digest(string text)
        {
            Need(text!=null && text.Length==64);
            foreach(char c in text)Need((c>='0' && c<='9') || (c>='a' && c<='f'));
            var bytes=new byte[32];for(int i=0;i<32;i++)bytes[i]=Convert.ToByte(text.Substring(i*2,2),16);return bytes;
        }
        public static ModAudioCapsule Create(IEnumerable<string> authorizedPackages,IEnumerable<ModAudioPin> assets)
        {
            Need(authorizedPackages!=null && assets!=null);
            var packages=new List<string>();
            foreach(string package in authorizedPackages){Need(packages.Count<32);Digest(package);packages.Add(package);}
            packages.Sort(StringComparer.Ordinal);Need(packages.Count>0);
            for(int i=1;i<packages.Count;i++)Need(packages[i]!=packages[i-1]);
            var pins=new List<ModAudioPin>();ulong total=0;
            foreach(var pin in assets) {
                Need(pin!=null && pins.Count<8192 && pin.Key!=UInt32.MaxValue && packages.Contains(pin.Package));
                Digest(pin.Sha256);Need(pin.Bytes>0 && pin.Bytes<=64UL*1024*1024 && pin.Bytes<=256UL*1024*1024-total);total+=pin.Bytes;
                pins.Add(new ModAudioPin{Key=pin.Key,Package=pin.Package,Sha256=pin.Sha256,Bytes=pin.Bytes});
            }
            pins.Sort((a,b)=>a.Key.CompareTo(b.Key));Need(pins.Count>0);
            for(int i=1;i<pins.Count;i++)Need(pins[i].Key!=pins[i-1].Key);
            using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)) {
                writer.Write(Encoding.ASCII.GetBytes("TSFPAUD1"));writer.Write((uint)packages.Count);writer.Write((uint)pins.Count);
                foreach(string package in packages)writer.Write(Digest(package));
                foreach(var pin in pins){writer.Write(pin.Key);writer.Write((uint)packages.IndexOf(pin.Package));writer.Write(pin.Bytes);writer.Write(Digest(pin.Sha256));}
                return new ModAudioCapsule(stream.ToArray());
            }
        }
    }
    // Source handoff only. The caller authenticates packages/catalog and verifies
    // the source against its package BEFORE creating a capsule. No paths, helper
    // hashes, policy assertions or executable/native payloads belong in this wire.
    internal sealed class ModScriptCapsule
    {
        public readonly byte[] Bytes;
        public readonly string Sha256,ProfileSha256;
        ModScriptCapsule(byte[] bytes,string profile){Bytes=bytes;Sha256=Hash(bytes);ProfileSha256=profile;}
        static void Need(bool value){if(!value)throw new InvalidDataException("Invalid mod script source handoff.");}
        static string Hash(byte[] bytes){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
        static byte[] Digest(string text)
        {
            Need(text!=null && text.Length==64);
            foreach(char c in text)Need((c>='0' && c<='9') || (c>='a' && c<='f'));
            var result=new byte[32];for(int i=0;i<32;i++)result[i]=Convert.ToByte(text.Substring(i*2,2),16);return result;
        }
        public static ModScriptCapsule Create(string package,IEnumerable<string> dependencies,byte[] source)
        {
            byte[] primary=Digest(package);Need(dependencies!=null && source!=null && source.Length>0 && source.Length<=1024*1024);
            source=(byte[])source.Clone();string text=new UTF8Encoding(false,true).GetString(source);
            Need(text.IndexOf('\0')<0 && !(source.Length>=4 && source[0]==27 && source[1]==76 && source[2]==117 && source[3]==97));
            var hashes=new List<string>();
            foreach(string dependency in dependencies){Need(hashes.Count<31);Digest(dependency);Need(dependency!=package);hashes.Add(dependency);}
            hashes.Sort(StringComparer.Ordinal);
            for(int i=1;i<hashes.Count;i++)Need(hashes[i]!=hashes[i-1]);
            byte[] identities;
            using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)) {
                writer.Write(primary);foreach(string hash in hashes)writer.Write(Digest(hash));identities=stream.ToArray();
            }
            string profile;
            using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)) {
                writer.Write(Encoding.ASCII.GetBytes("TSFP isolated mode saves v1\n"));writer.Write((uint)(hashes.Count+1));writer.Write(identities);profile=Hash(stream.ToArray());
            }
            using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)) {
                writer.Write(Encoding.ASCII.GetBytes("TSFPMOD1"));writer.Write((uint)(hashes.Count+1));writer.Write((uint)source.Length);
                writer.Write(identities);writer.Write(source);return new ModScriptCapsule(stream.ToArray(),profile);
            }
        }
    }
}
