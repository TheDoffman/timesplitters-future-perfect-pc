using System;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Markup;
using System.Windows.Media;
using TimeSplitters.Launcher;

// Reorganizes the existing SettingsPage view. The same model, controls, draft,
// validation, Save/Cancel handlers and advanced options remain in use.
public sealed class MinimalSettings
{
    static readonly System.Runtime.CompilerServices.ConditionalWeakTable<SettingsPage,MinimalSettings> attached=new System.Runtime.CompilerServices.ConditionalWeakTable<SettingsPage,MinimalSettings>();
    readonly SettingsPage page;
    readonly TabControl tabs;
    readonly List<Button> categories=new List<Button>();
    readonly TextBlock heading;
    readonly List<FrameworkElement> categoryViews=new List<FrameworkElement>();
    readonly Dictionary<int,string> names=new Dictionary<int,string> {{0,"Controls"},{1,"Graphics"},{2,"Discord"},{3,"Audio & language"},{4,"View & HUD"},{5,"General"},{6,"Textures"}};
    static Brush B(string value){return (Brush)new BrushConverter().ConvertFromString(value);}
    static T Private<T>(SettingsPage p,string name) where T:class{return typeof(SettingsPage).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).GetValue(p) as T;}
    static IEnumerable<DependencyObject> Tree(DependencyObject root){yield return root;foreach(var c in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())foreach(var d in Tree(c))yield return d;}
    public static void Apply(SettingsPage page,Action tools){MinimalSettings value;if(!attached.TryGetValue(page,out value)){value=new MinimalSettings(page,tools);attached.Add(page,value);}value.Refresh();}
    MinimalSettings(SettingsPage settings,Action tools){
        page=settings;tabs=Private<TabControl>(page,"tabs");int selected=tabs.SelectedIndex;
        var originalControls=new HashSet<Control>(Tree(page.View).OfType<Control>());
        // Textures get their own destination; every texture control is moved intact.
        // 1.7.2 uses a tile panel. Keep its actual section expanders and controls,
        // but arrange each category in the approved single-column presentation.
        foreach(TabItem tab in tabs.Items){var scroll=tab.Content as ScrollViewer;var tiles=scroll==null?null:scroll.Content as SettingsTiles;if(tiles==null)continue;var list=new StackPanel();foreach(var child in tiles.Children.OfType<UIElement>().ToArray()){tiles.Children.Remove(child);list.Children.Add(child);}scroll.Content=list;}
        var graphics=((TabItem)tabs.Items[1]).Content as ScrollViewer;var graphicsBody=graphics.Content as Panel;
        var texturePanel=graphicsBody.Children.OfType<FrameworkElement>().FirstOrDefault(p=>Tree(p).OfType<Expander>().Any(e=>e.Name=="SettingsEnhancedtexturesSection"));
        if(texturePanel!=null){graphicsBody.Children.Remove(texturePanel);tabs.Items.Add(new TabItem {Header="Textures",Content=new ScrollViewer {Content=texturePanel,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled,Padding=new Thickness(0,0,10,0)}});}
        Simplify(page.View);
        var originalLayout=Private<DockPanel>(page,"layout");
        // Present the original categories in the same equal-width top bar as Tools.
        // Host the real category contents directly. Hiding TabControl's header
        // alone makes WPF's automation peer hide the settings from screen readers.
        var categoryContent=new Grid {Margin=new Thickness(0,8,0,12)};
        foreach(TabItem item in tabs.Items){var view=(FrameworkElement)item.Content;item.Content=null;categoryViews.Add(view);categoryContent.Children.Add(view);}
        originalLayout.Children.Remove(tabs);tabs.Visibility=Visibility.Collapsed;page.View.Children.Add(tabs);originalLayout.Children.Add(categoryContent);
        foreach(var oldHeader in originalLayout.Children.OfType<StackPanel>())oldHeader.Visibility=Visibility.Collapsed;
        page.View.Children.Remove(originalLayout);
        var frame=new Grid {Name="MinimalSettingsFrame"};frame.RowDefinitions.Add(new RowDefinition {Height=new GridLength(40)});frame.RowDefinitions.Add(new RowDefinition());
        page.View.Children.Insert(0,frame);page.View.Margin=new Thickness(28,0,28,4);
        // The original page disables its layout while a modal is open. The new
        // category bar lives outside that layout and must follow the same lock.
        var overlay=Private<Border>(page,"overlay");
        Action syncModal=()=>frame.IsEnabled=overlay.Visibility!=Visibility.Visible;
        System.ComponentModel.DependencyPropertyDescriptor.FromProperty(UIElement.VisibilityProperty,typeof(Border)).AddValueChanged(overlay,(s,e)=>syncModal());syncModal();
        var toolsButton=new Button {Content="←  Back to Launcher tools",FontSize=11,MinHeight=38,Background=B("#1C3349"),BorderBrush=B("#405D76"),BorderThickness=new Thickness(1),Foreground=B("#DFEAF4"),Padding=new Thickness(12,9,12,9),HorizontalContentAlignment=HorizontalAlignment.Center,Margin=new Thickness(0,0,20,0),ToolTip="Return to Launcher tools. Your unsaved settings stay in this session."};
        System.Windows.Automation.AutomationProperties.SetName(toolsButton,"Back to Launcher tools");
        toolsButton.Click+=(s,e)=>tools();
        var choices=new UniformGrid {Rows=1,Columns=7};frame.Children.Add(choices);
        foreach(int index in new[]{0,1,3,4,6,2,5}){
            if(index>=tabs.Items.Count)continue;int captured=index;
            var button=new Button {Content=index==3?"Audio":names[index],ToolTip=names[index],Tag=index,HorizontalContentAlignment=HorizontalAlignment.Center,Padding=new Thickness(8,8,8,8),Margin=new Thickness(0),FontSize=12,MinHeight=34,BorderThickness=new Thickness(0,0,0,1),Background=Brushes.Transparent,Foreground=B("#B4C7D8")};
            System.Windows.Automation.AutomationProperties.SetName(button,names[index]);
            button.Click+=(s,e)=>{page.SelectTab(captured);var scroll=categoryViews[captured] as ScrollViewer;if(scroll!=null)scroll.ScrollToHome();Refresh();};categories.Add(button);choices.Children.Add(button);
        }
        var content=new DockPanel {LastChildFill=true,Margin=new Thickness(0,16,0,0)};Grid.SetRow(content,1);frame.Children.Add(content);
        heading=new TextBlock {Name="MinimalCategoryTitle",FontSize=23,FontWeight=FontWeights.Light,Foreground=B("#E4EDF5"),Margin=new Thickness(0,6,0,16)};DockPanel.SetDock(heading,Dock.Top);content.Children.Add(heading);content.Children.Add(originalLayout);
        var footer=Private<Border>(page,"footerBorder");footer.Padding=new Thickness(0,12,0,0);footer.BorderBrush=B("#263C50");
        var footerBody=(DockPanel)footer.Child;DockPanel.SetDock(toolsButton,Dock.Left);footerBody.Children.Insert(0,toolsButton);
        var status=Private<TextBlock>(page,"status");if(status.Text=="Home keeps your draft. Save when you’re ready.")status.Text="Changes apply when you save.";
        var save=Private<Button>(page,"save");save.Content="Save changes";
        MinimalTools.AlignButtonRows(page.View);
        var footerActions=save.Parent as StackPanel;if(footerActions!=null)foreach(var button in footerActions.Children.OfType<Button>()){button.Width=112;button.MinHeight=38;button.Margin=new Thickness(4,0,4,0);}
        tabs.SelectionChanged+=(s,e)=>{if(e.Source==tabs)Refresh();};page.SelectTab(selected>=0?selected:0);Refresh();
        var retained=new HashSet<Control>(Tree(page.View).OfType<Control>());
        if(originalControls.Any(c=>!retained.Contains(c)))throw new InvalidOperationException("A settings control was detached during layout changes.");
    }
    void Refresh(){if(heading==null)return;heading.Text=names.ContainsKey(tabs.SelectedIndex)?names[tabs.SelectedIndex]:"Settings";for(int i=0;i<categoryViews.Count;i++)categoryViews[i].Visibility=i==tabs.SelectedIndex?Visibility.Visible:Visibility.Collapsed;foreach(var button in categories){bool active=(int)button.Tag==tabs.SelectedIndex;button.Background=Brushes.Transparent;button.Foreground=active?B("#86CDFF"):B("#B4C7D8");button.BorderBrush=active?B("#A7D9FA"):B("#2D455C");button.BorderThickness=new Thickness(0,0,0,active?2:1);}}
    static void Simplify(DependencyObject root){
        foreach(var element in Tree(root).ToArray()){
            var text=element as TextBlock;
            if(text!=null && text.Text.StartsWith("In-game settings tabs:"))text.Visibility=Visibility.Collapsed;
            var border=element as Border;
            if(border!=null && border.Child is Expander){
                var section=(Expander)border.Child;var header=section.Header as StackPanel;var caption=header==null?null:header.Children.OfType<TextBlock>().FirstOrDefault();
                if(section.Name.StartsWith("Settings") && caption!=null){
                    border.Background=Brushes.Transparent;border.BorderThickness=new Thickness(0,0,0,1);border.BorderBrush=B("#263C50");border.Padding=new Thickness(0,12,0,16);border.Margin=new Thickness(0,0,0,6);
                    if(header.Children.Count>1 && header.Children[1] is TextBlock){var help=(TextBlock)header.Children[1];help.Visibility=Visibility.Collapsed;caption.ToolTip=help.ToolTip??help.Text;System.Windows.Automation.AutomationProperties.SetHelpText(section,Convert.ToString(help.ToolTip??help.Text));}
                    section.IsExpanded=caption.Text=="Input & mouse"||caption.Text=="Display"||caption.Text=="Frame rate"||caption.Text=="Volume"||caption.Text=="Listening options"||caption.Text=="Camera"||caption.Text=="HUD"||caption.Text=="Startup"||caption.Text=="Game language"||caption.Text=="Enhanced textures"||caption.Text=="Discord Rich Presence";
                }
            }
            if(border==null || !(border.Child is StackPanel))continue;
            var body=(StackPanel)border.Child;var title=body.Children.OfType<TextBlock>().FirstOrDefault();
            var fill=border.Background as SolidColorBrush;
            if(fill==null || fill.Color!=((SolidColorBrush)B("#122233")).Color)continue;
            border.Background=Brushes.Transparent;border.BorderThickness=new Thickness(0,0,0,1);border.BorderBrush=B("#263C50");border.Padding=new Thickness(0,12,0,18);border.Margin=new Thickness(0,0,0,8);
            if(title==null)continue;
            title.FontSize=13;title.FontWeight=FontWeights.SemiBold;title.Foreground=B("#B9D0E2");title.Margin=new Thickness(0,0,0,12);
            // Secondary descriptions are available beside the heading on demand.
            if(body.Children.Count>1 && body.Children[1] is TextBlock){
                var help=(TextBlock)body.Children[1];help.Visibility=Visibility.Collapsed;
                var head=new DockPanel {LastChildFill=false,Margin=new Thickness(0,0,0,2)};body.Children.Remove(title);
                head.Children.Add(title);var info=new Button {Content="?",FontSize=10,MinHeight=20,Width=22,Padding=new Thickness(0),Margin=new Thickness(8,-8,0,0),Background=Brushes.Transparent,BorderThickness=new Thickness(0),Foreground=B("#829BB2"),ToolTip=help.Text};
                System.Windows.Automation.AutomationProperties.SetName(info,"About "+title.Text);info.Click+=(s,e)=>help.Visibility=help.Visibility==Visibility.Visible?Visibility.Collapsed:Visibility.Visible;head.Children.Add(info);body.Children.Insert(0,head);
            }
            // Less-frequent input configuration stays accessible without filling
            // the default Controls view with a bindings table and player profiles.
            if((title.Text=="Key bindings" || title.Text=="Local multiplayer") && border.Parent is Panel){
                var parent=(Panel)border.Parent;int at=parent.Children.IndexOf(border);parent.Children.Remove(border);
                var details=new Expander {Header=title.Text,Content=border,IsExpanded=false,FontSize=12,Foreground=B("#B9D0E2"),Margin=new Thickness(0,10,0,4)};title.Visibility=Visibility.Collapsed;parent.Children.Insert(at,details);
            }
        }
        // Complete the same alignment for fields containing toggles, sliders or
        // compound selectors such as the previous/next resolution controls.
        foreach(var field in Tree(root).OfType<StackPanel>().ToArray()){
            if(field.Children.Count!=2 || !(field.Children[0] is TextBlock) || (!(field.Children[1] is Control) && !(field.Children[1] is DockPanel)))continue;
            var label=(FrameworkElement)field.Children[0];var input=(FrameworkElement)field.Children[1];field.Children.Clear();
            var row=new Grid();row.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(180)});row.ColumnDefinitions.Add(new ColumnDefinition());label.VerticalAlignment=VerticalAlignment.Center;label.Margin=new Thickness(0,0,20,0);input.Margin=new Thickness(0);Grid.SetColumn(input,1);row.Children.Add(label);row.Children.Add(input);field.Children.Add(row);field.Margin=new Thickness(0,8,0,8);
        }
    }
}
