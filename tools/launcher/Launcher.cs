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
    internal enum UpdatePresentationState { Checking, Ready, Available, Downloading, Verifying, Paused, Error, Empty }
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
                bool repairPreview = args.Length == 2 && args[0] == "--render-repair-preview";
                bool diagnosticsPreview = args.Length == 2 && args[0] == "--render-diagnostics-preview";
                bool newsPreview = args.Length == 2 && args[0] == "--render-news-preview";
                bool versionsPreview = args.Length == 2 && args[0] == "--render-versions-preview";
                bool graphicsPreview = args.Length == 2 && args[0] == "--render-settings-graphics";
                bool discordPreview = args.Length == 2 && args[0] == "--render-settings-discord";
                bool settingsPreview = discordPreview || graphicsPreview || (args.Length == 2 && args[0] == "--render-settings-preview");
                bool preview = versionsPreview || newsPreview || repairPreview || diagnosticsPreview || settingsPreview || (args.Length == 2 && args[0] == "--render-preview");
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
                        if(!preview) TexturePack.Recover(root);
                        var app = new Application();
                        var ui = new LauncherWindow(root, preview);
                        if (preview)
                        {
                            if (settingsPreview) ui.ShowSettings(discordPreview?2:graphicsPreview?1:0);
                            if (repairPreview || diagnosticsPreview) ui.ShowMaintenance(diagnosticsPreview);
                            if (newsPreview) ui.ShowNews();
                            if (versionsPreview) ui.ShowGameVersions();
                            var window = ui.Window;
                            var visual = (Panel)window.Content;
                            visual.Background = window.Background;
                            int width = 1152, height = 772;
                            visual.Measure(new Size(width, height)); visual.Arrange(new Rect(0, 0, width, height)); visual.UpdateLayout();
                            var bitmap = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
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
            return UpdateRestart.ApplyGame(root, parent, action == "--rollback",
                () => { if (relaunch) Process.Start(LauncherPlatform.RestartInfo(root)); },
                ex => { if (relaunch) MessageBox.Show(ex.Message + "\n\nKeep the .launcher folder for recovery. Details: .launcher\\launcher.log",
                    "TimeSplitters — Update failed", MessageBoxButton.OK, MessageBoxImage.Error); });
        }
    }

    public sealed class LauncherWindow
    {
        public readonly Window Window;
        readonly string root;
        readonly bool previewMode;
        readonly ReleaseClient client;
        readonly CancellationTokenSource lifetime = new CancellationTokenSource();
        Release available;
        SettingsPage settings;
        SavesPage savesPage;
        MaintenancePage repairPage, diagnosticsPage;
        MaintenanceArea maintenanceArea;
        WhatsNewPage newsPage;
        GameVersionsPage versionsPage;
        CancellationTokenSource versionsStop;
        int nextVersionsPage = 1;
        CancellationTokenSource newsStop;
        CancellationTokenSource issuesStop;
        DateTime issuesNextCheckUtc;
        System.Windows.Threading.DispatcherTimer issuesTimer;
        string newsContext="";
        bool newsReady;
        VerificationResult gameVerification, launcherVerification;
        bool diagnosticsReady;
        bool pendingCrash;
        bool gameMonitoring;
        System.Windows.Threading.DispatcherTimer sessionTimer;
        CancellationTokenSource maintenanceStop, updateStop;
        Func<Task> retryOperation;
        bool closeAfterTransfer;
        bool busy, checking, handingOff;
        bool validData, gameInstalled;
        string installed = "";
        bool? displayedDataVerified;
        UpdatePresentationState? updatePresentation;
        T Find<T>(string name) where T : class { return Window.FindName(name) as T; }
        Button Button(string name) { return Find<Button>(name); }
        void Text(string name, string value) { Find<TextBlock>(name).Text = value; }
        ProgressBar Bar { get { return Find<ProgressBar>("OperationProgress"); } }
        readonly GameChannelPickerPage channelPicker=new GameChannelPickerPage();
        string SelectedGameChannel { get { return channelPicker.Selected; } }
        bool PreviewChannel { get { return SelectedGameChannel!="stable"; } }
        void SetChannels(Release[] releases,string selected)
        {channelPicker.SetReleases(releases,selected);}
        string NewsGameVersion { get { return installed.Length>0?installed:gameInstalled?"Unversioned":""; } }
        public void SetStatus(string value) { Text("StatusText", value); }
        public LauncherWindow(string path, bool preview)
            : this(path,preview,new ReleaseClient()) { }
        internal LauncherWindow(string path,bool preview,ReleaseClient releases)
        {
            root = path; previewMode = preview; client = releases;
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.xaml"))
                Window = (Window)XamlReader.Load(stream);
            Window.Resources.MergedDictionaries.Add(PageTheme.Resources());
            Find<ContentControl>("GameChannelPickerHost").Content=channelPicker.View;
            channelPicker.SetReleases(new Release[0],GameChannels.Read(root),GameChannels.ReadPreviewTrack(root));
            ShowUpdateState(UpdatePresentationState.Checking);
            Find<Grid>("CommandCenter").SizeChanged+=(s,e)=> {
                // Keep the action cards reachable at the minimum window height.
                Find<Grid>("CommandCenter").RowDefinitions[2].Height=new GridLength(Math.Max(144,Math.Min(228,e.NewSize.Height-460)));
            };
            WindowTheme.Attach(Window);
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
                    Find<Image>("HeroTitle").Source = bitmap;
                    Find<Image>("HeroTitle").Visibility = Visibility.Visible;
                    Find<StackPanel>("HeroTitleFallback").Visibility = Visibility.Collapsed;
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
                    // Keep the complete supplied photograph. The surrounding WPF
                    // composition fills the wide header without cropping or redrawing it.
                    Find<Image>("HeroPhoto").Source = bitmap;
                    hero.Visibility = Visibility.Visible;
                    Find<Grid>("HeroFallback").Visibility = Visibility.Collapsed;
                }
            }
            using (var backdrop = Assembly.GetExecutingAssembly().GetManifestResourceStream("launcher-backdrop.png"))
            {
                if (backdrop != null)
                {
                    var bitmap = BitmapFrame.Create(backdrop, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
                    bitmap.Freeze();
                    Find<Image>("HeroBackdrop").Source = bitmap;
                }
            }
            Refresh();
            Button("SettingsNav").Click += (s,e)=>ShowSettings();
            Button("HomeNav").Click += (s,e)=>ShowHome();
            Button("RepairNav").Click += (s,e)=>ShowMaintenance(false);
            Button("NewsNav").Click += (s,e)=>ShowNews();
            Button("ReleaseNotesButton").Content="What's new";
            Button("ReleaseNotesButton").Click += (s,e)=>ShowNews();
            Button("GameVersionsButton").Click += (s,e)=>ShowGameVersions();
            Button("PauseDownloadButton").Click+=(s,e)=> {if(updateStop!=null)updateStop.Cancel();};
            Button("RetryButton").Click+=async(s,e)=> {if(!busy && !checking && retryOperation!=null)await retryOperation();};
            Button("RecoveryRepairButton").Click+=(s,e)=>ShowMaintenance(false);
            Button("RecoveryLogsButton").Click+=(s,e)=> {if(!previewMode)OpenFolder(".launcher");};
            Button("OpenDataButton").Click+=(s,e)=> {if(!previewMode)OpenFolder("Data");};
            Window.Closed += (s,e)=> { if(sessionTimer!=null)sessionTimer.Stop();if(issuesTimer!=null)issuesTimer.Stop();lifetime.Cancel(); client.Dispose(); };
            Window.Activated+=async(s,e)=>{if(KnownIssuesVisible())await RefreshKnownIssues(false);};
            if (previewMode)
            {
                SetChannels(new[]{new Release {Tag="v0.4.0-story-coop.1",Channel="story-coop",ChannelName="Story Co-op Preview"}},"story-coop");
                Text("InstalledLabel", "v0.4.0-story-coop.1 (sample)");
                Text("ChannelBadge", "\u25cf  PREVIEW CHANNEL");
                Text("ReadyTitle", "Ready to play");
                Text("ReadyDescription", "Your game is installed and ready to launch.");
                Text("DataLabel", "●  Verified US Xbox data");
                SetGameDataPresentation(true,true);
                Text("ReleaseTitle", "You're on the latest release.");
                Text("ReleaseDescription", "The installed game is up to date. New releases appear here when published.");
                SetStatus("Ready.");
                Button("PlayButton").IsEnabled = true;
                ShowUpdateState(UpdatePresentationState.Ready);
                SetHomePrimaryAction("PlayButton");
                return;
            }
            SetChannels(new Release[0],GameChannels.Read(root));
            channelPicker.Changed += async () =>
            {
                if(previewMode)return;
                try { GameChannels.Save(root,SelectedGameChannel,channelPicker.LastPreview); await Check(); }
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
            Button("SavesNav").Click += (s, e) => ShowSaves();
            Button("SupportNav").Click += (s, e) => Open("https://github.com/" + ReleaseClient.Repository + "/issues");
            Button("LogsButton").Click += (s, e) => OpenFolder(".launcher");
            Window.Loaded += async (s, e) =>
            {
                SelfUpdate.CleanupAfterStartup(root);
                busy = true; Refresh();
                try { await Task.Run(() => Core.CleanupGameAfterStartup(root)); }
                finally { busy = false; }
                await Check();
                string result = Core.PathIn(root, ".launcher/last-result.txt");
                if (File.Exists(result)) { SetStatus(File.ReadAllText(result)); File.Delete(result); }
                string selfResult = Core.PathIn(root, ".launcher/self-last-result.txt");
                if (File.Exists(selfResult)) { SetStatus(File.ReadAllText(selfResult)); File.Delete(selfResult); }
                RecoverGameSession();
                // An earlier launch can outlive a terminated launcher. Defer
                // inference while its exact process identity is still alive.
                sessionTimer=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromSeconds(2)};
                sessionTimer.Tick+=(sender,args)=> {if(!busy && !checking && !gameMonitoring)RecoverGameSession();};sessionTimer.Start();
            };
            Window.Closing += (s, e) =>
            {
                if (busy && !handingOff) {
                    e.Cancel=true;
                    if(updateStop!=null){closeAfterTransfer=true;updateStop.Cancel();SetStatus("Pausing download before closing…");}
                    else SetStatus("Please let the current operation finish before closing the launcher.");
                }
            };
        }
        public void ShowSettings(int tab=-1)
        {
            if(busy || (!previewMode && Core.GameRunning())) { SetStatus("Close the game before editing settings."); return; }
            try {
                if(settings==null) {
                    settings=new SettingsPage(Window.Resources,root,previewMode);
                    settings.TextureBusyChanged+=value=> {busy=value;Refresh();};
                    settings.Saved+=()=> { SetStatus("Settings saved."); Refresh(); };
                    settings.Cancelled+=()=> { ShowHome(); Find<ContentControl>("SettingsHost").Content=null; settings=null; SetStatus("Unsaved settings discarded."); };
                    Find<ContentControl>("SettingsHost").Content=settings.View;
                }
                if(tab>=0)settings.SelectTab(tab);
                Find<ContentControl>("SavesHost").Visibility=Visibility.Collapsed;Find<Grid>("CommandCenter").Visibility=Visibility.Collapsed;
                Find<ContentControl>("SettingsHost").Visibility=Visibility.Visible;
                Find<ContentControl>("MaintenanceHost").Visibility=Visibility.Collapsed;
                Find<ContentControl>("NewsHost").Visibility=Visibility.Collapsed;
                Find<ContentControl>("VersionsHost").Visibility=Visibility.Collapsed;
                SelectNavigation("SettingsNav");
                settings.CheckTexturesIfOptedIn();
            } catch(Exception ex) { Fail(ex); }
        }
        void ShowHome()
        {
            Find<ContentControl>("SavesHost").Visibility=Visibility.Collapsed;
            if(settings!=null)settings.DismissPrompt();
            Find<ContentControl>("SettingsHost").Visibility=Visibility.Collapsed;
            Find<Grid>("CommandCenter").Visibility=Visibility.Visible;
            Find<ContentControl>("MaintenanceHost").Visibility=Visibility.Collapsed;
            Find<ContentControl>("NewsHost").Visibility=Visibility.Collapsed;
            Find<ContentControl>("VersionsHost").Visibility=Visibility.Collapsed;
            SelectNavigation("HomeNav");
        }
        public async void ShowGameVersions()
        {
            if(busy)return;
            try {
                if(settings!=null)settings.DismissPrompt();
                if(versionsPage==null) {
                    versionsPage=new GameVersionsPage(url=>{if(!previewMode)Open(url);});Find<ContentControl>("VersionsHost").Content=versionsPage.View;
                    versionsPage.Refresh.Click+=async(s,e)=>await RefreshGameVersions(true);
                    versionsPage.Older.Click+=async(s,e)=>await RefreshGameVersions(false);
                    versionsPage.Install.Click+=async(s,e)=>await InstallSelectedVersion();
                    versionsPage.OnGitHub.Click+=(s,e)=> {if(!previewMode && versionsPage.Selected!=null)Open(versionsPage.Selected.Url);};
                }
                Find<ContentControl>("SavesHost").Visibility=Visibility.Collapsed;Find<Grid>("CommandCenter").Visibility=Visibility.Collapsed;
                foreach(string host in new[]{"SettingsHost","MaintenanceHost","NewsHost"})Find<ContentControl>(host).Visibility=Visibility.Collapsed;
                Find<ContentControl>("VersionsHost").Visibility=Visibility.Visible;SelectNavigation("HomeNav");
                versionsPage.SetInstalled(previewMode?"v0.2.0-alpha.5":NewsGameVersion);
                versionsPage.SetBusy(busy || checking,Core.GameRunning());
                if(previewMode) {
                    versionsPage.SetReleases(new GameReleaseList {Releases=new[]{
                        new Release {Tag="v0.2.0-alpha.6",IsPreview=true,Published="2026-10-17T12:00:00Z",Size=45000000,Notes="Interface sample.\n\nCompare a published game release with the version reported by a player."},
                        new Release {Tag="v0.2.0-alpha.5",IsPreview=true,Published="2026-10-16T12:00:00Z",Size=44000000,Notes="Interface sample.\n\nEarlier game release for bug reproduction.\nSaves, settings, imported data and launcher files are preserved."}},HasMore=true},true);
                } else if(nextVersionsPage==1 && versionsStop==null)await RefreshGameVersions(true);
            }catch(Exception ex){Fail(ex);}
        }
        async Task RefreshGameVersions(bool reset)
        {
            if(busy || versionsStop!=null)return;
            if(previewMode){versionsPage.Status("Preview only. No network request was made.");return;}
            using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)) {
                versionsStop=timeout;timeout.CancelAfter(TimeSpan.FromSeconds(25));versionsPage.SetLoading(true);
                int page=reset?1:nextVersionsPage;
                try {
                    var result=await client.ReadGameVersions(page,timeout.Token);
                    if(!lifetime.IsCancellationRequested){versionsPage.SetReleases(result,reset);nextVersionsPage=page+1;}
                }catch(Exception ex) {
                    if(!lifetime.IsCancellationRequested){versionsPage.Failed();Program.Log(root,"Game versions: "+ex.GetType().Name);}
                }finally {versionsStop=null;}
            }
        }
        async Task InstallSelectedVersion()
        {
            if(busy || checking || versionsPage==null || versionsPage.Selected==null)return;
            if(previewMode){versionsPage.Status("Preview only. No installed files were changed.");return;}
            if(Core.GameRunning()){versionsPage.Status("Close the game before switching versions.");Refresh();return;}
            var release=versionsPage.Selected;
            string current=gameInstalled?(installed.Length>0?installed:"an unversioned development build"):"no installed game";
            if(MessageBox.Show(Window,"Install "+release.Tag+"?\n\nCurrently: "+current+".\nThis replaces the installed game runtime. Your launcher, saves, settings, ISO and imported game data will be kept.\n\nOlder versions may read saves or settings differently. Back up saves before comparing builds.","Change game version",MessageBoxButton.YesNo,MessageBoxImage.Question,MessageBoxResult.No)!=MessageBoxResult.Yes)return;
            ShowHome();await DownloadGame(release,true);
        }
        void ShowSaves()
        {
            if(busy || (!previewMode && Core.GameRunning())) { SetStatus("Close the game before managing saves.");return; }
            if(settings!=null)settings.DismissPrompt();
            if(savesPage==null) {
                savesPage=new SavesPage(root,previewMode);
                savesPage.OpenFolder+=relative=>OpenFolder(relative);
                savesPage.BusyChanged+=value=>{busy=value;Refresh();};
                Find<ContentControl>("SavesHost").Content=savesPage.View;
            }
            savesPage.Reload();Find<Grid>("CommandCenter").Visibility=Visibility.Collapsed;
            foreach(string host in new[]{"SettingsHost","MaintenanceHost","NewsHost","VersionsHost"})Find<ContentControl>(host).Visibility=Visibility.Collapsed;
            Find<ContentControl>("SavesHost").Visibility=Visibility.Visible;SelectNavigation("SavesNav");
        }
        void SelectNavigation(string active)
        {
            foreach(string name in new[]{"HomeNav","NewsNav","SettingsNav","RepairNav","SavesNav"}) {
                bool selected=name==active;
                Button(name).Background=selected?new SolidColorBrush(Color.FromRgb(29,58,85)):Brushes.Transparent;
                Button(name).Foreground=new SolidColorBrush(selected?Color.FromRgb(134,205,255):Color.FromRgb(223,233,242));
            }
        }
        public async void ShowNews()
        {
            if(busy)return;
            if(settings!=null)settings.DismissPrompt();
            if(newsPage==null) {
                newsPage=new WhatsNewPage(url=>{if(!previewMode)Open(url);});Find<ContentControl>("NewsHost").Content=newsPage.View;
                newsPage.Refresh.Click+=async(s,e)=>await RefreshNews();
                newsPage.RefreshIssues.Click+=async(s,e)=>await RefreshKnownIssues(true);
                newsPage.OnGitHub.Click+=(s,e)=> {if(!previewMode && newsPage.SelectedUrl!=null)Open(newsPage.SelectedUrl);};
                if(!previewMode) {
                    issuesTimer=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromMinutes(5)};
                    issuesTimer.Tick+=async(s,e)=>{if(KnownIssuesVisible())await RefreshKnownIssues(false);};
                    issuesTimer.Start();
                }
            }
            Find<ContentControl>("SavesHost").Visibility=Visibility.Collapsed;Find<Grid>("CommandCenter").Visibility=Visibility.Collapsed;
            Find<ContentControl>("SettingsHost").Visibility=Visibility.Collapsed;
            Find<ContentControl>("MaintenanceHost").Visibility=Visibility.Collapsed;
            Find<ContentControl>("NewsHost").Visibility=Visibility.Visible;
            Find<ContentControl>("VersionsHost").Visibility=Visibility.Collapsed;SelectNavigation("NewsNav");
            string context=NewsGameVersion+"/"+LauncherVersion.Value+"/"+SelectedGameChannel;
            if(newsContext!=context) {if(newsStop!=null){newsStop.Cancel();newsStop=null;}newsReady=false;newsContext=context;newsPage.Reset();}
            newsPage.SetVersions(NewsGameVersion,LauncherVersion.Value,SelectedGameChannel);
            if(previewMode) {
                newsPage.SetVersions("v1.0.0",LauncherVersion.Value,false);
                newsPage.SetNews(new ReleaseNews {CheckedUtc=DateTime.UtcNow,Game=new Release {Tag="v1.1.0",Name="Game update (interface sample)",Published="2026-10-17T12:00:00Z",Url=ReleaseClient.ReleasesPage,
                    Notes="## Highlights\n- Updated display settings and resolution selection.\n- Improved update recovery and installation checks.\n\n## Known issues\n- Sample limitation: some effects are still being investigated.\n\n## Installation\nUpdate from Home. Your saves, settings and imported game data are preserved."},
                    InstalledGame=new Release {Tag="v1.0.0",Notes="Initial release notes (interface sample).",Url=ReleaseClient.ReleasesPage},
                    Launcher=new Release {Tag=LauncherVersion.Value,Name="Launcher update (interface sample)",Url=ReleaseClient.ReleasesPage,Notes="## Improvements\n- Resumable downloads with speed and time remaining.\n- Clearer recovery actions."},
                    InstalledLauncher=new Release {Tag=LauncherVersion.Value,Notes="Installed launcher notes (interface sample).",Url=ReleaseClient.ReleasesPage}});
                newsPage.SetIssues(new KnownIssues {CheckedUtc=DateTime.UtcNow,Items=new[]{
                    new KnownIssue {Number=101,Title="Sample report: an effect needs investigation (preview only)"},
                    new KnownIssue {Number=102,Title="Sample report: a menu layout needs adjustment (preview only)"}}});
                return;
            }
            await Task.WhenAll(RefreshKnownIssues(false),!newsReady && newsStop==null?RefreshNews():Task.FromResult(0));
        }
        bool KnownIssuesVisible()
        { return !previewMode && !busy && newsPage!=null && Window.IsVisible && Window.WindowState!=WindowState.Minimized && Find<ContentControl>("NewsHost").Visibility==Visibility.Visible; }
        internal async Task RefreshKnownIssues(bool force)
        {
            if(previewMode || newsPage==null || lifetime.IsCancellationRequested || issuesStop!=null || (!force && DateTime.UtcNow<issuesNextCheckUtc))return;
            using(var cancel=CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)) {
                issuesStop=cancel;issuesNextCheckUtc=DateTime.UtcNow.AddMinutes(5);newsPage.SetIssuesLoading();
                if(issuesTimer!=null){issuesTimer.Stop();issuesTimer.Start();}
                try {
                    var result=await client.ReadKnownIssues(cancel.Token);
                    if(!lifetime.IsCancellationRequested)newsPage.SetIssues(result);
                }catch(Exception ex) {
                    if(!lifetime.IsCancellationRequested){newsPage.IssuesFailed();Program.Log(root,"Known issues: "+ex.GetType().Name);}
                }finally {issuesStop=null;}
            }
        }
        async Task RefreshNews()
        {
            if(previewMode){ShowNews();return;}
            if(newsStop!=null)return;
            using(var timeout=CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)) {
                newsStop=timeout;timeout.CancelAfter(TimeSpan.FromSeconds(25));newsPage.SetLoading(true);
                string context=newsContext;
                try {
                    var notes=await client.ReadNews(SelectedGameChannel,NewsGameVersion,LauncherVersion.Value,timeout.Token);
                    if(!lifetime.IsCancellationRequested && !timeout.IsCancellationRequested && Object.ReferenceEquals(newsStop,timeout) && context==newsContext){newsPage.SetNews(notes);newsReady=true;}
                }catch(Exception ex) {
                    if(!lifetime.IsCancellationRequested && Object.ReferenceEquals(newsStop,timeout) && context==newsContext){newsPage.Failed();Program.Log(root,"Release notes: "+ex.GetType().Name);}
                }finally {if(Object.ReferenceEquals(newsStop,timeout))newsStop=null;}
            }
        }
        MaintenancePage CreateMaintenancePage(bool diagnostics)
        {
            var page=new MaintenancePage(diagnostics,true);
            page.Retry.Click+=async(s,e)=> {if(!busy && page.RetryOperation!=null)await page.RetryOperation();};
            page.ViewLogs.Click+=(s,e)=> {if(previewMode)page.Status("Preview: opens the local launcher log folder.");else OpenFolder(".launcher");};
            page.Cancel.Click+=(s,e)=> {if(maintenanceStop!=null)maintenanceStop.Cancel();page.Status("Cancellation requested; finishing the current file…");};
            if(diagnostics) {
                diagnosticsPage=page;
                page.OpenCrash.Click+=(s,e)=> {
                    if(previewMode){page.Status("Preview: opens the latest local crash report.");return;}
                    try { string name=CrashReports.Latest(root);if(name==null){page.Status("No completed crash report found.");return;}Open(CrashReports.Folder(root,name)); }
                    catch(Exception ex){page.Status("Could not open the crash report ("+ex.GetType().Name+"). Files retained.");}
                };
                page.ExportCrash.Click+=(s,e)=> {
                    if(previewMode){page.Status("Preview: saves a local crash report ZIP. Nothing is uploaded.");return;}
                    try {
                        string name=CrashReports.Latest(root);if(name==null){page.Status("No completed crash report found.");return;}
                        var dialog=new Microsoft.Win32.SaveFileDialog {Filter="ZIP archive (*.zip)|*.zip",FileName=name+".zip",Title="Save crash report - may contain personal information"};
                        if(dialog.ShowDialog(Window)==true){CrashReports.Export(root,name,dialog.FileName);page.Status("Crash report exported locally. Review the dump and log before sharing. Nothing was uploaded.");}
                    } catch(Exception ex){page.Status("Could not export the crash report ("+ex.GetType().Name+"). Choose a new filename and try again.");}
                };
                page.Refresh.Click+=async(s,e)=>await RefreshDiagnostics();
                page.ReportBug.Click+=(s,e)=>OpenDiagnosticIssue(false);
                page.RequestEnhancement.Click+=(s,e)=>OpenDiagnosticIssue(true);
                page.Copy.Click+=(s,e)=> {try{Clipboard.SetText(page.Report.Text);page.Status("Diagnostic report copied. Nothing was uploaded.");}catch(Exception ex){page.Status("Clipboard unavailable ("+ex.GetType().Name+"). Select and copy the report text instead.");}};
            } else {
                repairPage=page;
                page.OpenFiles.Click+=(s,e)=> {if(previewMode)page.Status("Preview: opens the game installation folder.");else Open(root);};
                page.VerifyGame.Click+=async(s,e)=>await VerifyFiles(false);
                page.VerifyLauncher.Click+=async(s,e)=>await VerifyFiles(true);
                page.RepairGame.Click+=async(s,e)=>await RepairFiles(false);
                page.RepairLauncher.Click+=async(s,e)=>await RepairFiles(true);
                page.SetVerification(gameVerification,launcherVerification);
            }
            return page;
        }
        async Task EnsureDiagnostics()
        {
            if(diagnosticsReady)return;
            if(gameMonitoring) { await RefreshDiagnostics();return; }
            if(previewMode) {diagnosticsPage.Report.Text="TimeSplitters launcher diagnostics — interface preview\n\nRunning launcher: "+LauncherVersion.Value+"\nInstalled game: v0.2.0-alpha.5 (sample)\nWindows: 64-bit\nGraphics: available in the installed launcher\nEmbedded artwork: banner present, logo present\n\nLast verification: not checked this session.\n\nRecent launcher errors: none in this preview.\n\nReport excludes raw logs, personal paths, saves and ISO information. Nothing is uploaded automatically.";diagnosticsReady=true;}
            else if(!diagnosticsReady && !busy)await RefreshDiagnostics();
        }
        public async void ShowMaintenance(bool diagnostics)
        {
            if(busy && !gameMonitoring)return;
            try {
                if(settings!=null)settings.DismissPrompt();
                if(maintenanceArea==null) {
                    maintenanceArea=new MaintenanceArea(CreateMaintenancePage(false),CreateMaintenancePage(true));
                    maintenanceArea.Selected+=async selected=> {try{if(selected)await EnsureDiagnostics();}catch(Exception ex){Fail(ex);}};
                    Find<ContentControl>("MaintenanceHost").Content=maintenanceArea.View;
                }
                Find<ContentControl>("SavesHost").Visibility=Visibility.Collapsed;Find<Grid>("CommandCenter").Visibility=Visibility.Collapsed;
                Find<ContentControl>("SettingsHost").Visibility=Visibility.Collapsed;
                Find<ContentControl>("NewsHost").Visibility=Visibility.Collapsed;
                Find<ContentControl>("VersionsHost").Visibility=Visibility.Collapsed;
                Find<ContentControl>("MaintenanceHost").Visibility=Visibility.Visible;
                SelectNavigation("RepairNav");
                repairPage.SetBusy(busy || checking,Core.GameRunning());diagnosticsPage.SetBusy((busy && !gameMonitoring) || checking,Core.GameRunning());
                maintenanceArea.SetBusy(busy && !gameMonitoring);
                bool changed=maintenanceArea.Tabs.SelectedIndex!=(diagnostics?1:0);
                maintenanceArea.Tabs.SelectedIndex=diagnostics?1:0;
                if(diagnostics && !changed)await EnsureDiagnostics();
            }catch(Exception ex){Fail(ex);}
        }
        async Task MaintenanceOperation(MaintenancePage page,Func<CancellationToken,Task> operation,bool download=false)
        {
            if(busy)return;
            if(previewMode){page.Status("Preview only. Verification and repairs use the installed launcher.");return;}
            page.ClearRecovery();page.RetryOperation=()=>MaintenanceOperation(page,operation,download);
            busy=true;
            using(var cancel=CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)) {
                maintenanceStop=cancel;Refresh();
                try { await operation(cancel.Token); }
                catch(OperationCanceledException ex){page.ShowRecovery(RecoveryInfo.From(ex,download));}
                catch(Exception ex){Fail(ex);page.ShowRecovery(RecoveryInfo.From(ex,download));}
                finally { maintenanceStop=null;busy=false;if(!handingOff)Refresh(); }
            }
        }
        async Task VerifyFiles(bool launcher)
        {
            await MaintenanceOperation(repairPage,async token=> {
                repairPage.Status("Verifying "+(launcher?"launcher":"game")+" files…");
                var progress=new Progress<double>(value=>repairPage.Progress(value));
                var result=await Task.Run(()=>InstallationMaintenance.Verify(root,launcher,progress,token),token);
                if(launcher)launcherVerification=result;else gameVerification=result;
                repairPage.SetVerification(gameVerification,launcherVerification);
                repairPage.Status(result.Summary+". Repairs use the exact published version; personal files are excluded.");
            });
        }
        async Task RefreshDiagnostics()
        {
            if(gameMonitoring) {
                diagnosticsPage.Report.Text="Game hang diagnostics\n\n"+CrashReports.CaptureHealth(root)+"\n\n"+CrashReports.Summary(root)+"\n\nThe game is still running. You can wait for recovery or close it yourself. Nothing was uploaded.";
                diagnosticsPage.Status("Hang report available locally. Open or export it while the game remains running.");
                diagnosticsReady=true;return;
            }
            await MaintenanceOperation(diagnosticsPage,async token=> {
                diagnosticsPage.Status("Collecting system details…");
                string graphics=await InstallationMaintenance.GraphicsWithTimeout(token);
                string report=await Task.Run(()=>InstallationMaintenance.Report(root,gameVerification,launcherVerification,graphics),token);
                token.ThrowIfCancellationRequested();diagnosticsPage.Report.Text=report;diagnosticsReady=true;
                diagnosticsPage.Status("Report excludes raw logs, personal paths, saves and ISO information. Nothing is uploaded automatically.");
            });
        }
        void OpenDiagnosticIssue(bool enhancement)
        {
            if(busy || diagnosticsPage==null)return;
            if(previewMode){diagnosticsPage.Status("Preview only. GitHub drafts open from the installed launcher; no report was sent.");return;}
            if(!enhancement && !diagnosticsReady){diagnosticsPage.Status("Refresh diagnostics before preparing a GitHub report.");return;}
            try {
                var draft=DiagnosticIssueDraft.Create(enhancement,diagnosticsPage.Report.Text);
                if(enhancement) {
                    Process.Start(new ProcessStartInfo(draft.Url){UseShellExecute=true});
                    diagnosticsPage.Status("Improvement form opened. Describe your idea and review it before submitting.");return;
                }
                bool copied=false;
                try {Clipboard.SetText(draft.ClipboardText);copied=true;}catch { /* The displayed report remains available for manual copying. */ }
                try {Process.Start(new ProcessStartInfo(draft.Url){UseShellExecute=true});}
                catch(Exception ex) {
                    diagnosticsPage.Status(copied?"Could not open the browser. The full report is copied; open GitHub Support and paste it into the report form.":
                        "Could not open the browser or clipboard. Copy the displayed report manually, then open GitHub Support.");
                    Program.Log(root,"GitHub draft browser handoff failed ("+ex.GetType().Name+").");return;
                }
                diagnosticsPage.Status(draft.RequiresPaste?
                    (copied?"GitHub draft opened. Paste the copied full report into Launcher diagnostics (optional), complete the form, and review before submitting.":
                        "GitHub draft opened. Clipboard unavailable: copy the report text here and paste it into Launcher diagnostics (optional)."):
                    "GitHub draft opened with diagnostics. Complete the form, review the details, and submit when ready."+(copied?" A full copy is on your clipboard.":" Clipboard copy was unavailable."));
            }catch(Exception ex){diagnosticsPage.Status("Could not prepare the GitHub draft. Refresh diagnostics and try again.");Program.Log(root,"GitHub draft preparation failed ("+ex.GetType().Name+").");}
        }
        async Task RepairFiles(bool launcher)
        {
            await MaintenanceOperation(repairPage,async token=> {
                Core.Require(!Core.GameRunning(),"Close the game before repairing files.");
                repairPage.Status("Finding the exact published release, then downloading and verifying its files…");
                var operation=maintenanceStop;
                var progress=new TransferProgress(value=> {if(maintenanceStop==operation){repairPage.Progress(value.Percent);repairPage.Status(value.Summary);}});
                bool prepared=false;
                try {
                    if(launcher) {
                        var release=await client.FindLauncher(LauncherVersion.Value,token);
                        prepared=await SelfUpdate.Prepare(root,release,client,progress,token,true);
                        Core.Require(prepared,"A newer launcher is installed. Restart it before repairing.");
                        token.ThrowIfCancellationRequested();
                        string helper=Core.PathIn(root,".launcher/self-helper.exe");
                        Core.Require(!Core.GameRunning(),"Close the game before repairing files.");
                        File.Copy(Core.PathIn(root,".launcher/self-pending/payload/"+Core.LauncherName),helper,true);
                        Process.Start(new ProcessStartInfo(helper,"--apply-launcher "+Quote(root)+" "+Process.GetCurrentProcess().Id){UseShellExecute=true,WorkingDirectory=root});
                        handingOff=true;Window.Close();
                    } else {
                        Core.Require(gameVerification!=null && gameVerification.Version.Length>0,"Verify the game before repairing it.");
                        await InstallationMaintenance.PrepareGameRepair(root,gameVerification.Version,client,progress,token);
                        prepared=true;token.ThrowIfCancellationRequested();HandOff(false);
                    }
                } catch {
                    // Only this operation's newly prepared staging can be discarded.
                    if(prepared && !handingOff)try {
                        Core.DiscardRepairStaging(root,launcher);
                    }catch { /* Retain locked or unsafe staging for recovery. */ }
                    throw;
                }
            },true);
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
        void ClearRecovery()
        {
            retryOperation=null;Find<Panel>("RecoveryActions").Visibility=Visibility.Collapsed;
            Find<Grid>("UpdateActions").Visibility=Visibility.Visible;
            Find<StackPanel>("UpdateChannelSection").Visibility=Visibility.Visible;
            Find<TextBlock>("TransferDetails").Visibility=Visibility.Collapsed;
        }
        void ShowRecovery(Exception error,Func<Task> retry,bool download)
        {
            var info=RecoveryInfo.From(error,download);retryOperation=retry;Bar.IsIndeterminate=false;Bar.Value=0;
            Text("ReleaseTitle",info.Title);Text("ReleaseDescription",info.Message);Button("RetryButton").Content=info.RetryLabel;
            Find<Panel>("RecoveryActions").Visibility=Visibility.Visible;Find<Grid>("UpdateActions").Visibility=Visibility.Collapsed;
            Find<StackPanel>("UpdateChannelSection").Visibility=Visibility.Collapsed;Find<TextBlock>("TransferDetails").Visibility=Visibility.Collapsed;
            ShowUpdateState(error is OperationCanceledException?UpdatePresentationState.Paused:UpdatePresentationState.Error);
            SetHomePrimaryAction("RetryButton");
            SetStatus(download?"The installed release is unchanged. Retry when ready.":"You can retry or inspect the installed files.");
        }
        internal void SetHomePrimaryAction(string name)
        {
            foreach(string action in new[]{"PlayButton","UpdateButton","ImportButton","OpenDataButton","RetryButton"})
                if(action==name)PageTheme.Primary(Button(action));else PageTheme.Secondary(Button(action));
        }
        internal void ShowUpdateState(UpdatePresentationState state)
        {
            if(updatePresentation==state)return;
            updatePresentation=state;
            string label,icon,color="#A7D9FA";
            switch(state) {
                case UpdatePresentationState.Checking:label="CHECKING";icon="M15,15 L21,21 M18,10 A8,8 0 1 1 2,10 A8,8 0 1 1 18,10";break;
                case UpdatePresentationState.Ready:label="UP TO DATE";icon="M4,12 L9,17 L20,6";color="#A4DDCC";break;
                case UpdatePresentationState.Available:label="AVAILABLE";icon="M12,3 L12,16 M6,10 L12,16 L18,10 M4,18 L4,21 L20,21 L20,18";break;
                case UpdatePresentationState.Downloading:label="DOWNLOADING";icon="M12,3 L12,16 M6,10 L12,16 L18,10 M4,21 L20,21";break;
                case UpdatePresentationState.Verifying:label="VERIFYING";icon="M12,2 L21,6 L20,15 L12,22 L4,15 L3,6 Z M7,11 L11,15 L17,8";break;
                case UpdatePresentationState.Paused:label="PAUSED";icon="M7,4 L7,20 M17,4 L17,20";color="#E3C794";break;
                case UpdatePresentationState.Error:label="NEEDS ATTENTION";icon="M12,2 L23,21 L1,21 Z M12,8 L12,14 M12,17 L12,17.5";color="#EDB4AC";break;
                default:label="NO RELEASE";icon="M12,2 A10,10 0 1 1 11.9,2 M12,10 L12,17 M12,6 L12,6.5";color="#B4C7D8";break;
            }
            Text("UpdateStateLabel",label);Find<TextBlock>("UpdateStateLabel").Foreground=PageTheme.Brush(color);
            var glyph=Find<System.Windows.Shapes.Path>("UpdateStateIcon");glyph.Data=Geometry.Parse(icon);glyph.Stroke=PageTheme.Brush(color);
            var progress=Find<ProgressBar>("UpdateProgress");
            progress.Visibility=state==UpdatePresentationState.Checking || state==UpdatePresentationState.Downloading || state==UpdatePresentationState.Verifying?Visibility.Visible:Visibility.Hidden;
            progress.IsIndeterminate=state==UpdatePresentationState.Checking || state==UpdatePresentationState.Verifying;
            if(state!=UpdatePresentationState.Downloading)progress.Value=0;
        }
        internal void SetGameDataPresentation(bool verified, bool exists)
        {
            var expander=Find<Expander>("GameDataExpander");
            expander.Header=verified?"Manage game data":exists?"Inspect game data":"Set up game data";
            if(displayedDataVerified!=verified)expander.IsExpanded=!verified;
            displayedDataVerified=verified;
            Text("GameDataHint",verified?"Your game data is ready. You only need to import it once.":exists?
                "Existing data needs attention. Open the folder to inspect it; your files have been kept.":
                "Import your supported US Xbox ISO or an extracted game folder.");
            Find<Grid>("ImportActions").Visibility=exists?Visibility.Collapsed:Visibility.Visible;
            Button("OpenDataButton").Visibility=exists?Visibility.Visible:Visibility.Collapsed;
        }
        void Refresh()
        {
            try { installed = Core.InstalledVersion(root); }
            catch (Exception ex) { installed = ""; SetStatus("Installed manifest could not be read: " + ex.Message); }
            gameInstalled = File.Exists(Core.PathIn(root, "Bin/futureperfect_recomp.exe")) && File.Exists(Core.PathIn(root, "Launch.ps1"));
            string xbe = Core.PathIn(root, "Data/default.xbe");
            validData = File.Exists(xbe) && Directory.Exists(Core.PathIn(root, "Data/pak")) && Directory.Exists(Core.PathIn(root, "Data/xmv")) && Core.Hash(xbe) == TsfpImport.ExpectedHash;
            Text("InstalledLabel", installed.Length > 0 ? installed : gameInstalled ? "UNVERSIONED" : "NOT INSTALLED");
            Text("ReadyTitle", gameInstalled && validData ? "Ready to play" : gameInstalled ? "Import game data" : "Install game");
            Text("ReadyDescription", gameInstalled && validData ? "Your game is installed and ready to launch." : gameInstalled ? "Import your US Xbox game once to start playing." : "Install a release, then import your US Xbox ISO.");
            bool dataExists = Directory.Exists(Core.PathIn(root, "Data"));
            if (dataExists && !validData)
            {
                Text("ReadyTitle", "Check your game data.");
                Text("ReadyDescription", "Existing Data is incomplete or unsupported. Choose Repair / Verify → Open game folder to inspect it; your files were kept.");
            }
            Text("DataLabel", validData ? "●  Verified US Xbox data" : dataExists ? "○  Data needs attention" : "○  ISO required");
            SetGameDataPresentation(validData,dataExists);
            Button("ImportButton").IsEnabled = Button("FolderButton").IsEnabled = !busy && !dataExists;
            Button("PlayButton").IsEnabled = !busy && gameInstalled && validData && !Core.GameRunning();
            Button("SettingsNav").IsEnabled = !busy && (previewMode || !Core.GameRunning());
            Button("HomeNav").IsEnabled = Button("NewsNav").IsEnabled = Button("RepairNav").IsEnabled = Button("SavesNav").IsEnabled = !busy;
            if(maintenanceArea!=null)maintenanceArea.SetBusy(busy && !gameMonitoring);
            if(repairPage!=null)repairPage.SetBusy(busy || checking,Core.GameRunning());
            if(diagnosticsPage!=null)diagnosticsPage.SetBusy((busy && !gameMonitoring) || checking,Core.GameRunning());
            Button("GameVersionsButton").IsEnabled=!busy && !checking;
            if(versionsPage!=null)versionsPage.SetBusy(busy || checking,Core.GameRunning());
            Button("RetryButton").IsEnabled=!busy && !checking;
            Button("RecoveryRepairButton").IsEnabled=!busy && !checking;
            Button("RecoveryLogsButton").IsEnabled=!busy;
            Button("CheckButton").IsEnabled = !busy && !checking;
            channelPicker.SetEnabled(!busy && !checking);
            Button("UpdateButton").IsEnabled = !busy && !checking && CanUpdate();
            Button("UpdateButton").Content = DownloadTransfer.SavedBytes(Core.PathIn(root,".launcher/downloads/game"),available)>0?"Resume download":gameInstalled ? (available!=null && available.Channel!=GameChannels.Installed(root)?"Switch game channel":"Update release") : "Install release";
            Button("RollbackButton").IsEnabled = !busy && !checking && Core.CanRollback(root) && !Core.GameRunning();
            SetHomePrimaryAction(retryOperation!=null?"RetryButton":!gameInstalled?"UpdateButton":!validData?(dataExists?"OpenDataButton":"ImportButton"):"PlayButton");
            Text("ChannelBadge", PreviewChannel?"\u25cf  PREVIEW CHANNEL":"\u25cf  STABLE CHANNEL");
        }
        bool CanUpdate()
        {
            return GameChannels.CanInstall(available,installed);
        }
        async Task Check()
        {
            if (busy || checking) return;
            ClearRecovery();checking = true; available = null;
            Refresh();
            ShowUpdateState(UpdatePresentationState.Checking);
            Text("ReleaseTitle", "Checking for updates…");
            Text("ReleaseDescription", "Looking for published " + GameChannels.Name(SelectedGameChannel) + " releases on GitHub.");
            SetStatus("Checking GitHub Releases…");
            using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token))
            {
                timeout.CancelAfter(TimeSpan.FromSeconds(25));
                try
                {
                    string selected=SelectedGameChannel;
                    var catalog=await client.ReadGameCatalog(timeout.Token);
                    SetChannels(catalog,selected);available=GameChannels.Select(catalog,selected);
                    if (available == null)
                    {
                        ShowUpdateState(UpdatePresentationState.Empty);
                        Text("ReleaseTitle", PreviewChannel ? "No compatible release yet." : "No stable release yet.");
                        Text("ReleaseDescription", PreviewChannel ? "A verified package for this testing channel will appear here." : "Stable excludes test builds. Choose Preview and a testing track to opt into testing.");
                        SetStatus(gameInstalled ? "Your installed game is available offline." : "No compatible published release was found for this channel.");
                    }
                    else
                    {
                        bool newer = CanUpdate();
                        ShowUpdateState(newer?UpdatePresentationState.Available:UpdatePresentationState.Ready);
                        Text("ReleaseTitle", newer ? available.Tag + " is ready." : "You're on the latest release.");
                        Text("ReleaseDescription", newer ? "Published on GitHub · " + (available.Size / 1048576.0).ToString("0.0") + " MB\n" + (gameInstalled ? "Update available." : "Install the game runtime, then import your game data.") : "The installed game is up to date. Check release notes for details.");
                        SetStatus(newer ? "Release available. Select " + (gameInstalled ? "Update" : "Install") + " to download and verify." : "Ready.");
                    }
                }
                catch (OperationCanceledException) { if (!lifetime.IsCancellationRequested) { Text("ReleaseTitle", "Release check timed out."); Text("ReleaseDescription", "Your installed game is available offline. Check again when connected."); ShowRecovery(new TimeoutException("Release check timed out."),Check,false); } }
                catch (Exception ex) { Text("ReleaseTitle", "Update check failed."); Text("ReleaseDescription", "Your installed game is available offline. Try checking again later."); Fail(ex);ShowRecovery(ex,Check,false); }
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
            await DownloadGame(available,false);
        }
        async Task DownloadGame(Release release,bool specific)
        {
            if (busy || checking || release==null || (!specific && !CanUpdate())) return;
            if(previewMode){SetStatus("Preview only. No download was started.");return;}
            if(!specific && gameInstalled && release.Channel!=GameChannels.Installed(root) &&
                MessageBox.Show(Window,"Switch to "+release.ChannelName+" ("+release.Tag+")?\n\nThis can install an older game version. Saves and settings are preserved, but compatibility can vary. Back up saves before testing.","Switch game channel",MessageBoxButton.YesNo,MessageBoxImage.Question,MessageBoxResult.No)!=MessageBoxResult.Yes)return;
            if(Core.GameRunning()){SetStatus("Close the game before installing a release.");return;}
            ClearRecovery();busy=true;
            using(var cancel=CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)) {
                updateStop=cancel;Refresh();
                ShowUpdateState(UpdatePresentationState.Downloading);
                Button("PauseDownloadButton").Visibility=Visibility.Visible;
                Find<StackPanel>("UpdateChannelSection").Visibility=Visibility.Collapsed;
                Find<TextBlock>("TransferDetails").Visibility=Visibility.Visible;
                Text("ReleaseTitle","Downloading "+release.Tag);Text("ReleaseDescription","Matching saved downloads resume automatically. Installed files stay unchanged until verification completes.");
                try {
                    Core.CheckSpace(root,release.Size*4+256L*1024*1024);
                    Core.DiscardGameStaging(root);
                    string pending=Core.PathIn(root,".launcher/pending");Directory.CreateDirectory(pending);
                    string archive=Path.Combine(pending,"release.zip");
                    SetStatus("Downloading game update…");
                    var progress=new TransferProgress(value=> {
                        if(updateStop!=cancel)return;
                        Bar.IsIndeterminate=value.Phase=="Verifying" || value.Phase=="Preparing";Bar.Value=value.Percent;Text("TransferDetails",value.Summary);
                        ShowUpdateState(value.Phase=="Verifying" || value.Phase=="Preparing" || value.Phase=="Complete"?UpdatePresentationState.Verifying:UpdatePresentationState.Downloading);
                        Find<ProgressBar>("UpdateProgress").Value=value.Percent;
                        Button("PauseDownloadButton").Content=value.Phase=="Verifying" || value.Phase=="Preparing" || value.Phase=="Complete"?"Cancel preparation":"Pause download";
                    });
                    await client.Download(release,archive,progress,cancel.Token,Core.PathIn(root,".launcher/downloads/game"));
                    Bar.IsIndeterminate=true;Button("PauseDownloadButton").Content="Cancel preparation";
                    ShowUpdateState(UpdatePresentationState.Verifying);
                    Text("ReleaseTitle","Preparing "+release.Tag);
                    Text("ReleaseDescription","Verifying and preparing release files before installation.");
                    SetStatus("Verifying and preparing the release…");
                    await Task.Run(()=>Core.Unpack(archive,Path.Combine(pending,"payload"),release.Tag,cancel.Token),cancel.Token);
                    cancel.Token.ThrowIfCancellationRequested();
                    GameChannels.Save(root,release.Channel);Core.Write(Path.Combine(pending,"release.json"),Json.Encode(new {version=release.Tag,game_only=specific}));File.Delete(archive);DownloadTransfer.Forget(Core.PathIn(root,".launcher/downloads/game"));
                    Program.Log(root,"Prepared "+release.Tag+"; handing off to updater.");HandOff(false);
                }catch(Exception ex){
                    if(!(ex is OperationCanceledException))Fail(ex);
                    ShowRecovery(ex,()=>DownloadGame(release,specific),true);
                    try{Core.DiscardGameStaging(root);}catch(Exception cleanup){Program.Log(root,"Update staging retained: "+cleanup.GetType().Name);}
                }finally{
                    updateStop=null;
                    if(!handingOff){busy=false;Bar.IsIndeterminate=false;Bar.Value=0;Button("PauseDownloadButton").Visibility=Visibility.Collapsed;Refresh();if(closeAfterTransfer)Window.Close();}
                }
            }
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
        internal async Task MonitorGame(Process process,string previousReport = null)
        {
            bool minimizedForGame = false;
            bool gameWindowShown = false;
            string observedBuild = Core.GameBuildHash(root);
            WindowState previousState = Window.WindowState;
            string lastReport=previousReport;
            var reportClock=Stopwatch.StartNew();long nextReportScan=0;
            gameMonitoring=true;
            try
            {
                SetStatus("Starting game…");
                // Process.Start has succeeded. Do not delay taskbar minimization
                // while waiting for a detectable game window; cleanup still must.
                if (!process.HasExited)
                {
                    Window.ShowInTaskbar = true;
                    Window.WindowState = WindowState.Minimized;
                    minimizedForGame = true;
                }
                while (!process.HasExited)
                {
                    if (!gameWindowShown && GameWindowReady(root))
                    {
                        gameWindowShown = true;
                        SetStatus("Game running.");
                    }
                    if(reportClock.ElapsedMilliseconds>=nextReportScan) {
                        nextReportScan=reportClock.ElapsedMilliseconds+2000;
                        string report=CrashReports.Latest(root);
                        if(report!=null && report!=lastReport && CrashReports.IsHang(root,report)) {
                            Window.WindowState=previousState;Window.Activate();
                            diagnosticsReady=false;ShowMaintenance(true);
                            SetStatus("The game may be frozen. Hang report saved; the game is still running.");
                        }
                        if(report!=null)lastReport=report;
                    }
                    await Task.Delay(200);
                }
                await Task.Run(() => process.WaitForExit());
            }
            finally
            {
                gameMonitoring=false;
                if (minimizedForGame) { Window.WindowState = previousState; Window.Activate(); }
            }
            await Task.Run(() => Core.FinishGameSession(root, observedBuild, gameWindowShown, process.ExitCode));
        }
        internal async Task LaunchGame(string crashTestMode = null)
        {
            using (var process = new Process())
            {
                // Fresh installs use the same native launch path as configured
                // ones. A PowerShell fallback fails under ConstrainedLanguage.
                try {GameSessions.Recover(root);}catch(Exception ex){Program.Log(root,"Session recovery deferred: "+ex.GetType().Name);}
                string previousCrash = CrashReports.Latest(root);
                process.StartInfo = GameSettings.StartInfo(root);
                if(crashTestMode!=null) {
                    Core.Require(crashTestMode=="main" || crashTestMode=="worker" || crashTestMode=="normal" || crashTestMode=="handled" || crashTestMode=="hang-recover" || crashTestMode=="hang-unarmed", "Unknown crash test mode.");
                    process.StartInfo.Arguments=crashTestMode=="hang-recover"?"--tsfp-hang-test=recover":crashTestMode=="hang-unarmed"?"--tsfp-hang-test=unarmed":"--tsfp-crash-test="+crashTestMode;
                }
                string logs = Core.PathIn(root, "Logs"); Directory.CreateDirectory(logs);
                string tag = DateTime.Now.ToString("yyyyMMdd-HHmmss-fff", System.Globalization.CultureInfo.InvariantCulture);
                string errorPath = Path.Combine(logs, tag + ".err");
                process.StartInfo.EnvironmentVariables["RECOMP_LOG_FILE"] = errorPath;
                using (var output = File.Create(Path.Combine(logs, tag + ".out")))
                using (var errors = new FileStream(errorPath, FileMode.Create, FileAccess.Write, FileShare.ReadWrite))
                {
                    string session=null;
                    try {session=GameSessions.Begin(root,tag);}
                    catch(Exception ex) {Program.Log(root,"Session tracking unavailable: "+ex.GetType().Name);}
                    process.StartInfo.EnvironmentVariables["TSFP_CAPTURE_SESSION"]=session??"";
                    try {process.Start();}
                    catch {
                        try {if(session!=null)GameSessions.CancelLaunch(root,session);}catch(Exception ex){Program.Log(root,"Session launch cancellation: "+ex.GetType().Name);}
                        throw;
                    }
                    try {if(session!=null)GameSessions.Started(root,session,process);}
                    catch(Exception ex) {Program.Log(root,"Session process identity unavailable: "+ex.GetType().Name);}
                    try {
                        var stdout = process.StandardOutput.BaseStream.CopyToAsync(output);
                        var stderr = process.StandardError.BaseStream.CopyToAsync(errors);
                        await MonitorGame(process,previousCrash); await Task.WhenAll(stdout, stderr);
                        // Publish even a short buffered diagnostic before the
                        // fallback reporter reads this session's log tail.
                        errors.Flush();
                    } finally {
                        try {if(session!=null && process.HasExited)GameSessions.Complete(root,session,process);}
                        catch(Exception ex) {Program.Log(root,"Session completion deferred: "+ex.GetType().Name);}
                    }
                }
                string latestCrash = CrashReports.Latest(root);
                pendingCrash = (latestCrash != null && latestCrash != previousCrash) || process.ExitCode!=0;
                diagnosticsReady = false;
                SetStatus(latestCrash!=null && latestCrash!=previousCrash ? CrashReports.IsUnexpectedExit(root,latestCrash)?"The game closed unexpectedly. No crash dump was captured; session report saved - see Diagnostics.":CrashReports.IsHang(root,latestCrash)?"Game closed. Hang report saved - see Diagnostics.":"The game closed unexpectedly. Crash report saved - see Diagnostics." : process.ExitCode == 0 ? "Game closed." : "The game closed unexpectedly. No crash report is available - see Diagnostics and logs.");
            }
        }
        internal void RecoverGameSession()
        {
            try {
                string report=GameSessions.Recover(root);if(report==null)return;
                diagnosticsReady=false;ShowMaintenance(true);
                SetStatus(CrashReports.IsUnexpectedExit(root,report)?"The previous game session ended unexpectedly. No crash dump was captured; see Diagnostics.":"The previous game session ended unexpectedly. Crash report saved - see Diagnostics.");
            } catch(Exception ex) {Program.Log(root,"Session recovery deferred: "+ex.GetType().Name);}
        }
        async Task Play()
        {
            if (busy) return;
            busy = true; Refresh();
            try
            {
                Core.Require(validData && gameInstalled, "Install the release and import the supported US Xbox data first.");
                Core.Require(!Core.GameRunning(), "The game is already running.");
                await LaunchGame();
            }
            catch (Exception ex) { Fail(ex); }
            finally { busy = false; Refresh(); if(pendingCrash){pendingCrash=false;ShowMaintenance(true);} }
        }
    }
}
