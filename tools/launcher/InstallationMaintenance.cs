using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
#if !MACOS
using System.Management;
#endif
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace TimeSplitters.Launcher
{
    public sealed class VerificationResult
    {
        public string Component, Version = "", ManifestHash = "";
        public int Checked, Skipped;
        public readonly List<string> Issues = new List<string>();
        public bool Healthy { get { return Version.Length > 0 && Issues.Count == 0; } }
        public string Summary { get { return Healthy ? "Verified against installed manifest" : "Needs attention"; } }
        public override string ToString()
        {
            return Component + " " + (Version.Length == 0 ? "(unknown version)" : Version) + ": " + Summary +
                "\n" + Checked + " files checked; " + Skipped + " personal/independent files excluded.\n" +
                String.Join("\n", Issues.Take(100)) + (Issues.Count > 100 ? "\nAdditional issues: " + (Issues.Count-100) : "");
        }
    }
    public static class InstallationMaintenance
    {
        public static string Version(string root, bool launcher)
        {
            try {
                string path=Core.PathIn(root,launcher?Core.SelfManifest:"BUILD.json");
                Core.Require(new FileInfo(path).Length <= (launcher?LauncherPlatform.LauncherManifestLimit:1024*1024),"Manifest too large.");
                string version=Json.String(Json.Object(File.ReadAllText(path)),"version");
                GameVersion.Parse(version); return version;
            } catch { return ""; }
        }
        static string Hash(string path,CancellationToken token)
        {
            using(var input=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))
            using(var sha=SHA256.Create()) {
                byte[] buffer=new byte[65536]; int read;
                while((read=input.Read(buffer,0,buffer.Length))>0) {
                    token.ThrowIfCancellationRequested(); sha.TransformBlock(buffer,0,read,buffer,0);
                }
                sha.TransformFinalBlock(new byte[0],0,0);
                return BitConverter.ToString(sha.Hash).Replace("-","").ToLowerInvariant();
            }
        }
        // Read-only: no log writes, recovery, settings creation, or cleanup.
        public static VerificationResult Verify(string root,bool launcher,IProgress<double> progress,CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            var result=new VerificationResult { Component=launcher?"Launcher":"Game" };
            try {
                string manifest=Core.PathIn(root,launcher?Core.SelfManifest:"BUILD.json");
                Core.Require(new FileInfo(manifest).Length <= (launcher?LauncherPlatform.LauncherManifestLimit:1024*1024),"Manifest too large.");
                result.ManifestHash=Hash(manifest,token);
                result.Version=Version(root,launcher);
                Core.Require(result.Version.Length>0,"Invalid version.");
                var files=launcher?SelfUpdate.Manifest(File.ReadAllText(manifest),result.Version):Core.GameFiles(manifest);
                int visited=0;
                foreach(var pair in files) {
                    token.ThrowIfCancellationRequested();
                    if(!launcher && Core.PreserveGameFile(pair.Key)) result.Skipped++;
                    else {
                        result.Checked++;
                        try {
                            string path=Core.PathIn(root,pair.Key);
                            if(!File.Exists(path)) result.Issues.Add(pair.Key+": missing");
                            else if(!Hash(path,token).Equals(pair.Value,StringComparison.OrdinalIgnoreCase)) result.Issues.Add(pair.Key+": checksum mismatch");
                        } catch(OperationCanceledException) { throw; }
                        catch(Exception ex) { result.Issues.Add(pair.Key+": unreadable ("+ex.GetType().Name+")"); }
                    }
                    if(progress!=null)progress.Report(100.0*++visited/files.Count);
                }
                if(Hash(manifest,token)!=result.ManifestHash) result.Issues.Add("Manifest changed during verification; verify again.");
            } catch(OperationCanceledException) { throw; }
            catch(Exception ex) { result.Issues.Add("Manifest missing, unreadable or invalid ("+ex.GetType().Name+")."); }
            return result;
        }
        public static async Task PrepareGameRepair(string root,string version,ReleaseClient client,IProgress<double> progress,CancellationToken token)
        {
            Core.Require(!Core.GameRunning(),"Close the game before repairing files.");
            Core.Require(Version(root,false)==version && version.Length>0,"The installed game version is unknown or changed. Verify again before repairing.");
            string pending=Core.PathIn(root,".launcher/pending");
            Core.Require(!Directory.Exists(pending),"An unfinished game update is already staged. Its files were kept; use Support before starting another repair.");
            Core.Require(!Directory.Exists(Core.PathIn(root,".launcher/transaction")) && !Directory.Exists(Core.PathIn(root,".launcher/self-transaction")) && !Directory.Exists(Core.PathIn(root,".launcher/repair-transaction")),"An update needs recovery. Restart the launcher before repairing.");
            var release=await client.FindGame(version,token).ConfigureAwait(false);
            token.ThrowIfCancellationRequested();
            Core.CheckSpace(root,release.Size*4+256L*1024*1024);
            // Recheck after the request; never take ownership of someone else's staging.
            Core.Require(!Directory.Exists(pending),"A game update is already staged.");
            Directory.CreateDirectory(pending);
            try {
                string archive=Path.Combine(pending,"release.zip");
                await client.Download(release,archive,progress,token,Core.PathIn(root,".launcher/downloads/game")).ConfigureAwait(false);
                await Task.Run(()=>Core.Unpack(archive,Path.Combine(pending,"payload"),version,token),token).ConfigureAwait(false);
                token.ThrowIfCancellationRequested();
                Core.Require(Version(root,false)==version,"The installed game version changed during download. Verify again.");
                Core.Write(Path.Combine(pending,"release.json"),Json.Encode(new { version=version,repair=true }));
                File.Delete(archive);DownloadTransfer.Forget(Core.PathIn(root,".launcher/downloads/game"));
            } catch {
                try { Core.DiscardGameStaging(root); } catch { /* Retain unsafe/locked staging for recovery. */ }
                throw;
            }
        }
        static string Clean(string text)
        { text=Regex.Replace(text??"",@"[\r\n\t\x00-\x1f]"," ").Trim(); return text.Substring(0,Math.Min(256,text.Length)); }
        static readonly object graphicsGate=new object();
        static Task<string> graphicsQuery;
        public static async Task<string> GraphicsWithTimeout(CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            Task<string> query;
            lock(graphicsGate) {
                if(graphicsQuery==null || graphicsQuery.IsCompleted)graphicsQuery=Task.Run(()=>Graphics());
                query=graphicsQuery;
            }
            var finished=await Task.WhenAny(query,Task.Delay(TimeSpan.FromSeconds(4),token)).ConfigureAwait(false);
            token.ThrowIfCancellationRequested();
            return finished==query?await query.ConfigureAwait(false):"Unavailable (graphics query timed out)";
        }
        public static string Graphics()
        {
#if MACOS
            return NativeHost.Graphics();
#else
            try {
                var options=new EnumerationOptions { Timeout=TimeSpan.FromSeconds(3),ReturnImmediately=false };
                using(var search=new ManagementObjectSearcher("root\\CIMV2","SELECT Name,DriverVersion FROM Win32_VideoController",options))
                using(var objects=search.Get()) {
                    var entries=new List<string>();
                    foreach(ManagementObject item in objects) using(item) { entries.Add(Clean(Convert.ToString(item["Name"]))+" / driver "+Clean(Convert.ToString(item["DriverVersion"]))); if(entries.Count==4)break; }
                    return entries.Count==0?"Unavailable":String.Join("; ",entries);
                }
            } catch { return "Unavailable (graphics query did not complete)"; }
#endif
        }
        public static string RecentErrors(string root)
        {
            try {
                string path=Core.PathIn(root,".launcher/launcher.log");
                using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                    stream.Position=Math.Max(0,stream.Length-32768);
                    byte[] bytes=new byte[Math.Min(32768,(int)(stream.Length-stream.Position))];
                    int count=stream.Read(bytes,0,bytes.Length);
                    // Allowlist structured exception types only. Never export arbitrary log messages.
                    var errors=new List<string>();
                    foreach(string line in Encoding.UTF8.GetString(bytes,0,count).Split('\n')) {
                        var match=Regex.Match(line,@"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z) (System\.[A-Za-z0-9_.]{1,100}Exception)\b");
                        if(match.Success) errors.Add(match.Groups[1].Value+" "+match.Groups[2].Value);
                    }
                    return errors.Count==0?"No structured exceptions in the recent log tail.":String.Join("\n",errors.Skip(Math.Max(0,errors.Count-8)));
                }
            } catch { return "Recent error summary unavailable."; }
        }
        public static string Report(string root,VerificationResult game,VerificationResult launcher,string graphics)
        {
            var text=new StringBuilder();
            text.AppendLine("TimeSplitters launcher diagnostics");
            text.AppendLine("Generated (UTC): "+DateTime.UtcNow.ToString("u"));
            text.AppendLine("Running launcher: "+LauncherVersion.Value);
            text.AppendLine("Installed launcher: "+Empty(Version(root,true)));
            text.AppendLine("Installed game: "+Empty(Version(root,false)));
            text.AppendLine("Host: "+Environment.OSVersion.Version+" / "+(Environment.Is64BitOperatingSystem?"64-bit":"32-bit"));
            text.AppendLine(".NET CLR: "+Environment.Version+" / process "+(Environment.Is64BitProcess?"64-bit":"32-bit"));
            text.AppendLine("Graphics: "+graphics);
            var resources=Assembly.GetExecutingAssembly().GetManifestResourceNames();
            text.AppendLine("Embedded artwork: banner "+(resources.Contains("launcher-banner.png")?"present":"fallback")+", logo "+(resources.Contains("launcher-logo.png")?"present":"fallback"));
            try { text.AppendLine("Installation volume free: "+(new DriveInfo(Path.GetPathRoot(root)).AvailableFreeSpace/1073741824.0).ToString("0.0",CultureInfo.InvariantCulture)+" GiB"); } catch { text.AppendLine("Free space: unavailable"); }
            try {
                var settings=GameSettings.Load(root,null);
                text.AppendLine("Saved graphics: "+settings.Width+"x"+settings.Height+", render scale "+settings.Scale+", vertical FOV "+settings.VerticalFov+", fullscreen "+settings.Fullscreen+", frame cap "+(new[]{"adaptive","30","60","0"}.Contains(settings.FrameCap)?settings.FrameCap:"unknown"));
            } catch { text.AppendLine("Saved graphics: unreadable or unsupported; files retained."); }
            text.AppendLine(); text.AppendLine("Last game verification:\n"+(game==null?"Not checked this session.":game.ToString()));
            text.AppendLine(); text.AppendLine("Last launcher verification:\n"+(launcher==null?"Not checked this session.":launcher.ToString()));
            text.AppendLine(); text.AppendLine(CrashReports.CaptureHealth(root));
            text.AppendLine(); text.AppendLine("Latest game incident:\n"+CrashReports.Summary(root));
            text.AppendLine(); text.AppendLine("Recent launcher errors (types only):\n"+RecentErrors(root));
            text.AppendLine(); text.AppendLine("Report excludes raw logs, personal paths, file contents, saves and ISO information. Nothing is uploaded automatically.");
            return text.ToString();
        }
        static string Empty(string value) { return value.Length==0?"Unknown / not installed":value; }
    }
}
