using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;

namespace TimeSplitters.Launcher
{
    // The copied helper owns a real UI while the installed launcher is replaced.
    // It deliberately does not acquire the main-window lock or check for updates.
    public sealed class NativeUpdateWindow : Window
    {
        readonly string root;
        readonly string[] args;
        readonly TextBlock status, elapsed;
        readonly Button close;
        readonly ProgressBar progressBar;
        readonly DispatcherTimer timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        readonly Stopwatch watch = Stopwatch.StartNew();
        bool finished;
        public static bool IsHelper(string[] args) {
            return NativeStartup.HelperArguments(args) && (args[0] == "--apply-game" || args[0] == "--rollback-game" || args[0] == "--test-game-helper");
        }
        public NativeUpdateWindow(string root, string[] args) {
            this.root = root; this.args = args;
            NativeApplication.ApplyWindowIcon(this);
            FontFamily=Ui.Serif;Title="TimeSplitters · Updating";Width=620;Height=360;CanResize=false;
            WindowStartupLocation=WindowStartupLocation.CenterScreen;Background=Brush.Parse("#071321");
            var page=new UpdateWindowPage(args[0]=="--rollback-game"?"Restoring previous game":"Installing game update","Waiting for the launcher to close","Keep this window open. The launcher will reopen when installation finishes.");
            status=page.Status;elapsed=page.Note;progressBar=page.Progress;
            close=page.AddAction("Close",()=>((IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime).Shutdown(1));
            close.IsVisible=false;Content=page.View;
#if LINUX
            if(LinuxHandheld.Profile.Enabled){new DeckControls(this,root,()=>page.View,()=>finished,()=>{if(finished)Close();});Opened+=(s,e)=>LinuxHandheld.Style(page.View);}
#endif
            Closing += (s,e) => { if (!finished) e.Cancel = true; };
            Closed += (s,e) => timer.Stop();
            timer.Tick += (s,e) => elapsed.Text = "Working · " + (int)watch.Elapsed.TotalSeconds + " s elapsed. The launcher will reopen when finished.";
            Opened += async (s,e) => await Install();
        }
        void Capture(string name) {
            var visual = (Control)Content;
            using (var bitmap = new RenderTargetBitmap(PixelSize.FromSize(visual.Bounds.Size,1),new Vector(96,96))) {
                bitmap.Render(visual);bitmap.Save(Core.PathIn(root,name));
            }
        }
        async Task Install() {
            timer.Start();
            bool probe = args[0] == "--test-game-helper";
            try {
                if (probe) {
                    // The lifecycle test retains evidence of the real helper UI.
                    await Task.Delay(150);
                    Close();Core.Require(IsVisible,"The updater closed during installation.");
                    Capture("update-progress-preview.png");
                }
                bool reopened = false;
                int result;
                bool capturedInstall = false;
                using (var progress = new UpdateProgress(root,new Progress<string>(message => {
                    status.Text = message;
                    if(probe && !capturedInstall && message.StartsWith("Installing files")) {
                        capturedInstall = true;Capture("update-installing-preview.png");
                    }
                })))
                    result = await Task.Run(() => UpdateRestart.ApplyGame(root,Int32.Parse(args[2]),args[0] == "--rollback-game",
                        () => { NativeStartup.Restart(root,probe,"game-update-preview.png"); reopened = true; },null,progress));
                finished = true;
                if (result != 0 && !reopened) throw new IOException("The update could not finish. Keep the .launcher folder for recovery and check launcher.log.");
                ((IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime).Shutdown(result);
            } catch (Exception ex) {
                finished = true;timer.Stop();progressBar.IsIndeterminate=false;LauncherLog.Write(root,ex.ToString());
                status.Text = "Update failed: " + ex.Message;elapsed.Text = "Keep the .launcher folder for recovery. Details are in .launcher/launcher.log.";close.IsVisible = true;
            }
        }
    }
}
