using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading;
using TimeSplitters.Launcher;

public static class LauncherSessionChecks
{
    static int passed;
    static void Require(bool value,string message) {if(!value)throw new Exception(message);}
    static string Executable {get {return typeof(LauncherSessionChecks).Assembly.Location;}}
    static Process Child(string arguments) {return Process.Start(new ProcessStartInfo(Executable,arguments){UseShellExecute=false,CreateNoWindow=true});}
    static string Marker(string root) {return Core.PathIn(root,".launcher/game-session.json");}
    static void Fixture(string root)
    {
        Directory.CreateDirectory(root);Directory.CreateDirectory(Path.Combine(root,"Logs"));
        File.WriteAllText(Path.Combine(root,"BUILD.json"),"{\"schema\":1,\"version\":\"session-fixture\"}");
        File.WriteAllText(Path.Combine(root,"Logs/20261019-120000-000.err"),new string('x',70000));
    }
    static string Begin(string root) {return GameSessions.Begin(root,"20261019-120000-000");}
    static void Test(string parent,string name,Action<string> action)
    {
        string root=Path.Combine(parent,Guid.NewGuid().ToString("N"));Fixture(root);action(root);
        ++passed;Console.WriteLine("PASS session "+name);
    }
    static void Abandon(string root)
    {
        var data=Json.Object(File.ReadAllText(Marker(root)));
        // Same PID with a different start time models PID reuse without relying
        // on the operating system to allocate any particular PID.
        data["launcher_started_ticks"]="1";File.WriteAllText(Marker(root),Json.Encode(data));
    }
    public static int Owner(string root)
    {
        string session=Begin(root);
        using(var game=Child("--session-sleeper")) {
            GameSessions.Started(root,session,game);
            Core.Write(Core.PathIn(root,".launcher/session-child-ready.json"),Json.Encode(new {pid=game.Id}));
            Thread.Sleep(30000);
        }
        return 0;
    }
    public static int Run()
    {
        string parent=Path.Combine(Path.GetDirectoryName(Executable),"session-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(parent);
        try {
            Test(parent,"observed zero exit stays clean",root=> {
                string id=Begin(root);
                using(var game=Child("--session-exit 0")) {game.WaitForExit();Require(GameSessions.Complete(root,id,game)==null,"Normal exit must not create a report.");}
                Require(GameSessions.Recover(root)==null && CrashReports.Latest(root)==null,"Clean record must survive reopening without a warning.");
            });
            Test(parent,"failed process launch is cancelled",root=> {
                string id=Begin(root);bool failed=false;
                try {Process.Start(new ProcessStartInfo(Path.Combine(root,"missing.exe")){UseShellExecute=false});}catch(System.ComponentModel.Win32Exception){failed=true;GameSessions.CancelLaunch(root,id);}
                Require(failed && GameSessions.Recover(root)==null && CrashReports.Latest(root)==null,"Failed launch must not be called a game crash.");
            });
            Test(parent,"observed nonzero exit creates bounded exportable report",root=> {
                string id=Begin(root), name;
                using(var game=Child("--session-exit 23")) {game.WaitForExit();name=GameSessions.Complete(root,id,game);}
                Require(name==CrashReports.Latest(root) && CrashReports.IsUnexpectedExit(root,name),"Nonzero exit must be discoverable without a helper.");
                string summary=CrashReports.Summary(root);
                Require(summary.Contains("Exit code: 23") && summary.Contains("not captured") && !summary.Contains("Exception:"),"Summary must not invent an exception.");
                Require(new FileInfo(Path.Combine(CrashReports.Folder(root,name),"game-log-tail.txt")).Length==65536,"Only the bounded tail of this launch's log is copied.");
                string zip=Path.Combine(root,"report.zip");CrashReports.Export(root,name,zip);
                using(var archive=ZipFile.OpenRead(zip))Require(archive.Entries.Count==3 && archive.GetEntry("report.json")!=null && archive.GetEntry("crash.dmp")==null,"Fallback export contains a summary and log, never a fabricated dump.");
                Require(GameSessions.Recover(root)==null,"Completed unexpected exit must not create repeated notifications.");
            });
            Test(parent,"live launcher before process start is not abandoned",root=> {
                Begin(root);Require(GameSessions.Recover(root)==null && CrashReports.Latest(root)==null,"Prelaunch record is still owned by a live launcher.");
            });
            Test(parent,"stale PID identity produces an uncertain report once",root=> {
                Begin(root);Abandon(root);
                string report=GameSessions.Recover(root);
                Require(report!=null && CrashReports.Summary(root).Contains("cause is unknown"),"PID reuse must not suppress recovery or assert a crash cause.");
                var data=Json.Object(File.ReadAllText(Path.Combine(CrashReports.Folder(root,report),"report.json")));
                Require(data["exit_code"]==null && Json.String(data,"reason")=="unobserved_exit","Unknown exit code stays unknown.");
                Require(GameSessions.Recover(root)==null,"Repeated startup must not duplicate the incident.");
                string next=Begin(root);GameSessions.CancelLaunch(root,next);Require(GameSessions.Recover(root)==null,"A later failed launch cannot revive an older session.");
            });
            Test(parent,"launcher termination defers while exact game remains alive",root=> {
                Process game=null;
                using(var owner=Child("--session-owner \""+root+"\"")) {
                    try {
                        string ready=Core.PathIn(root,".launcher/session-child-ready.json");
                        var timeout=Stopwatch.StartNew();while(!File.Exists(ready) && !owner.HasExited && timeout.ElapsedMilliseconds<10000)Thread.Sleep(20);
                        Require(File.Exists(ready),"Session owner must persist a real child process identity.");
                        game=Process.GetProcessById(Convert.ToInt32(Json.Object(File.ReadAllText(ready))["pid"]));
                        owner.Kill();owner.WaitForExit();
                        Require(GameSessions.Recover(root)==null && !game.HasExited,"Reopening while an orphan game runs must not flag it.");
                        game.Kill();game.WaitForExit();
                        string report=GameSessions.Recover(root);
                        Require(report!=null && CrashReports.IsUnexpectedExit(root,report),"A killed game after launcher termination must be flagged on recovery.");
                        Require(GameSessions.Recover(root)==null,"Restart recovery must be idempotent.");
                    } finally {
                        if(!owner.HasExited){owner.Kill();owner.WaitForExit();}
                        if(game!=null){if(!game.HasExited){game.Kill();game.WaitForExit();}game.Dispose();}
                    }
                }
            });
            Test(parent,"native crash report takes precedence",root=> {
                string id=Begin(root);
                using(var game=Child("--session-exit 7")) {
                    game.WaitForExit();
                    string name="crash-20261019-130000000-0000001234-000001", folder=CrashReports.Folder(root,name);Directory.CreateDirectory(folder);
                    File.WriteAllText(Path.Combine(folder,"capture.lock"),"");
                    File.WriteAllText(Path.Combine(folder,"report.json"),Json.Encode(new {schema=1,kind="unhandled_exception",pid=game.Id,dump_saved=true}));
                    Require(GameSessions.Complete(root,id,game)==name && Directory.GetDirectories(Path.Combine(root,"Logs/Crashes")).Length==1,"An existing native fatal report must not be duplicated.");
                }
            });
            Test(parent,"a prior hang cannot hide a subsequent exit",root=> {
                string id=Begin(root);
                using(var game=Child("--session-exit 9")) {
                    game.WaitForExit();
                    string name="crash-20000101-130000000-0000001234-000001", folder=CrashReports.Folder(root,name);Directory.CreateDirectory(folder);
                    File.WriteAllText(Path.Combine(folder,"capture.lock"),"");
                    File.WriteAllText(Path.Combine(folder,"report.json"),Json.Encode(new {schema=1,kind="suspected_hang",pid=game.Id,dump_saved=true}));
                    string report=GameSessions.Complete(root,id,game);
                    Require(report!=name && CrashReports.IsUnexpectedExit(root,CrashReports.Latest(root)),"An exit after a hang needs its own current outcome.");
                }
            });
            Test(parent,"startup recovery opens Diagnostics without a dump",root=> {
                Begin(root);Abandon(root);Exception failure=null;
                var thread=new Thread(()=> {
                    try {
                        var ui=new LauncherWindow(root,true);
                        try {
                            ui.RecoverGameSession();
                            Require(((System.Windows.Controls.ContentControl)ui.Window.FindName("MaintenanceHost")).Visibility==System.Windows.Visibility.Visible,"Recovered session must open Diagnostics.");
                            Require(((System.Windows.Controls.TextBlock)ui.Window.FindName("StatusText")).Text.Contains("No crash dump was captured"),"Startup status must explain the missing dump.");
                            Require(InstallationMaintenance.Report(root,null,null,"not queried").Contains("Type: Unexpected exit"),"Copyable Diagnostics must contain the fallback incident.");
                        } finally {ui.Window.Close();}
                    } catch(Exception ex){failure=ex;}
                });
                thread.SetApartmentState(ApartmentState.STA);thread.Start();Require(thread.Join(15000),"Session UI recovery timed out.");if(failure!=null)throw failure;
            });
            Test(parent,"failed capture retries the same incident",root=> {
                Begin(root);Abandon(root);File.WriteAllText(Path.Combine(root,"Logs/Crashes"),"blocked");
                bool failed=false;try{GameSessions.Recover(root);}catch(IOException){failed=true;}
                string record=File.ReadAllText(Marker(root));string name=Json.String(Json.Object(record),"report_id");
                Require(failed && name.Length>0 && Json.String(Json.Object(record),"state")=="unexpected","Failed report storage must retain the pending outcome.");
                File.Delete(Path.Combine(root,"Logs/Crashes"));
                Require(GameSessions.Recover(root)==name && GameSessions.Recover(root)==null,"Storage recovery must retry without creating duplicate reports.");
            });
            Test(parent,"report writes fit supported long installation paths",root=> {
                string nested=Path.Combine(root,new string('p',180-root.Length-1));Fixture(nested);
                string id=Begin(nested);
                using(var game=Child("--session-exit 23")) {
                    game.WaitForExit();string report=GameSessions.Complete(nested,id,game);
                    Require(CrashReports.Latest(nested)==report,"Atomic report writes must not exceed Windows path limits when final report paths fit.");
                    string exported=Path.Combine(nested,new string('e',60)+".zip");
                    CrashReports.Export(nested,report,exported);
                    Require(File.Exists(exported),"Export must also support a valid long destination without appending a large temporary suffix.");
                }
            });
            Test(parent,"malformed and oversized records are retained",root=> {
                Directory.CreateDirectory(Path.GetDirectoryName(Marker(root)));
                foreach(string text in new[]{"invalid JSON",new string('x',8193)}) {
                    File.WriteAllText(Marker(root),text);bool failed=false;
                    try{GameSessions.Recover(root);}catch{failed=true;}
                    Require(failed && File.ReadAllText(Marker(root))==text && CrashReports.Latest(root)==null,"Unreadable state must not be overwritten or treated as a crash.");
                }
            });
            Test(parent,"fallback retention preserves unknown files",root=> {
                for(int i=0;i<7;++i) {
                    Begin(root);Abandon(root);GameSessions.Recover(root);Thread.Sleep(3);
                    if(i==0)File.WriteAllText(Path.Combine(CrashReports.Folder(root,CrashReports.Latest(root)),"keep.txt"),"unrelated");
                }
                Require(Directory.GetDirectories(Path.Combine(root,"Logs/Crashes")).Length==6,"Five newest incidents plus unknown protected files must remain.");
            });
            Test(parent,"late completion cannot erase a newer session",root=> {
                string old=Begin(root);GameSessions.CancelLaunch(root,old);string current=Begin(root);
                using(var game=Child("--session-exit 0")){game.WaitForExit();GameSessions.Complete(root,old,game);}
                Require(Json.String(Json.Object(File.ReadAllText(Marker(root))),"session_id")==current,"Session identity prevents stale completion.");
            });
            Console.WriteLine(passed+" unfinished-session checks passed.");return 0;
        } finally {
            string target=Path.GetFullPath(parent), basePath=Path.GetFullPath(Path.GetDirectoryName(Executable))+Path.DirectorySeparatorChar;
            Require(target.StartsWith(basePath,StringComparison.OrdinalIgnoreCase) && Path.GetFileName(target).StartsWith("session-tests-") && (File.GetAttributes(target)&FileAttributes.ReparsePoint)==0,"Unexpected session test root.");
            Directory.Delete(target,true);
        }
    }
}
