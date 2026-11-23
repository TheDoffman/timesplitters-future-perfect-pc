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
        readonly CancellationTokenSource stop=new CancellationTokenSource();readonly TextBlock status;
        readonly string root;readonly string[] args;bool handedOff;
        public NativeStartup(string root,string[] args)
        {
            NativeApplication.ApplyWindowIcon(this);
            this.root=root;this.args=args;Title="TimeSplitters · Future Perfect";Width=540;Height=300;CanResize=false;WindowStartupLocation=WindowStartupLocation.CenterScreen;Background=Brush.Parse("#0B1420");
            var panel=new StackPanel{Margin=new Thickness(28),Spacing=14};panel.Children.Add(NativeWindow.Text("TIMESPLITTERS",30));panel.Children.Add(NativeWindow.Text("FUTURE PERFECT",18));
            status=NativeWindow.Text("Checking for launcher updates…",13);panel.Children.Add(status);panel.Children.Add(NativeWindow.Button("Open installed launcher",()=>stop.Cancel()));Content=panel;
            Opened+=async(s,e)=>await Start();Closed+=(s,e)=>{if(!handedOff)stop.Cancel();};
        }
        async Task Start()
        {
            try{
                // Developer output and source checkouts never replace themselves.
                if(!LauncherPlatform.Checkout(root) && SelfUpdate.Complete(root) && !Array.Exists(args,a=>a=="--skip-self-update")){
                    using(var client=new ReleaseClient()){
                        var release=await SelfUpdate.CheckWithTimeout(client,SelfUpdate.PreviewChannel(root),TimeSpan.FromSeconds(8),stop.Token);
                        if(release!=null && GameVersion.Parse(release.Tag).CompareTo(GameVersion.Parse(LauncherVersion.Value))>0){
                            using(var deadline=CancellationTokenSource.CreateLinkedTokenSource(stop.Token)){
                                deadline.CancelAfter(TimeSpan.FromMinutes(2));status.Text="Downloading launcher "+release.Tag+"…";
                                if(await SelfUpdate.Prepare(root,release,client,new Progress<double>(n=>status.Text="Updating launcher · "+n.ToString("0")+"%"),deadline.Token)){
                                    handedOff=true;Handoff(root);return;
                                }
                            }
                        }
                    }
                }
            }catch(Exception ex){LauncherLog.Write(root,"Startup update fallback: "+ex.Message);}
            if(!IsVisible)return;handedOff=true;var desktop=(IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime;
            var window=new NativeWindow(root,args);desktop.MainWindow=window;window.Show();Close();
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
            var info=new ProcessStartInfo(helper){UseShellExecute=false,WorkingDirectory=root};info.ArgumentList.Add(action);info.ArgumentList.Add(root);info.ArgumentList.Add(Environment.ProcessId.ToString());return info;
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
                foreach(string arg in new[]{probe?"--test-game-helper":rollback?"--rollback-game":"--apply-game",root,parent.ToString()})info.ArgumentList.Add(arg);
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
