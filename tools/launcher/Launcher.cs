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
    public static class Program
    {
        [STAThread]
        public static int Main(string[] args)
        {
            string root = Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory);
            if (Path.GetFileName(Assembly.GetExecutingAssembly().Location) == Core.AppName)
                root = Directory.GetParent(root.TrimEnd(Path.DirectorySeparatorChar)).FullName;
            try
            {
                if (args.Length == 3 && (args[0] == "--apply" || args[0] == "--rollback"))
                    return Helper(args[0], Path.GetFullPath(args[1]), Int32.Parse(args[2]));
                bool preview = args.Length == 2 && args[0] == "--render-preview";
                bool fromBootstrap = args.Length == 2 && args[0] == "--from-bootstrap";
                if (args.Length == 0)
                {
                    Process.Start(new ProcessStartInfo(Core.PathIn(root, Core.LauncherName)) { WorkingDirectory = root, UseShellExecute = true });
                    return 0;
                }
                if (!preview && !fromBootstrap) throw new ArgumentException("Unknown launcher arguments.");
                if (fromBootstrap) SelfUpdate.WaitForParent(Int32.Parse(args[1]));
                using (var mutex = new Mutex(false, Core.MutexName(root)))
                {
                    bool locked;
                    try { locked = mutex.WaitOne(0); } catch (AbandonedMutexException) { locked = true; }
                    if (!locked) { MessageBox.Show("The launcher is already open for this installation.", "TimeSplitters"); return 2; }
                    try
                    {
                        bool recovered = !preview && Core.Recover(root);
                        var app = new Application();
                        var ui = new LauncherWindow(root, preview);
                        if (preview)
                        {
                            var window = ui.Window;
                            var visual = (Grid)window.Content;
                            visual.Background = window.Background;
                            visual.Measure(new Size(1152, 772)); visual.Arrange(new Rect(0, 0, 1152, 772)); visual.UpdateLayout();
                            var bitmap = new RenderTargetBitmap(1152, 772, 96, 96, PixelFormats.Pbgra32);
                            bitmap.Render(visual);
                            var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
                            using (var stream = File.Create(args[1])) encoder.Save(stream);
                            return 0;
                        }
                        if (recovered) ui.SetStatus("Recovered an interrupted update. Your previous files have been restored.");
                        app.Run(ui.Window);
                    }
                    finally { mutex.ReleaseMutex(); }
                }
                return 0;
            }
            catch (Exception ex)
            {
                try { Log(root, ex.ToString()); } catch { }
                MessageBox.Show(ex.Message + "\n\nDetails are in .launcher\\launcher.log.", "TimeSplitters — Launcher", MessageBoxButton.OK, MessageBoxImage.Error);
                return 1;
            }
        }
        public static void Log(string root, string text)
        { LauncherLog.Write(root, text); }
        static int Helper(string action, string root, int parent)
        { return RunHelper(action, root, parent, true); }
        internal static int RunHelper(string action, string root, int parent, bool relaunch)
        {
            // This executable lives outside the replaceable payload. Parent exits first.
            try
            {
                using (var p = Process.GetProcessById(parent))
                    if (!p.WaitForExit(30000)) throw new IOException("The launcher did not close. No files were changed.");
            }
            catch (ArgumentException) { }
            bool success = false;
            using (var mutex = new Mutex(false, Core.MutexName(root)))
            {
                bool locked;
                try { locked = mutex.WaitOne(30000); } catch (AbandonedMutexException) { locked = true; }
                Core.Require(locked, "Another launcher is using this installation. No update was applied.");
                try
                {
                    Core.Recover(root);
                    if (action == "--rollback") Core.Rollback(root); else Core.InstallPending(root);
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"), action == "--rollback" ? "Previous release restored." : "Release installed successfully.");
                    success = true;
                }
                catch (Exception ex)
                {
                    Log(root, ex.ToString());
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"), "Update failed: " + ex.Message + " Check launcher.log before retrying.");
                    if (relaunch) MessageBox.Show(ex.Message + "\n\nKeep the .launcher folder for recovery. Details: .launcher\\launcher.log", "TimeSplitters — Update failed", MessageBoxButton.OK, MessageBoxImage.Error);
                }
                finally { mutex.ReleaseMutex(); }
            }
            string launcher = Core.PathIn(root, Core.LauncherName);
            if (relaunch && File.Exists(launcher)) Process.Start(new ProcessStartInfo(launcher) { WorkingDirectory = root, UseShellExecute = true });
            return success ? 0 : 1;
        }
    }

    public sealed class LauncherWindow
    {
        public readonly Window Window;
        readonly string root;
        readonly bool previewMode;
        readonly ReleaseClient client = new ReleaseClient();
        readonly CancellationTokenSource lifetime = new CancellationTokenSource();
        Release available;
        bool busy, checking, handingOff;
        bool validData, gameInstalled;
        string installed = "";
        T Find<T>(string name) where T : class { return Window.FindName(name) as T; }
        Button Button(string name) { return Find<Button>(name); }
        void Text(string name, string value) { Find<TextBlock>(name).Text = value; }
        ProgressBar Bar { get { return Find<ProgressBar>("OperationProgress"); } }
        ComboBox Channel { get { return Find<ComboBox>("ChannelPicker"); } }
        bool PreviewChannel { get { return Channel.SelectedIndex == 1; } }
        public void SetStatus(string value) { Text("StatusText", value); }
        public LauncherWindow(string path, bool preview)
        {
            root = path; previewMode = preview;
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.xaml"))
                Window = (Window)XamlReader.Load(stream);
            using (var icon = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher.ico"))
                Window.Icon = BitmapFrame.Create(icon, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
            Text("LauncherVersionLabel", "TSFP  /  COMMUNITY PC PROJECT  /  LAUNCHER v" + LauncherVersion.Value);
            using (var logo = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher-logo.png"))
            {
                if (logo != null)
                {
                    var bitmap = BitmapFrame.Create(logo, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
                    bitmap.Freeze();
                    Find<Image>("SidebarLogo").Source = bitmap;
                    // Display the original subtitle lettering at a readable sidebar size.
                    // Coordinates refer to the unchanged 500x187 source lockup.
                    var subtitle = new CroppedBitmap(bitmap, new Int32Rect(263, 150, 228, 22));
                    subtitle.Freeze();
                    Find<Image>("SidebarSubtitle").Source = subtitle;
                    Find<StackPanel>("SidebarBrand").Visibility = Visibility.Visible;
                    Find<StackPanel>("SidebarBrandFallback").Visibility = Visibility.Collapsed;
                }
            }
            using (var banner = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher-banner.png"))
            {
                if (banner != null)
                {
                    var bitmap = BitmapFrame.Create(banner, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
                    bitmap.Freeze();
                    var hero = Find<Border>("HeroBanner");
                    hero.Background = new ImageBrush(bitmap) { Stretch = Stretch.UniformToFill,
                        AlignmentX = AlignmentX.Center, AlignmentY = AlignmentY.Top };
                    hero.Visibility = Visibility.Visible;
                    Find<Grid>("HeroFallback").Visibility = Visibility.Collapsed;
                }
            }
            Refresh();
            if (previewMode)
            {
                Text("InstalledLabel", "v0.1.0-alpha.1");
                Text("ReadyTitle", "It's time to split.");
                Text("ReadyDescription", "Your game is installed and ready to launch.");
                Text("DataLabel", "●  Verified US Xbox data");
                Text("ReleaseTitle", "You're on the latest release.");
                Text("ReleaseDescription", "Your timeline is up to date. New releases appear here when published.");
                SetStatus("Ready when you are.");
                Button("PlayButton").IsEnabled = true;
                return;
            }
            var prefsPath = Core.PathIn(root, ".launcher/preferences.json");
            if (File.Exists(prefsPath))
            {
                try { Channel.SelectedIndex = Json.Flag(Json.Object(File.ReadAllText(prefsPath)), "preview") ? 1 : 0; }
                catch (Exception ex) { Program.Log(root, "Preferences: " + ex.Message); }
            }
            Channel.SelectionChanged += async (s, e) =>
            {
                try { Core.Write(prefsPath, Json.Encode(new { preview = PreviewChannel })); await Check(); }
                catch (Exception ex) { Fail(ex); }
            };
            Button("PlayButton").Click += async (s, e) => await Play();
            Button("ImportButton").Click += async (s, e) => await Import(false);
            Button("FolderButton").Click += async (s, e) => await Import(true);
            Button("CheckButton").Click += async (s, e) => await Check();
            Button("UpdateButton").Click += async (s, e) => await Update();
            Button("RollbackButton").Click += (s, e) =>
            {
                if (MessageBox.Show(Window, "Restore the previous installed release? Your game data, saves and settings will be kept.", "Restore previous release", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes) return;
                try { HandOff(true); } catch (Exception ex) { Fail(ex); }
            };
            Button("ReleaseNotesButton").Click += (s, e) => Open(available == null ? ReleaseClient.ReleasesPage : available.Url);
            Button("FilesNav").Click += (s, e) => Open(root);
            Button("SavesNav").Click += (s, e) => OpenFolder("Saves");
            Button("SupportNav").Click += (s, e) => Open("https://github.com/" + ReleaseClient.Repository + "/issues");
            Button("HomeNav").Click += (s, e) => SetStatus("Install a release, import your game once, and jump in.");
            Button("LogsButton").Click += (s, e) => OpenFolder(".launcher");
            Window.Loaded += async (s, e) =>
            {
                await Check();
                string result = Core.PathIn(root, ".launcher/last-result.txt");
                if (File.Exists(result)) { SetStatus(File.ReadAllText(result)); File.Delete(result); }
                string selfResult = Core.PathIn(root, ".launcher/self-last-result.txt");
                if (File.Exists(selfResult)) { SetStatus(File.ReadAllText(selfResult)); File.Delete(selfResult); }
            };
            Window.Closing += (s, e) =>
            {
                if (busy && !handingOff) { e.Cancel = true; SetStatus("Please let the current operation finish before closing the launcher."); }
            };
            Window.Closed += (s, e) => { lifetime.Cancel(); client.Dispose(); };
        }
        void Open(string path)
        {
            try { Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); }
            catch (Exception ex) { Fail(ex); }
        }
        void OpenFolder(string name)
        {
            try { string path = Core.PathIn(root, name); Directory.CreateDirectory(path); Open(path); }
            catch (Exception ex) { Fail(ex); }
        }
        void Fail(Exception ex)
        {
            if (lifetime.IsCancellationRequested) return;
            SetStatus(ex.Message);
            try { Program.Log(root, ex.ToString()); } catch { }
        }
        void Refresh()
        {
            try { installed = Core.InstalledVersion(root); }
            catch (Exception ex) { installed = ""; SetStatus("Installed manifest could not be read: " + ex.Message); }
            gameInstalled = File.Exists(Core.PathIn(root, "Bin/futureperfect_recomp.exe")) && File.Exists(Core.PathIn(root, "Launch.ps1"));
            string xbe = Core.PathIn(root, "Data/default.xbe");
            validData = File.Exists(xbe) && Directory.Exists(Core.PathIn(root, "Data/pak")) && Directory.Exists(Core.PathIn(root, "Data/xmv")) && Core.Hash(xbe) == TsfpImport.ExpectedHash;
            Text("InstalledLabel", installed.Length > 0 ? installed : gameInstalled ? "UNVERSIONED" : "NOT INSTALLED");
            Text("ReadyTitle", gameInstalled && validData ? "It's time to split." : gameInstalled ? "One last jump: your ISO." : "Prepare your next jump");
            Text("ReadyDescription", gameInstalled && validData ? "Your game is installed and ready to launch." : gameInstalled ? "Import your US Xbox game once to start playing." : "Install a release, then import your US Xbox ISO.");
            bool dataExists = Directory.Exists(Core.PathIn(root, "Data"));
            if (dataExists && !validData)
            {
                Text("ReadyTitle", "Check your game data.");
                Text("ReadyDescription", "Existing Data is incomplete or unsupported. Open Game files to inspect it; your files were kept.");
            }
            Text("DataLabel", validData ? "●  Verified US Xbox data" : dataExists ? "○  Data needs attention" : "○  ISO required");
            Button("ImportButton").IsEnabled = Button("FolderButton").IsEnabled = !busy && !dataExists;
            Button("PlayButton").IsEnabled = !busy && gameInstalled && validData && !Core.GameRunning();
            Button("CheckButton").IsEnabled = !busy && !checking;
            Channel.IsEnabled = !busy && !checking;
            Button("UpdateButton").IsEnabled = !busy && !checking && CanUpdate();
            Button("UpdateButton").Content = gameInstalled ? "Update release" : "Install release";
            Button("RollbackButton").IsEnabled = !busy && !checking && Core.CanRollback(root) && !Core.GameRunning();
            Text("ChannelBadge", PreviewChannel ? "●  PREVIEW CHANNEL" : "●  STABLE CHANNEL");
        }
        bool CanUpdate()
        {
            if (available == null) return false;
            if (!gameInstalled || installed.Length == 0) return true;
            try { return GameVersion.Parse(available.Tag).CompareTo(GameVersion.Parse(installed)) > 0; }
            catch (InvalidDataException) { return true; }
        }
        async Task Check()
        {
            if (busy || checking) return;
            checking = true; available = null;
            Refresh();
            Text("ReleaseTitle", "Checking the timeline…");
            Text("ReleaseDescription", "Looking for published " + (PreviewChannel ? "preview or stable" : "stable") + " releases on GitHub.");
            SetStatus("Checking GitHub Releases…");
            using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token))
            {
                timeout.CancelAfter(TimeSpan.FromSeconds(25));
                try
                {
                    available = await client.Check(PreviewChannel, timeout.Token);
                    if (available == null)
                    {
                        Text("ReleaseTitle", PreviewChannel ? "No compatible release yet." : "No stable release yet.");
                        Text("ReleaseDescription", PreviewChannel ? "A published Windows ZIP with a checksum will appear here." : "Stable excludes test builds. Choose Preview to check published prereleases.");
                        SetStatus(gameInstalled ? "Your installed game is available offline." : "No compatible published release was found for this channel.");
                    }
                    else
                    {
                        bool newer = CanUpdate();
                        Text("ReleaseTitle", newer ? available.Tag + " is ready." : "You're on the latest release.");
                        Text("ReleaseDescription", newer ? "Published on GitHub · " + (available.Size / 1048576.0).ToString("0.0") + " MB\n" + (gameInstalled ? "Ready to update when you are." : "Install the game files, then bring your ISO.") : "Your timeline is up to date. Check release notes for details.");
                        SetStatus(newer ? "Release available. Select " + (gameInstalled ? "Update" : "Install") + " to download and verify." : "Ready when you are.");
                    }
                }
                catch (OperationCanceledException) { if (!lifetime.IsCancellationRequested) { Text("ReleaseTitle", "Release check timed out."); Text("ReleaseDescription", "Your installed game is available offline. Check again when connected."); SetStatus("GitHub did not respond in time. You can retry the release check."); } }
                catch (Exception ex) { Text("ReleaseTitle", "Release uplink unavailable."); Text("ReleaseDescription", "Your installed game is available offline. Try checking again later."); Fail(ex); }
            }
            checking = false; Refresh();
        }
        async Task Import(bool folder)
        {
            if (busy) return;
            string source;
            if (folder)
            {
                using (var picker = new System.Windows.Forms.FolderBrowserDialog { Description = "Choose your extracted US Xbox game folder (default.xbe, pak, xmv)." })
                {
                    if (picker.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                    source = picker.SelectedPath;
                }
            }
            else
            {
                var picker = new Microsoft.Win32.OpenFileDialog { Title = "Locate your TimeSplitters: Future Perfect US Xbox ISO", Filter = "Xbox disc image (*.iso)|*.iso", CheckFileExists = true };
                if (picker.ShowDialog(Window) != true) return;
                source = picker.FileName;
            }
            busy = true; Refresh(); Bar.IsIndeterminate = true;
            SetStatus("Validating and importing game data. This can take a few minutes…");
            try
            {
                await Task.Run(() =>
                {
                    string dest = Core.PathIn(root, "Data");
                    if (folder) TsfpImport.ImportFolder(source, dest); else TsfpImport.ImportIso(source, dest);
                });
                SetStatus("Game data imported. Your ISO is no longer needed to play.");
                Program.Log(root, "Game data imported successfully.");
            }
            catch (Exception ex) { Fail(ex); }
            finally { busy = false; Bar.IsIndeterminate = false; Refresh(); }
        }
        async Task Update()
        {
            if (busy || !CanUpdate()) return;
            if (Core.GameRunning()) { SetStatus("Close the game before installing a release."); return; }
            busy = true; Refresh();
            try
            {
                Core.CheckSpace(root, available.Size * 4 + 256L * 1024 * 1024);
                Core.RemoveOwned(root, ".launcher/pending");
                string pending = Core.PathIn(root, ".launcher/pending"); Directory.CreateDirectory(pending);
                string archive = Path.Combine(pending, "release.zip");
                SetStatus("Downloading " + available.Tag + "…");
                var progress = new Progress<double>(value => { Bar.Value = value; SetStatus("Downloading " + available.Tag + " · " + value.ToString("0") + "%"); });
                await client.Download(available, archive, progress, lifetime.Token);
                Bar.IsIndeterminate = true;
                SetStatus("Verifying and preparing the release…");
                string tag = available.Tag;
                await Task.Run(() => Core.Unpack(archive, Path.Combine(pending, "payload"), tag));
                Core.Write(Path.Combine(pending, "release.json"), Json.Encode(new { version = tag }));
                // Only payload and one previous release are retained, not the downloaded ZIP.
                File.Delete(archive);
                Program.Log(root, "Prepared " + tag + "; handing off to updater.");
                HandOff(false);
            }
            catch (Exception ex) { Fail(ex); }
            finally { if (!handingOff) { busy = false; Bar.IsIndeterminate = false; Bar.Value = 0; Refresh(); } }
        }
        void HandOff(bool rollback)
        {
            Core.Require(!Core.GameRunning(), "Close the game before changing the installed release.");
            string helper = Core.PathIn(root, ".launcher/update-helper.exe");
            Directory.CreateDirectory(Path.GetDirectoryName(helper));
            File.Copy(Assembly.GetExecutingAssembly().Location, helper, true);
            var info = new ProcessStartInfo(helper, (rollback ? "--rollback " : "--apply ") + Quote(root) + " " + Process.GetCurrentProcess().Id) { UseShellExecute = true, WorkingDirectory = root };
            Process.Start(info);
            handingOff = true; Window.Close();
        }
        public static string Quote(string value)
        { return SelfUpdate.Quote(value); }
        internal static bool GameWindowReady(string root)
        {
            string executable = Core.PathIn(root, "Bin/futureperfect_recomp.exe");
            bool ready = false;
            foreach (var game in Process.GetProcessesByName("futureperfect_recomp"))
            {
                using (game)
                {
                    try
                    {
                        if (!game.HasExited && game.MainWindowHandle != IntPtr.Zero &&
                            String.Equals(game.MainModule.FileName, executable, StringComparison.OrdinalIgnoreCase))
                            ready = true;
                    }
                    catch (InvalidOperationException) { } // Process exited during observation.
                    catch (System.ComponentModel.Win32Exception) { } // Unrelated/inaccessible process.
                }
            }
            return ready;
        }
        internal async Task MonitorGame(Process process)
        {
            bool minimizedForGame = false;
            WindowState previousState = Window.WindowState;
            try
            {
                SetStatus("Starting game…");
                while (!process.HasExited)
                {
                    if (!minimizedForGame && GameWindowReady(root))
                    {
                        Window.WindowState = WindowState.Minimized;
                        minimizedForGame = true;
                        SetStatus("Game running. Have a good jump.");
                    }
                    await Task.Delay(200);
                }
                await Task.Run(() => process.WaitForExit());
            }
            finally
            {
                if (minimizedForGame) { Window.WindowState = previousState; Window.Activate(); }
            }
        }
        async Task Play()
        {
            if (busy) return;
            busy = true; Refresh();
            try
            {
                Core.Require(validData && gameInstalled, "Install the release and import the supported US Xbox data first.");
                Core.Require(!Core.GameRunning(), "The game is already running.");
                string shell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "WindowsPowerShell", "v1.0", "powershell.exe");
                using (var process = new Process())
                {
                    process.StartInfo = new ProcessStartInfo(shell, "-NoProfile -STA -ExecutionPolicy Bypass -File " + Quote(Core.PathIn(root, "Launch.ps1"))) {
                        WorkingDirectory = root, UseShellExecute = false, CreateNoWindow = true, RedirectStandardError = true, RedirectStandardOutput = true };
                    // Isolate inherited developer/test environment overrides.
                    var keys = new System.Collections.Generic.List<string>();
                    foreach (string key in process.StartInfo.EnvironmentVariables.Keys)
                        if (key.StartsWith("RECOMP_", StringComparison.OrdinalIgnoreCase) || key.StartsWith("TSFP_", StringComparison.OrdinalIgnoreCase)) keys.Add(key);
                    foreach (string key in keys) process.StartInfo.EnvironmentVariables.Remove(key);
                    process.OutputDataReceived += (s, e) => { if (e.Data != null) Program.Log(root, e.Data); };
                    process.ErrorDataReceived += (s, e) => { if (e.Data != null) Program.Log(root, e.Data); };
                    process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine();
                    await MonitorGame(process);
                    SetStatus(process.ExitCode == 0 ? "Welcome back. Ready for another jump?" : "Game launch failed. View logs for details.");
                }
            }
            catch (Exception ex) { Fail(ex); }
            finally { busy = false; Refresh(); }
        }
    }
}
