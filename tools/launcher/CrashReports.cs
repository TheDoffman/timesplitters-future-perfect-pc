using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.RegularExpressions;

namespace TimeSplitters.Launcher
{
    public static class CrashReports
    {
        static readonly string[] Names = { "report.txt", "report.json", "crash.dmp", "game-log-tail.txt", "native-stack.txt", "breadcrumbs.txt" };
        // Fixed summaries only: never include paths, arbitrary errors or raw metadata.
        public static string CaptureHealth(string root)
        {
            const string prefix="Capture health: ";
            try {
                string game=Core.PathIn(root,LauncherPlatform.GamePath(root));
                if(!File.Exists(game))return prefix+"Game not installed.";
                string area=Core.PathIn(root,"Logs/Crashes");
                try {
                    Directory.CreateDirectory(area);
                    string probe=Core.PathIn(root,"Logs/Crashes/"+Path.GetRandomFileName());
                    using(var output=new FileStream(probe,FileMode.CreateNew,FileAccess.Write,FileShare.None,1,FileOptions.DeleteOnClose)) {
                        output.WriteByte(0);output.Flush(true);
                    }
                } catch {return prefix+"Report storage unavailable. Capture may fail; check folder permissions and free space.";}
#if !MACOS
                if(!File.Exists(Path.Combine(Path.GetDirectoryName(game),"tsfp_crash_helper.exe")))
                    return prefix+"Crash helper missing. Native crash and freeze capture unavailable; session summaries may still work.";
#endif
                var session=GameSessions.CaptureSession(root);
                if(session==null)return prefix+"Report storage writable; no tracked game running. Native support is verified when a game starts.";
                string path=Core.PathIn(root,"Logs/Crashes/capture-health.json");
                if(!File.Exists(path))return prefix+"Unverified: this game has not confirmed capture support. Older builds may not support health reporting.";
                if(new FileInfo(path).Length>2048)return prefix+"Unverified: health record is invalid.";
                var health=Json.Object(File.ReadAllText(path));
                if(Json.String(health,"schema")!="1" || Json.String(health,"session_id")!=Json.String(session,"session_id") ||
                    Json.String(health,"pid")!=Json.String(session,"game_pid"))
                    return prefix+"Unverified: no matching health record for this game session.";
                DateTime written=File.GetLastWriteTimeUtc(path),now=DateTime.UtcNow;
                if(written>now.AddSeconds(2) || now-written>TimeSpan.FromSeconds(15))
                    return prefix+"Degraded: capture helper heartbeat is stale. New reports may not be captured.";
                int pid;long gameStarted;
                if(!Int32.TryParse(Json.String(health,"helper_pid"),out pid) || pid<=0 ||
                    !Int64.TryParse(Json.String(session,"game_started_ticks"),out gameStarted))
                    return prefix+"Unverified: health record is invalid.";
                try {
                    using(var helper=System.Diagnostics.Process.GetProcessById(pid)) {
                        DateTime started=helper.StartTime.ToUniversalTime();
                        if(helper.HasExited || started.Ticks<gameStarted || started>written)
                            return prefix+"Degraded: the recorded capture helper is no longer running.";
                    }
                } catch {return prefix+"Degraded: capture helper identity could not be confirmed.";}
                object armed;
                if(!health.TryGetValue("armed",out armed) || !(armed is bool))return prefix+"Unverified: health record is invalid.";
                return prefix+"Helper responding; report storage writable. "+((bool)armed?"Freeze monitoring armed (normal focus/modal grace applies).":"Waiting for guest startup or game shutting down.");
            } catch {return prefix+"Unverified: session or health information unavailable. Existing reports are retained.";}
        }
        public static string Latest(string root)
        {
            try {
            string area = Core.PathIn(root, "Logs/Crashes");
            if (!Directory.Exists(area)) return null;
            foreach (string directory in Directory.EnumerateDirectories(area).OrderByDescending(Path.GetFileName).Take(128)) {
                string name = Path.GetFileName(directory);
                if (!Regex.IsMatch(name, @"^crash-\d{8}-\d{9}-\d{10}-\d{6}$")) continue;
                try {
                    string report = Core.PathIn(root, "Logs/Crashes/" + name + "/report.json");
                    if (!File.Exists(report) || new FileInfo(report).Length > 8192) continue;
                    using (var locked = new FileStream(Core.PathIn(root,"Logs/Crashes/"+name+"/capture.lock"), FileMode.Open, FileAccess.Read, FileShare.None)) { }
                    var metadata = Json.Object(File.ReadAllText(report));
                    string kind=Json.String(metadata,"kind");
                    if (Convert.ToInt32(metadata["schema"]) == 1 && (kind == "unhandled_exception" || kind == "suspected_hang" || kind == "unexpected_exit")) return name;
                } catch { /* A report may still be writing or inaccessible. */ }
            }
            } catch { /* Unavailable diagnostics must not prevent starting the game. */ }
            return null;
        }
        public static string Folder(string root, string name)
        {
            Core.Require(name != null && Regex.IsMatch(name,@"^crash-\d{8}-\d{9}-\d{10}-\d{6}$"), "Invalid crash report.");
            return Core.PathIn(root,"Logs/Crashes/"+name);
        }
        public static string Summary(string root)
        {
            try {
                string name = Latest(root);
                if (name == null) return "No completed crash report found.";
                var metadata = Json.Object(File.ReadAllText(Path.Combine(Folder(root,name),"report.json")));
                if(Json.String(metadata,"kind")=="unexpected_exit") {
                    string reason=Json.String(metadata,"reason");long exit;
                    string detail=reason=="nonzero_exit" && Int64.TryParse(Json.String(metadata,"exit_code"),out exit) && exit>=Int32.MinValue && exit<=Int32.MaxValue?
                        "Exit code: "+exit.ToString(System.Globalization.CultureInfo.InvariantCulture):"A normal exit was not recorded; the cause is unknown.";
                    return name+"\nType: Unexpected exit\n"+detail+"\nMinidump: not captured for this session\nAn interrupted launcher, forced closure or reboot can also leave an unfinished session.";
                }
                string code = Json.String(metadata,"exception_code"), hash = Json.String(metadata,"executable_sha256");
                if(Json.String(metadata,"platform")=="macos-arm64")return name+"\nType: "+(Json.String(metadata,"kind")=="suspected_hang"?"Suspected hang (the game was left running)":"Fatal native signal "+Json.String(metadata,"signal"))+"\nNative context: "+(metadata.ContainsKey("native_capture_saved")&&Object.Equals(metadata["native_capture_saved"],true)?"saved":"unavailable")+"\nExecutable SHA-256: "+(Regex.IsMatch(hash,@"^[0-9a-f]{64}$")?hash:"unavailable");
                if (!Regex.IsMatch(code,@"^0x[0-9A-Fa-f]{8}$")) code="unknown";
                if (!Regex.IsMatch(hash,@"^[0-9a-f]{64}$")) hash="unavailable";
                return name+(Json.String(metadata,"kind")=="suspected_hang"?"\nType: Suspected hang (the game was left running)":"\nException: "+code)+"\nMinidump: "+(metadata.ContainsKey("dump_saved") && Object.Equals(metadata["dump_saved"],true)?"saved":"unavailable; summary retained")+"\nExecutable SHA-256: "+hash;
            } catch { return "Crash reports unavailable; files retained."; }
        }
        public static bool IsHang(string root,string name)
        {
            if(name==null)return false;
            try {
                string path=Core.PathIn(root,"Logs/Crashes/"+Path.GetFileName(Folder(root,name))+"/report.json");
                if(new FileInfo(path).Length>8192)return false;
                return Json.String(Json.Object(File.ReadAllText(path)),"kind")=="suspected_hang";
            } catch {return false;}
        }
        public static bool IsUnexpectedExit(string root,string name)
        {
            try {
                string path=Core.PathIn(root,"Logs/Crashes/"+Path.GetFileName(Folder(root,name))+"/report.json");
                return new FileInfo(path).Length<=8192 && Json.String(Json.Object(File.ReadAllText(path)),"kind")=="unexpected_exit";
            } catch {return false;}
        }
        internal static void Trim(string root,string keep)
        {
            try {
                var directories=Directory.EnumerateDirectories(Core.PathIn(root,"Logs/Crashes")).OrderByDescending(Path.GetFileName).ToArray();
                int retained=0;
                foreach(string directory in directories) {
                    string name=Path.GetFileName(directory);
                    if(!Regex.IsMatch(name,@"^crash-\d{8}-\d{9}-\d{10}-\d{6}$"))continue;
                    if(++retained<=5 || name==keep)continue;
                    try {
                        string folder=Folder(root,name);
                        if(Directory.EnumerateFileSystemEntries(folder).Any(path=>!Names.Contains(Path.GetFileName(path)) && Path.GetFileName(path)!="capture.lock"))continue;
                        string lockPath=Core.PathIn(root,"Logs/Crashes/"+name+"/capture.lock");
                        using(var locked=new FileStream(lockPath,FileMode.Open,FileAccess.ReadWrite,FileShare.None))
                            foreach(string file in Names)File.Delete(Core.PathIn(root,"Logs/Crashes/"+name+"/"+file));
                        File.Delete(lockPath);Directory.Delete(folder);
                    } catch { /* Active, linked or unknown files are preserved. */ }
                }
            } catch { /* Retention must not hide the new incident. */ }
        }
        public static void Export(string root, string name, string destination)
        {
            string folder = Folder(root,name);
            Core.Require(IsHang(root,name) || !Core.GameRunning(), "Close the game before exporting a crash report.");
            Core.Require(Latest(root) == name, "Refresh diagnostics before exporting this report.");
            string temporary = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(destination)),Path.GetRandomFileName());
            // Only this report's known files; never sweep the game, settings or saves.
            try {
            using (var locked = new FileStream(Path.Combine(folder,"capture.lock"),FileMode.Open,FileAccess.Read,FileShare.Read))
            using (var output = new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None))
            using (var zip = new ZipArchive(output,ZipArchiveMode.Create)) {
                foreach (string file in Names) {
                    string path=Core.PathIn(root,"Logs/Crashes/"+name+"/"+file);
                    if (!File.Exists(path)) continue;
                    long limit=file=="crash.dmp"?16*1024*1024:file=="native-stack.txt"?262144:file=="game-log-tail.txt"?65536:8192;
                    using (var input=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)) {
                        Core.Require(input.Length<=limit,"Crash report file exceeds its size limit.");
                        using (var entry=zip.CreateEntry(file,CompressionLevel.Fastest).Open()) input.CopyTo(entry);
                    }
                }
            }
            File.Move(temporary,destination);
            } finally { if(File.Exists(temporary))File.Delete(temporary); }
        }
    }
}
