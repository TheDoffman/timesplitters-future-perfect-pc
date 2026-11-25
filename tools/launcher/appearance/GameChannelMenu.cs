using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Markup;
using System.Windows.Media;
using TimeSplitters.Launcher;

public sealed class GameChannelMenu
{
    readonly Button entry;readonly TextBlock label;readonly Border panel;
    readonly GameVersionMenuPage page=new GameVersionMenuPage();Release installed;
    public event Action OpenChanged;
    static Brush B(string value){return (Brush)new BrushConverter().ConvertFromString(value);}
    public GameChannelMenu(Window window,Grid stage,TextBlock text,GameChannelPickerPage native,Action browse,Action<Release> confirm,Release current){
        label=text;installed=current;var parent=(Panel)label.Parent;parent.Children.Remove(label);
        entry=new Button {Content=label,Padding=new Thickness(4,2,4,2),Margin=new Thickness(0),MinHeight=24,Background=Brushes.Transparent,BorderThickness=new Thickness(0),Foreground=B("#A7D9FA"),HorizontalAlignment=HorizontalAlignment.Right,Cursor=Cursors.Hand};
        label.FontSize=11;label.MaxWidth=252;label.TextWrapping=TextWrapping.NoWrap;label.TextTrimming=TextTrimming.CharacterEllipsis;parent.Children.Add(entry);
        System.Windows.Automation.AutomationProperties.SetName(entry,"Game channel and version");entry.Click+=(s,e)=>{if(IsOpen)Close();else ShowMain();};
        panel=new Border {Child=new ScrollViewer {Content=page.View,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled},Width=360,MaxHeight=470,Padding=new Thickness(16),Background=B("#101E2D"),BorderBrush=B("#3B5268"),BorderThickness=new Thickness(1),Margin=new Thickness(0,0,28,128),HorizontalAlignment=HorizontalAlignment.Right,VerticalAlignment=VerticalAlignment.Bottom,Visibility=Visibility.Collapsed};
        KeyboardNavigation.SetTabNavigation(panel,KeyboardNavigationMode.Cycle);stage.Children.Add(panel);
        page.Opened+=()=>{panel.Visibility=Visibility.Visible;if(OpenChanged!=null)OpenChanged();};page.Closed+=()=>{panel.Visibility=Visibility.Collapsed;if(OpenChanged!=null)OpenChanged();};page.Browse+=browse;page.Selected+=confirm;
        window.SizeChanged+=(s,e)=>panel.MaxHeight=Math.Max(240,window.ActualHeight-160);
        window.PreviewKeyDown+=(s,e)=>{if(IsOpen&&e.Key==Key.Escape){Close();entry.Focus();e.Handled=true;}};
        window.PreviewMouseDown+=(s,e)=>{if(IsOpen&&!Inside(e.OriginalSource as DependencyObject,panel)&&!Inside(e.OriginalSource as DependencyObject,entry))Close();};
        Update(new Release[0],current,false,false);
    }
    static bool Inside(DependencyObject element,DependencyObject parent){while(element!=null){if(element==parent)return true;element=element is Visual?VisualTreeHelper.GetParent(element):LogicalTreeHelper.GetParent(element);}return false;}
    public bool IsOpen {get{return page.IsOpen;}}
    public string InstalledChannel {get{return installed.Channel;}}
    public string InstalledTag {get{return installed.Tag;}}
    public string FirstPreviewChannel {get{return page.FirstPreviewChannel;}}
    public void Close(){page.Close();}
    public void ShowMain(){page.ShowMain();}
    public void ShowPreview(){page.ShowPreview();}
    public void Select(string channel){page.Select(channel);}
    public void Confirm(){page.Confirm();}
    public void Update(Release[] catalog,Release current,bool restricted,bool disconnected){
        page.Update(catalog,current,restricted,disconnected);installed=current;entry.IsEnabled=!restricted;
        label.Text=GameVersionMenuPage.Name(installed)+" · "+installed.Tag+"  ▾";
        entry.ToolTip="Installed: "+GameVersionMenuPage.Name(installed)+" · "+installed.Tag+". Choose a channel or game version.";
    }
}
