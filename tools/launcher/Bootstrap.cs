using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
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
                LauncherLog.Write(root, "Bootstrap v" + LauncherVersion.Value + ": " + args.Length + " argument(s) " + DescribeArguments(args));
                int previewExit;
                if (TryForwardPreview(root, args, out previewExit)) return previewExit;
                if (args.Length != 0 && !afterUpdate && !render)
                    throw new ArgumentException("Unknown bootstrap arguments: " + DescribeArguments(args) + ". Open " + Core.LauncherName + " without extra arguments for normal startup.");
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
                            visual.Measure(new Size(544, 270)); visual.Arrange(new Rect(0, 0, 544, 270)); visual.UpdateLayout();
                            var bitmap = new RenderTargetBitmap(544, 270, 96, 96, PixelFormats.Pbgra32); bitmap.Render(visual);
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
        internal static string DescribeArguments(string[] args)
        {
            // Bound diagnostics and JSON-escape control characters so a rejected
            // command remains one log entry, even with malformed shell input.
            return Json.Encode(args.Take(8).Select(arg => arg.Length > 256 ? arg.Substring(0,256) + "..." : arg).ToArray())
                + (args.Length > 8 ? " (additional arguments omitted)" : "");
        }
        internal static bool TryForwardPreview(string root, string[] args, out int exitCode)
        {
            exitCode = 0;
            if (args.Length != 2 || (args[0] != "--render-preview" && args[0] != "--render-settings-preview" && args[0] != "--render-settings-graphics" && args[0] != "--render-settings-audio" && args[0] != "--render-repair-preview" && args[0] != "--render-diagnostics-preview" && args[0] != "--render-news-preview" && args[0] != "--render-versions-preview" && args[0] != "--render-mods-preview")) return false;
            string application = Core.PathIn(root, Core.AppPath);
            Core.Require(File.Exists(application), "Launcher application files are missing: " + application + ". Extract the complete launcher ZIP into this folder and try again.");
            var info = new ProcessStartInfo(application, args[0] + " " + SelfUpdate.Quote(Path.GetFullPath(args[1]))) {
                WorkingDirectory = root, UseShellExecute = false, CreateNoWindow = true };
            using (var child = AntivirusErrors.Start(info)) {
                if (!child.WaitForExit(30000)) {
                    child.Kill(); child.WaitForExit(5000);
                    throw new TimeoutException("Launcher preview did not finish within 30 seconds. Close the launcher and try again.");
                }
                exitCode = child.ExitCode;
            }
            return true;
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
                    string detail = AntivirusErrors.Find(ex) == null ? ex.Message : RecoveryInfo.From(ex, false).Message;
                    Core.Write(Core.PathIn(root, ".launcher/self-last-result.txt"), "Launcher update did not complete. " + detail + " See launcher.log for recovery details.");
                }
                finally { mutex.ReleaseMutex(); }
            }
            // Skip the network check once after a handoff, avoiding retry loops
            // when an update cannot be installed. Future normal starts check again.
            if (relaunch)
                AntivirusErrors.Start(new ProcessStartInfo(Core.PathIn(root, Core.LauncherName), "--after-self-update") { WorkingDirectory = root, UseShellExecute = true });
            return success ? 0 : 1;
        }
    }

    sealed class StartupWindow
    {
        public readonly Window Window;
        readonly string root;
        readonly bool skip;
        readonly Action startApplication;
        readonly Action<string> openDirectory;
        readonly CancellationTokenSource stop = new CancellationTokenSource();
        bool finished, closeRequested, running, launchFailed;
        int attempt;
        TextBlock Status { get { return (TextBlock)Window.FindName("Status"); } }
        ProgressBar Progress { get { return (ProgressBar)Window.FindName("Progress"); } }
        public StartupWindow(string path, bool skipCheck, bool preview, Action launch = null, Action<string> openFolder = null)
        {
            root = path; skip = skipCheck;
            startApplication = launch ?? (() => SelfUpdate.StartApplication(root, Process.GetCurrentProcess().Id));
            openDirectory = openFolder ?? (directory => { Process.Start(new ProcessStartInfo(directory) {UseShellExecute=true}); });
            Window = (Window)XamlReader.Parse(@"<Window xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'
                xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' Title='TimeSplitters — Starting'
                Width='560' Height='310' ResizeMode='NoResize' WindowStartupLocation='CenterScreen'
                Background='#101E2D' Foreground='#E4EDF5' FontFamily='Segoe UI' ShowInTaskbar='True'>
                <Window.Resources>
                  <Style TargetType='Button'>
                    <Setter Property='Background' Value='#1B2C3E'/><Setter Property='Foreground' Value='#E4EDF5'/>
                    <Setter Property='BorderBrush' Value='#506C86'/><Setter Property='Padding' Value='10,7'/>
                    <Setter Property='VerticalAlignment' Value='Center'/><Setter Property='Cursor' Value='Hand'/>
                    <Setter Property='Template'><Setter.Value><ControlTemplate TargetType='Button'>
                      <Border x:Name='Face' Background='{TemplateBinding Background}' BorderBrush='{TemplateBinding BorderBrush}' BorderThickness='1' CornerRadius='3' Padding='{TemplateBinding Padding}'>
                        <ContentPresenter HorizontalAlignment='Center' VerticalAlignment='Center'/>
                      </Border>
                      <ControlTemplate.Triggers>
                        <Trigger Property='IsMouseOver' Value='True'><Setter TargetName='Face' Property='Opacity' Value='0.8'/></Trigger>
                        <Trigger Property='IsKeyboardFocused' Value='True'><Setter TargetName='Face' Property='BorderBrush' Value='#FFFFFF'/></Trigger>
                        <Trigger Property='IsEnabled' Value='False'><Setter TargetName='Face' Property='Opacity' Value='0.4'/></Trigger>
                      </ControlTemplate.Triggers>
                    </ControlTemplate></Setter.Value></Setter>
                  </Style>
                </Window.Resources>
                <Grid Margin='26,22'>
                  <Grid.RowDefinitions><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='*'/></Grid.RowDefinitions>
                  <TextBlock Text='TIMESPLITTERS  /  FUTURE PERFECT' FontFamily='Bahnschrift' FontWeight='Bold' FontSize='14' Foreground='#86CDFF'/>
                  <TextBlock x:Name='Heading' Grid.Row='1' Text='Checking launcher updates…' FontSize='21' FontWeight='SemiBold' Margin='0,18,0,0'/>
                  <ScrollViewer x:Name='StatusScroll' Grid.Row='2' MaxHeight='240' VerticalScrollBarVisibility='Auto' HorizontalScrollBarVisibility='Disabled' Margin='0,7,0,15'><TextBlock x:Name='Status' Text='Checking GitHub Releases.' FontSize='12' Foreground='#A4B8CA' TextWrapping='Wrap'/></ScrollViewer>
                  <ProgressBar x:Name='Progress' Grid.Row='3' Height='3' IsIndeterminate='True' Background='#30485F' Foreground='#86CDFF' BorderThickness='0'/>
                  <DockPanel Grid.Row='4' Margin='0,15,0,0' VerticalAlignment='Bottom' LastChildFill='False'>
                    <StackPanel DockPanel.Dock='Left' Orientation='Horizontal'><Button x:Name='Retry' Content='Retry update' Background='#1B2C3E' Foreground='#E4EDF5' BorderBrush='#506C86' Visibility='Collapsed' Padding='8,6' Margin='0,0,8,0'/><Button x:Name='Folder' Content='Open launcher folder' Visibility='Collapsed' Padding='8,6' Margin='0,0,8,0'/><Button x:Name='Logs' Content='View logs' Background='#1B2C3E' Foreground='#E4EDF5' BorderBrush='#506C86' Visibility='Collapsed' Padding='8,6'/></StackPanel>
                    <TextBlock x:Name='Version' DockPanel.Dock='Left' Foreground='#8CA6BE' FontSize='10' VerticalAlignment='Center'/>
                    <Button x:Name='Continue' DockPanel.Dock='Right' Content='Open installed launcher' Background='#1B2C3E' Foreground='#E4EDF5' BorderBrush='#506C86' Padding='10,6' FontSize='11' Cursor='Hand'/>
                  </DockPanel>
                </Grid>
              </Window>");
            WindowTheme.Attach(Window);
            ((TextBlock)Window.FindName("Version")).Text = "LAUNCHER v" + LauncherVersion.Value;
            using (var icon = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher.ico"))
                Window.Icon = BitmapFrame.Create(icon, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
            var open = (Button)Window.FindName("Continue");
            open.IsEnabled = File.Exists(Core.PathIn(root, Core.AppPath));
            open.Click += (s, e) => { Status.Text="Opening your installed launcher…";if(running)stop.Cancel();else Finish(false); };
            ((Button)Window.FindName("Retry")).Click+=async(s,e)=> {if(launchFailed)Finish(false);else await Start();};
            ((Button)Window.FindName("Folder")).Click+=(s,e)=>OpenDirectory(root);
            ((Button)Window.FindName("Logs")).Click+=(s,e)=>OpenDirectory(Core.PathIn(root,".launcher"));
            Window.Closing += (s, e) => { if (!finished && running) { e.Cancel = true; closeRequested = true; stop.Cancel(); } };
            if (!preview) Window.Loaded += async (s, e) => await Start();
        }
        void OpenDirectory(string directory)
        {
            try { openDirectory(directory); }
            catch(Exception ex) {
                LauncherLog.Write(root,"Startup folder could not open: "+ex);
                const string notice="\nWindows couldn't open the folder. You can open it manually in File Explorer.";
                if(!Status.Text.EndsWith(notice,StringComparison.Ordinal))Status.Text+=notice;
            }
        }
        async Task Start()
        {
            if(running)return;
            running=true;int generation=++attempt;bool attemptedDownload=false;
            Progress.IsIndeterminate=true;Progress.Value=0;
            ((Button)Window.FindName("Retry")).Visibility=Visibility.Collapsed;((Button)Window.FindName("Logs")).Visibility=Visibility.Collapsed;
            ((TextBlock)Window.FindName("Version")).Visibility=Visibility.Visible;
            ((TextBlock)Window.FindName("Heading")).Text="Checking launcher updates…";
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
                            ((TextBlock)Window.FindName("Heading")).Text="Updating launcher…";Status.Text = "Preparing launcher v" + release.Tag + "…";
                            using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(stop.Token))
                            {
                                timeout.CancelAfter(TimeSpan.FromMinutes(2));
                                var progress = new TransferProgress(value => { if(!running || generation!=attempt || stop.IsCancellationRequested)return;Progress.IsIndeterminate = value.Phase=="Verifying" || value.Phase=="Preparing"; Progress.Value = value.Percent; Status.Text = "Launcher v"+release.Tag+" · "+value.Summary; });
                                attemptedDownload=true;
                                if (await SelfUpdate.Prepare(root, release, client, progress, timeout.Token))
                                {
                                    stop.Token.ThrowIfCancellationRequested();
                                    string helper = Core.PathIn(root, ".launcher/self-helper.exe");
                                    AntivirusErrors.Copy(Assembly.GetExecutingAssembly().Location, helper, true);
                                    AntivirusErrors.Start(new ProcessStartInfo(helper, "--apply-launcher " + SelfUpdate.Quote(root) + " " + Process.GetCurrentProcess().Id) { WorkingDirectory = root, UseShellExecute = true });
                                    handedOff = true;
                                }
                            }
                        }
                        LauncherLog.Write(root, release == null ? "Startup: no launcher update published for this channel." : "Startup: checked launcher " + release.Tag);
                    }
                }
                catch (Exception ex) {
                    LauncherLog.Write(root,"Startup update skipped: "+ex);
                    if(attemptedDownload && !stop.IsCancellationRequested){
                        var info=RecoveryInfo.From(ex is OperationCanceledException?new TimeoutException("Launcher download timed out."):ex,true);
                        Window.Height=490;
                        ((TextBlock)Window.FindName("Heading")).Text=info.Title;Status.Text=info.Message;
                        Progress.IsIndeterminate=false;Progress.Value=0;
                        ((TextBlock)Window.FindName("Version")).Visibility=Visibility.Collapsed;
                        ((Button)Window.FindName("Retry")).Visibility=Visibility.Visible;((Button)Window.FindName("Logs")).Visibility=Visibility.Visible;
                        running=false;return;
                    }
                }
            }
            running=false;Finish(handedOff);
        }
        void Finish(bool handedOff)
        {
            if (!handedOff && !closeRequested)
            {
                try { startApplication(); }
                catch (Exception ex) {
                    var native = ex as System.ComponentModel.Win32Exception;
                    LauncherLog.Write(root,"Startup application launch failed"+(native==null?"": " (Windows error "+native.NativeErrorCode+")")+": "+ex);
                    var info=RecoveryInfo.LauncherStart(ex, Core.PathIn(root, Core.AppPath));
                    launchFailed=true;
                    Window.Height=490;
                    ((TextBlock)Window.FindName("Heading")).Text=info.Title;Status.Text=info.Message;
                    Progress.IsIndeterminate=false;Progress.Value=0;
                    ((TextBlock)Window.FindName("Version")).Visibility=Visibility.Collapsed;
                    ((Button)Window.FindName("Continue")).Visibility=Visibility.Collapsed;
                    var retry=(Button)Window.FindName("Retry");retry.Content=info.RetryLabel;retry.Visibility=Visibility.Visible;
                    ((Button)Window.FindName("Folder")).Visibility=Visibility.Visible;
                    ((Button)Window.FindName("Logs")).Visibility=Visibility.Visible;
                    return;
                }
            }
            finished = true; Window.Close();
        }
    }
}
