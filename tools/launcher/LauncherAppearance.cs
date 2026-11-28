using System;
using System.Linq;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Effects;
using System.Windows.Shell;
namespace TimeSplitters.Launcher {
public sealed class LauncherAppearance {
    readonly LauncherWindow ui;readonly Window window;readonly string root;
    Grid shell,stage,shade,pages,originalHome,dialog,wizard;
    UniformGrid nav;TextBlock status,kind,version,title,toast;Button primary;ProgressBar progress;
    GameChannelMenu channels;LauncherMotion motion;GameUpdateActionPage updateChoice;Grid actionDock;bool toolsRequested,pulsing;string lastPage;
    StackPanel playingContent;System.Windows.Shapes.Ellipse playingDot;System.Windows.Threading.DispatcherTimer timer;
    readonly HashSet<MaintenanceArea> observed=new HashSet<MaintenanceArea>();
    bool setupActionRunning;LauncherTutorialPage tutorial;TutorialShade tutorialShade;Grid tutorialLayer;Border tutorialHighlight;Button replayTutorial;FrameworkElement toolsAnchor,newsAnchor;
    static readonly string[] Hosts={"SettingsHost","SavesHost","NewsHost","VersionsHost","MaintenanceHost"};
    internal Button MainAction {get{return primary;}}
    public LauncherAppearance(LauncherWindow owner){ui=owner;window=ui.Window;root=ui.AppearanceRoot;ui.Appearance=this;Build();}
    T Find<T>(string name) where T:class {return window.FindName(name) as T;}
    static Brush Ink(string value){return (Brush)new BrushConverter().ConvertFromString(value);}
    static TextBlock Text(string value,double size,string color){return new TextBlock {Text=value,FontSize=size,Foreground=Ink(color),TextWrapping=TextWrapping.Wrap};}
    static LinearGradientBrush Gradient(string a,string b){return new LinearGradientBrush((Color)ColorConverter.ConvertFromString(a),(Color)ColorConverter.ConvertFromString(b),90);}
    static IEnumerable<DependencyObject> Descendants(DependencyObject root){yield return root;foreach(var child in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())foreach(var item in Descendants(child))yield return item;}
    static void Detach(FrameworkElement c){var p=c.Parent;if(p is Panel)((Panel)p).Children.Remove(c);else if(p is Decorator)((Decorator)p).Child=null;else if(p is ContentControl)((ContentControl)p).Content=null;c.ClearValue(Grid.RowProperty);c.ClearValue(Grid.ColumnProperty);c.ClearValue(Grid.RowSpanProperty);c.ClearValue(Grid.ColumnSpanProperty);}
    static Button Button(string text,Action action){var b=new Button {Content=text,Padding=new Thickness(14,9,14,9),MinHeight=34,FontSize=10,FontWeight=FontWeights.SemiBold,Cursor=Cursors.Hand,Background=Ink("#1B2C3E"),Foreground=Ink("#DFE9F2"),BorderBrush=Ink("#3B5268"),BorderThickness=new Thickness(1),Margin=new Thickness(3)};b.Click+=(s,e)=>action();return b;}
    bool SettingsPromptOpen(){if(ui.AppearanceSettings==null || Find<ContentControl>("SettingsHost").Visibility!=Visibility.Visible)return false;var overlay=(Border)typeof(SettingsPage).GetField("overlay",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(ui.AppearanceSettings);return overlay.Visibility==Visibility.Visible;}
    void Home(){if(SettingsPromptOpen()){ui.AppearanceSettings.DismissPrompt();return;}ui.AppearanceNavigate("Home");}
    internal void Navigate(string page){if(SettingsPromptOpen()){ui.SetStatus("Finish or cancel the current settings dialog first.");return;}toolsRequested=page=="Installation";try{ui.AppearanceNavigate(page);}finally{toolsRequested=false;}}
    public void SynchronizeNavigation(){
        if(pages==null || channels==null)return;
        string page=ui.AppearancePage;if(page=="Home" && toolsRequested)page="Installation";
        if(page=="Home"){lastPage=null;pages.BeginAnimation(UIElement.OpacityProperty,null);originalHome.Visibility=Visibility.Collapsed;shade.Visibility=Visibility.Collapsed;stage.IsEnabled=true;stage.Effect=null;return;}
        channels.Close();bool settingsPage=page=="Game settings",standalone=page=="Versions"||page=="What's new";
        nav.Visibility=settingsPage||standalone?Visibility.Collapsed:Visibility.Visible;replayTutorial.Visibility=settingsPage||standalone?Visibility.Collapsed:Visibility.Visible;dialog.RowDefinitions[1].Height=new GridLength(settingsPage||standalone?0:40);wizard.Visibility=Visibility.Collapsed;pages.Visibility=Visibility.Visible;
        originalHome.Visibility=page=="Installation"?Visibility.Visible:Visibility.Collapsed;
        var area=ui.AppearanceMaintenance;if(area!=null){MinimalTools.Apply(area);if(observed.Add(area))area.Tabs.SelectionChanged+=(s,e)=>{if(e.Source==area.Tabs)SynchronizeNavigation();};}
        Restyle.Apply(pages);if(page=="Installation")MinimalTools.Polish(originalHome,page);if(page=="Saves")MinimalTools.Polish(Find<ContentControl>("SavesHost").Content as FrameworkElement,page);
        if(ui.AppearanceSettings!=null)MinimalSettings.Apply(ui.AppearanceSettings,()=>Navigate("Installation"));
        if(page=="Diagnostics")MinimalTools.ResetDiagnostics(pages);
        if(page=="Saves"||page=="Console"||page=="Diagnostics")LauncherAppearanceDetails.EmptyStates(pages,page,root);
        title.Text=settingsPage?"Settings":page=="Installation"?"Launcher tools":page;
        foreach(Button b in nav.Children){bool active=(string)b.Tag==page;b.Foreground=Ink(active?"#65C7FF":"#B4C7D8");b.BorderBrush=Ink(active?"#85D4FF":"#294E70");b.BorderThickness=new Thickness(0,0,0,active?2:1);}
        shade.Visibility=Visibility.Visible;stage.IsEnabled=false;stage.Effect=new BlurEffect {Radius=12};LauncherAppearanceDetails.Fade(pages,window.IsVisible && lastPage!=page && motion.Enabled);lastPage=page;
        window.Dispatcher.BeginInvoke(new Action(()=>{Restyle.Apply(pages);dialog.Focus();}));RefreshState();
    }
    public void RefreshState(){
        if(channels==null)return;bool running=ui.AppearanceRunning;ui.AppearanceRefreshPlayAvailability(running);string action=ui.AppearanceCommand;var native=ui.AppearanceButton;
        string caption=running?"Playing":action=="PauseDownloadButton"?(Convert.ToString(native.Content).StartsWith("Cancel")?"Cancel":"Pause"):action=="PlayButton"?"Play":action=="UpdateButton"?(ui.AppearanceOffersUpdate?GameUpdateActionPage.UpdateNow:Convert.ToString(native.Content)):action=="ImportButton"?"Import game data":action=="RetryButton"?Convert.ToString(native.Content):"Open data folder";
        primary.Content=running?(object)playingContent:caption.ToUpperInvariant();primary.IsEnabled=!running && native.IsEnabled;primary.Background=running?Ink("#667583"):Gradient("#C3E5FC","#96CFF2");primary.Foreground=Ink(running?"#E4EDF5":"#091C2C");primary.BorderBrush=Ink(running?"#8593A0":"#D6EDFF");primary.ToolTip=running?"The game is running. Close it to play again.":native.ToolTip??caption;System.Windows.Automation.AutomationProperties.SetName(primary,caption);
        updateChoice.Update(ui.AppearanceOffersUpdate&&!running,Find<Button>("PlayButton").IsEnabled,ui.AppearanceInstalled.Tag);
        actionDock.Height=updateChoice.PlayCurrent.Visibility==Visibility.Visible?132:104;
        kind.Text=running?"GAME RUNNING":ui.AppearanceChecking?"CHECKING FOR UPDATES":ui.AppearanceBusy?"OPERATION IN PROGRESS":Convert.ToString(Find<TextBlock>("UpdateStateLabel").Text);
        channels.Update(ui.AppearanceCatalog,ui.AppearanceInstalled,ui.AppearanceBusy||ui.AppearanceChecking||running,ui.AppearanceOffline);
        foreach(Button b in nav.Children)b.IsEnabled=ui.AppearanceCanNavigate((string)b.Tag);
        if(tutorial!=null)tutorial.UpdateSetup(ui.AppearanceGameInstalled,ui.AppearanceGameInstalled&&ui.AppearanceDataReady,System.IO.Directory.Exists(Core.PathIn(root,"Data")),Find<Button>("UpdateButton").IsEnabled,Find<TextBlock>("StatusText").Text);
        if(replayTutorial!=null)replayTutorial.IsEnabled=!ui.AppearanceBusy&&!ui.AppearanceChecking&&!running;
        bool animate=running && window.IsVisible && window.WindowState!=WindowState.Minimized && stage.IsEnabled && motion.Enabled && SystemParameters.ClientAreaAnimation && !SystemParameters.HighContrast;
        if(pulsing!=animate){playingDot.BeginAnimation(UIElement.OpacityProperty,null);if(animate){var pulse=new DoubleAnimation(.45,1,TimeSpan.FromSeconds(2)){AutoReverse=true,RepeatBehavior=RepeatBehavior.Forever};Timeline.SetDesiredFrameRate(pulse,30);playingDot.BeginAnimation(UIElement.OpacityProperty,pulse);}pulsing=animate;}
    }
    async void SetupAction(string action){
        if(setupActionRunning)return;setupActionRunning=true;
        tutorialLayer.Visibility=Visibility.Collapsed;stage.IsHitTestVisible=true;motion.Paused=false;
        try{
            if(action=="settings"){Navigate("Game settings");return;}
            if(action=="review"){tutorial.Dismiss();Navigate("Installation");Find<Expander>("GameDataExpander").IsExpanded=true;return;}
            await ui.AppearanceSetup(action);
        }catch(Exception ex){Notice(ex.Message);}
        finally{
            setupActionRunning=false;RefreshState();
            if(tutorial.IsOpen&&window.IsVisible){Home();tutorialLayer.Visibility=Visibility.Visible;stage.IsHitTestVisible=false;motion.Paused=true;PlaceTutorial();tutorial.Next.Focus();}
        }
    }
    void ShowTutorial(bool manual){
        if(ui.AppearanceBusy||ui.AppearanceChecking||ui.AppearanceRunning||SettingsPromptOpen())return;
        if(!manual && (!window.IsActive||shade.Visibility==Visibility.Visible||channels.IsOpen||!tutorial.ShouldOffer))return;
        Home();channels.Close();tutorialLayer.Visibility=Visibility.Visible;stage.IsHitTestVisible=false;motion.Paused=true;tutorial.Open();PlaceTutorial();tutorial.Next.Focus();
    }
    void PlaceTutorial(){
        if(tutorial==null||!tutorial.IsOpen)return;tutorial.View.MaxHeight=Math.Max(240,shell.ActualHeight-84);
        FrameworkElement anchor=tutorial.Target=="play"?primary:tutorial.Target=="versions"?version.Parent as FrameworkElement:tutorial.Target=="tools"?toolsAnchor:tutorial.Target=="news"?newsAnchor:null;
        tutorialHighlight.Visibility=anchor==null?Visibility.Collapsed:Visibility.Visible;if(anchor==null)tutorialShade.SetSpotlight(null);
        if(anchor!=null){var point=anchor.TranslatePoint(new Point(0,0),shell);tutorialHighlight.Width=anchor.ActualWidth+12;tutorialHighlight.Height=anchor.ActualHeight+12;tutorialHighlight.Margin=new Thickness(Math.Max(0,point.X-6),Math.Max(0,point.Y-44),0,0);tutorialShade.SetSpotlight(new Rect(tutorialHighlight.Margin.Left,tutorialHighlight.Margin.Top,tutorialHighlight.Width,tutorialHighlight.Height));}
    }
    void Notice(string message){ui.SetStatus(message);}
    void Build(){
        window.Title="TimeSplitters — Launcher 1.7.2 · Local working build";window.Width=1184;window.Height=684;window.MinWidth=940;window.MinHeight=640;
        window.WindowStyle=WindowStyle.None;window.ResizeMode=ResizeMode.CanResize;
        WindowChrome.SetWindowChrome(window,new WindowChrome {CaptionHeight=34,ResizeBorderThickness=new Thickness(6),GlassFrameThickness=new Thickness(0),CornerRadius=new CornerRadius(1),UseAeroCaptionButtons=false});
        window.Background=Ink("#071321");window.Foreground=Ink("#E4EDF5");shell=new Grid {Background=window.Background,ClipToBounds=true};window.Content=shell;
        stage=new Grid();shell.Children.Add(stage);
        var backdrop=Find<Image>("HeroBackdrop");Detach(backdrop);backdrop.Stretch=Stretch.UniformToFill;backdrop.Opacity=0.12;stage.Children.Add(backdrop);
        var art=Find<Image>("HeroPhoto");Detach(art);art.Width=680;art.Margin=new Thickness(0,0,-45,20);art.Stretch=Stretch.Uniform;art.HorizontalAlignment=HorizontalAlignment.Right;art.VerticalAlignment=VerticalAlignment.Center;art.Opacity=0.30;
        art.VerticalAlignment=VerticalAlignment.Top;art.Margin=new Thickness(0,38,-45,0);
        art.OpacityMask=new LinearGradientBrush(new GradientStopCollection {new GradientStop(Colors.Transparent,0),new GradientStop(Colors.White,0.35),new GradientStop(Colors.White,0.8),new GradientStop(Colors.Transparent,1)},new Point(0,0),new Point(1,0));stage.Children.Add(art);
        stage.Children.Add(new Border {IsHitTestVisible=false,Background=new RadialGradientBrush {GradientOrigin=new Point(0.97,0.9),Center=new Point(0.97,0.9),RadiusX=0.68,RadiusY=0.95,GradientStops=new GradientStopCollection {new GradientStop(Color.FromArgb(38,134,205,255),0),new GradientStop(Colors.Transparent,1)}}});
        // Animate the temporal aperture behind the TimeSplitters artwork.
        var aperture=new Grid {Width=480,Height=480,VerticalAlignment=VerticalAlignment.Center,HorizontalAlignment=HorizontalAlignment.Center,Margin=new Thickness(0,0,0,12),Opacity=0.25,IsHitTestVisible=false};
        foreach(int size in new[]{470,410,340,180,145})aperture.Children.Add(new System.Windows.Shapes.Ellipse {Width=size,Height=size,Stroke=Ink("#85D4FF"),StrokeThickness=size==410?2:1,StrokeDashArray=size==470?new DoubleCollection {1,9}:null});stage.Children.Add(aperture);
        var logo=Find<Image>("HeroTitle");Detach(logo);logo.Width=600;logo.MaxWidth=600;logo.HorizontalAlignment=HorizontalAlignment.Center;logo.VerticalAlignment=VerticalAlignment.Center;logo.Margin=new Thickness(0,0,0,12);logo.Effect=new DropShadowEffect {Color=Colors.Black,BlurRadius=30,ShadowDepth=5,Opacity=1};stage.Children.Add(logo);
        var dock=actionDock=new Grid {Margin=new Thickness(28,0,28,18),VerticalAlignment=VerticalAlignment.Bottom,Height=104};dock.ColumnDefinitions.Add(new ColumnDefinition());dock.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(80)});dock.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(8)});dock.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(260)});
        var operation=new Grid {VerticalAlignment=VerticalAlignment.Bottom,Margin=new Thickness(0,0,22,0),Height=32};operation.RowDefinitions.Add(new RowDefinition());operation.RowDefinitions.Add(new RowDefinition {Height=new GridLength(3)});
        var copy=new DockPanel {LastChildFill=true,Margin=new Thickness(0,0,0,8)};kind=Text("READY TO PLAY",8,"#65C7FF");kind.FontWeight=FontWeights.SemiBold;kind.Margin=new Thickness(0,0,20,0);DockPanel.SetDock(kind,Dock.Left);copy.Children.Add(kind);
        status=Find<TextBlock>("StatusText");Detach(status);status.FontSize=10;status.Foreground=Ink("#B4C7D8");status.TextAlignment=TextAlignment.Right;status.TextTrimming=TextTrimming.CharacterEllipsis;status.TextWrapping=TextWrapping.NoWrap;copy.Children.Add(status);operation.Children.Add(copy);
        progress=Find<ProgressBar>("OperationProgress");Detach(progress);progress.Height=3;progress.Foreground=Ink("#65C7FF");progress.Background=Ink("#204566");Grid.SetRow(progress,1);operation.Children.Add(progress);dock.Children.Add(operation);
        var homeLinks=new StackPanel {Orientation=Orientation.Horizontal,Margin=new Thickness(0,0,0,48),HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Bottom};dock.Children.Add(homeLinks);
        var newsLink=Button("What's new  →",()=>Navigate("What's new"));newsLink.FontSize=12;newsLink.Padding=new Thickness(0);newsLink.MinHeight=28;newsLink.Margin=new Thickness(0,0,20,0);newsLink.Background=Brushes.Transparent;newsLink.BorderThickness=new Thickness(0,0,0,1);newsLink.BorderBrush=Brushes.Transparent;newsLink.Foreground=Ink("#85D4FF");newsLink.ToolTip="Read game and launcher release notes";System.Windows.Automation.AutomationProperties.SetName(newsLink,"What's new");homeLinks.Children.Add(newsLink);
        newsAnchor=homeLinks;
        homeLinks.Children.Add(new Border {Width=1,Height=12,Background=Ink("#294E70"),Margin=new Thickness(0,0,20,0),VerticalAlignment=VerticalAlignment.Center});
        var discord=Find<Button>("JoinDiscordButton");Detach(discord);discord.FontSize=12;discord.Padding=new Thickness(0);discord.MinHeight=28;discord.Margin=new Thickness(0);discord.Background=Brushes.Transparent;discord.BorderThickness=new Thickness(0,0,0,1);discord.BorderBrush=Brushes.Transparent;discord.Foreground=Ink("#85D4FF");discord.ToolTip="Open the TimeSplitters Discord community in your browser";
        var discordContent=new StackPanel {Orientation=Orientation.Horizontal};var discordIcon=Descendants(Find<Button>("DiscordNav")).OfType<Viewbox>().First();Detach(discordIcon);discordIcon.Width=16;discordIcon.Height=16;discordIcon.Margin=new Thickness(0,0,8,0);foreach(var path in Descendants(discordIcon).OfType<System.Windows.Shapes.Path>())path.Fill=Ink("#85D4FF");discordContent.Children.Add(discordIcon);discordContent.Children.Add(Text("Join Discord  ↗",12,"#85D4FF"));discord.Content=discordContent;System.Windows.Automation.AutomationProperties.SetName(discord,"Join Discord community");
        homeLinks.Children.Add(discord);
        homeLinks.Children.Add(new Border {Width=1,Height=12,Background=Ink("#294E70"),Margin=new Thickness(20,0,20,0),VerticalAlignment=VerticalAlignment.Center});
        var github=Button("View on GitHub  ↗",()=>{try{System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("https://github.com/"+ReleaseClient.Repository){UseShellExecute=true});}catch(Exception ex){Notice("Could not open GitHub: "+ex.Message);}});github.FontSize=12;github.Padding=new Thickness(0);github.MinHeight=28;github.Margin=new Thickness(0);github.Background=Brushes.Transparent;github.BorderThickness=new Thickness(0,0,0,1);github.BorderBrush=Brushes.Transparent;github.Foreground=Ink("#85D4FF");github.ToolTip="Open the TimeSplitters project on GitHub";System.Windows.Automation.AutomationProperties.SetName(github,"View project on GitHub");homeLinks.Children.Add(github);
        var gear=Button("",()=>Navigate("Installation"));gear.Padding=new Thickness(0);gear.Width=80;gear.Height=80;gear.Margin=new Thickness(0);gear.VerticalAlignment=VerticalAlignment.Bottom;gear.ToolTip="Launcher tools and game settings";
        toolsAnchor=gear;
        gear.Content=new System.Windows.Shapes.Path {Data=Geometry.Parse("M9.4,3.2 L14.6,3.2 L15.1,5.4 L16.3,6.1 L18.4,5.4 L21,9.9 L19.4,11.4 L19.4,12.8 L21,14.3 L18.4,18.8 L16.3,18.1 L15.1,18.8 L14.6,21 L9.4,21 L8.9,18.8 L7.7,18.1 L5.6,18.8 L3,14.3 L4.6,12.8 L4.6,11.4 L3,9.9 L5.6,5.4 L7.7,6.1 L8.9,5.4 Z M15,12 A3,3 0 1 1 9,12 A3,3 0 1 1 15,12"),Stroke=Ink("#D6E3F0"),StrokeThickness=1.5,Stretch=Stretch.Uniform,Width=26,Height=26};System.Windows.Automation.AutomationProperties.SetName(gear,"Settings");Grid.SetColumn(gear,1);dock.Children.Add(gear);
        var actions=new Grid();actions.RowDefinitions.Add(new RowDefinition {Height=new GridLength(24)});actions.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});actions.RowDefinitions.Add(new RowDefinition {Height=new GridLength(80)});Grid.SetColumn(actions,3);dock.Children.Add(actions);
        version=Text("LATEST RELEASE   SAMPLE",9,"#65C7FF");version.HorizontalAlignment=HorizontalAlignment.Right;actions.Children.Add(version);
        Find<Button>("PlayButton").Visibility=Visibility.Collapsed;
        primary=Button("PLAY",async()=>{await ui.AppearanceActivate();RefreshState();});primary.Name="MainGameAction";primary.Height=80;primary.Margin=new Thickness(0);primary.FontSize=18;Grid.SetRow(primary,2);actions.Children.Add(primary);updateChoice=new GameUpdateActionPage();updateChoice.PlayCurrentRequested+=async()=>await ui.AppearancePlayCurrent();Grid.SetRow(updateChoice.PlayCurrent,1);actions.Children.Add(updateChoice.PlayCurrent);stage.Children.Add(dock);
        var versionBadge=Text("LAUNCHER v"+LauncherVersion.Value+" · LOCAL BUILD",8,"#829BB2");versionBadge.Margin=new Thickness(28,12,0,0);versionBadge.HorizontalAlignment=HorizontalAlignment.Left;versionBadge.VerticalAlignment=VerticalAlignment.Top;stage.Children.Add(versionBadge);
        shade=new Grid {Background=Ink("#C2050D18"),Visibility=Visibility.Collapsed};shell.Children.Add(shade);
        var dismiss=new Border {Background=Brushes.Transparent};dismiss.MouseLeftButtonDown+=(s,e)=>Home();shade.Children.Add(dismiss);
        var surface=new Border {Background=Ink("#071321"),MaxWidth=900,Margin=new Thickness(36,42,36,42),Effect=new DropShadowEffect {BlurRadius=55,ShadowDepth=18,Opacity=0.45},HorizontalAlignment=HorizontalAlignment.Stretch};shade.Children.Add(surface);
        dialog=new Grid {Focusable=true};KeyboardNavigation.SetTabNavigation(dialog,KeyboardNavigationMode.Cycle);surface.Child=dialog;dialog.RowDefinitions.Add(new RowDefinition {Height=new GridLength(62)});dialog.RowDefinitions.Add(new RowDefinition {Height=new GridLength(40)});dialog.RowDefinitions.Add(new RowDefinition());
        var header=new DockPanel {Margin=new Thickness(32,0,16,0),LastChildFill=true};dialog.Children.Add(header);
        var close=Button("×",Home);close.FontSize=23;close.FontWeight=FontWeights.Light;close.BorderThickness=new Thickness(0);close.Background=Brushes.Transparent;close.Width=36;close.Padding=new Thickness(0);close.ToolTip="Close settings (Esc)";DockPanel.SetDock(close,Dock.Right);header.Children.Add(close);
        replayTutorial=Button("Launcher tour",()=>ShowTutorial(true));replayTutorial.Name="ReplayLauncherTour";replayTutorial.FontSize=11;replayTutorial.Background=Brushes.Transparent;replayTutorial.Foreground=Ink("#85D4FF");replayTutorial.BorderThickness=new Thickness(0);DockPanel.SetDock(replayTutorial,Dock.Right);header.Children.Add(replayTutorial);
        title=Text("Settings",21,"#E4EDF5");title.FontWeight=FontWeights.Light;title.VerticalAlignment=VerticalAlignment.Center;header.Children.Add(title);
        nav=new UniformGrid {Rows=1,Columns=6,Margin=new Thickness(28,0,28,0)};Grid.SetRow(nav,1);dialog.Children.Add(nav);
        foreach(string name in new[]{"Installation","Game settings","Saves","Repair","Diagnostics","Console"}){string captured=name;var b=Button(name,()=>Navigate(captured));b.Tag=name;b.FontSize=12;b.Padding=new Thickness(12,8,12,8);b.Margin=new Thickness(0);b.Background=Brushes.Transparent;nav.Children.Add(b);}
        pages=new Grid {Margin=new Thickness(0,0,0,12)};Grid.SetRow(pages,2);dialog.Children.Add(pages);
        wizard=new Grid {Margin=new Thickness(32,0,32,22),Visibility=Visibility.Collapsed};Grid.SetRow(wizard,1);Grid.SetRowSpan(wizard,2);dialog.Children.Add(wizard);
        originalHome=Find<Grid>("CommandCenter");Detach(originalHome);pages.Children.Add(originalHome);originalHome.RowDefinitions[2].MaxHeight=0;originalHome.RowDefinitions[1].Height=new GridLength(0);originalHome.RowDefinitions[3].Height=new GridLength(0);
        foreach(var item in originalHome.Children.OfType<FrameworkElement>().Where(c=>Grid.GetRow(c)==0).ToArray())item.Visibility=Visibility.Collapsed;
        foreach(string name in Hosts){var host=Find<ContentControl>(name);Detach(host);pages.Children.Add(host);}
        // Keep the original import, update and channel widgets inside a flat list.
        var cards=Find<ScrollViewer>("HomeCardsScroll").Content as Grid;if(cards!=null){cards.ColumnDefinitions.Clear();cards.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});cards.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});int row=0;foreach(var card in cards.Children.OfType<FrameworkElement>()){Grid.SetColumn(card,0);Grid.SetRow(card,row++);}}
        Find<TextBlock>("ReadyTitle").Text="Current install";
        var communityCard=Find<Border>("DiscordCommunityCard");Detach(communityCard);communityCard.Visibility=Visibility.Collapsed;
        Find<Button>("ReleaseNotesButton").Visibility=Visibility.Collapsed;
        Find<Button>("GameVersionsButton").Visibility=Visibility.Collapsed;
        var chrome=new StackPanel {Orientation=Orientation.Horizontal,HorizontalAlignment=HorizontalAlignment.Right,VerticalAlignment=VerticalAlignment.Top};foreach(string label in new[]{"−","□","×"}){string captured=label;var b=Button(label,()=>{if(captured=="−")window.WindowState=WindowState.Minimized;else if(captured=="□")window.WindowState=window.WindowState==WindowState.Maximized?WindowState.Normal:WindowState.Maximized;else window.Close();});b.Width=42;b.Height=36;b.MinHeight=0;b.Margin=new Thickness(0);b.Padding=new Thickness(0);b.Background=Brushes.Transparent;b.BorderThickness=new Thickness(0);b.FontWeight=FontWeights.Normal;b.FontSize=14;b.FontFamily=new FontFamily("Segoe UI");b.ToolTip=label=="−"?"Minimize":label=="□"?"Maximize / restore":"Close";WindowChrome.SetIsHitTestVisibleInChrome(b,true);chrome.Children.Add(b);}shell.Children.Add(chrome);
        toast=Text("",11,"#65C7FF");toast.Visibility=Visibility.Collapsed;toast.HorizontalAlignment=HorizontalAlignment.Center;toast.VerticalAlignment=VerticalAlignment.Bottom;toast.Margin=new Thickness(20,0,20,12);shell.Children.Add(toast);
        shell.Children.Add(new Border {BorderBrush=Ink("#295277"),BorderThickness=new Thickness(1),IsHitTestVisible=false});window.PreviewKeyDown+=(s,e)=>{if(e.Key==Key.Escape && shade.Visibility==Visibility.Visible){Home();e.Handled=true;}};
        channels=new GameChannelMenu(window,stage,version,ui.AppearancePicker,()=>Navigate("Versions"),async release=>{try{await ui.AppearanceSwitch(release);}catch(Exception ex){ui.SetStatus(ex.Message);}RefreshState();},ui.AppearanceInstalled);
        motion=new LauncherMotion(window,stage,aperture,backdrop,art,logo,channels);
        playingContent=new StackPanel {Orientation=Orientation.Horizontal};playingDot=new System.Windows.Shapes.Ellipse {Width=6,Height=6,Fill=Ink("#B6E3FF"),Margin=new Thickness(0,0,12,0),VerticalAlignment=VerticalAlignment.Center};playingContent.Children.Add(playingDot);playingContent.Children.Add(Text("P L A Y I N G",14,"#E4EDF5"));
        var textureStatus=Find<TextBlock>("TextureUpdateStatus");Detach(textureStatus);textureStatus.Margin=new Thickness(28,0,390,34);textureStatus.FontSize=10;textureStatus.VerticalAlignment=VerticalAlignment.Bottom;stage.Children.Add(textureStatus);
        LauncherAppearanceDetails.Remember(window,Core.PathIn(root,".launcher/window-state.json"));
        LauncherAppearanceDetails.FitWindow(window,shell);
        tutorial=new LauncherTutorialPage(root,ui.AppearanceGameInstalled&&ui.AppearanceDataReady);
        tutorialLayer=new Grid {Background=Brushes.Transparent,Margin=new Thickness(0,38,0,0),Visibility=Visibility.Collapsed};
        tutorialShade=new TutorialShade {IsHitTestVisible=false};tutorialLayer.Children.Add(tutorialShade);
        tutorialHighlight=new Border {BorderBrush=Ink("#85D4FF"),BorderThickness=new Thickness(2),HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Top,IsHitTestVisible=false};tutorialLayer.Children.Add(tutorialHighlight);tutorialLayer.Children.Add(tutorial.View);tutorial.View.Margin=new Thickness(24);KeyboardNavigation.SetTabNavigation(tutorialLayer,KeyboardNavigationMode.Cycle);shell.Children.Add(tutorialLayer);
        tutorial.SetupRequested+=SetupAction;tutorial.Changed+=PlaceTutorial;tutorial.Warning+=Notice;tutorial.Closed+=()=>{tutorialLayer.Visibility=Visibility.Collapsed;stage.IsHitTestVisible=true;motion.Paused=false;primary.Focus();RefreshState();};
        window.SizeChanged+=(s,e)=>PlaceTutorial();window.LayoutUpdated+=(s,e)=>PlaceTutorial();window.PreviewKeyDown+=(s,e)=>{if(e.Key==Key.Escape && tutorial.IsOpen){tutorial.Dismiss();e.Handled=true;}};
        timer=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromSeconds(1)};timer.Tick+=(s,e)=>{RefreshState();if(!tutorial.IsOpen)ShowTutorial(false);};window.Loaded+=(s,e)=>timer.Start();window.Closed+=(s,e)=>{timer.Stop();playingDot.BeginAnimation(UIElement.OpacityProperty,null);};
        SynchronizeNavigation();RefreshState();
    }
    bool FileExists(string path){return System.IO.File.Exists(Core.PathIn(root,path));}
}
}
