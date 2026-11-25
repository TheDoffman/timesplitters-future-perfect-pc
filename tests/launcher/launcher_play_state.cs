using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Windows.Controls;
using TimeSplitters.Launcher;

// Standalone focused check: safe to run while a different checkout's game/test
// is alive. Only the two inert child processes created here are stopped.
public static class LauncherPlayStateChecks
{
    [STAThread] public static int Main(string[] args)
    {
        if(args.Length==1&&args[0]=="--play-state-hold"){Thread.Sleep(30000);return 0;}
        Exception failure=null;var worker=new Thread(()=>{try{Check();}catch(Exception ex){failure=ex;}});worker.SetApartmentState(ApartmentState.STA);worker.IsBackground=true;worker.Start();
        if(!worker.Join(20000)){Console.Error.WriteLine("Play-state check timed out.");return 1;}if(failure!=null){Console.Error.WriteLine(failure);return 1;}return 0;
    }
    static void Need(bool condition,string message){if(!condition)throw new Exception(message);}
    static void Flag(LauncherWindow ui,string name,bool value){typeof(LauncherWindow).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).SetValue(ui,value);}
    static Process Hold(string root){string path=Core.PathIn(root,"Bin/futureperfect_recomp.exe");Directory.CreateDirectory(Path.GetDirectoryName(path));File.Copy(typeof(LauncherPlayStateChecks).Assembly.Location,path);return Process.Start(new ProcessStartInfo(path,"--play-state-hold"){UseShellExecute=false,CreateNoWindow=true});}
    static void Stop(Process child){if(child==null)return;try{if(!child.HasExited)child.Kill();child.WaitForExit();}finally{child.Dispose();}}
    static void Check()
    {
        string scope=Path.Combine(Path.GetTempPath(),"tsfp-play-state-"+Guid.NewGuid().ToString("N")),root=Path.Combine(scope,"current"),other=Path.Combine(scope,"other");Directory.CreateDirectory(root);
        Process unrelated=null,own=null;LauncherWindow ui=null;
        try {
            unrelated=Hold(other);Need(!unrelated.WaitForExit(150),"Unrelated inert probe exited early.");
            Need(Core.GameRunning(other)&&!Core.GameRunning(root),"Process detection did not distinguish installations.");
            ui=new LauncherWindow(root,false);var shell=new LauncherAppearance(ui);
            // Supply the native validator's cached success, without copying any
            // original game data or bypassing validation in production.
            Flag(ui,"gameInstalled",true);Flag(ui,"validData",true);
            typeof(LauncherWindow).GetMethod("SetHomePrimaryAction",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(ui,new object[]{"PlayButton"});
            var primary=(Button)typeof(LauncherAppearance).GetProperty("MainAction",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(shell,null);
            var original=(Button)ui.Window.FindName("PlayButton");
            original.IsEnabled=false;shell.RefreshState();Need(original.IsEnabled&&primary.IsEnabled,"Stale disabled Play did not recover with only another installation running.");
            own=Hold(root);Need(!own.WaitForExit(150),"Own inert probe exited early.");shell.RefreshState();Need(!primary.IsEnabled&&System.Windows.Automation.AutomationProperties.GetName(primary)=="Playing","Own running game did not show disabled Playing.");
            Stop(own);own=null;shell.RefreshState();Need(primary.IsEnabled&&System.Windows.Automation.AutomationProperties.GetName(primary)=="Play","Play did not recover after an externally launched game exited.");
            Flag(ui,"busy",true);shell.RefreshState();Need(!primary.IsEnabled,"Busy operation unexpectedly enabled Play.");Flag(ui,"busy",false);
            Flag(ui,"validData",false);shell.RefreshState();Need(!primary.IsEnabled,"Missing or invalid data unexpectedly enabled Play.");Flag(ui,"validData",true);
            shell.RefreshState();Need(primary.IsEnabled&&!unrelated.HasExited,"Ready Play was not restored while the unrelated test remained alive.");
            Console.WriteLine("PASS Play: installation identity, stale-state recovery, external start/exit, Playing label, busy and data guards; unrelated probe kept alive.");
        } finally {
            Stop(own);Stop(unrelated);if(ui!=null){Flag(ui,"busy",false);ui.Window.Close();}
            string full=Path.GetFullPath(scope);if(full.StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase)&&Path.GetFileName(full).StartsWith("tsfp-play-state-",StringComparison.Ordinal))Directory.Delete(full,true);
        }
    }
}
