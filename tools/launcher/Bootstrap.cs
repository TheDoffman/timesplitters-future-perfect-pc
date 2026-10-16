using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace TimeSplitters.Launcher
{
    public static class Bootstrap
    {
        [STAThread]
        public static int Main(string[] args)
        {
            string root = Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory);
            try
            {
                if (args.Length == 3 && args[0] == "--apply-launcher")
                {
                    root = Path.GetFullPath(args[1]);
                    return RunHelper(root, Int32.Parse(args[2]), true);
                }
                bool afterUpdate = args.Length == 1 && args[0] == "--after-self-update";
                bool render = args.Length == 2 && args[0] == "--render-splash";
                if (args.Length != 0 && !afterUpdate && !render) throw new ArgumentException("Unknown bootstrap arguments.");
                using (var mutex = new Mutex(false, Core.MutexName(root)))
                {
                    bool locked;
                    try { locked = mutex.WaitOne(0); } catch (AbandonedMutexException) { locked = true; }
                    if (!locked) { MessageBox.Show("The launcher is already open for this installation.", "TimeSplitters"); return 2; }
                    try
                    {
                        if (!render) { Core.RecoverLauncher(root); Core.Recover(root); }
                        var app = new Application();
                        var startup = new StartupWindow(root, afterUpdate, render);
                        if (render)
                        {
                            var visual = (Grid)startup.Window.Content;
                            visual.Background = startup.Window.Background;
                            visual.Measure(new Size(480, 208)); visual.Arrange(new Rect(0, 0, 480, 208)); visual.UpdateLayout();
                            var bitmap = new RenderTargetBitmap(480, 208, 96, 96, PixelFormats.Pbgra32); bitmap.Render(visual);
                            var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
                            using (var output = File.Create(args[1])) encoder.Save(output);
                            return 0;
                        }
                        app.Run(startup.Window);
                    }
                    finally { mutex.ReleaseMutex(); }
                }
                return 0;
            }
            catch (Exception ex)
            {
                LauncherLog.Write(root, ex.ToString());
                MessageBox.Show(ex.Message + "\n\nDetails: .launcher\\launcher.log", "TimeSplitters — Startup", MessageBoxButton.OK, MessageBoxImage.Error);
                return 1;
            }
        }
        internal static int RunHelper(string root, int parent, bool relaunch)
        {
            SelfUpdate.WaitForParent(parent);
            bool success = false;
            using (var mutex = new Mutex(false, Core.MutexName(root)))
            {
                bool locked;
                try { locked = mutex.WaitOne(30000); } catch (AbandonedMutexException) { locked = true; }
                Core.Require(locked, "Another launcher is using this installation. No launcher update was applied.");
                try
                {
                    Core.RecoverLauncher(root);
                    SelfUpdate.InstallPending(root);
                    LauncherLog.Write(root, "Launcher updated to " + SelfUpdate.InstalledVersion(root));
                    Core.Write(Core.PathIn(root, ".launcher/self-last-result.txt"), "Launcher updated to v" + SelfUpdate.InstalledVersion(root) + ". Your game version is unchanged.");
                    success = true;
                }
                catch (Exception ex)
                {
                    LauncherLog.Write(root, ex.ToString());
                    Core.Write(Core.PathIn(root, ".launcher/self-last-result.txt"), "Launcher update did not complete. See launcher.log for recovery details.");
                }
                finally { mutex.ReleaseMutex(); }
            }
            // Skip the network check once after a handoff, avoiding retry loops
            // when an update cannot be installed. Future normal starts check again.
            if (relaunch)
                Process.Start(new ProcessStartInfo(Core.PathIn(root, Core.LauncherName), "--after-self-update") { WorkingDirectory = root, UseShellExecute = true });
            return success ? 0 : 1;
        }
    }

    sealed class StartupWindow
    {
        public readonly Window Window;
        readonly string root;
        readonly bool skip;
        readonly CancellationTokenSource stop = new CancellationTokenSource();
        bool finished, closeRequested;
        TextBlock Status { get { return (TextBlock)Window.FindName("Status"); } }
        ProgressBar Progress { get { return (ProgressBar)Window.FindName("Progress"); } }
        public StartupWindow(string path, bool skipCheck, bool preview)
        {
            root = path; skip = skipCheck;
            Window = (Window)XamlReader.Parse(@"<Window xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'
                xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' Title='TimeSplitters — Starting'
                Width='496' Height='247' ResizeMode='NoResize' WindowStartupLocation='CenterScreen'
                Background='#101E2D' Foreground='#E4EDF5' FontFamily='Segoe UI' ShowInTaskbar='True'>
                <Grid Margin='26,22'>
                  <Grid.RowDefinitions><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='*'/></Grid.RowDefinitions>
                  <TextBlock Text='TIMESPLITTERS  /  FUTURE PERFECT' FontFamily='Bahnschrift' FontWeight='Bold' FontSize='14' Foreground='#86CDFF'/>
                  <TextBlock Grid.Row='1' Text='Checking launcher updates…' FontSize='21' FontWeight='SemiBold' Margin='0,18,0,0'/>
                  <TextBlock x:Name='Status' Grid.Row='2' Text='Getting everything ready.' FontSize='12' Foreground='#A4B8CA' TextWrapping='Wrap' Margin='0,7,0,15'/>
                  <ProgressBar x:Name='Progress' Grid.Row='3' Height='3' IsIndeterminate='True' Background='#30485F' Foreground='#86CDFF' BorderThickness='0'/>
                  <DockPanel Grid.Row='4' Margin='0,15,0,0' LastChildFill='False'>
                    <TextBlock x:Name='Version' DockPanel.Dock='Left' Foreground='#8CA6BE' FontSize='10' VerticalAlignment='Center'/>
                    <Button x:Name='Continue' DockPanel.Dock='Right' Content='Open installed launcher' Background='#1B2C3E' Foreground='#E4EDF5' BorderBrush='#506C86' Padding='10,6' FontSize='11' Cursor='Hand'/>
                  </DockPanel>
                </Grid>
              </Window>");
            ((TextBlock)Window.FindName("Version")).Text = "LAUNCHER v" + LauncherVersion.Value;
            using (var icon = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher.ico"))
                Window.Icon = BitmapFrame.Create(icon, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
            var open = (Button)Window.FindName("Continue");
            open.IsEnabled = File.Exists(Core.PathIn(root, Core.AppPath));
            if (preview) return;
            open.Click += (s, e) => { Status.Text = "Opening your installed launcher…"; stop.Cancel(); };
            Window.Closing += (s, e) => { if (!finished) { e.Cancel = true; closeRequested = true; stop.Cancel(); } };
            Window.Loaded += async (s, e) => await Start();
        }
        async Task Start()
        {
            bool handedOff = false;
            using (var client = new ReleaseClient())
            {
                try
                {
                    if (!skip && !Core.GameRunning())
                    {
                        var release = await SelfUpdate.CheckWithTimeout(client, SelfUpdate.PreviewChannel(root), TimeSpan.FromSeconds(8), stop.Token);
                        if (release != null)
                        {
                            Status.Text = "Preparing launcher v" + release.Tag + "…";
                            using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(stop.Token))
                            {
                                timeout.CancelAfter(TimeSpan.FromMinutes(2));
                                var progress = new Progress<double>(value => { Progress.IsIndeterminate = false; Progress.Value = value; Status.Text = "Downloading launcher v" + release.Tag + " · " + value.ToString("0") + "%"; });
                                if (await SelfUpdate.Prepare(root, release, client, progress, timeout.Token))
                                {
                                    stop.Token.ThrowIfCancellationRequested();
                                    string helper = Core.PathIn(root, ".launcher/self-helper.exe");
                                    File.Copy(Assembly.GetExecutingAssembly().Location, helper, true);
                                    Process.Start(new ProcessStartInfo(helper, "--apply-launcher " + SelfUpdate.Quote(root) + " " + Process.GetCurrentProcess().Id) { WorkingDirectory = root, UseShellExecute = true });
                                    handedOff = true;
                                }
                            }
                        }
                        LauncherLog.Write(root, release == null ? "Startup: no launcher update published for this channel." : "Startup: checked launcher " + release.Tag);
                    }
                }
                catch (Exception ex) { LauncherLog.Write(root, "Startup update skipped: " + ex.Message); }
            }
            if (!handedOff && !closeRequested)
            {
                try { SelfUpdate.StartApplication(root, Process.GetCurrentProcess().Id); }
                catch (Exception ex) { LauncherLog.Write(root, ex.ToString()); MessageBox.Show(Window, ex.Message, "TimeSplitters — Startup", MessageBoxButton.OK, MessageBoxImage.Error); }
            }
            finished = true; Window.Close();
        }
    }
}
