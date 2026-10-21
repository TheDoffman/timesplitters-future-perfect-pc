using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
namespace TimeSplitters.Launcher
{
    public sealed partial class NativeWindow:Window
    {
        readonly string root;readonly bool render;readonly string[] args;
        readonly ReleaseClient client=new ReleaseClient();
        CancellationTokenSource operation;Release latest;bool busy,gameRunning;int checkRevision;SettingsPage settings;
        FileStream installationLock;DispatcherTimer issuesTimer;NativeNews newsPage;
        public NativeWindow():this(Program.Root??AppContext.BaseDirectory,Program.Arguments??Array.Empty<string>()) { }
        public NativeWindow(string root,string[] args)
        {
            this.root=root;this.args=args;render=args.Any(a=>a.StartsWith("--render-"));
            AvaloniaXamlLoader.Load(this);Artwork();
            Label("LauncherVersionLabel","LAUNCHER "+LauncherVersion.Value+"  /  macOS ARM64");
            Click("HomeNav",()=>Page("CommandCenter"));Click("SettingsNav",ShowSettings);Click("NewsNav",()=>Run(News));
            Click("RepairNav",()=>Maintenance());Click("SupportNav",()=>NativeHost.Open("https://github.com/"+ReleaseClient.Repository+"/issues"));
            Click("SavesNav",()=>OpenFolder(LauncherPlatform.Saves(root)));Click("LogsButton",()=>OpenFolder("logs"));
            Click("OpenDataButton",()=>OpenFolder(LauncherPlatform.Data(root)));
            Click("CheckButton",()=>Run(Check));Click("UpdateButton",()=>Run(()=>Install(latest)));Click("PlayButton",()=>Run(Play));
            Click("ImportButton",()=>Run(()=>Import(false)));Click("FolderButton",()=>Run(()=>Import(true)));
            Click("ReleaseNotesButton",()=>Run(News));Click("GameVersionsButton",()=>Run(Versions));
            Click("RollbackButton",()=>Run(Rollback));Click("PauseDownloadButton",()=>operation?.Cancel());
            Find<ComboBox>("ChannelPicker").SelectedIndex=render?1:SelfUpdate.PreviewChannel(root)?1:0;
            Find<ComboBox>("ChannelPicker").SelectionChanged+=(s,e)=>{if(render)return;Core.Write(Core.PathIn(root,".launcher/preferences.json"),Json.Encode(new{preview=Preview}));Run(Check);};
            Opened+=async(s,e)=>{try{
                if(args.Contains("--test-session")){await TestSession();return;}
                if(render){Refresh();await Render();return;}
                Directory.CreateDirectory(Core.PathIn(root,".launcher"));installationLock=new FileStream(Core.PathIn(root,".launcher/mac.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
                Core.RecoverLauncher(root);Core.Recover(root);TexturePack.Recover(root);Core.CleanupGameAfterStartup(root);string recovered=null;try{recovered=GameSessions.Recover(root);}catch(Exception ex){LauncherLog.Write(root,"Session recovery deferred: "+ex.GetType().Name);}Refresh();if(recovered!=null)Maintenance(true);SelfUpdate.CleanupAfterStartup(root);
                foreach(var name in new[]{".launcher/mac-self-helper",".launcher/mac-update-helper"})try{File.Delete(Core.PathIn(root,name));}catch(IOException){}
                await Check();string result=Core.PathIn(root,".launcher/last-result.txt");if(File.Exists(result)){Status(File.ReadAllText(result));File.Delete(result);}
                issuesTimer=new DispatcherTimer{Interval=TimeSpan.FromMinutes(5)};issuesTimer.Tick+=(a,b)=>{if(Find<ContentControl>("NewsHost").IsVisible && WindowState!=WindowState.Minimized && !busy)Run(()=>newsPage.RefreshIssues());};issuesTimer.Start();
            }catch(Exception ex){Status(ex.Message);if(installationLock==null)IsEnabled=false;}};
            Activated+=(s,e)=>{if(newsPage!=null&&Find<Control>("NewsHost").IsVisible&&!busy)Run(newsPage.RefreshIssuesIfDue);};
            Closing+=(s,e)=>{if(!NativeStartup.HandingOff && (busy || gameRunning)){e.Cancel=true;Status("Wait for the operation or game to finish before closing the launcher.");}};
            Closed+=(s,e)=>{issuesTimer?.Stop();newsPage?.Dispose();operation?.Cancel();installationLock?.Dispose();client.Dispose();};
        }
        T Find<T>(string name) where T:Control { return this.FindControl<T>(name)??throw new InvalidOperationException("Missing launcher control: "+name); }
        void Click(string name,Action action){Find<Button>(name).Click+=(s,e)=>{try{action();}catch(Exception ex){Status(ex.Message);}};}
        void Label(string name,string value){Find<TextBlock>(name).Text=value;}
        void Status(string message){Label("StatusText",message);if(!render)LauncherLog.Write(root,message);}
        internal bool GameRunning { get { return gameRunning; } }
        bool Preview=>Find<ComboBox>("ChannelPicker").SelectedIndex==1;
        void Page(string name){
            if(name!="SettingsHost"&&settings!=null)settings.DismissPrompt();
            foreach(var pair in new[]{("HomeNav","CommandCenter"),("NewsNav","NewsHost"),("SettingsNav","SettingsHost"),("RepairNav","MaintenanceHost")}){var nav=Find<Button>(pair.Item1);nav.Background=Brush.Parse(name==pair.Item2?"#1D3A55":"Transparent");nav.Foreground=Brush.Parse(name==pair.Item2?"#86CDFF":"#DFE9F2");}
            foreach(var p in new[]{"CommandCenter","NewsHost","SettingsHost","MaintenanceHost","VersionsHost"})Find<Control>(p).IsVisible=p==name;}
        void OpenFolder(string name){var path=Core.PathIn(root,name);if(!Directory.Exists(path)){Status("This folder has not been created yet.");return;}NativeHost.Open(path);}
        void Artwork()
        {
            foreach(var pair in new[]{("HeroPhoto","launcher-banner.png"),("HeroBackdrop","launcher-backdrop.png"),("HeroTitle","launcher-logo.png"),("SidebarLogo","launcher-logo.png"),("SidebarSubtitle","launcher-logo.png")}){
                using(var stream=typeof(NativeWindow).Assembly.GetManifestResourceStream(pair.Item2))if(stream!=null){var image=this.FindControl<Image>(pair.Item1);if(image!=null){image.Source=pair.Item1=="SidebarSubtitle"?new CroppedBitmap(new Bitmap(stream),new PixelRect(263,150,228,22)):new Bitmap(stream);image.IsVisible=true;}}}
            bool banner=Find<Image>("HeroPhoto").Source!=null;Find<Control>("HeroBanner").IsVisible=banner;Find<Control>("HeroFallback").IsVisible=!banner;
            bool logo=Find<Image>("HeroTitle").Source!=null;Find<Control>("HeroTitleFallback").IsVisible=!logo;
            Find<Control>("SidebarBrand").IsVisible=logo;var fallback=this.FindControl<Control>("SidebarBrandFallback");if(fallback!=null)fallback.IsVisible=!logo;
        }
        void Refresh()
        {
            bool installed=File.Exists(Core.PathIn(root,LauncherPlatform.GamePath(root)));
            bool data=File.Exists(Core.PathIn(root,LauncherPlatform.Data(root)+"/default.xbe"));
            Label("InstalledLabel",render?"LOCAL BUILD":NativeHost.Version(root));
            Label("ReadyTitle",gameRunning?"Game running":installed||render?(data||render?"Ready to play":"Set up game data"):"Install game");
            Label("ReadyDescription",gameRunning?"The launcher will return when the game closes.":installed||render?"Your native Mac game is ready. Adjust settings before you play.":"Install a native Mac release, then import your US Xbox game data.");
            Find<Button>("PlayButton").IsEnabled=(installed&&data||render)&&!busy&&!gameRunning;
            Label("DataLabel",data||render?"✓  Game data installed":"○  ISO required");
            Label("GameDataHint",data?"Your local game files are kept between updates.":"Import your supported US Xbox ISO or extracted game folder.");
            Find<Control>("ImportActions").IsVisible=!data&&!render;Find<Button>("OpenDataButton").IsVisible=data||render;Find<Expander>("GameDataExpander").IsExpanded=!data&&!render;
            Find<Button>("ImportButton").IsEnabled=!busy;Find<Button>("FolderButton").IsEnabled=!busy;
            Find<Button>("RollbackButton").IsEnabled=!busy&&!gameRunning&&!LauncherPlatform.Checkout(root)&&Core.CanRollback(root);
            Label("ChannelBadge",Preview?"●  PREVIEW CHANNEL":"●  STABLE CHANNEL");
            if(render){Label("ReleaseTitle","Native Mac launcher");Label("ReleaseDescription","Preview layout · native Mac releases appear here when published.");Label("UpdateStateLabel","MAC PREVIEW");Status("Preview only · no game launched, network requests or personal files changed.");}
        }
        async void Run(Func<Task> action){try{await action();}catch(OperationCanceledException){Status("Download paused. Choose Install release to resume.");}catch(Exception e){Status(e.Message);if(!Find<Control>("CommandCenter").IsVisible)await MessageDialog("TimeSplitters",e.Message,false).ShowDialog(this);}}
        async Task Check()
        {
            if(busy)return;int revision=++checkRevision;bool channel=Preview;Label("ReleaseTitle","Checking for updates…");
            using(var timeout=new CancellationTokenSource(TimeSpan.FromSeconds(8))){try{
                var found=await client.Check(channel,timeout.Token);if(revision!=checkRevision)return;latest=found;
                Label("ReleaseTitle",latest==null?"No Mac release available":"Available: "+latest.Tag);
                Label("ReleaseDescription",latest==null?"No compatible native Mac package is published on this channel. Your local build remains available.":ReleaseNews.Availability(Core.InstalledVersion(root),latest,false));
                bool newer=latest!=null && (String.IsNullOrEmpty(Core.InstalledVersion(root)) || GameVersion.Parse(latest.Tag).CompareTo(GameVersion.Parse(Core.InstalledVersion(root)))>0);
                Find<Button>("UpdateButton").IsEnabled=newer&&!LauncherPlatform.Checkout(root)&&!gameRunning;
                Label("UpdateStateLabel",latest==null?"NATIVE MAC":"CHECKED");
                Status(LauncherPlatform.Checkout(root)?"Development build · published updates install into a separate portable installation.":"Update check complete.");
            }catch(Exception e){Label("ReleaseTitle","Couldn't check updates");Label("ReleaseDescription","Your installed game remains available. "+e.Message);}}
            Refresh();
        }
        async Task Play()
        {
            if(busy||gameRunning)return;
            Core.Require(!Core.GameRunning(),"Close the running game before starting another session.");
            await Task.Run(()=>{NativeHost.CheckData(root);NativeHost.CheckBuild(root);});
            Core.Require(!Core.GameRunning(),"The game is already running.");
            NativeHost.Prepare(root);var info=GameSettings.StartInfo(root);
            await MonitorGame(info,root);
        }
        bool lastWindowObserved;
        async Task<int> MonitorGame(ProcessStartInfo info,string sessionRoot)
        {
            string previous=CrashReports.Latest(sessionRoot);string id=null;try{id=GameSessions.Begin(sessionRoot,DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff"));}catch(Exception e){LauncherLog.Write(sessionRoot,"Session tracking unavailable: "+e.GetType().Name);}Process process;
            string observedBuild=LauncherPlatform.Checkout(sessionRoot)?"":Core.GameBuildHash(sessionRoot);lastWindowObserved=false;var previousState=WindowState;
            try{process=Process.Start(info);}catch{if(id!=null)GameSessions.CancelLaunch(sessionRoot,id);throw;}
            try{if(id!=null)GameSessions.Started(sessionRoot,id,process);}catch(Exception e){LauncherLog.Write(sessionRoot,"Session start observation: "+e.Message);}
            gameRunning=true;Refresh();WindowState=WindowState.Minimized;
            try{Directory.CreateDirectory(Core.PathIn(sessionRoot,"logs"));var stdout=NativeHost.Drain(process.StandardOutput.BaseStream,Core.PathIn(sessionRoot,"logs/native-play.out"));var stderr=NativeHost.Drain(process.StandardError.BaseStream,Core.PathIn(sessionRoot,"logs/native-play.err"));
                while(!process.HasExited){if(!lastWindowObserved)lastWindowObserved=NativeHost.GameWindowReady(process.Id);await Task.Delay(500);string report=CrashReports.Latest(sessionRoot);if(report!=null && report!=previous && CrashReports.IsHang(sessionRoot,report)){previous=report;CrashReports.Trim(sessionRoot,report);WindowState=WindowState.Normal;Maintenance(true);Status("The game may be stalled. A local hang report is available; the game is still running.");}}
                await process.WaitForExitAsync();await Task.WhenAll(stdout,stderr);try{if(id!=null)GameSessions.Complete(sessionRoot,id,process);}catch(Exception e){LauncherLog.Write(sessionRoot,"Session completion deferred: "+e.GetType().Name);}int exit=process.ExitCode;
                if(!LauncherPlatform.Checkout(sessionRoot))await Task.Run(()=>Core.FinishGameSession(sessionRoot,observedBuild,lastWindowObserved,exit));string latestReport=CrashReports.Latest(sessionRoot);if(latestReport!=null)CrashReports.Trim(sessionRoot,latestReport);
                Status(exit==0?"Game closed.":"Game closed unexpectedly. Open Diagnostics to review its local report.");if(exit!=0||latestReport!=previous)Maintenance(true);return exit;
            }finally{gameRunning=false;process.Dispose();WindowState=previousState;Activate();Refresh();}
        }

        async Task Import(bool folder)
        {
            string source;
            if(folder){var picked=await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions{Title="Choose extracted US Xbox game files",AllowMultiple=false});source=picked.FirstOrDefault()?.TryGetLocalPath();}
            else{var picked=await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions{Title="Choose your US Xbox ISO",AllowMultiple=false,FileTypeFilter=new[]{new FilePickerFileType("Xbox ISO"){Patterns=new[]{"*.iso"}}}});source=picked.FirstOrDefault()?.TryGetLocalPath();}
            if(source==null||busy||gameRunning)return;busy=true;Refresh();Status("Importing game data…");
            try{var destination=Core.PathIn(root,LauncherPlatform.Data(root));await Task.Run(()=>{if(folder)TsfpImport.ImportFolder(source,destination);else TsfpImport.ImportIso(source,destination);});Status("Game data imported.");}
            finally{busy=false;Refresh();}
        }
        async Task Install(Release release)
        {
            if(release==null||busy)return;Core.Require(!LauncherPlatform.Checkout(root),"Release updates cannot overwrite a developer checkout.");Core.Require(!gameRunning&&!Core.GameRunning(),"Close the game before updating.");
            if(!await Confirm("Install "+release.Tag+"?","This replaces the game runtime and restarts the launcher. Saves, game data and settings are preserved."))return;
            if(busy||gameRunning)return;busy=true;operation=new CancellationTokenSource();Refresh();Find<Control>("PauseDownloadButton").IsVisible=true;Find<Control>("UpdateProgress").IsVisible=true;
            try{string pending=Core.PathIn(root,".launcher/pending");Core.Require(!Directory.Exists(pending),"An unfinished update is staged. Restart for recovery before continuing.");Directory.CreateDirectory(pending);
                try{string zip=Path.Combine(pending,"release.zip");await client.Download(release,zip,new TransferProgress(snapshot=>{Label("TransferDetails",snapshot.Summary);Find<Control>("TransferDetails").IsVisible=true;Find<ProgressBar>("UpdateProgress").Value=snapshot.Percent;}),operation.Token,Core.PathIn(root,".launcher/downloads/game"));
                    await Task.Run(()=>Core.Unpack(zip,Path.Combine(pending,"payload"),release.Tag,operation.Token));operation.Token.ThrowIfCancellationRequested();
                    Core.Write(Path.Combine(pending,"release.json"),Json.Encode(new{version=release.Tag}));File.Delete(zip);Status("Restarting the launcher to install "+release.Tag+"…");NativeStartup.HandoffGame(root);}
                catch{if(!NativeStartup.HandingOff)Core.DiscardGameStaging(root);throw;}
            }finally{operation.Dispose();operation=null;if(!NativeStartup.HandingOff){busy=false;Find<Control>("PauseDownloadButton").IsVisible=false;Find<Control>("TransferDetails").IsVisible=false;Find<Control>("UpdateProgress").IsVisible=false;Refresh();}}
        }
        async Task Rollback(){if(busy||gameRunning)return;if(await Confirm("Restore previous game?","The launcher restarts after restoring the previous game. Your saves and settings stay unchanged.")){if(busy||gameRunning)return;busy=true;Refresh();try{NativeStartup.HandoffGame(root,true);}finally{if(!NativeStartup.HandingOff){busy=false;Refresh();}}}}
        void ShowSettings(){if(gameRunning||busy){Status("Close the game and finish operations before changing settings.");return;}if(settings==null){settings=new SettingsPage(new ResourceDictionary(),root,render);settings.Saved+=()=>{Status("Settings saved.");Refresh();};settings.Cancelled+=()=>{Page("CommandCenter");Find<ContentControl>("SettingsHost").Content=null;settings=null;Status("Unsaved settings discarded.");};Find<ContentControl>("SettingsHost").Content=settings.View;}Page("SettingsHost");}
        async Task News()
        {
            if(newsPage==null){newsPage=new NativeNews(root,client,()=>Preview);Find<ContentControl>("NewsHost").Content=newsPage.View;}
            Page("NewsHost");await newsPage.Refresh();
        }
        NativeVersions versionsPage;
        async Task Versions()
        {
            if(versionsPage==null){versionsPage=new NativeVersions(root,client,()=>busy||gameRunning,Install);Find<ContentControl>("VersionsHost").Content=versionsPage.View;}
            Page("VersionsHost");await versionsPage.Load(true);
        }
        NativeMaintenance maintenance;
        void Maintenance(bool diagnostics=false)
        {
            if(maintenance==null){maintenance=new NativeMaintenance(root,this);Find<ContentControl>("MaintenanceHost").Content=maintenance.View;}
            maintenance.Select(diagnostics);Page("MaintenanceHost");
        }
        internal async Task Maintain(Func<CancellationToken,Task> action)
        {
            Core.Require(!busy,"Finish the current operation first.");busy=true;operation=new CancellationTokenSource();Refresh();
            try{await action(operation.Token);}finally{operation.Dispose();operation=null;if(!NativeStartup.HandingOff){busy=false;Refresh();}}
        }
        internal void CancelMaintenance(){operation?.Cancel();}
        internal async Task Repair(bool launcher,CancellationToken token)
        {
            Core.Require(!gameRunning&&!Core.GameRunning(),"Close the game before repairing files.");
            Core.Require(!LauncherPlatform.Checkout(root),"Rebuild development files from source. Release repair is available in portable installations.");
            if(launcher){var release=await client.FindLauncher(LauncherVersion.Value,token);Core.Require(await SelfUpdate.Prepare(root,release,client,null,token,true),"Launcher repair could not be staged.");NativeStartup.Handoff(root);}
            else {await InstallationMaintenance.PrepareGameRepair(root,Core.InstalledVersion(root),client,null,token);token.ThrowIfCancellationRequested();NativeStartup.HandoffGame(root);}
        }
        public static TextBlock Text(string text,int size=13)=>new TextBlock{Text=text,FontSize=size,Foreground=Brush.Parse("#E4EDF5"),TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,0,0,10)};
        public static Button Button(string label,Action action){var b=new Button{Content=label,Margin=new Thickness(0,4,0,8),Padding=new Thickness(16,10),HorizontalAlignment=HorizontalAlignment.Left};b.Click+=(s,e)=>action();return b;}
        public static StackPanel Heading(string eyebrow,string title,string description){var p=new StackPanel{Margin=new Thickness(30,24,30,18),Spacing=8};p.Children.Add(Text(eyebrow,11));p.Children.Add(Text(title,28));p.Children.Add(Text(description,13));return p;}
        public static ScrollViewer Scroll(Control content)=>new ScrollViewer{Content=content,HorizontalScrollBarVisibility=Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled};
        static Window MessageDialog(string title,string text,bool confirmation)
        {
            var dialog=new Window{Title=title,Width=480,SizeToContent=SizeToContent.Height,CanResize=false,
                WindowStartupLocation=WindowStartupLocation.CenterOwner,Background=PageTheme.Brush("#102031"),FontFamily=new FontFamily("Arial")};
            // Owned windows have a separate style scope. Load the same translated
            // button templates used by the shared pages instead of Fluent defaults.
            Ui.Theme(dialog);
            var panel=new StackPanel{Margin=new Thickness(24),Spacing=24};
            panel.Children.Add(new ScrollViewer{Content=PageTheme.Text(text,14),MaxHeight=360,
                HorizontalScrollBarVisibility=Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled});
            var actions=new StackPanel{Orientation=Orientation.Horizontal,HorizontalAlignment=HorizontalAlignment.Right,Spacing=12};
            if(confirmation){
                var cancel=new Button{Content="Cancel",MinWidth=100,IsCancel=true};
                cancel.Click+=(s,e)=>dialog.Close(false);PageTheme.Secondary(cancel);actions.Children.Add(cancel);
            }
            var accept=new Button{Content=confirmation?"Continue":"Close",MinWidth=100};
            accept.Click+=(s,e)=>dialog.Close(true);PageTheme.Primary(accept);actions.Children.Add(accept);
            panel.Children.Add(actions);dialog.Content=new Border{Background=dialog.Background,Child=panel};
            return dialog;
        }
        public Task<bool> Confirm(string title,string text)=>MessageDialog(title,text,true).ShowDialog<bool>(this);
        async Task Render()
        {
            string mode=args.First(a=>a.StartsWith("--render-"));int at=Array.IndexOf(args,mode);string output=args[at+1];
            if(mode.Contains("confirm")||mode.Contains("alert")){
                string message="This replaces the game runtime and restarts the launcher. Saves, game data and settings are preserved.";
                if(mode.Contains("long"))message+="\n\n"+string.Join("\n\n",Enumerable.Repeat("Matching files will be updated. Other files are kept. You can cancel before installation begins.",8));
                var dialog=MessageDialog(mode.Contains("alert")?"TimeSplitters":"Install v0.3.1-alpha.1?",message,!mode.Contains("alert"));
                var shown=dialog.ShowDialog<bool>(this);await Task.Delay(350);
                var body=(Control)dialog.Content;
                using(var bitmap=new RenderTargetBitmap(PixelSize.FromSize(body.Bounds.Size,1),new Vector(96,96))){bitmap.Render(body);bitmap.Save(output);}
                dialog.Close(false);await shown;Close();return;
            }
            if(mode.Contains("settings")){ShowSettings();if(mode.Contains("graphics"))settings.SelectTab(1);if(mode.Contains("discord"))settings.SelectTab(2);}
            if(mode.Contains("repair")||mode.Contains("diagnostics"))Maintenance(mode.Contains("diagnostics"));
            if(mode.Contains("versions")){versionsPage=new NativeVersions(root,client,()=>false,Install);Find<ContentControl>("VersionsHost").Content=versionsPage.View;Page("VersionsHost");}
            if(mode.Contains("news")){newsPage=new NativeNews(root,client,()=>Preview);Find<ContentControl>("NewsHost").Content=newsPage.View;Page("NewsHost");}
            if(mode.Contains("game-data"))Find<Expander>("GameDataExpander").IsExpanded=true;
            if(mode.Contains("download")){
                var snapshot=new TransferSnapshot{Phase="Downloading",Bytes=(long)(22.2*1048576),Total=(long)(84.5*1048576),BytesPerSecond=1.7*1048576};
                Label("ReleaseTitle","Available: v0.3.1-alpha.1");Label("ReleaseDescription","Available to install. Open Home to install the game runtime.");Label("UpdateStateLabel","CHECKED");
                Label("TransferDetails",snapshot.Summary);Find<Control>("TransferDetails").IsVisible=true;
                Find<ProgressBar>("UpdateProgress").Value=snapshot.Percent;Find<Control>("UpdateProgress").IsVisible=true;
                Find<Control>("PauseDownloadButton").IsVisible=true;Find<Control>("UpdateButton").IsVisible=false;Find<Control>("CheckButton").IsVisible=false;
            }
            int width=mode.Contains("compact")?1000:1152,height=mode.Contains("compact")?720:772;
            Width=width;Height=height;
            await Task.Delay(350);var visual=(Control)Content;visual.Measure(new Size(width,height));visual.Arrange(new Rect(0,0,width,height));
            if(mode.Contains("scrolled")){var scroll=Find<ScrollViewer>("HomeCardsScroll");scroll.Offset=new Vector(0,scroll.Extent.Height);await Task.Delay(100);}
            using(var bitmap=new RenderTargetBitmap(new PixelSize(width,height),new Vector(96,96))){bitmap.Render(visual);bitmap.Save(output);}Close();
        }
    }
}
