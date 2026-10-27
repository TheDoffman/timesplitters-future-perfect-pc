using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using TimeSplitters.Launcher;

public static class LauncherCrashChecks
{
    static void Require(bool value,string message) { if(!value)throw new Exception(message); }
    public static int Integration(string executable,bool hang=false,string fallback=null)
    {
        Exception failure=null;
        var thread=new System.Threading.Thread(()=>{try{IntegrationSTA(executable,hang,fallback);}catch(Exception ex){failure=ex;}});
        thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();
        if(!thread.Join(45000))throw new Exception("Launcher crash integration timed out.");
        if(failure!=null)throw failure;
        return 0;
    }
    static void IntegrationSTA(string executable,bool hang,string fallback)
    {
        string parent=Path.GetDirectoryName(typeof(LauncherCrashChecks).Assembly.Location);
        string root=Path.Combine(parent,"crash-report-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root,"Bin"));
        string oldPath=Environment.GetEnvironmentVariable("PATH");
        try {
            string bin=Path.GetDirectoryName(Path.GetFullPath(executable));
            File.Copy(executable,Path.Combine(root,"Bin/futureperfect_recomp.exe"));
            if(fallback!="missing-helper")File.Copy(Path.Combine(bin,"tsfp_crash_helper.exe"),Path.Combine(root,"Bin/tsfp_crash_helper.exe"));
            string repository=Directory.GetParent(Directory.GetParent(Directory.GetParent(bin).FullName).FullName).FullName;
            Environment.SetEnvironmentVariable("PATH",Path.Combine(repository,"external/ts3FPrecomp/third_party/ffmpeg/bin")+";"+oldPath);
            GameSettings.Load(root,null).Save();
            var ui=new LauncherWindow(root,true);
            // Play owns this busy flag in production. Keep it set so the test
            // verifies hang diagnostics/export remain usable during that state.
            if(hang)typeof(LauncherWindow).GetField("busy",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).SetValue(ui,true);
            var dispatcher=System.Windows.Threading.Dispatcher.CurrentDispatcher;
            var previous=System.Threading.SynchronizationContext.Current;
            var timer=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromMilliseconds(100)};
            var clock=System.Diagnostics.Stopwatch.StartNew();
            var frame=new System.Windows.Threading.DispatcherFrame();bool timedOut=false,exportedWhileRunning=false,killed=false;
            Exception pollFailure=null;
            try {
                System.Threading.SynchronizationContext.SetSynchronizationContext(new System.Windows.Threading.DispatcherSynchronizationContext(dispatcher));
                var task=ui.LaunchGame(fallback=="kill"?"hang-unarmed":hang?"hang-recover":"main");
                timer.Tick+=(s,e)=>{
                    try {
                        if(clock.ElapsedMilliseconds>25000){timedOut=true;frame.Continue=false;return;}
                        if(fallback=="kill" && !killed && clock.ElapsedMilliseconds>500) {
                            var marker=Json.Object(File.ReadAllText(Core.PathIn(root,".launcher/game-session.json")));
                            using(var game=System.Diagnostics.Process.GetProcessById(Convert.ToInt32(marker["game_pid"]))) {
                                Require(Path.GetFullPath(game.MainModule.FileName)==Path.GetFullPath(Path.Combine(root,"Bin/futureperfect_recomp.exe")),"Only the isolated integration game's process may be terminated.");
                                game.Kill();killed=true;
                            }
                        }
                        if(hang && !exportedWhileRunning && ((System.Windows.Controls.TextBlock)ui.Window.FindName("StatusText")).Text.Contains("Hang report saved; the game is still running")) {
                            Require(!task.IsCompleted,"Notification must arrive before the hung game exits.");
                            Require(CrashReports.CaptureHealth(root).Contains("Helper responding"),"Live hang must retain a current-session health handshake.");
                            Require(((System.Windows.Controls.ContentControl)ui.Window.FindName("MaintenanceHost")).Visibility==System.Windows.Visibility.Visible,"Hang opens Diagnostics while game runs.");
                            var button=(System.Windows.Controls.Button)System.Windows.LogicalTreeHelper.FindLogicalNode(ui.Window,"ExportCrashReport");
                            Require(button!=null && button.IsEnabled,"Hang export remains enabled while game runs.");
                            string incident=CrashReports.Latest(root);Require(CrashReports.IsHang(root,incident),"Native hang metadata recognized.");
                            CrashReports.Export(root,incident,Path.Combine(root,"while-running.zip"));
                            exportedWhileRunning=true;
                        }
                    } catch(Exception ex){pollFailure=ex;frame.Continue=false;}
                };timer.Start();
                task.ContinueWith(done=>dispatcher.BeginInvoke(new Action(()=>frame.Continue=false)));
                System.Windows.Threading.Dispatcher.PushFrame(frame);
                if(pollFailure!=null)throw pollFailure;
                Require(!timedOut,"Launcher did not finish the deliberate crash.");task.GetAwaiter().GetResult();
                Require(((System.Windows.Controls.TextBlock)ui.Window.FindName("StatusText")).Text.Contains(fallback!=null?"No crash dump was captured; session report saved":hang?"Hang report saved":"Crash report saved"),"Launcher must announce the newly captured report.");
                if(hang)Require(exportedWhileRunning,"The launcher must allow inspecting and exporting a hang before the game exits.");
                string latest=CrashReports.Latest(root);Require(latest!=null,"Launcher discovers real native report.");
                Require(CrashReports.Summary(root).Contains(fallback!=null?"Unexpected exit":hang?"Suspected hang":"0xC0000005"),"Incident metadata is readable by the launcher.");
                string zip=Path.Combine(root,"native-crash-export.zip");CrashReports.Export(root,latest,zip);
                using(var archive=ZipFile.OpenRead(zip))Require(fallback!=null?archive.GetEntry("crash.dmp")==null && archive.GetEntry("report.json")!=null:archive.GetEntry("crash.dmp")!=null,"Export must represent whether a native dump was captured.");
                if(fallback==null)using(var archive=ZipFile.OpenRead(zip))Require(archive.GetEntry("breadcrumbs.txt")!=null,"Native report export must include breadcrumbs.");
                Require(GameSessions.Recover(root)==null,"Observed exit must not produce a second startup incident.");
                Console.WriteLine(fallback!=null?"PASS actual launcher/game "+fallback+" produces an exportable unexpected-exit report without a dump.":hang?"PASS launcher detects native hang while game runs, opens Diagnostics, exports the dump before exit and preserves recovery.":"PASS launcher launches actual deliberate crash, announces new report, reads native metadata and exports its dump.");
            } finally { timer.Stop();System.Threading.SynchronizationContext.SetSynchronizationContext(previous);ui.Window.Close(); }
        } finally {
            Environment.SetEnvironmentVariable("PATH",oldPath);
            string target=Path.GetFullPath(root);
            if(!target.StartsWith(Path.GetFullPath(parent)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase) ||
                !Path.GetFileName(target).StartsWith("crash-report-tests-") ||
                (File.GetAttributes(target)&FileAttributes.ReparsePoint)!=0)throw new Exception("Unexpected integration test root.");
            // The game has exited, but its helper may still be releasing its image.
            // Retry only this validated, disposable fixture; never stop other processes.
            for(int attempt=0;;attempt++) {
                try {Directory.Delete(target,true);break;}
                catch(IOException){if(attempt>=50)throw;}
                catch(UnauthorizedAccessException){if(attempt>=50)throw;}
                System.Threading.Thread.Sleep(100);
            }
        }
    }
    public static int Run()
    {
        Exception failure=null;
        var thread=new System.Threading.Thread(()=>{try{RunSTA();}catch(Exception ex){failure=ex;}});
        thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();
        if(!thread.Join(30000))throw new Exception("Crash report tests timed out.");
        if(failure!=null)throw failure;
        return 0;
    }
    static int RunSTA()
    {
        string parent=Path.GetDirectoryName(typeof(LauncherCrashChecks).Assembly.Location);
        string root=Path.Combine(parent,"crash-report-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try {
            Require(CrashReports.Latest(root)==null,"Fresh installation has no crash report.");
            string name="crash-20261019-120000000-0000001234-000001";
            string folder=CrashReports.Folder(root,name);Directory.CreateDirectory(folder);
            File.WriteAllText(Path.Combine(folder,"capture.lock"),"");
            File.WriteAllText(Path.Combine(folder,"report.json"),"{\"schema\":1,\"kind\":\"unhandled_exception\",\"exception_code\":\"0xC0000005\",\"dump_saved\":true,\"executable_sha256\":\""+new string('a',64)+"\"}");
            File.WriteAllText(Path.Combine(folder,"report.txt"),"Readable crash summary");
            File.WriteAllText(Path.Combine(folder,"crash.dmp"),"Synthetic dump fixture");
            File.WriteAllText(Path.Combine(folder,"game-log-tail.txt"),"Synthetic log fixture");
            File.WriteAllText(Path.Combine(folder,"private-save"),"Must never export");
            Require(CrashReports.Latest(root)==name,"Completed report discovered.");
            Require(CrashReports.Summary(root).Contains("0xC0000005"),"Summary includes validated exception.");
            using(var locked=new FileStream(Path.Combine(folder,"capture.lock"),FileMode.Open,FileAccess.Read,FileShare.None))
                Require(CrashReports.Latest(root)==null,"In-progress reports excluded.");
            string zip=Path.Combine(root,"export.zip");CrashReports.Export(root,name,zip);
            using(var archive=ZipFile.OpenRead(zip)) {
                Require(archive.Entries.Count==4,"Exactly the four allowlisted files exported.");
                Require(!archive.Entries.Any(x=>x.FullName.Contains("private-save")),"Unknown files excluded.");
            }
            string oversized=Path.Combine(root,"oversized.zip");
            string[] filesBefore=Directory.GetFiles(root).OrderBy(path=>path).ToArray();
            using(var dump=new FileStream(Path.Combine(folder,"crash.dmp"),FileMode.Open,FileAccess.Write))dump.SetLength(16*1024*1024+1);
            bool sizeRefused=false;try{CrashReports.Export(root,name,oversized);}catch{sizeRefused=true;}
            Require(sizeRefused && !File.Exists(oversized) && Directory.GetFiles(root).OrderBy(path=>path).SequenceEqual(filesBefore),"Oversized export leaves no partial ZIP or temporary file.");
            File.WriteAllText(Path.Combine(folder,"crash.dmp"),"Synthetic dump fixture");
            byte[] original=File.ReadAllBytes(zip);
            bool overwriteRefused=false;try{CrashReports.Export(root,name,zip);}catch{overwriteRefused=true;}
            Require(overwriteRefused && File.ReadAllBytes(zip).SequenceEqual(original),"Existing export is preserved.");
            bool refused=false;try{CrashReports.Folder(root,"../../Saves");}catch{refused=true;}
            Require(refused,"Invalid report path rejected.");
            File.WriteAllText(Path.Combine(folder,"report.json"),"{\"schema\":1,\"kind\":\"unhandled_exception\",\"exception_code\":\"secret personal path\",\"executable_sha256\":\"secret\"}");
            Require(!CrashReports.Summary(root).Contains("secret"),"Unvalidated metadata excluded from diagnostics.");
            File.WriteAllText(Path.Combine(folder,"report.json"),"{\"schema\":1,\"kind\":\"suspected_hang\",\"exception_code\":null,\"dump_saved\":true}");
            Require(CrashReports.Latest(root)==name && CrashReports.IsHang(root,name),"Hang reports discovered without a synthetic exception.");
            Require(CrashReports.Summary(root).Contains("Suspected hang") && !CrashReports.Summary(root).Contains("Exception:"),"Hang summary is distinguished from a crash.");
            var page=new MaintenancePage(true,true);
            Require(page.OpenCrash!=null && page.ExportCrash!=null,"Diagnostics exposes crash actions.");
            page.SetBusy(true,false);Require(!page.ExportCrash.IsEnabled,"Export disabled during maintenance.");
            Console.WriteLine("Crash report discovery, active-report exclusion, bounded export, path validation, summary redaction and diagnostics actions passed.");
            return 0;
        } finally {
            string target=Path.GetFullPath(root);
            if(!target.StartsWith(Path.GetFullPath(parent)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase) ||
                !Path.GetFileName(target).StartsWith("crash-report-tests-") ||
                (File.GetAttributes(target)&FileAttributes.ReparsePoint)!=0)throw new Exception("Unexpected crash test root.");
            Directory.Delete(target,true);
        }
    }
}
