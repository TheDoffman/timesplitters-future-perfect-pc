using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media;
namespace TimeSplitters.Launcher
{
    public sealed class NativeStartup:Window
    {
        internal static bool HandingOff;
        CancellationTokenSource stop;readonly TextBlock status;
        readonly UpdateWindowPage page;readonly Button open,retry,logs;
        readonly string root;readonly string[] args;bool handedOff,running,openRequested;
        readonly Func<CancellationToken,Task> checkOverride;readonly Action launchOverride,logsOverride;
        public NativeStartup(string root,string[] args,Func<CancellationToken,Task> check=null,Action launch=null,Action viewLogs=null,bool preview=false)
        {
            NativeApplication.ApplyWindowIcon(this);this.root=root;this.args=args;checkOverride=check;launchOverride=launch;logsOverride=viewLogs;
            Title="TimeSplitters · Launcher update";Width=620;Height=360;CanResize=false;WindowStartupLocation=WindowStartupLocation.CenterScreen;Background=Brush.Parse("#0B1420");
            page=new UpdateWindowPage("Checking launcher updates…","Looking for a new launcher version.","LAUNCHER v"+LauncherVersion.Value);status=page.Status;
            retry=page.AddAction("Retry update",async()=>await Start());retry.IsVisible=false;
            logs=page.AddAction("View logs",()=>{try{if(logsOverride!=null)logsOverride();else NativeHost.Open(Core.PathIn(root,".launcher"));}catch(Exception ex){status.Text+="\nCould not open logs: "+ex.Message;}});logs.IsVisible=false;
            open=page.AddAction("Open installed launcher",()=>{openRequested=true;if(running)stop.Cancel();else OpenInstalled();},true);Content=page.View;
#if LINUX
            if(!preview&&LinuxHandheld.Profile.Enabled){new DeckControls(this,root,()=>page.View,()=>true,()=>{openRequested=true;if(running)stop.Cancel();else OpenInstalled();});Opened+=(s,e)=>LinuxHandheld.Style(page.View);}
#endif
            if(!preview)Opened+=async(s,e)=>await Start();Closed+=(s,e)=>{if(!handedOff&&stop!=null)stop.Cancel();};
        }
        internal async Task Start()
        {
            if(running||handedOff)return;running=true;openRequested=false;stop?.Dispose();stop=new CancellationTokenSource();
            page.Actions.Columns=1;retry.IsVisible=logs.IsVisible=false;page.Progress.IsIndeterminate=true;page.Progress.Value=0;page.Heading.Text="Checking launcher updates…";status.Text="Looking for a new launcher version.";
            try{if(checkOverride!=null)await checkOverride(stop.Token);else await CheckUpdate();}
            catch(Exception ex){
                if(!stop.IsCancellationRequested){handedOff=false;LauncherLog.Write(root,"Startup update failed: "+ex);page.Heading.Text="Launcher update couldn’t finish";status.Text=ex is TimeoutException||ex is OperationCanceledException?"The update timed out. Check your connection and retry, or open your installed launcher.":ex.Message;
                    page.Note.Text="Your installed launcher and game files are kept. Retry, view the logs, or continue with the installed launcher.";
                    page.Progress.IsIndeterminate=false;page.Progress.Value=0;page.Actions.Columns=3;retry.IsVisible=logs.IsVisible=true;Height=420;retry.Focus();return;}
            }finally{running=false;}
            if(!handedOff&&(IsVisible||launchOverride!=null))OpenInstalled();
        }
        void OpenInstalled(){if(handedOff)return;try{if(launchOverride!=null)launchOverride();else{var desktop=(IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime;var window=new NativeWindow(root,args);desktop.MainWindow=window;window.Show();}handedOff=true;Close();}catch(Exception ex){page.Heading.Text="Couldn’t open the launcher";status.Text=ex.Message;page.Progress.IsIndeterminate=false;}}
        async Task CheckUpdate()
        {
            if(LauncherPlatform.Checkout(root)||!SelfUpdate.Complete(root)||Array.Exists(args,a=>a=="--skip-self-update"))return;
            using(var client=new ReleaseClient()){
                var release=await SelfUpdate.CheckWithTimeout(client,SelfUpdate.PreviewChannel(root),TimeSpan.FromSeconds(8),stop.Token);
                if(release!=null&&GameVersion.Parse(release.Tag).CompareTo(GameVersion.Parse(LauncherVersion.Value))>0){
                    using(var deadline=CancellationTokenSource.CreateLinkedTokenSource(stop.Token)){
                        deadline.CancelAfter(TimeSpan.FromMinutes(2));page.Heading.Text="Updating launcher…";status.Text="Preparing launcher "+release.Tag+"…";
                        if(await SelfUpdate.Prepare(root,release,client,new TransferProgress(value=>{page.Progress.IsIndeterminate=value.Phase=="Verifying"||value.Phase=="Preparing";page.Progress.Value=value.Percent;status.Text="Launcher "+release.Tag+" · "+value.Summary;}),deadline.Token)){Handoff(root);handedOff=true;}
                    }
                }
            }
        }
        internal static bool HelperArguments(string[] args){
#if LINUX
            return HandheldProfile.HelperArguments(args);
#else
            return args.Length==3;
#endif
        }
        static void ForwardHandheld(ProcessStartInfo info){
#if LINUX
            foreach(var option in HandheldProfile.Forward(Program.Arguments))info.ArgumentList.Add(option);
#endif
        }
        public static void Handoff(string root)
        {
            StartHelper(root,".launcher/mac-self-helper","--apply-self");
        }
        public static async Task HandoffGame(string root,bool rollback=false)
        {
            Core.Require(!LauncherPlatform.Checkout(root),"Release updates cannot overwrite a developer checkout.");
            Core.Require(!Core.GameRunning(),"Close the game before changing the installed release.");
            ProcessStartInfo info;
            using(var progress=new UpdateProgress(root)) {
                progress.Report("Preparing the installer");
                info=await Task.Run(()=>PrepareGameHelper(root,Environment.ProcessId,rollback));
            }
            LaunchHelper(info);
        }
        static void StartHelper(string root,string name,string action) { LaunchHelper(PrepareHelper(root,name,action)); }
        static ProcessStartInfo PrepareHelper(string root,string name,string action)
        {
            string helper=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(helper));
            LauncherPlatform.CopyFile(Environment.ProcessPath,helper);File.SetUnixFileMode(helper,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
            var info=new ProcessStartInfo(helper){UseShellExecute=false,WorkingDirectory=root};info.ArgumentList.Add(action);info.ArgumentList.Add(root);info.ArgumentList.Add(Environment.ProcessId.ToString());ForwardHandheld(info);return info;
        }
        internal static ProcessStartInfo PrepareGameHelper(string root,int parent,bool rollback=false,bool probe=false) {
            string relative=".launcher/game-helper-"+Guid.NewGuid().ToString("N");
            string folder=Core.PathIn(root,relative);Directory.CreateDirectory(folder);
            string executable=Path.Combine(folder,"TimeSplitters.Launcher");
            try {
                LauncherPlatform.CopyFile(Environment.ProcessPath,executable);
#if MACOS
                // Signed Mac bundles keep native UI libraries beside their host.
                // The helper must own its copy while that bundle is replaced.
                foreach(string library in Directory.GetFiles(Path.GetDirectoryName(Environment.ProcessPath),"*.dylib")) {
                    Core.RequireUnlinked(library,"Linked helper UI library.");
                    LauncherPlatform.CopyFile(library,Path.Combine(folder,Path.GetFileName(library)));
                }
#endif
                var info=new ProcessStartInfo(executable){UseShellExecute=false,WorkingDirectory=root};
                foreach(string arg in new[]{probe?"--test-game-helper":rollback?"--rollback-game":"--apply-game",root,parent.ToString()})info.ArgumentList.Add(arg);ForwardHandheld(info);
                return info;
            } catch { CleanupGameHelper(root,executable);throw; }
        }
        internal static void CleanupGameHelper(string root,string executable) {
            try {
                string folder=Path.GetDirectoryName(executable),name=Path.GetFileName(folder);Guid id;
                if(!name.StartsWith("game-helper-",StringComparison.Ordinal) || !Guid.TryParseExact(name.Substring(12),"N",out id))return;
                string relative=".launcher/"+name;
                if(Path.GetFullPath(folder)!=Core.PathIn(root,relative))return;
                foreach(string entry in Directory.GetFileSystemEntries(folder))
                    if(Directory.Exists(entry) || (Path.GetFileName(entry)!="TimeSplitters.Launcher" && Path.GetExtension(entry)!=".dylib"))return;
                Core.RemoveOwned(root,relative);
            } catch(Exception ex) {LauncherLog.Write(root,"Game helper cleanup deferred: "+ex.Message);}
        }
        static void LaunchHelper(ProcessStartInfo info) {
            Process.Start(info);
            HandingOff=true;((IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime).Shutdown();
        }
        internal static void Restart(string root,bool probe,string preview)
        {
            if(probe){
                var app=new ProcessStartInfo(Core.PathIn(root,Core.LauncherName)){UseShellExecute=false,WorkingDirectory=root};
                foreach(var arg in new[]{"--render-preview",Core.PathIn(root,preview),"--root",root,"--skip-self-update"})app.ArgumentList.Add(arg);
                using(var process=Process.Start(app)){if(!process.WaitForExit(20000)){process.Kill(true);process.WaitForExit(3000);throw new IOException("Restarted launcher preview timed out.");}Core.Require(process.ExitCode==0,"Restarted launcher preview failed.");}
            } else Process.Start(LauncherPlatform.RestartInfo(root));
        }
        public static int Apply(string root,int parent,bool probe=false)
        {
            try{SelfUpdate.WaitForParent(parent);using(var guard=new FileStream(Core.PathIn(root,LauncherPlatform.LockFile),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
                Core.RecoverLauncher(root);SelfUpdate.InstallPending(root);
            }
            Restart(root,probe,"self-update-preview.png");return 0;
            }catch(Exception e){LauncherLog.Write(root,e.ToString());return 1;}
        }
    }
}
