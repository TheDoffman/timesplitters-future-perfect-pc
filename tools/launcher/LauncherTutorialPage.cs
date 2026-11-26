using System;
using System.IO;
using System.Collections.Generic;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Automation;
using FontWeights = Avalonia.Media.FontWeight;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Automation;
#endif

namespace TimeSplitters.Launcher
{
    // A real transparent cutout: the live control underneath keeps its normal
    // colors. The parent overlay still catches input across the entire window.
#if MACOS || LINUX
    public sealed class TutorialShade : Control
#else
    public sealed class TutorialShade : FrameworkElement
#endif
    {
        Rect? spotlight;
        readonly
#if MACOS || LINUX
        IBrush
#else
        Brush
#endif
        fill=Ui.Brush("#C90B1420");
        public void SetSpotlight(Rect? value){if(Nullable.Equals(spotlight,value))return;spotlight=value;InvalidateVisual();}
        public Geometry Mask(Size size){var shape=new GeometryGroup {FillRule=FillRule.EvenOdd};shape.Children.Add(new RectangleGeometry(new Rect(new Point(0,0),size)));if(spotlight.HasValue)shape.Children.Add(new RectangleGeometry(spotlight.Value));return shape;}
#if MACOS || LINUX
        public override void Render(DrawingContext context){context.DrawGeometry(fill,null,Mask(Bounds.Size));}
#else
        protected override void OnRender(DrawingContext context){context.DrawGeometry(fill,null,Mask(RenderSize));}
#endif
    }
    // Shared copy, interaction and opt-out policy. Host adapters only position
    // this card and outline the real home control for the current step.
    public sealed class LauncherTutorialPage
    {
        public const int Revision=1;
        const string StateFile=".launcher/tutorial.json";
        readonly string root;readonly bool persist;bool returning;
        readonly TextBlock eyebrow,heading,description,location,note;
        readonly ProgressBar progress;
        readonly UniformGrid actions;
        bool offered;bool? shouldOffer;
        bool setup,setupPending,installed,ready,dataPresent,canInstall;string setupStatus="";
        public bool IsSetup {get{return setup;}}
        public event Action<string> SetupRequested;
        public readonly Border View;
        public readonly Button Back,Skip,Next,Explore;
        public event Action Changed,Closed;
        public event Action<string> Warning;
        public int Index {get;private set;}
        public bool IsOpen {get;private set;}
        public const int StepCount=6;
        public string Target {get{if(setup)return Index==0?"":"play";return Index==1?"play":Index==2?"versions":Index==3||Index==4||Index==5?"tools":Index==6?"news":"";}}
        public LauncherTutorialPage(string root,bool returning,bool persist=true)
        {
            this.root=root;this.returning=returning;this.persist=persist;
            try{var data=Read();setupPending=data.ContainsKey("setupPending")&&Object.Equals(data["setupPending"],true);}catch(Exception){}
            setup=!returning||setupPending;
            View=new Border {Name="LauncherTutorialCard",MaxWidth=460,Padding=new Thickness(24),Background=Ui.Brush("#101E2D"),BorderBrush=Ui.Brush("#4779A0"),BorderThickness=new Thickness(1),HorizontalAlignment=HorizontalAlignment.Center,VerticalAlignment=VerticalAlignment.Center};Ui.Theme(View);
            var frame=new DockPanel();View.Child=frame;
            var footer=new StackPanel {Margin=new Thickness(0,20,0,0)};DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);
            progress=new ProgressBar {Minimum=0,Maximum=StepCount,Height=3,Foreground=Ui.Brush("#65C7FF"),Background=Ui.Brush("#204566"),Margin=new Thickness(0,0,0,18)};footer.Children.Add(progress);
            Explore=Action("Show me around","TutorialExplore");Explore.Margin=new Thickness(3,0,3,10);footer.Children.Add(Explore);
            Explore.Click+=(s,e)=>{
                if(!IsOpen||!setup||Index!=3||!ready)return;
                setup=false;this.returning=true;setupPending=false;SavePreference(false);Index=1;Render();Next.Focus();
            };
            actions=new UniformGrid {Rows=1,Columns=2};footer.Children.Add(actions);
            Back=Action("Back","TutorialBack");Skip=Action("Not now","TutorialSkip");Next=Action("Show me around","TutorialNext");actions.Children.Add(Back);actions.Children.Add(Skip);actions.Children.Add(Next);PageTheme.Primary(Next);
            Back.Click+=(s,e)=>{if(setup&&Index==2)RequestSetup("folder");else if(setup&&Index==3){Dismiss();if(SetupRequested!=null)SetupRequested("settings");}else if(Index>0){Index--;Render();}};
            Skip.Click+=(s,e)=>Dismiss();Next.Click+=(s,e)=>{
                if(setup){if(Index==0){setupPending=true;SavePreference(false);Index=SetupIndex;Render();}else if(Index==3){Dismiss();if(SetupRequested!=null)SetupRequested("play");}else RequestSetup(Index==1?"install":dataPresent?"review":"iso");}
                else if(Index==StepCount)Dismiss();else{Index++;Render();}};
            var body=new StackPanel();eyebrow=PageTheme.Text("",10,"#65C7FF");eyebrow.Margin=new Thickness(0,0,0,12);body.Children.Add(eyebrow);
            heading=PageTheme.Text("",25);heading.Name="TutorialTitle";heading.Margin=new Thickness(0,0,0,16);body.Children.Add(heading);
            description=PageTheme.Text("",14,"#D5E2ED");description.Margin=new Thickness(0,0,0,18);body.Children.Add(description);
            location=PageTheme.Text("",11,"#85D4FF");location.Margin=new Thickness(0,0,0,12);body.Children.Add(location);
            note=PageTheme.Text("",11,"#9FB5C9");body.Children.Add(note);
            frame.Children.Add(new ScrollViewer {Content=body,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled});
            AutomationProperties.SetName(View,"Launcher guided tour");Render();
        }
        static Button Action(string label,string name){var button=new Button {Name=name,Content=label,MinHeight=38,Padding=new Thickness(8,9,8,9),Margin=new Thickness(3,0,3,0),HorizontalAlignment=HorizontalAlignment.Stretch,HorizontalContentAlignment=HorizontalAlignment.Center};PageTheme.Secondary(button);AutomationProperties.SetName(button,label);return button;}
        static int Number(Dictionary<string,object> data,string key){object value;return data.TryGetValue(key,out value)?Convert.ToInt32(value):0;}
        Dictionary<string,object> Read()
        {
            string path=Core.PathIn(root,StateFile);if(!File.Exists(path))return new Dictionary<string,object>();
            if(new FileInfo(path).Length>8192)return new Dictionary<string,object>{{"schema",Int32.MaxValue}};
            return Json.Object(File.ReadAllText(path));
        }
        public bool ShouldOffer {
            get {if(offered)return false;if(shouldOffer.HasValue)return shouldOffer.Value;try{var data=Read();shouldOffer=Number(data,"schema")<=1&&(Number(data,"revision")<Revision||(Number(data,"revision")==Revision&&setupPending));}catch(Exception){shouldOffer=true;}return shouldOffer.Value;}
        }
        int SetupIndex {get{return !installed?1:!ready?2:3;}}
        public void UpdateSetup(bool gameInstalled,bool gameReady,bool existingData,bool installAvailable,string status)
        {
            if(!setup)return;
            bool changed=installed!=gameInstalled||ready!=gameReady||dataPresent!=existingData||canInstall!=installAvailable||setupStatus!=status;
            installed=gameInstalled;ready=gameReady;dataPresent=existingData;canInstall=installAvailable;setupStatus=status??"";
            if(IsOpen&&Index>0)Index=SetupIndex;
            if(changed)Render();
        }
        void RequestSetup(string action){if(SetupRequested!=null)SetupRequested(action);}
        public void PreviewStep(int index){if(persist)throw new InvalidOperationException("Preview steps require a non-persistent tutorial.");offered=true;IsOpen=true;Index=Math.Max(0,Math.Min(index,setup?3:StepCount));Render();}
        public void Open(){if(offered&&setup&&ready&&!setupPending){setup=false;returning=true;}offered=true;Index=setup&&setupPending?SetupIndex:0;IsOpen=true;Render();}
        public void Dismiss()
        {
            if(!IsOpen)return;IsOpen=false;
            setupPending=false;SavePreference(true);
            if(Closed!=null)Closed();
        }
        void SavePreference(bool dismissed)
        {
            if(!persist)return;
            try{
                Dictionary<string,object> data;try{data=Read();}catch(Exception){data=new Dictionary<string,object>();}
                if(Number(data,"schema")<=1&&Number(data,"revision")<=Revision){
                    data["schema"]=1;data["setupPending"]=setupPending;
                    if(dismissed)data["revision"]=Revision;
                    Core.Write(Core.PathIn(root,StateFile),Json.Encode(data));
                }
            }catch(Exception){if(Warning!=null)Warning("Your setup preference could not be saved. You can reopen the guide from Launcher tools.");}
        }
        void RenderSetup()
        {
            eyebrow.Text=Index==0?"WELCOME TO YOUR LAUNCHER":"GAME SETUP  ·  "+Index+" OF 3";
            if(Index==0){heading.Text="Let’s get you playing";description.Text="We’ll help you install the game, add your Xbox game data and get ready for your first mission. You’ll need a supported US Xbox ISO or an extracted copy of your game.";location.Text="Install the game → Add your data → Play";note.Text="Already started? We’ll pick up from what’s installed. You can leave setup and come back from Launcher tools → Launcher tour.";}
            else if(Index==1){heading.Text="First, install the game";description.Text="Download the version that runs on your computer. Stable is a good place to start. This download doesn’t include the original Xbox game data; we’ll add yours next.";location.Text="1. Install game   2. Add game data   3. Play";note.Text=canInstall?"Choose Install game below. You can pause the download on Home. After the launcher restarts, setup will pick up here.":"No download is ready yet. Choose Check again to look for a compatible release.\n\n"+setupStatus;}
            else if(Index==2){heading.Text=dataPresent?"Let’s check your game data":"Now add your game data";description.Text=dataPresent?"There are already files in your game-data folder, but the game isn’t ready to play yet. Review the files in Launcher tools before trying another import. Your existing files will be kept.":"Choose your supported US Xbox ISO and we’ll check and copy the files the game needs. If you’ve already extracted the disc, choose Use folder instead and select the folder containing default.xbe, pak and xmv.";location.Text="✓ Game installed   2. Add game data   3. Play";note.Text="Cancelling the file picker leaves you here. If importing fails, the message on Home explains what needs attention.\n\n"+setupStatus;}
            else{heading.Text="You’re ready to play";description.Text="The game and your data are in place. Jump into your first mission, or open Game settings to adjust your controls, picture and sound before you start.";location.Text="✓ Game installed   ✓ Game data added";note.Text="Choose Show me around for a quick look at versions, tools, settings, backups and help. Or choose Play to jump straight in, or Finish to return to Home.";}
            Ui.Show(Explore,Index==3);
            Ui.Show(Back,Index>0);Back.Content=Index==2?"Use folder":Index==3?"Settings":"Back";Back.IsEnabled=Index!=2||!dataPresent;
            actions.Columns=Index==0?2:3;Skip.Content=Index==0?"Not now":Index==3?"Finish":"Set up later";
            Next.Content=Index==0?"Let’s set up":Index==1?(canInstall?"Install game":"Check again"):Index==2?(dataPresent?"Review data":"Choose ISO"):"Play";
            progress.Maximum=3;progress.Value=Index;Ui.Show(progress,Index>0);
            AutomationProperties.SetName(Back,Convert.ToString(Back.Content));AutomationProperties.SetName(Skip,Convert.ToString(Skip.Content));AutomationProperties.SetName(Next,Convert.ToString(Next.Content));AutomationProperties.SetName(View,heading.Text);
            if(Changed!=null)Changed();
        }

        void Render()
        {
            if(setup){RenderSetup();return;}
            Ui.Show(Explore,false);Back.Content="Back";AutomationProperties.SetName(Back,"Back");progress.Maximum=StepCount;
            eyebrow.Text=Index==0?(returning?"A QUICK LOOK AT THE NEW LAYOUT":"WELCOME TO YOUR LAUNCHER"):"LAUNCHER TOUR  ·  "+Index+" OF "+StepCount;
            switch(Index){
                case 0:
                    heading.Text=returning?"New look. Familiar tools.":"Ready for a quick tour?";
                    description.Text=returning?"Your saves and settings are still here. Let’s take a quick look at where to start playing, try new builds and find your tools.":"Let’s get you ready to play. We’ll show you how to set up the game, make it your own and find help if you need it.";
                    location.Text="About a minute · Skip whenever you like";note.Text="Want another look later? Open Launcher tools → Launcher tour.";break;
                case 1:
                    heading.Text=returning?"Jump back into the game":"Your first stop: Play";
                    description.Text=returning?"Ready for another mission? Hit Play to start your installed game. While you’re playing, it shows Playing. When you’re done, it’s ready for your next session.":"Choose Install release to download the game software, then add your game data from a supported US Xbox ISO or extracted folder. When everything’s ready, this button changes to Play.";
                    location.Text="Home · Bottom-right button";note.Text="Check the bottom of the launcher for download progress or anything you need to do next.";break;
                case 2:
                    heading.Text="Play your way, on your build";description.Text="Stick with Stable for the latest regular release, or try a Preview track to check out upcoming changes. Need an earlier build? You can browse all game versions from this same menu.";location.Text="Home · Version menu above Play";note.Text="Make a save backup before switching. Saves and online play may work differently between builds.";break;
                case 3:
                    heading.Text="One place for your tools";description.Text="Need to update the game, add your game data or find your installation folder? Open the gear beside Play. Your settings, save backups and troubleshooting tools are here too.";location.Text="Home · Gear beside Play";note.Text="The gear opens Launcher tools. Pick a tab across the top to find what you need.";break;
                case 4:
                    heading.Text="Make it feel like your game";description.Text="Change your controls, adjust the picture and sound, or fine-tune the camera and HUD in Game settings. It’s also where you can switch between Classic and Enhanced textures.";location.Text="Gear → Game settings";note.Text="Choose Save changes to keep your adjustments. Back to Launcher tools keeps your draft; Cancel discards it.";break;
                case 5:
                    heading.Text="Keep your progress safe";description.Text="Make a backup in Saves before trying a different build, or restore one when you need it. If the game isn’t working, Repair checks its files. Diagnostics and Console give you more detail when you need help.";location.Text="Gear → Saves · Repair · Diagnostics · Console";note.Text="You choose whether to share a report. Take a look at it before sending it to someone for help.";break;
                default:
                    heading.Text="See what’s new. Stay in touch.";description.Text="Check What’s new to see what changed in the game and launcher. Join Discord to chat with other players or ask for help, and visit GitHub to follow development or report an issue.";location.Text="Home · Links at the bottom-left";note.Text="That’s everything! You can replay this tour from Launcher tools whenever you like.";break;
            }
            Back.IsEnabled=Index>0;Ui.Show(Back,Index>0);actions.Columns=Index==0?2:3;Skip.Content=Index==0?"Not now":"Skip tour";Next.Content=Index==0?"Show me around":Index==StepCount?"Got it":"Next";progress.Value=Index;Ui.Show(progress,Index>0);
            AutomationProperties.SetName(Skip,Convert.ToString(Skip.Content));AutomationProperties.SetName(Next,Convert.ToString(Next.Content));AutomationProperties.SetName(View,heading.Text);
            if(Changed!=null)Changed();
        }
    }
}
