using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace TimeSplitters.Launcher
{
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
