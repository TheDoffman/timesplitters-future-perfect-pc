using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TimeSplitters.Launcher
{
    // One installation's current/last session. This records observation, not a
    // diagnosis: an unobserved exit may be a reboot or a launcher failure.
    public static class GameSessions
    {
        const string Record = ".launcher/game-session.json";
        static string PathFor(string root) { return Core.PathIn(root, Record); }
        static Dictionary<string,object> Read(string root)
        {
            string path=PathFor(root);
            if(!File.Exists(path))return null;
            Core.Require(new FileInfo(path).Length<=8192,"Oversized game session record; file retained.");
            var data=Json.Object(File.ReadAllText(path));
            Core.Require(Convert.ToInt32(data["schema"])==1 && Regex.IsMatch(Json.String(data,"session_id"),@"^[a-f0-9]{32}$"),"Invalid game session record; file retained.");
            Core.Require(new[]{"active","clean","unexpected","reported","launch_failed"}.Contains(Json.String(data,"state")),"Invalid game session state; file retained.");
            DateTimeOffset started;
            Core.Require(DateTimeOffset.TryParseExact(Json.String(data,"started_utc"),"o",CultureInfo.InvariantCulture,DateTimeStyles.None,out started) &&
                Regex.IsMatch(Json.String(data,"log_tag"),@"^\d{8}-\d{6}-\d{3}$") &&
                Number(data,"launcher_pid")>0 && Number(data,"launcher_pid")<=Int32.MaxValue && Number(data,"launcher_started_ticks")>0,
                "Invalid game session identity; file retained.");
            if(Active(data) && data.ContainsKey("game_pid"))
                Core.Require(Number(data,"game_pid")>0 && Number(data,"game_pid")<=Int32.MaxValue && Number(data,"game_started_ticks")>0,"Invalid game process identity; file retained.");
            return data;
        }
        static void Save(string root,Dictionary<string,object> data)
        {
            Write(root,Record,Json.Encode(data));
        }
        static void Write(string root,string relative,string text)
        {
            // Keep the temporary basename short: appending a GUID to a report
            // filename can exceed MAX_PATH even when its final path fits.
            string path=Core.PathIn(root,relative), temporary=Core.PathIn(root,relative.Substring(0,relative.LastIndexOf('/')+1)+Path.GetRandomFileName());
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            try {
                using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None,4096,FileOptions.WriteThrough)) {
                    byte[] bytes=new UTF8Encoding(false).GetBytes(text);output.Write(bytes,0,bytes.Length);output.Flush(true);
                }
                if(File.Exists(path))Core.AtomicReplace(temporary,path);else File.Move(temporary,path);
            } finally {if(File.Exists(temporary))File.Delete(temporary);}
        }
        internal static Dictionary<string,object> CaptureSession(string root)
        {
            var data=Read(root);
            if(!Active(data) || Number(data,"game_pid")<=0)return null;
            try {
                using(var game=Process.GetProcessById((int)Number(data,"game_pid")))
                    return !game.HasExited && game.StartTime.ToUniversalTime().Ticks==Number(data,"game_started_ticks")?data:null;
            } catch(ArgumentException){return null;} catch(InvalidOperationException){return null;}
            // Access denied is unknown, not a positively verified live session.
        }
        static bool Active(Dictionary<string,object> data) { return data!=null && Json.String(data,"state")=="active"; }
        static long Number(Dictionary<string,object> data,string key)
        {
            long value;return Int64.TryParse(Json.String(data,key),NumberStyles.Integer,CultureInfo.InvariantCulture,out value)?value:0;
        }
        static bool MayBeAlive(Dictionary<string,object> data,string prefix)
        {
            long id=Number(data,prefix+"_pid"), ticks=Number(data,prefix+"_started_ticks");
            if(id<=0 || id>Int32.MaxValue || ticks<=0)return false;
            try {using(var process=Process.GetProcessById((int)id))return !process.HasExited && process.StartTime.ToUniversalTime().Ticks==ticks;}
            catch(ArgumentException) {return false;} // PID no longer exists.
            catch(InvalidOperationException) {return false;}
            catch(System.ComponentModel.Win32Exception) {return true;} // Inaccessible is not proof of exit.
        }
        static void Identity(Dictionary<string,object> data,string prefix,Process process)
        {
            data[prefix+"_pid"]=process.Id;
            data[prefix+"_started_ticks"]=process.StartTime.ToUniversalTime().Ticks.ToString(CultureInfo.InvariantCulture);
        }
        public static string Begin(string root,string logTag)
        {
            var previous=Read(root);
            Core.Require(!Active(previous) && (previous==null || Json.String(previous,"state")!="unexpected"),"Previous game session still needs recovery.");
            Core.Require(Regex.IsMatch(logTag,@"^\d{8}-\d{6}-\d{3}$"),"Invalid session log ID.");
            var data=new Dictionary<string,object>();
            data["schema"]=1;data["session_id"]=Guid.NewGuid().ToString("N");data["state"]="active";
            data["started_utc"]=DateTime.UtcNow.ToString("o",CultureInfo.InvariantCulture);
            data["log_tag"]=logTag;data["previous_report"]=CrashReports.Latest(root);
            data["build_manifest_sha256"]=Core.GameBuildHash(root);
            using(var owner=Process.GetCurrentProcess()) {
                Identity(data,"launcher",owner);
            }
            Save(root,data);return Json.String(data,"session_id");
        }
        public static void Started(string root,string id,Process game)
        {
            var data=Read(root);if(data==null || Json.String(data,"session_id")!=id)return;
            Identity(data,"game",game);Save(root,data);
        }
        public static void CancelLaunch(string root,string id)
        {
            var data=Read(root);if(data==null || Json.String(data,"session_id")!=id)return;
            data["state"]="launch_failed";Save(root,data);
        }
        public static string Complete(string root,string id,Process game)
        {
            var data=Read(root);if(data==null || Json.String(data,"session_id")!=id)return null;
            Core.Require(game.HasExited,"Cannot finish a running game session.");
            data["game_pid"]=game.Id;data["exit_code"]=game.ExitCode;
            data["state"]=game.ExitCode==0?"clean":"unexpected";
            data["reason"]=game.ExitCode==0?"normal_exit":"nonzero_exit";Save(root,data);
            return game.ExitCode==0?null:Report(root,data);
        }
        public static string Recover(string root)
        {
            var data=Read(root);if(data==null)return null;
            if(Active(data)) {
                if(MayBeAlive(data,"game"))return null;
                if(Number(data,"game_pid")==0 && (MayBeAlive(data,"launcher") || Core.GameRunning()))return null;
                data["state"]="unexpected";data["reason"]="unobserved_exit";Save(root,data);
            }
            return Json.String(data,"state")=="unexpected"?Report(root,data):null;
        }
        static string Report(string root,Dictionary<string,object> data)
        {
            // Prefer the native crash report for this process. A previous hang
            // does not explain a later unobserved/nonzero exit.
            string latest=CrashReports.Latest(root);
            if(latest!=null && latest!=Json.String(data,"previous_report")) {
                var native=Json.Object(File.ReadAllText(Core.PathIn(root,"Logs/Crashes/"+latest+"/report.json")));
                if(Json.String(native,"kind")=="unhandled_exception" && Number(native,"pid")==Number(data,"game_pid") && Number(data,"game_pid")>0) {
                    data["state"]="reported";Save(root,data);return latest;
                }
            }
            if(Json.String(data,"report_id").Length==0) {
                using(var owner=Process.GetCurrentProcess())
                    data["report_id"]="crash-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmssfff",CultureInfo.InvariantCulture)+"-"+owner.Id.ToString("D10",CultureInfo.InvariantCulture)+"-"+(100000+(BitConverter.ToUInt32(Guid.NewGuid().ToByteArray(),0)%900000)).ToString(CultureInfo.InvariantCulture);
                Save(root,data); // Stable ID makes interrupted report creation retryable.
            }
            string name=Json.String(data,"report_id"), folder=CrashReports.Folder(root,name);
            Directory.CreateDirectory(folder);
            using(var locked=new FileStream(Core.PathIn(root,"Logs/Crashes/"+name+"/capture.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
                string reason=Json.String(data,"reason");
                Core.Require(reason=="nonzero_exit" || reason=="unobserved_exit","Invalid session outcome.");
                string explanation=reason=="nonzero_exit"?"The game exited with a nonzero exit code.":"The previous game session ended without the launcher recording a normal exit.";
                var report=new Dictionary<string,object> {
                    {"schema",1},{"kind","unexpected_exit"},{"dump_saved",false},{"exception_code",null},
                    {"session_id",Json.String(data,"session_id")},{"started_utc",Json.String(data,"started_utc")},
                    {"detected_utc",DateTime.UtcNow.ToString("o",CultureInfo.InvariantCulture)},
                    {"pid",Number(data,"game_pid")},{"reason",reason},
                    {"exit_code",reason=="nonzero_exit"?(object)Number(data,"exit_code"):null},
                    {"build_manifest_sha256",ValidHash(Json.String(data,"build_manifest_sha256"))}
                };
                string text="TimeSplitters unexpected game exit\n"+explanation+"\nNo crash dump was captured for this session.\n"+
                    "This record does not establish a crash cause. Forced closure, a reboot, or interrupted launcher monitoring can leave an unfinished session.\n"+
                    (reason=="nonzero_exit"?"Exit code: "+Number(data,"exit_code").ToString(CultureInfo.InvariantCulture)+"\n":"")+
                    "Nothing was uploaded. Review the report and any log excerpt before sharing.\n";
                Write(root,"Logs/Crashes/"+name+"/report.txt",text);
                CopyLog(root,name,Json.String(data,"log_tag"));
                Write(root,"Logs/Crashes/"+name+"/report.json",Json.Encode(report));
            }
            data["state"]="reported";Save(root,data);
            CrashReports.Trim(root,name);
            return name;
        }
        static string ValidHash(string hash) {return Regex.IsMatch(hash,@"^[a-f0-9]{64}$")?hash:null;}
        static void CopyLog(string root,string name,string tag)
        {
            // Only this launch's stderr; never a path supplied by the record.
            if(!Regex.IsMatch(tag,@"^\d{8}-\d{6}-\d{3}$"))return;
            try {
                using(var input=new FileStream(Core.PathIn(root,LauncherPlatform.SessionLog(tag)),FileMode.Open,FileAccess.Read,FileShare.ReadWrite)) {
                    input.Position=Math.Max(0,input.Length-65536);
                    using(var output=new FileStream(Core.PathIn(root,"Logs/Crashes/"+name+"/game-log-tail.txt"),FileMode.Create,FileAccess.Write,FileShare.None)) {
                        byte[] bytes=new byte[65536];int count=input.Read(bytes,0,bytes.Length);output.Write(bytes,0,count);
                    }
                }
            } catch(IOException) {} catch(UnauthorizedAccessException) {}
        }
    }
}
