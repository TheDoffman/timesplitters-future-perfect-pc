using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace TimeSplitters.Launcher
{
    public sealed class TextureRelease
    {
        public string Version;
        public long Bytes;
        public int Count;
        public List<Release> Parts = new List<Release>();
        public Dictionary<string, Dictionary<string,string>> Files = new Dictionary<string, Dictionary<string,string>>();
    }
    public static class TextureDownload
    {
        public const string ManifestUrl = "https://raw.githubusercontent.com/" + ReleaseClient.Repository + "/main/config/textures/texture-download.json";
        public static TextureRelease Parse(string text)
        {
            var value = Json.Object(text);
            Core.Require(value == null || Json.String(value,"delivery") != "bundled", "Textures are included with game downloads. Install or update the game through the launcher, then select Display settings → Textures → Enhanced.");
            Core.Require(value == null || !value.ContainsKey("published") || Json.Flag(value,"published"), "The optional texture pack is not published yet. Your installed textures and Classic mode remain available.");
            bool indexed = value != null && Json.String(value,"schema") == "3";
            Core.Require(value != null && (indexed || Json.String(value,"schema") == "1"), "Texture download information is unavailable or unsupported.");
            string version = Json.String(value,"version");
            Core.Require(Regex.IsMatch(version,@"^[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.]+)?$"), "Invalid texture pack version.");
            var pack = new TextureRelease { Version=version };
            Core.Require(long.TryParse(Json.String(value,"installed_bytes"),out pack.Bytes) && pack.Bytes>0 && pack.Bytes<=TexturePack.Budget &&
                int.TryParse(Json.String(value,"texture_count"),out pack.Count) && pack.Count>0 && pack.Count<=20000,
                "Texture pack exceeds supported installation limits.");
            object entries;Core.Require(value.TryGetValue("parts",out entries) && entries is System.Collections.IList, "Texture download parts are missing.");
            var parts=((System.Collections.IList)entries).Cast<object>().ToArray();
            Core.Require(parts.Length>0 && parts.Length<=256,"Invalid texture download part count.");
            var allFiles=new HashSet<string>();
            foreach(var entry in parts) {
                var part=entry as Dictionary<string,object>;Core.Require(part!=null,"Invalid texture download part.");
                string name=Json.String(part,"name"),hash=Json.String(part,"sha256");
                long size=0;
                Core.Require(Regex.IsMatch(name,@"^textures-[0-9]{3}\.zip$") && Regex.IsMatch(hash,"^[0-9a-f]{64}$") &&
                    long.TryParse(Json.String(part,"size"),out size) && size>0 && size<=64L*1024*1024,
                    "Texture download size, name or checksum is invalid.");
                if(indexed) {
                    object fileValue;
                    Core.Require(part.TryGetValue("files",out fileValue) && fileValue is Dictionary<string,object>, "Texture file index is missing.");
                    var indexedFiles=(Dictionary<string,object>)fileValue;
                    Core.Require(indexedFiles.Count>0 && indexedFiles.Count<=40000,"Invalid texture file count.");
                    var files=new Dictionary<string,string>();
                    foreach(var file in indexedFiles) {
                        string fileHash=file.Value as string;
                        Core.Require(Regex.IsMatch(file.Key,@"\A[0-9a-f]{16}\.(dds|json)\z") && fileHash!=null &&
                            Regex.IsMatch(fileHash,@"\A[0-9a-f]{64}\z") && allFiles.Add(file.Key),"Invalid or duplicate indexed texture.");
                        files.Add(file.Key,fileHash);
                    }
                    Core.Require(files.Keys.All(f=>files.ContainsKey(Path.ChangeExtension(f,f.EndsWith(".dds")?".json":".dds"))),"Incomplete indexed texture pair.");
                    Core.Require(!pack.Files.ContainsKey(name),"Duplicate texture download part.");
                    pack.Files.Add(name,files);
                }
                string url=ReleaseClient.ReleasesPage+"/download/textures-v"+version+"/"+name;
                ReleaseClient.ValidateDownloadUrl(url);
                pack.Parts.Add(new Release {Tag=version,AssetName=name,AssetUrl=url,Size=size,Digest="sha256:"+hash,ChecksumUrl=""});
            }
            Core.Require(pack.Parts.Select(p=>p.AssetName).Distinct().Count()==pack.Parts.Count,"Duplicate texture download part.");
            Core.Require(!indexed || allFiles.Count==pack.Count*2,"Texture count does not match the file index.");
            return pack;
        }
        public static bool OptedIn(string root)
        { return File.Exists(Core.PathIn(root,".launcher/texture-opt-in.json")); }
        public static void SetOptedIn(string root,bool value)
        {
            string path=Core.PathIn(root,".launcher/texture-opt-in.json");
            if(value)Core.Write(path,"{\"schema\":1}");
            else if(File.Exists(path))File.Delete(path);
        }
        public static long DownloadBytes(string root,TextureRelease pack,CancellationToken token)
        { return pack.Parts.Where(p=>!InstalledPart(root,pack,p,token)).Sum(p=>p.Size); }
        static bool InstalledPart(string root,TextureRelease pack,Release part,CancellationToken token)
        {
            Dictionary<string,string> files;
            if(!pack.Files.TryGetValue(part.AssetName,out files))return InstalledPart(root,part,token);
            return FilesMatch(root,files,token);
        }
        static bool FilesMatch(string root,Dictionary<string,string> files,CancellationToken token)
        {
            foreach(var file in files) {
                token.ThrowIfCancellationRequested();
                string target=Core.PathIn(TexturePack.Destination(root),file.Key);
                if(!File.Exists(target) || new FileInfo(target).Length>24*1024*1024 || Core.Hash(target)!=file.Value)return false;
            }
            return true;
        }
        static void VerifyIndex(TextureRelease pack,Release part,string zip)
        {
            Dictionary<string,string> expected;
            if(!pack.Files.TryGetValue(part.AssetName,out expected))return;
            using(var archive=ZipFile.OpenRead(zip)) {
                Core.Require(archive.Entries.Count==expected.Count,"Texture archive differs from its file index.");
                var seen=new HashSet<string>();long expanded=0;
                foreach(var entry in archive.Entries) {
                    string hash;
                    Core.Require(expected.TryGetValue(entry.FullName,out hash) && seen.Add(entry.FullName) &&
                        entry.Length>0 && entry.Length<=(entry.FullName.EndsWith(".json")?65536:24L*1024*1024),"Unexpected indexed texture file.");
                    expanded+=entry.Length;Core.Require(expanded<=64L*1024*1024,"Expanded texture part exceeds its limit.");
                    using(var input=entry.Open())using(var sha=System.Security.Cryptography.SHA256.Create())
                        Core.Require(BitConverter.ToString(sha.ComputeHash(input)).Replace("-","").ToLowerInvariant()==hash,"Indexed texture checksum mismatch.");
                }
            }
        }
        public static async Task<TextureRelease> Check(ReleaseClient client,CancellationToken token)
        {
            using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
                timeout.CancelAfter(TimeSpan.FromSeconds(30));
                try {
                    var text=await client.ReadTextureManifest(timeout.Token).ConfigureAwait(false);
                    Core.Require(!String.IsNullOrWhiteSpace(text),"The optional texture pack has not been published yet. Your installed textures and Classic mode remain available.");
                    return Parse(text);
                }catch(OperationCanceledException){token.ThrowIfCancellationRequested();throw new TimeoutException("Texture download check timed out. Try again when your connection is available.");}
            }
        }
        public static async Task Install(string root,ReleaseClient client,TextureRelease pack,IProgress<string> progress,CancellationToken token)
        {
            Core.Require(!Core.GameRunning(),"Close the game before downloading textures.");
            string zip=Core.PathIn(root,".launcher/texture-download.zip");
            string cache=Core.PathIn(root,".launcher/downloads/textures");
            TexturePack.Recover(root);
            Core.CheckSpace(root,160L*1024*1024);
            for(int i=0;i<pack.Parts.Count;i++) {
                token.ThrowIfCancellationRequested();
                var part=pack.Parts[i];string label="Part "+(i+1)+" of "+pack.Parts.Count;
                if(await Task.Run(()=>InstalledPart(root,pack,part,token),token).ConfigureAwait(false)) {
                    if(progress!=null)progress.Report(label+" · already installed and verified");
                    continue;
                }
                // An interrupted transfer may have already published this exact ZIP.
                if(File.Exists(zip)) {
                    Core.Require(new FileInfo(zip).Length<=64L*1024*1024,"Unexpected texture download file was kept for inspection.");
                    if(new FileInfo(zip).Length!=part.Size || Core.Hash(zip)!=part.Digest.Substring(7))File.Delete(zip);
                }
                try {
                    if(!File.Exists(zip))await client.Download(part,zip,new TransferProgress(s=> {
                        if(progress!=null)progress.Report(label+" · "+s.Summary);
                    }),token,cache).ConfigureAwait(false);
                    await Task.Run(()=>VerifyIndex(pack,part,zip),token).ConfigureAwait(false);
                    await Task.Run(()=>TexturePack.Install(root,zip,progress,token),token).ConfigureAwait(false);
                    RecordPart(root,part,zip);
                    DownloadTransfer.Forget(cache);
                } finally {
                    // Never keep a second completed archive beside the resumable cache.
                    if(File.Exists(zip))File.Delete(zip);
                }
            }
            DownloadTransfer.Forget(cache);
            Core.Write(Core.PathIn(root,".launcher/texture-pack.json"),Json.Encode(new { version=pack.Version, texture_count=pack.Count }));
        }
        static string Receipt(string root,Release part)
        { return Core.PathIn(root,".launcher/texture-parts/"+part.Digest.Substring(7)+".json"); }
        internal static bool InstalledPart(string root,Release part,CancellationToken token)
        {
            try {
                string path=Receipt(root,part);
                if(!File.Exists(path) || new FileInfo(path).Length>4*1024*1024)return false;
                var files=Json.Serializer().Deserialize<Dictionary<string,string>>(File.ReadAllText(path));
                if(files==null || files.Count==0 || files.Count>40000)return false;
                foreach(var file in files) {
                    token.ThrowIfCancellationRequested();
                    if(!Regex.IsMatch(file.Key,"^[0-9a-f]{16}\\.(dds|json)$"))return false;
                    string target=Core.PathIn(TexturePack.Destination(root),file.Key);
                    if(!File.Exists(target) || new FileInfo(target).Length>24*1024*1024 || Core.Hash(target)!=file.Value)return false;
                }
                return true;
            }catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}catch(ArgumentException){return false;}
        }
        static void RecordPart(string root,Release part,string zip)
        {
            var files=new Dictionary<string,string>();
            using(var archive=ZipFile.OpenRead(zip))foreach(var entry in archive.Entries) {
                if(!entry.FullName.EndsWith(".dds",StringComparison.Ordinal))continue;
                string name=Path.GetFileName(entry.FullName);
                foreach(var file in new[]{name,Path.ChangeExtension(name,".json")})
                    files[file]=Core.Hash(Core.PathIn(TexturePack.Destination(root),file));
            }
            Core.Write(Receipt(root,part),Json.Encode(files));
        }
        public static string Description(TextureRelease pack,long? downloadBytes=null)
        {
            return "Download texture pack "+pack.Version+"? "+pack.Count.ToString("N0")+" replacements, "+
                ((downloadBytes??pack.Parts.Sum(p=>p.Size))/1048576.0).ToString("0.0",CultureInfo.InvariantCulture)+" MiB download, "+
                (pack.Bytes/1048576.0).ToString("N0",CultureInfo.InvariantCulture)+" MiB installed. "+
                "This alpha covers part of the game. Matching texture keys are updated; other textures, saves and settings are kept.";
        }
    }
}
