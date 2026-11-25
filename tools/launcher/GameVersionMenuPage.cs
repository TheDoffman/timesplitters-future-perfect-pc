using System;
using System.Linq;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Automation;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Automation;
#endif
namespace TimeSplitters.Launcher
{
    // One picker model and layout for every host; installation remains host-owned.
    public sealed class GameVersionMenuPage
    {
        public readonly StackPanel View=new StackPanel();
        Release[] releases=new Release[0];Release installed=new Release {Tag="Not installed",Channel="stable",ChannelName="Stable"},pending;
        bool blocked,offline;public bool IsOpen {get;private set;}
        public event Action Opened,Closed,Browse;
        public event Action<Release> Selected;
        public static string Name(Release release){return GameChannels.Name(release.Channel).Replace(" Preview","");}
        public string FirstPreviewChannel {get{return GameChannels.Choices(releases,"stable").Where(c=>c.Id!="stable"&&releases.Any(r=>r.Channel==c.Id)).Select(c=>c.Id).FirstOrDefault();}}
        public void Update(Release[] catalog,Release current,bool restricted,bool disconnected){if(IsOpen&&(restricted||current.Tag!=installed.Tag||current.Channel!=installed.Channel||disconnected!=offline))Close();releases=catalog??new Release[0];installed=current;blocked=restricted;offline=disconnected;}
        public void Close(){IsOpen=false;pending=null;if(Closed!=null)Closed();}
        TextBlock Text(string value,int size=12,string color="#D5E2ED"){return PageTheme.Text(value,size,color);}
        Button Row(string label,Action action,bool active=false){var button=new Button {Content=label,MinHeight=38,Padding=new Thickness(12,10,12,10),Margin=new Thickness(0,3,0,3),HorizontalAlignment=HorizontalAlignment.Stretch,HorizontalContentAlignment=HorizontalAlignment.Left};PageTheme.Secondary(button);button.Background=Ui.Brush(active?"#1D3A55":"#101E2D");button.BorderBrush=Ui.Brush(active?"#86CDFF":"#2D455C");button.BorderThickness=new Thickness(active?2:0,0,0,1);button.Click+=(s,e)=>action();AutomationProperties.SetName(button,label);View.Children.Add(button);return button;}
        Button Option(string title,string description,string version,Action action,bool active){var button=Row(title,action,active);var content=new StackPanel();content.Children.Add(Text(title+(active?"  ·  INSTALLED":""),12,"#A7D9FA"));content.Children.Add(Text(description,11,"#9FB5C9"));if(!String.IsNullOrEmpty(version))content.Children.Add(Text(version,11,"#A7D9FA"));button.Content=content;AutomationProperties.SetName(button,title+". "+description+". "+version+(active?". Installed":""));return button;}
        void Header(string title,bool back=false){View.Children.Clear();var header=new DockPanel {Margin=new Thickness(0,0,0,12)};var close=new Button {Content="×",MinWidth=28,MinHeight=28,Padding=new Thickness(4)};PageTheme.Secondary(close);AutomationProperties.SetName(close,"Close version picker");close.Click+=(s,e)=>Close();DockPanel.SetDock(close,Dock.Right);header.Children.Add(close);header.Children.Add(Text(title,18));View.Children.Add(header);if(back)Row("← Game version",ShowMain);}
        void Present(){IsOpen=true;if(Opened!=null)Opened();var first=View.Children.OfType<Button>().FirstOrDefault(b=>b.IsEnabled);if(first!=null)first.Focus();}
        void Footer(){View.Children.Add(new Border {Height=1,Background=Ui.Brush("#2D455C"),Margin=new Thickness(0,10,0,8)});Row("All game versions…",()=>{Close();if(Browse!=null)Browse();});View.Children.Add(Text(offline?"Offline · installed Play remains available":"Releases from GitHub · saves and settings are kept",10,"#829BB2"));}
        public void ShowMain(){if(blocked)return;pending=null;Header("Game version");bool has=installed.Tag!="Not installed";View.Children.Add(Text(has?"Installed · "+Name(installed)+" · "+installed.Tag:"Choose a channel to install.",11));var stable=GameChannels.Select(releases,"stable");Option("Stable","Recommended",stable==null?"No published release":stable.Tag,()=>Select("stable"),has&&installed.Channel=="stable").IsEnabled=!offline&&stable!=null;int count=GameChannels.Choices(releases,"stable").Count(c=>c.Id!="stable"&&releases.Any(r=>r.Channel==c.Id));Option("Preview tracks",count+" testing tracks · Choose a preview build",null,ShowPreview,false).IsEnabled=!offline&&count>0;Footer();Present();}
        public void ShowPreview(){if(blocked)return;pending=null;Header("Preview tracks",true);foreach(var choice in GameChannels.Choices(releases,"stable").Where(c=>c.Id!="stable")){var release=GameChannels.Select(releases,choice.Id);if(release==null)continue;string id=choice.Id;string detail=id=="beta"?"Test game updates and online multiplayer changes.":id=="story-coop"?"Test online Story co-op with matching game builds.":"Experimental builds for this testing track.";Option(choice.Name,detail,release.Tag,()=>Select(id),installed.Tag!="Not installed"&&installed.Channel==id).IsEnabled=!offline;}View.Children.Add(Text("Testing builds may have issues. Back up your saves before switching.",11));Footer();Present();}
        public void Select(string channel){if(blocked||offline)return;var target=GameChannels.Select(releases,channel);if(target==null)return;pending=target;Header("Switch game version",true);var comparison=new Grid {Margin=new Thickness(0,8,0,16)};comparison.ColumnDefinitions.Add(new ColumnDefinition());comparison.ColumnDefinitions.Add(new ColumnDefinition());for(int i=0;i<2;i++){var release=i==0?installed:target;var column=new StackPanel {Margin=new Thickness(0,0,12,0)};column.Children.Add(Text(i==0?"CURRENT":"SELECTED",10,"#91AFC6"));column.Children.Add(Text(Name(release),13));column.Children.Add(Text(release.Tag,12));Grid.SetColumn(column,i);comparison.Children.Add(column);}View.Children.Add(comparison);if(installed.Tag==target.Tag&&installed.Channel==target.Channel){View.Children.Add(Text("This is your installed version.",12));Row("Back",ShowMain);}else{View.Children.Add(Text("Your installed game stays available until the download is verified. Saves and settings are kept; compatibility may vary.",12));var cancel=Row("Cancel",Close);var apply=Row("Switch to "+Name(target),Confirm);PageTheme.Primary(apply);View.Children.Remove(cancel);View.Children.Remove(apply);var actions=new UniformGrid {Rows=1,Columns=2,Margin=new Thickness(0,16,0,0)};cancel.Margin=new Thickness(0,0,4,0);apply.Margin=new Thickness(4,0,0,0);actions.Children.Add(cancel);actions.Children.Add(apply);View.Children.Add(actions);}Present();}
        public void Confirm(){if(pending==null||blocked||offline||(pending.Tag==installed.Tag&&pending.Channel==installed.Channel))return;var target=pending;Close();if(Selected!=null)Selected(target);}
    }
}
