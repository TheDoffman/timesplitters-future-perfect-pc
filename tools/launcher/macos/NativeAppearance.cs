using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.LogicalTree;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Input;
using Avalonia.Automation;
using Avalonia.Animation;
using Avalonia.Animation.Easings;
using Avalonia.Styling;

namespace TimeSplitters.Launcher {
public sealed partial class NativeWindow {
    NativeAppearance appearance;
    internal void AppearanceMaintenanceChanged(){appearance?.Page("MaintenanceHost");}
    Release[] appearanceCatalog=Array.Empty<Release>();
    // Presentation delegates to the original native commands and process guard.
    internal sealed class NativeAppearance {
        readonly NativeWindow owner;
        readonly Grid shell=new Grid(),home=new Grid(),pages=new Grid(),shade=new Grid();
        readonly Grid originalHome;
        readonly Border dialog=new Border(),channelPanel=new Border();
        readonly GameVersionMenuPage versionMenu=new GameVersionMenuPage();
        StackPanel channelBody {get{return versionMenu.View;}}
        readonly UniformGrid tabs=new UniformGrid {Rows=1,Columns=6};
        readonly Dictionary<string,Button> navigation=new Dictionary<string,Button>();
        readonly TextBlock title=Copy("Launcher tools",22),version=Copy("Game version ▾",11),kind=Copy("READY",9);
        readonly Button primary,channel;
        readonly string[] hosts={"SettingsHost","SavesHost","NewsHost","VersionsHost","MaintenanceHost"};
        readonly Avalonia.Threading.DispatcherTimer timer=new Avalonia.Threading.DispatcherTimer {Interval=TimeSpan.FromSeconds(1)};
        string current="Home";bool refresh,tools;
        CancellationTokenSource fade;
        readonly List<Control> loops=new List<Control>();
        CancellationTokenSource motion,pulse;
        bool moving,pulsing;
        string setupDataStamp;bool setupDataReady;bool setupActionRunning;LauncherTutorialPage tutorial;TutorialShade tutorialShade;Grid tutorialLayer;Border tutorialHighlight;Button replayTutorial;Control toolsAnchor,newsAnchor;
        static IBrush Ink(string color)=>Brush.Parse(color);
        static TextBlock Copy(string value,double size)=>new TextBlock {Text=value,FontSize=size,Foreground=Ink("#B4C7D8"),TextWrapping=TextWrapping.Wrap};
        static Button Action(string label,System.Action click){var b=new Button {Content=label,MinHeight=38,Padding=new Thickness(12,9),Margin=new Thickness(3),Background=Ink("#173A59"),Foreground=Ink("#DFE9F2"),BorderBrush=Ink("#4779A0"),BorderThickness=new Thickness(1),HorizontalContentAlignment=HorizontalAlignment.Center};AutomationProperties.SetName(b,label);b.Click+=(s,e)=>click();return b;}
        static void Detach(Control view){if(view.Parent is Panel p)p.Children.Remove(view);else if(view.Parent is Decorator d)d.Child=null;else if(view.Parent is ContentControl c)c.Content=null;view.ClearValue(Grid.RowProperty);view.ClearValue(Grid.ColumnProperty);view.ClearValue(Grid.RowSpanProperty);view.ClearValue(Grid.ColumnSpanProperty);}
        internal NativeAppearance(NativeWindow owner){
            this.owner=owner;
#if LINUX
            if(LinuxHandheld.Profile.Enabled)owner.LayoutUpdated+=(s,e)=>LinuxHandheld.Style(shell);
#endif
            originalHome=owner.Find<Grid>("CommandCenter");Detach(originalHome);Restyle.Apply(originalHome);MinimalTools.Polish(originalHome,"Installation");
            originalHome.RowDefinitions[1].Height=new GridLength(0);originalHome.RowDefinitions[2].Height=new GridLength(0);originalHome.RowDefinitions[3].Height=new GridLength(0);
            foreach(var item in originalHome.Children.Where(c=>Grid.GetRow(c)==0||Grid.GetRow(c)==2))item.IsVisible=false;
            var cards=(Grid)owner.Find<ScrollViewer>("HomeCardsScroll").Content;cards.ColumnDefinitions.Clear();cards.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});cards.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});int cardRow=0;foreach(var card in cards.Children){Grid.SetColumn(card,0);Grid.SetRow(card,cardRow++);}
            foreach(var text in originalHome.GetLogicalDescendants().OfType<TextBlock>())if(text.Foreground is ISolidColorBrush ink&&ink.Color.R<40&&ink.Color.G<40&&ink.Color.B<40)text.Foreground=Ink("#E4EDF5");
            foreach(var button in originalHome.GetLogicalDescendants().OfType<Button>())PageTheme.Secondary(button);
            owner.Find<Control>("DiscordCommunityCard").IsVisible=false;owner.Find<Control>("ReleaseNotesButton").IsVisible=false;owner.Find<Control>("PlayButton").IsVisible=false;
            shell.Background=Ink("#071321");shell.ClipToBounds=true;owner.Content=shell;shell.Children.Add(home);
            owner.Title="TimeSplitters — Launcher "+LauncherVersion.Value;
            var background=owner.Find<Image>("HeroBackdrop");Detach(background);background.Stretch=Stretch.UniformToFill;background.Opacity=.12;home.Children.Add(background);
            var art=owner.Find<Image>("HeroPhoto");Detach(art);art.Width=680;art.Stretch=Stretch.Uniform;art.HorizontalAlignment=HorizontalAlignment.Right;art.VerticalAlignment=VerticalAlignment.Top;art.Margin=new Thickness(0,38,-45,0);art.Opacity=.30;art.OpacityMask=new LinearGradientBrush {StartPoint=new RelativePoint(0,0,RelativeUnit.Relative),EndPoint=new RelativePoint(1,0,RelativeUnit.Relative),GradientStops=new GradientStops {new GradientStop(Colors.Transparent,0),new GradientStop(Colors.White,.35),new GradientStop(Colors.White,.8),new GradientStop(Colors.Transparent,1)}};home.Children.Add(art);
            var rings=new Grid {Width=480,Height=480,Opacity=.25,IsHitTestVisible=false,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};
            foreach(int size in new[]{470,410,340,180,145})rings.Children.Add(new Avalonia.Controls.Shapes.Ellipse {Width=size,Height=size,HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center,Stroke=Ink("#85D4FF"),StrokeThickness=1,StrokeDashArray=size==470?new Avalonia.Collections.AvaloniaList<double>{1,9}:size==410?new Avalonia.Collections.AvaloniaList<double>{24,7,1,9}:null});home.Children.Add(rings);
            var logo=owner.Find<Image>("HeroTitle");Detach(logo);logo.Width=600;logo.MaxWidth=600;logo.Stretch=Stretch.Uniform;logo.HorizontalAlignment=HorizontalAlignment.Center;logo.VerticalAlignment=VerticalAlignment.Center;home.Children.Add(logo);
            if(logo.Source==null)home.Children.Add(new TextBlock {Text="TIMESPLITTERS\nFUTURE PERFECT",FontSize=42,Foreground=Ink("#DFE9F2"),HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center});
            var header=Copy("LAUNCHER v"+LauncherVersion.Value+" · "+LauncherPlatform.HostName,9);
#if LINUX
            if(LinuxHandheld.Profile.Enabled)header.Text+=" · SteamOS support: experimental";
#endif
            header.Margin=new Thickness(28,14,28,0);header.VerticalAlignment=VerticalAlignment.Top;home.Children.Add(header);
            home.Children.Add(new Border {Height=1,Background=Ink("#294E70"),Margin=new Thickness(0,38,0,0),VerticalAlignment=VerticalAlignment.Top});
            var dock=new Grid {ColumnDefinitions=new ColumnDefinitions("*,80,8,260"),Height=108,Margin=new Thickness(28,0,28,18),VerticalAlignment=VerticalAlignment.Bottom};home.Children.Add(dock);
            var operation=new Grid {RowDefinitions=new RowDefinitions("*,3"),Height=32,Margin=new Thickness(0,0,22,0),VerticalAlignment=VerticalAlignment.Bottom};dock.Children.Add(operation);
            var row=new Grid {ColumnDefinitions=new ColumnDefinitions("Auto,*"),Margin=new Thickness(0,0,0,8)};operation.Children.Add(row);row.Children.Add(kind);
            var status=owner.Find<TextBlock>("StatusText");Detach(status);status.FontSize=10;status.TextWrapping=TextWrapping.NoWrap;status.TextTrimming=TextTrimming.CharacterEllipsis;status.HorizontalAlignment=HorizontalAlignment.Right;Grid.SetColumn(status,1);row.Children.Add(status);
            var progress=owner.Find<ProgressBar>("UpdateProgress");Detach(progress);progress.Height=3;Grid.SetRow(progress,1);operation.Children.Add(progress);
            var details=owner.Find<TextBlock>("TransferDetails");Detach(details);details.FontSize=10;details.TextTrimming=TextTrimming.CharacterEllipsis;details.MaxHeight=28;details.Margin=new Thickness(28,0,390,64);details.VerticalAlignment=VerticalAlignment.Bottom;home.Children.Add(details);
            var links=new StackPanel {Orientation=Orientation.Horizontal,Spacing=20,VerticalAlignment=VerticalAlignment.Bottom,Margin=new Thickness(0,0,0,48)};dock.Children.Add(links);
            foreach(var pair in new[]{("What's new →",(System.Action)(()=>Navigate("What's new"))),("Join Discord ↗",()=>NativeHost.Open(ReleaseClient.DiscordInvite)),("View on GitHub ↗",()=>NativeHost.Open(ReleaseClient.ReleasesPage.Replace("/releases","")))}){var b=Action(pair.Item1,pair.Item2);b.FontSize=12;b.Padding=new Thickness(0);b.Background=Brushes.Transparent;b.BorderThickness=new Thickness(0);b.Foreground=Ink("#85D4FF");b.Margin=new Thickness(0);links.Children.Add(b);}
            var gear=Action("⚙",()=>Navigate("Installation"));gear.Content=new Avalonia.Controls.Shapes.Path {Data=Geometry.Parse("M9.4,3.2 L14.6,3.2 L15.1,5.4 L16.3,6.1 L18.4,5.4 L21,9.9 L19.4,11.4 L19.4,12.8 L21,14.3 L18.4,18.8 L16.3,18.1 L15.1,18.8 L14.6,21 L9.4,21 L8.9,18.8 L7.7,18.1 L5.6,18.8 L3,14.3 L4.6,12.8 L4.6,11.4 L3,9.9 L5.6,5.4 L7.7,6.1 L8.9,5.4 Z M15,12 A3,3 0 1 1 9,12 A3,3 0 1 1 15,12"),Stroke=Ink("#D6E3F0"),StrokeThickness=1.5,Stretch=Stretch.Uniform,Width=26,Height=26};gear.Margin=new Thickness(0);gear.Height=80;gear.VerticalAlignment=VerticalAlignment.Bottom;AutomationProperties.SetName(gear,"Launcher tools and game settings");Grid.SetColumn(gear,1);dock.Children.Add(gear);
            primary=Action("Play",Activate);primary.Margin=new Thickness(0);primary.Height=80;primary.FontSize=18;primary.VerticalAlignment=VerticalAlignment.Bottom;primary.Background=Ink("#85D4FF");primary.Foreground=Ink("#091C2C");Grid.SetColumn(primary,3);dock.Children.Add(primary);
            channel=Action("Game channel and version",()=>{if(channelPanel.IsVisible)CloseChannel();else Channels();});channel.Content=version;channel.Background=Brushes.Transparent;channel.BorderThickness=new Thickness(0);channel.Margin=new Thickness(0);channel.Padding=new Thickness(0);channel.MinHeight=24;channel.VerticalAlignment=VerticalAlignment.Top;channel.HorizontalAlignment=HorizontalAlignment.Right;Grid.SetColumn(channel,3);dock.Children.Add(channel);
#if LINUX
            if(LinuxHandheld.Profile.Enabled){dock.Height=132;links.Margin=new Thickness(0,0,0,52);channel.MinHeight=44;channelPanel.Margin=new Thickness(0,0,28,154);var help=Copy("A Select   B Back   LB/RB Focus   Y Keyboard   Right stick Scroll",13);handheldHint=help;help.Margin=new Thickness(28,43,28,0);help.VerticalAlignment=VerticalAlignment.Top;home.Children.Add(help);}
#endif
            toolsAnchor=gear;newsAnchor=links;
            var texture=owner.Find<TextBlock>("TextureUpdateStatus");Detach(texture);texture.FontSize=10;texture.Margin=new Thickness(28,0,390,35);texture.VerticalAlignment=VerticalAlignment.Bottom;home.Children.Add(texture);
            shade.Background=Ink("#E60B1420");shade.IsVisible=false;shell.Children.Add(shade);dialog.Background=Ink("#0D1926");dialog.BorderBrush=Ink("#36506A");dialog.BorderThickness=new Thickness(1);dialog.Margin=new Thickness(22,48,22,20);shade.Children.Add(dialog);
            var frame=new Grid {RowDefinitions=new RowDefinitions("56,40,*")};dialog.Child=frame;
            var heading=new DockPanel {Margin=new Thickness(26,4,20,0)};var back=Action("← Home",Home);DockPanel.SetDock(back,Dock.Right);heading.Children.Add(back);
            replayTutorial=Action("Launcher tour",()=>ShowTutorial(true));replayTutorial.Name="ReplayLauncherTour";replayTutorial.FontSize=11;replayTutorial.Background=Brushes.Transparent;replayTutorial.BorderThickness=new Thickness(0);replayTutorial.Foreground=Ink("#85D4FF");DockPanel.SetDock(replayTutorial,Dock.Right);heading.Children.Add(replayTutorial);
#if LINUX
            if(LinuxHandheld.Profile.Enabled){var deckHelp=Action("Deck help",()=>owner.ShowHandheldHelp());DockPanel.SetDock(deckHelp,Dock.Right);heading.Children.Add(deckHelp);}
#endif
            title.VerticalAlignment=VerticalAlignment.Center;heading.Children.Add(title);frame.Children.Add(heading);
            foreach(string page in new[]{"Installation","Game settings","Saves","Repair","Diagnostics","Console"}){string captured=page;var b=Action(page,()=>Navigate(captured));b.FontSize=12;b.Margin=new Thickness(0);b.Padding=new Thickness(6);tabs.Children.Add(b);navigation[page]=b;}tabs.Margin=new Thickness(26,0,26,0);Grid.SetRow(tabs,1);frame.Children.Add(tabs);Grid.SetRow(pages,2);frame.Children.Add(pages);
            pages.Children.Add(originalHome);originalHome.IsVisible=false;
            foreach(string host in hosts){var control=owner.Find<ContentControl>(host);Detach(control);control.Margin=new Thickness(0);pages.Children.Add(control);}
            versionMenu.Opened+=()=>{channelPanel.MaxHeight=Math.Max(240,owner.ClientSize.Height-170);channelPanel.IsVisible=true;};
            versionMenu.Closed+=()=>channelPanel.IsVisible=false;
            versionMenu.Browse+=()=>Navigate("Versions");versionMenu.Selected+=release=>owner.Run(()=>owner.Install(release));
            owner.Find<Control>("GameVersionsButton").IsVisible=false;
            channelPanel.Child=new ScrollViewer {Content=channelBody,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};channelPanel.Width=360;channelPanel.MaxHeight=460;channelPanel.Padding=new Thickness(16);channelPanel.Background=Ink("#101E2D");channelPanel.BorderBrush=Ink("#4779A0");channelPanel.BorderThickness=new Thickness(1);channelPanel.Margin=new Thickness(0,0,28,128);channelPanel.HorizontalAlignment=HorizontalAlignment.Right;channelPanel.VerticalAlignment=VerticalAlignment.Bottom;channelPanel.IsVisible=false;shell.Children.Add(channelPanel);KeyboardNavigation.SetTabNavigation(channelPanel,KeyboardNavigationMode.Cycle);
            tutorial=new LauncherTutorialPage(owner.root,owner.render||File.Exists(Core.PathIn(owner.root,LauncherPlatform.GamePath(owner.root)))&&SetupDataValid(),!owner.render);
            tutorialLayer=new Grid {Background=Brushes.Transparent,Margin=new Thickness(0,38,0,0),IsVisible=false};tutorialShade=new TutorialShade {IsHitTestVisible=false};tutorialLayer.Children.Add(tutorialShade);tutorialHighlight=new Border {BorderBrush=Ink("#85D4FF"),BorderThickness=new Thickness(2),HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Top,IsHitTestVisible=false};tutorialLayer.Children.Add(tutorialHighlight);tutorialLayer.Children.Add(tutorial.View);tutorial.View.Margin=new Thickness(24);shell.Children.Add(tutorialLayer);KeyboardNavigation.SetTabNavigation(tutorialLayer,KeyboardNavigationMode.Cycle);
            tutorial.SetupRequested+=SetupAction;tutorial.Changed+=PlaceTutorial;tutorial.Warning+=owner.Status;tutorial.Closed+=()=>{tutorialLayer.IsVisible=false;home.IsHitTestVisible=true;primary.Focus();Refresh();};owner.SizeChanged+=(s,e)=>PlaceTutorial();owner.LayoutUpdated+=(s,e)=>PlaceTutorial();
            owner.KeyDown+=(s,e)=>{if(e.Key==Key.Escape){if(tutorial.IsOpen)tutorial.Dismiss();else if(channelPanel.IsVisible){CloseChannel();channel.Focus();}else if(shade.IsVisible)Home();e.Handled=true;}};
            loops.Add(rings);loops.Add(art);loops.Add(background);timer.Tick+=(s,e)=>{Refresh();if(!tutorial.IsOpen)ShowTutorial(false);};KeyboardNavigation.SetTabNavigation(shade,KeyboardNavigationMode.Cycle);
            owner.Opened+=(s,e)=>{timer.Start();
#if LINUX
                if(LinuxHandheld.Profile.Enabled)primary.Focus();
#endif
            };owner.Closed+=(s,e)=>{timer.Stop();fade?.Cancel();motion?.Cancel();pulse?.Cancel();};
            Refresh();
        }
#if LINUX
        internal Control HandheldScope {get{if(tutorialLayer.IsVisible)return tutorial.View;if(channelPanel.IsVisible)return channelPanel;if(PromptOpen)return (Border)typeof(SettingsPage).GetField("overlay",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(owner.settings);return shade.IsVisible?shade:home;}}
        internal void HandheldBack(){if(tutorial.IsOpen)tutorial.Dismiss();else if(channelPanel.IsVisible){CloseChannel();channel.Focus();}else if(PromptOpen)owner.settings.DismissPrompt();else if(shade.IsVisible)Home();else if(owner.busy)owner.Status("Pause or finish the current operation before closing the launcher.");else owner.Run(async()=>{if(await owner.Confirm("Close launcher?","Return to Steam or your desktop?"))owner.Close();});}
#endif
#if LINUX
        TextBlock handheldHint;
#endif
        bool PromptOpen=>owner.settings!=null&&owner.Find<Control>("SettingsHost").IsVisible&&(Border)typeof(SettingsPage).GetField("overlay",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(owner.settings) is Border overlay&&overlay.IsVisible;
        bool SetupDataValid(){
            try{
                string data=Core.PathIn(owner.root,LauncherPlatform.Data(owner.root));var xbe=new FileInfo(Path.Combine(data,"default.xbe"));
                string stamp=(xbe.Exists?xbe.Length+":"+xbe.LastWriteTimeUtc.Ticks:"missing")+":"+Directory.GetLastWriteTimeUtc(Path.Combine(data,"pak")).Ticks+":"+Directory.GetLastWriteTimeUtc(Path.Combine(data,"xmv")).Ticks;
                if(stamp!=setupDataStamp){setupDataStamp=stamp;setupDataReady=false;try{NativeHost.CheckData(owner.root);setupDataReady=true;}catch(Exception){}}
                return setupDataReady;
            }catch(Exception){return false;}
        }
        async void SetupAction(string action){
            if(owner.render||setupActionRunning||owner.busy||owner.AppearanceGameRunning)return;setupActionRunning=true;
            tutorialLayer.IsVisible=false;home.IsHitTestVisible=true;
            try{
                if(action=="settings"){Navigate("Game settings");return;}
                if(action=="review"){tutorial.Dismiss();Navigate("Installation");owner.Find<Expander>("GameDataExpander").IsExpanded=true;return;}
                if(action=="install"){if(owner.Find<Button>("UpdateButton").IsEnabled)await owner.Install(owner.latest);else await owner.Check();}
                else if(action=="iso"||action=="folder")await owner.Import(action=="folder");
                else if(action=="play")await owner.Play();
            }catch(OperationCanceledException){owner.Status("Download paused. Choose Install game to resume.");}
            catch(Exception ex){owner.Status(ex.Message);}
            finally{
                setupActionRunning=false;Refresh();
                if(tutorial.IsOpen&&owner.IsVisible&&!NativeStartup.HandingOff){Home();tutorialLayer.IsVisible=true;home.IsHitTestVisible=false;PlaceTutorial();tutorial.Next.Focus();}
            }
        }
        void ShowTutorial(bool manual){
            if(owner.busy||owner.AppearanceGameRunning||PromptOpen)return;
            if(!manual&&(owner.render||!owner.IsActive||current!="Home"||channelPanel.IsVisible||owner.WindowState==WindowState.Minimized||!tutorial.ShouldOffer))return;
            Home();CloseChannel();tutorialLayer.IsVisible=true;home.IsHitTestVisible=false;tutorial.Open();PlaceTutorial();tutorial.Next.Focus();
        }
        void PlaceTutorial(){
            if(tutorial==null||!tutorial.IsOpen)return;tutorial.View.MaxHeight=Math.Max(240,shell.Bounds.Height-84);
            Control anchor=tutorial.Target=="play"?primary:tutorial.Target=="versions"?channel:tutorial.Target=="tools"?toolsAnchor:tutorial.Target=="news"?newsAnchor:null;tutorialHighlight.IsVisible=anchor!=null;if(anchor==null)tutorialShade.SetSpotlight(null);
            if(anchor!=null){var point=anchor.TranslatePoint(new Point(0,0),shell);if(point.HasValue){tutorialHighlight.Width=anchor.Bounds.Width+12;tutorialHighlight.Height=anchor.Bounds.Height+12;tutorialHighlight.Margin=new Thickness(Math.Max(0,point.Value.X-6),Math.Max(0,point.Value.Y-44),0,0);tutorialShade.SetSpotlight(new Rect(tutorialHighlight.Margin.Left,tutorialHighlight.Margin.Top,tutorialHighlight.Width,tutorialHighlight.Height));}}
        }
        internal void PreviewTutorial(int step){ShowTutorial(true);tutorial.PreviewStep(step);}
        void Home(){if(PromptOpen){owner.settings.DismissPrompt();return;}owner.Page("CommandCenter");}
        internal void Navigate(string page){if(PromptOpen){owner.Status("Finish or cancel the settings dialog first.");return;}if(owner.busy)return;tools=page=="Installation";try{switch(page){case "Installation":owner.Page("CommandCenter");break;case "Game settings":owner.ShowSettings();break;case "Saves":owner.ShowSaves();break;case "What's new":owner.Run(owner.News);break;case "Versions":owner.Run(owner.Versions);break;case "Console":owner.ShowConsole();break;default:owner.Maintenance(page=="Diagnostics");break;}}finally{tools=false;}}
        internal void Page(string host){
            string page=host=="CommandCenter"?(tools?"Installation":"Home"):host=="SettingsHost"?"Game settings":host=="SavesHost"?"Saves":host=="NewsHost"?"What's new":host=="VersionsHost"?"Versions":owner.maintenance?.SelectedPage??"Repair";
            CloseChannel();originalHome.IsVisible=page=="Installation";shade.IsVisible=page!="Home";home.IsEnabled=page=="Home";title.Text=page=="Installation"?"Launcher tools":page;
            tabs.IsVisible=page!="Game settings"&&page!="What's new"&&page!="Versions";replayTutorial.IsVisible=tabs.IsVisible;((Grid)dialog.Child).RowDefinitions[1].Height=tabs.IsVisible?new GridLength(40):new GridLength(0);
            if(owner.settings!=null)MinimalSettings.Apply(owner.settings,()=>Navigate("Installation"));
            if(owner.savesPage!=null){Restyle.Apply(owner.savesPage.View);MinimalTools.Polish(owner.savesPage.View,"Saves");Empty(owner.savesPage.View,"SaveBackupChoices","No backups yet. Create a backup here before changing versions or experimenting with saves.");}
            if(owner.maintenance!=null){MinimalTools.Apply(owner.maintenance.Area);if(page=="Diagnostics")MinimalTools.ResetDiagnostics(pages);}
            Restyle.Apply(pages);foreach(var pair in navigation){bool selected=pair.Key==page;pair.Value.Background=Brushes.Transparent;pair.Value.Foreground=Ink(selected?"#65C7FF":"#B4C7D8");pair.Value.BorderThickness=new Thickness(0,0,0,selected?2:1);}
            if(current!=page&&MotionAllowed){fade?.Cancel();fade=new CancellationTokenSource();_ = Fade(pages,fade.Token);}current=page;Refresh();
        }
        async Task Fade(Control view,CancellationToken token){try{var animation=new Animation {Duration=TimeSpan.FromMilliseconds(140),Children={new KeyFrame {Cue=new Cue(0),Setters={new Setter(Visual.OpacityProperty,.25)}},new KeyFrame {Cue=new Cue(1),Setters={new Setter(Visual.OpacityProperty,1d)}}}};await animation.RunAsync(view,token);}catch(OperationCanceledException){} }
        string PrimaryAction=>owner.operation!=null?"PauseDownloadButton":owner.Find<Button>("PlayButton").IsEnabled?"PlayButton":owner.Find<Button>("UpdateButton").IsEnabled?"UpdateButton":File.Exists(Core.PathIn(owner.root,LauncherPlatform.GamePath(owner.root)))&&!File.Exists(Core.PathIn(owner.root,LauncherPlatform.Data(owner.root)+"/default.xbe"))?"ImportButton":"CheckButton";
        string InstalledCaption {get{if(owner.render)return "Stable · local build";try{string tag=NativeHost.Version(owner.root);return tag=="Not installed"?"Choose game version":GameChannels.Name(GameChannels.Installed(owner.root))+" · "+tag;}catch(Exception){return "Unknown version · review Diagnostics";}}}
        void Activate(){if(owner.gameRunning||!primary.IsEnabled)return;switch(PrimaryAction){case "PauseDownloadButton":owner.operation?.Cancel();break;case "PlayButton":owner.Run(owner.Play);break;case "UpdateButton":owner.Run(()=>owner.Install(owner.latest));break;case "ImportButton":owner.Run(()=>owner.Import(false));break;default:owner.Run(owner.Check);break;}}
        internal void Refresh(){if(refresh)return;refresh=true;try{
            bool running=owner.AppearanceGameRunning;owner.AppearanceRefreshPlayAvailability(running);bool installed=File.Exists(Core.PathIn(owner.root,LauncherPlatform.GamePath(owner.root)));bool canPlay=owner.Find<Button>("PlayButton").IsEnabled;bool canUpdate=owner.Find<Button>("UpdateButton").IsEnabled;
            string action=running?"●  Playing":owner.operation!=null?Convert.ToString(owner.Find<Button>("PauseDownloadButton").Content):canPlay?"Play":canUpdate?Convert.ToString(owner.Find<Button>("UpdateButton").Content):installed?"Import game data":"Check for updates";
            primary.Content=action.ToUpperInvariant();primary.IsEnabled=!running&&(!owner.busy||owner.operation!=null)&&owner.Find<Button>(PrimaryAction).IsEnabled;primary.Background=Ink(running?"#667583":"#85D4FF");AutomationProperties.SetName(primary,running?"Playing":action);kind.Text=running?"GAME RUNNING":owner.busy?"OPERATION IN PROGRESS":"READY";
            UpdateVersionMenu();channel.IsEnabled=!owner.busy&&!running;version.Text=InstalledCaption+" ▾";
            foreach(var pair in navigation)pair.Value.IsEnabled=!owner.busy&&(!running||pair.Key=="Diagnostics"||pair.Key=="Console");
            if(tutorial!=null)tutorial.UpdateSetup(installed,installed&&SetupDataValid(),Directory.Exists(Core.PathIn(owner.root,LauncherPlatform.Data(owner.root))),canUpdate,owner.Find<TextBlock>("StatusText").Text);
            if(replayTutorial!=null)replayTutorial.IsEnabled=!owner.busy&&!running;
            Empty(pages,"GameConsoleOutput","Game output appears here after you launch the game. Use the filters above to focus on errors.");
            var report=pages.GetLogicalDescendants().OfType<Button>().Where(b=>b.Name=="OpenCrashReport"||b.Name=="ExportCrashReport");if(!owner.busy){bool available=CrashReports.Latest(owner.root)!=null;foreach(var b in report)b.IsEnabled=available;}
            var disclosure=pages.GetLogicalDescendants().OfType<Expander>().FirstOrDefault(e=>e.Name=="DiagnosticsSupportOptions");if(disclosure?.Content is StackPanel options){var note=options.Children.OfType<TextBlock>().FirstOrDefault(t=>t.Name=="NoGameReports");if(note==null){note=Copy("No game reports yet. Reports appear here when a game session creates one. Nothing is uploaded automatically.",12);note.Name="NoGameReports";note.Margin=new Thickness(0,8);options.Children.Insert(0,note);}note.IsVisible=CrashReports.Latest(owner.root)==null;}
#if LINUX
            if(LinuxHandheld.Profile.Enabled){if(handheldHint!=null&&owner.HandheldActive)handheldHint.Text=owner.HandheldInputHint;LinuxHandheld.Style(shell);((Grid)dialog.Child).RowDefinitions[1].Height=tabs.IsVisible?new GridLength(48):new GridLength(0);}
#endif
            bool active=MotionAllowed&&owner.IsVisible&&owner.WindowState!=WindowState.Minimized&&home.IsEnabled&&!channelPanel.IsVisible&&!(tutorial?.IsOpen??false);
            if(moving!=active){motion?.Cancel();motion?.Dispose();motion=null;moving=active;if(active){motion=new CancellationTokenSource();_ = Loop(loops[0],RotateTransform.AngleProperty,0,360,90,false,motion.Token);_ = Loop(loops[1],TranslateTransform.XProperty,-3,3,37,true,motion.Token);_ = Loop(loops[2],TranslateTransform.XProperty,-6,6,32,true,motion.Token);}}
            bool playing=active&&running;if(pulsing!=playing){pulse?.Cancel();pulse?.Dispose();pulse=null;pulsing=playing;if(playing){pulse=new CancellationTokenSource();_ = Loop(primary,Visual.OpacityProperty,.72,1,2,true,pulse.Token);}}
        }finally{refresh=false;}}
        bool MotionAllowed=>!owner.render&&!NativeMotion.ReduceMotion()&&(owner.PlatformSettings?.GetColorValues().ContrastPreference??Avalonia.Platform.ColorContrastPreference.NoPreference)==Avalonia.Platform.ColorContrastPreference.NoPreference;
        static async Task Loop(Control target,AvaloniaProperty property,double from,double to,int seconds,bool alternate,CancellationToken token){try{var animation=new Animation {Duration=TimeSpan.FromSeconds(seconds),IterationCount=new IterationCount(alternate?2UL:1UL),PlaybackDirection=alternate?PlaybackDirection.Alternate:PlaybackDirection.Normal,Children={new KeyFrame {Cue=new Cue(0),Setters={new Setter(property,from)}},new KeyFrame {Cue=new Cue(1),Setters={new Setter(property,to)}}}};while(!token.IsCancellationRequested)await animation.RunAsync(target,token);}catch(OperationCanceledException){}}
        internal static void Empty(Control root,string name,string description){var target=root.GetLogicalDescendants().OfType<Control>().FirstOrDefault(c=>c.Name==name);if(target==null)return;
            if(target.Parent is Border border){border.Child=null;var holder=new Grid();holder.Children.Add(target);border.Child=holder;}
            if(!(target.Parent is Panel parent))return;var note=parent.Children.OfType<TextBlock>().FirstOrDefault(c=>c.Name==name+"Empty");if(note==null){note=Copy(description,12);note.Name=name+"Empty";note.Margin=new Thickness(20);note.IsHitTestVisible=false;note.VerticalAlignment=VerticalAlignment.Top;parent.Children.Add(note);}note.IsVisible=target is ComboBox choices?choices.ItemCount==0:target is TextBox output?string.IsNullOrWhiteSpace(output.Text):false;
        }
        void Channels(){UpdateVersionMenu();versionMenu.ShowMain();}
        void Tracks(){UpdateVersionMenu();versionMenu.ShowPreview();}
        void CloseChannel(){versionMenu.Close();}
        void UpdateVersionMenu(){string tag="",installedChannel="stable";try{tag=Core.InstalledVersion(owner.root);installedChannel=GameChannels.Installed(owner.root);}catch(Exception){}versionMenu.Update(owner.appearanceCatalog,new Release {Tag=String.IsNullOrEmpty(tag)?(File.Exists(Core.PathIn(owner.root,LauncherPlatform.GamePath(owner.root)))?"Unversioned":"Not installed"):tag,Channel=installedChannel},owner.busy||owner.AppearanceGameRunning,owner.appearanceOffline);}
        // Uses the real native visual tree and window scaling. No network,
        // installation or personal preferences are touched by this render route.
        internal async Task Validate(string output){
            Directory.CreateDirectory(output);var evidence=new List<object>();
            Navigate("Game settings");var draft=owner.settings;var vsync=draft.View.GetLogicalDescendants().OfType<CheckBox>().Single(c=>c.Name=="VSync");bool previous=vsync.IsChecked==true;vsync.IsChecked=!previous;
            var backAction=draft.View.GetLogicalDescendants().OfType<Button>().First(b=>Convert.ToString(b.Content)=="←  Back to Launcher tools");backAction.RaiseEvent(new Avalonia.Interactivity.RoutedEventArgs(Avalonia.Controls.Button.ClickEvent));Core.Require(current=="Installation","Settings Back did not open Launcher tools.");Navigate("Game settings");Core.Require(owner.settings==draft&&vsync.IsChecked==!previous,"Settings Back lost the original draft.");vsync.IsChecked=previous;Home();
            owner.gameRunning=true;owner.Refresh();Core.Require(!primary.IsEnabled&&!channel.IsEnabled&&Convert.ToString(primary.Content).Contains("PLAYING"),"Running game did not lock Play and version selection.");owner.gameRunning=false;owner.Refresh();
            Console.WriteLine("PASS native appearance: settings Back retains the real draft; Playing and channel selection follow native process state.");
            foreach(var size in new[]{new Size(940,640),new Size(1184,684),new Size(1600,900),new Size(800,600)}){
                owner.Width=size.Width;owner.Height=size.Height;await Task.Delay(100);
                var original=new SettingsPage(new ResourceDictionary(),owner.root,true);var before=original.View.GetLogicalDescendants().OfType<TemplatedControl>().ToArray();MinimalSettings.Apply(original,()=>{});var retained=original.View.GetLogicalDescendants().OfType<TemplatedControl>().ToHashSet();Core.Require(before.All(retained.Contains),"Settings controls lost during native adaptation.");
                var cases=new[]{"Home","Installation","Controls","Graphics","Audio","View","Textures","Discord","General","Saves","Repair","Diagnostics","Console","Channels","Tracks"};
                foreach(string page in cases){
                    if(page=="Home")Home();else if(page=="Channels"){Home();Channels();}else if(page=="Tracks")Tracks();else if(new[]{"Controls","Graphics","Audio","View","Textures","Discord","General"}.Contains(page)){
                        Navigate("Game settings");int index=Array.IndexOf(new[]{"Controls","Graphics","Discord","Audio","View","General","Textures"},page);owner.settings.SelectTab(index);
                    }else Navigate(page);
                    await Task.Delay(40);var visual=(Control)owner.Content;visual.UpdateLayout();
                    if(page=="Installation")Core.Require(originalHome.GetLogicalDescendants().OfType<TextBlock>().Where(t=>t.IsEffectivelyVisible).All(t=>!(t.Foreground is ISolidColorBrush ink)||ink.Color.R>=40||ink.Color.G>=40||ink.Color.B>=40),"Installation text lost its dark-theme contrast.");
                    if(page=="Home")Core.Require(primary.Bounds.Width>=250&&primary.Bounds.Height>=70,"Main action clipped.");
                    if(tabs.IsVisible&&shade.IsVisible){var widths=tabs.Children.Select(c=>c.Bounds.Width).ToArray();Core.Require(widths.Min()>60&&widths.Max()-widths.Min()<1,"Tools tabs do not fit evenly.");}
                    if(page=="Diagnostics"){var expander=pages.GetLogicalDescendants().OfType<Expander>().Single(e=>e.Name=="DiagnosticsSupportOptions");Core.Require(!expander.IsExpanded,"Reports and support should start closed.");}
                    if(page=="Controls"){var back=pages.GetLogicalDescendants().OfType<Button>().First(b=>Convert.ToString(b.Content)=="←  Back to Launcher tools");Core.Require(back.Bounds.Width>100&&back.Bounds.Height>30,"Settings back action clipped.");}
                    var pixel=PixelSize.FromSize(visual.Bounds.Size,owner.RenderScaling);string name=page.ToLowerInvariant()+"-"+(int)size.Width;
                    using(var bitmap=new RenderTargetBitmap(pixel,new Vector(96*owner.RenderScaling,96*owner.RenderScaling))){bitmap.Render(visual);bitmap.Save(Path.Combine(output,name+".png"));}
                    evidence.Add(new {page,width=size.Width,height=size.Height,scale=owner.RenderScaling,pixels=new[]{pixel.Width,pixel.Height}});
                }
            }
            Home();bool reducedMotion=NativeMotion.ReduceMotion();
            using(var clock=new CancellationTokenSource()){var task=Loop(loops[0],RotateTransform.AngleProperty,0,360,90,false,clock.Token);await Task.Delay(250);Core.Require(Math.Abs(loops[0].RenderTransform?.Value.M12??0)>.00001,"Native decorative clock did not advance.");clock.Cancel();await task;}
            File.WriteAllText(Path.Combine(output,"checks.json"),Json.Encode(evidence));Console.WriteLine("PASS native animation clock advances and cancels; system reduced motion: "+reducedMotion);Console.WriteLine("PASS native appearance: all settings controls retained; 60 page/size renders, equal navigation, closed reports and visible primary/back actions. Actual host scale: "+owner.RenderScaling);
        }
        static double Number(Dictionary<string,object> state,string key){return state.TryGetValue(key,out var value)?Convert.ToDouble(value,System.Globalization.CultureInfo.InvariantCulture):0;}
        internal void Remember(){
            if(owner.render||owner.args.Contains("--test-session"))return;
#if LINUX
            if(LinuxHandheld.Profile.GamingMode)return;
#endif
            string path=Core.PathIn(owner.root,".launcher/window-state.json");bool ready=false,maximized=false;Size normalSize=new Size(1152,772);PixelPoint normalPosition=owner.Position;
            owner.Opened+=(s,e)=>{
                try{if(File.Exists(path)&&new FileInfo(path).Length<=8192){var state=Json.Object(File.ReadAllText(path));double w=Number(state,"width"),h=Number(state,"height");int x=(int)Number(state,"left"),y=(int)Number(state,"top");
                    if(w>=640&&w<=8192&&h>=400&&h<=8192){var screen=owner.Screens.All.FirstOrDefault(a=>a.WorkingArea.Contains(new PixelPoint(x+100,y+40)))??owner.Screens.ScreenFromWindow(owner)??owner.Screens.Primary;
                        if(screen!=null){owner.Width=Math.Min(w,screen.WorkingArea.Width/screen.Scaling-16);owner.Height=Math.Min(h,screen.WorkingArea.Height/screen.Scaling-40);var area=screen.WorkingArea;owner.Position=new PixelPoint(Math.Clamp(x,area.X,Math.Max(area.X,area.Right-(int)(owner.Width*screen.Scaling))),Math.Clamp(y,area.Y,Math.Max(area.Y,area.Bottom-(int)(owner.Height*screen.Scaling))));}
                        if(state.TryGetValue("maximized",out var value)&&value is bool flag)maximized=flag;
                    }
                }}catch(Exception ex){LauncherLog.Write(owner.root,"Window placement ignored: "+ex.GetType().Name);}
                normalSize=new Size(owner.Width,owner.Height);normalPosition=owner.Position;ready=true;if(maximized)owner.WindowState=WindowState.Maximized;
            };
            owner.PropertyChanged+=(s,e)=>{if(!ready)return;if(owner.WindowState!=WindowState.Minimized)maximized=owner.WindowState==WindowState.Maximized;if(owner.WindowState==WindowState.Normal){normalSize=new Size(owner.Width,owner.Height);normalPosition=owner.Position;}};
            owner.Closing+=(s,e)=>{if(owner.busy||owner.gameRunning||!ready)return;try{Directory.CreateDirectory(Path.GetDirectoryName(path));Core.Write(path,Json.Encode(new{width=normalSize.Width,height=normalSize.Height,left=normalPosition.X,top=normalPosition.Y,maximized}));}catch(Exception ex){LauncherLog.Write(owner.root,"Window placement not saved: "+ex.GetType().Name);}};
        }

    }
}
}
