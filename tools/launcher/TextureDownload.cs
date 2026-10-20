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
    }
    public static class TextureDownload
    {
        public const string ManifestUrl = "https://raw.githubusercontent.com/" + ReleaseClient.Repository + "/main/config/texture-pack.json";
        public static TextureRelease Parse(string text)
        {
            var value = Json.Object(text);
            Core.Require(value == null || Json.String(value,"delivery") != "bundled", "Textures are included with game downloads. Install or update the game through the launcher, then select Display settings → Textures → Enhanced.");
            Core.Require(value != null && Json.String(value,"schema") == "1", "Texture download information is unavailable or unsupported.");
            string version = Json.String(value,"version");
            Core.Require(Regex.IsMatch(version,@"^[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.]+)?$"), "Invalid texture pack version.");
            var pack = new TextureRelease { Version=version };
            Core.Require(long.TryParse(Json.String(value,"installed_bytes"),out pack.Bytes) && pack.Bytes>0 && pack.Bytes<=TexturePack.Budget &&
                int.TryParse(Json.String(value,"texture_count"),out pack.Count) && pack.Count>0 && pack.Count<=20000,
                "Texture pack exceeds supported installation limits.");
            object entries;Core.Require(value.TryGetValue("parts",out entries) && entries is System.Collections.IList, "Texture download parts are missing.");
            var parts=((System.Collections.IList)entries).Cast<object>().ToArray();
            Core.Require(parts.Length>0 && parts.Length<=128,"Invalid texture download part count.");
            foreach(var entry in parts) {
                var part=entry as Dictionary<string,object>;Core.Require(part!=null,"Invalid texture download part.");
                string name=Json.String(part,"name"),hash=Json.String(part,"sha256");
                long size=0;
                Core.Require(Regex.IsMatch(name,@"^textures-[0-9]{3}\.zip$") && Regex.IsMatch(hash,"^[0-9a-f]{64}$") &&
                    long.TryParse(Json.String(part,"size"),out size) && size>0 && size<=64L*1024*1024,
                    "Texture download size, name or checksum is invalid.");
                string url=ReleaseClient.ReleasesPage+"/download/textures-v"+version+"/"+name;
                ReleaseClient.ValidateDownloadUrl(url);
                pack.Parts.Add(new Release {Tag=version,AssetName=name,AssetUrl=url,Size=size,Digest="sha256:"+hash,ChecksumUrl=""});
            }
            Core.Require(pack.Parts.Select(p=>p.AssetName).Distinct().Count()==pack.Parts.Count,"Duplicate texture download part.");
            return pack;
        }
        public static async Task<TextureRelease> Check(ReleaseClient client,CancellationToken token)
        {
            using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(token)) {
                timeout.CancelAfter(TimeSpan.FromSeconds(30));
                try {
                    var text=await client.ReadTextureManifest(timeout.Token).ConfigureAwait(false);
                    Core.Require(!String.IsNullOrWhiteSpace(text),"The optional texture pack has not been published yet. You can still install a local pack.");
                    return Parse(text);
                }catch(OperationCanceledException){token.ThrowIfCancellationRequested();throw new TimeoutException("Texture download check timed out. Try again when your connection is available.");}
            }
        }
        public static async Task Install(string root,ReleaseClient client,TextureRelease pack,IProgress<string> progress,CancellationToken token)
        {
            Core.Require(!Core.GameRunning(),"Close the game before downloading textures.");
            string zip=Core.PathIn(root,".launcher/texture-download.zip");
            string cache=Core.PathIn(root,".launcher/downloads/textures");
            Core.CheckSpace(root,pack.Bytes+160L*1024*1024);
            for(int i=0;i<pack.Parts.Count;i++) {
                token.ThrowIfCancellationRequested();
                var part=pack.Parts[i];string label="Part "+(i+1)+" of "+pack.Parts.Count;
                if(await Task.Run(()=>InstalledPart(root,part,token),token).ConfigureAwait(false)) {
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
        public static string Description(TextureRelease pack)
        {
            return "Download texture pack "+pack.Version+"? "+pack.Count.ToString("N0")+" replacements, "+
                (pack.Parts.Sum(p=>p.Size)/1048576.0).ToString("N0",CultureInfo.InvariantCulture)+" MiB download, "+
                (pack.Bytes/1048576.0).ToString("N0",CultureInfo.InvariantCulture)+" MiB installed. "+
                "This alpha covers part of the game. Matching texture keys are updated; other textures, saves and settings are kept.";
        }
    }
}
