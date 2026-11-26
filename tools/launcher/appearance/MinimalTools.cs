using System;
using System.Linq;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using TimeSplitters.Launcher;

// The outer launcher-tools navigation already selects repair/reports/console.
// Keep the real tab selection/events, without displaying a second navigation row.
public static class MinimalTools
{
    static bool Mark(System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> cache,DependencyObject key){object value;if(cache.TryGetValue(key,out value))return false;cache.Add(key,new object());return true;}
    static readonly System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> attached=new System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object>();
    static readonly System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> polished=new System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object>();
    static Brush Ink(string color){return (Brush)new BrushConverter().ConvertFromString(color);}
    static IEnumerable<DependencyObject> Tree(DependencyObject root){yield return root;foreach(var child in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())foreach(var item in Tree(child))yield return item;}
    static void Link(Button button){button.Background=Brushes.Transparent;button.BorderThickness=new Thickness(0,0,0,1);button.BorderBrush=Brushes.Transparent;button.Foreground=Ink("#85D4FF");button.Padding=new Thickness(0,6,0,6);button.MinHeight=30;}
    static void SizeAction(Button button){
        button.HorizontalAlignment=HorizontalAlignment.Stretch;button.ClearValue(FrameworkElement.WidthProperty);button.MinWidth=0;button.MinHeight=38;button.Margin=new Thickness(4,0,4,0);button.Padding=new Thickness(8,9,8,9);
        if(button.Background==Brushes.Transparent){button.Background=Ink("#173A59");button.BorderBrush=Ink("#4779A0");button.BorderThickness=new Thickness(1);button.Foreground=Ink("#DFEAF4");}
    }
    public static void AlignButtonRows(DependencyObject root){
        foreach(var panel in Tree(root).OfType<WrapPanel>().Where(p=>p.Children.Count>=2 && p.Children.OfType<Button>().Count()==p.Children.Count).ToArray()){
            var parent=panel.Parent as Panel;if(parent==null)continue;
            var row=new UniformGrid {Rows=1,Columns=panel.Children.Count,Margin=new Thickness(panel.Margin.Left-4,panel.Margin.Top,panel.Margin.Right-4,panel.Margin.Bottom)};
            Grid.SetRow(row,Grid.GetRow(panel));Grid.SetColumn(row,Grid.GetColumn(panel));Grid.SetColumnSpan(row,Grid.GetColumnSpan(panel));DockPanel.SetDock(row,DockPanel.GetDock(panel));
            foreach(var button in panel.Children.OfType<Button>().ToArray()){panel.Children.Remove(button);SizeAction(button);row.Children.Add(button);}
            int at=parent.Children.IndexOf(panel);parent.Children.Remove(panel);parent.Children.Insert(at,row);
        }
    }
    // Presentation only: retain every original control instance and its handlers.
    public static void Polish(FrameworkElement root,string page){
        if(root==null || !Mark(polished,root))return;
        var controls=new HashSet<Control>(Tree(root).OfType<Control>());
        foreach(var border in Tree(root).OfType<Border>().ToArray()){
            var fill=border.Background as SolidColorBrush;
            if(fill==null || fill.Color!=((SolidColorBrush)Ink("#10243B")).Color)continue;
            if(border.Child is TextBox){border.Padding=new Thickness(0);border.Background=Ink("#0E1D2C");border.BorderThickness=new Thickness(1);continue;}
            if(!(border.Child is Panel) && !(border.Child is Expander))continue;
            border.Background=Brushes.Transparent;border.Padding=new Thickness(0,12,0,16);border.Margin=new Thickness(0,0,0,8);border.BorderThickness=new Thickness(0,0,0,1);
        }
        if(page=="Installation"){
            var scroll=Tree(root).OfType<ScrollViewer>().First(v=>v.Name=="HomeCardsScroll");
            scroll.Margin=new Thickness(0,8,0,0);var cards=(Grid)scroll.Content;cards.ClearValue(FrameworkElement.MinHeightProperty);
            foreach(var card in cards.Children.OfType<Border>()){
                var grid=card.Child as Grid;if(grid==null)continue;
                foreach(var row in grid.RowDefinitions){row.MinHeight=0;row.Height=GridLength.Auto;}
                card.Padding=new Thickness(0,6,0,12);
                foreach(var text in grid.Children.OfType<TextBlock>())text.Margin=new Thickness(0,6,0,8);
                foreach(var panel in grid.Children.OfType<StackPanel>())panel.Margin=new Thickness(0,6,0,6);
            }
            // Channel choice is already on Home; do not duplicate it in Tools.
            var channel=Tree(root).OfType<FrameworkElement>().FirstOrDefault(v=>v.Name=="GameChannelPickerHost");
            if(channel!=null){channel.Visibility=Visibility.Collapsed;}
            var update=Tree(root).OfType<Grid>().First(g=>g.Name=="UpdateActions");update.ColumnDefinitions[0].Width=new GridLength(1,GridUnitType.Star);update.ColumnDefinitions[2].Width=new GridLength(1,GridUnitType.Star);
            var progress=Tree(root).OfType<ProgressBar>().First(g=>g.Name=="UpdateProgress");if(progress.Visibility==Visibility.Hidden)progress.Visibility=Visibility.Collapsed;
            foreach(var button in update.Children.OfType<Button>()){SizeAction(button);button.Margin=new Thickness(0);}
        }
        if(page=="Saves"){
            root.Margin=new Thickness(30,16,30,12);
            foreach(var panel in Tree(root).OfType<StackPanel>().Where(p=>p.Children.Count==4 && p.Children[0] is TextBlock && p.Children[1] is TextBlock && p.Children[3] is Border)){System.Windows.Automation.AutomationProperties.SetHelpText(root,((TextBlock)panel.Children[2]).Text);panel.Visibility=Visibility.Collapsed;}
            foreach(var card in Tree(root).OfType<Border>().Where(b=>b.Child is StackPanel && b.BorderThickness.Bottom==1)){card.Padding=new Thickness(0,8,0,10);card.Margin=new Thickness(0,0,0,4);}
            var location=Tree(root).OfType<TextBlock>().First(t=>t.Name=="ActiveSaveLocation");
            var section=(StackPanel)location.Parent;
            var actions=section.Children.OfType<WrapPanel>().First();int position=section.Children.IndexOf(actions);section.Children.Remove(actions);section.Children.Remove(location);
            var body=new StackPanel();body.Children.Add(location);body.Children.Add(actions);
            var disclosure=new Expander {Header="Save folder location",Content=body,Foreground=Ink("#B4C7D8"),FontSize=12,Margin=new Thickness(0,8,0,8)};
            section.Children.Insert(Math.Max(0,position-1),disclosure);
            foreach(var button in Tree(root).OfType<Button>().Where(b=>b.Name=="OpenSaveFolder"||b.Name=="OpenSaveBackups"||b.Name=="RefreshSaveBackups"))Link(button);
        }
        if(page=="Repair"){
            foreach(var panel in Tree(root).OfType<StackPanel>().Where(p=>p.Children.OfType<WrapPanel>().Any(w=>w.Children.OfType<Button>().Any(b=>b.Name=="VerifyGameFiles"||b.Name=="VerifyLauncherFiles")))){
                var top=panel.Children.OfType<DockPanel>().First();top.Margin=new Thickness(0,0,0,10);
                foreach(var text in panel.Children.OfType<TextBlock>()){text.MinHeight=0;text.Margin=new Thickness(0,6,0,10);}
                var card=panel.Parent as Border;if(card!=null){card.Margin=Grid.GetColumn(card)==0?new Thickness(0,0,16,8):new Thickness(16,0,0,8);}
            }
            foreach(var button in Tree(root).OfType<Button>().Where(b=>b.Name=="OpenGameFiles"||b.Name=="MaintenanceLogs"))Link(button);
        }
        if(page=="Diagnostics"){
            var report=Tree(root).OfType<TextBox>().First(t=>t.Name=="DiagnosticsReport");var dock=Tree(root).OfType<DockPanel>().First(d=>d.Children.OfType<WrapPanel>().Count()==2);
            var toolbar=dock.Children.OfType<WrapPanel>().First();var crash=dock.Children.OfType<WrapPanel>().Last();dock.Children.Remove(crash);
            var support=new WrapPanel();foreach(var button in toolbar.Children.OfType<Button>().Where(b=>b.Name=="ReportDiagnosticBug"||b.Name=="RequestDiagnosticEnhancement").ToArray()){toolbar.Children.Remove(button);support.Children.Add(button);}
            var extended=dock.Children.OfType<CheckBox>().First();dock.Children.Remove(extended);
            var options=new StackPanel();var actionRow=new UniformGrid {Rows=1,Columns=4,Margin=new Thickness(-4,8,-4,12)};
            foreach(var button in support.Children.OfType<Button>().Concat(crash.Children.OfType<Button>()).ToArray()){
                ((Panel)button.Parent).Children.Remove(button);button.Margin=new Thickness(4,0,4,0);button.Padding=new Thickness(8,9,8,9);button.HorizontalAlignment=HorizontalAlignment.Stretch;actionRow.Children.Add(button);
            }
            options.Children.Add(actionRow);options.Children.Add(extended);
            var disclosure=new Expander {Name="DiagnosticsSupportOptions",Header="Reports & support options",Content=options,IsExpanded=false,Foreground=Ink("#B4C7D8"),FontSize=12,Margin=new Thickness(0,0,0,14)};DockPanel.SetDock(disclosure,Dock.Top);dock.Children.Insert(1,disclosure);
            foreach(var button in toolbar.Children.OfType<Button>())Link(button);
            report.Padding=new Thickness(16);report.FontSize=12;
            foreach(var button in Tree(root).OfType<Button>().Where(b=>b.Name=="MaintenanceLogs"))Link(button);
        }
        if(page=="Console"){
            root.Margin=new Thickness(0);
            var output=Tree(root).OfType<TextBox>().First(t=>t.Name=="GameConsoleOutput");output.Padding=new Thickness(16);output.FontSize=12;
            var toolbar=Tree(root).OfType<WrapPanel>().First(p=>p.Children.OfType<Button>().Any(b=>b.Name=="PauseConsole"));
            var parent=(Panel)toolbar.Parent;var row=new UniformGrid {Rows=1,Columns=4,Margin=new Thickness(-4,0,-4,16)};Grid.SetRow(row,Grid.GetRow(toolbar));
            foreach(var item in toolbar.Children.OfType<FrameworkElement>().ToArray()){toolbar.Children.Remove(item);var button=item as Button;if(button!=null)SizeAction(button);else {item.Margin=new Thickness(4,0,4,0);item.HorizontalAlignment=HorizontalAlignment.Center;}row.Children.Add(item);}
            int index=parent.Children.IndexOf(toolbar);parent.Children.Remove(toolbar);parent.Children.Insert(index,row);
        }
        AlignButtonRows(root);
        var retained=new HashSet<Control>(Tree(root).OfType<Control>());if(controls.Any(c=>!retained.Contains(c)))throw new InvalidOperationException("A tools control was detached during visual refinement: "+page);
    }
    public static void ResetDiagnostics(DependencyObject root){foreach(var section in Tree(root).OfType<Expander>().Where(e=>e.Name=="DiagnosticsSupportOptions"))section.IsExpanded=false;}
    public static void Apply(MaintenanceArea area){
        if(area==null || !Mark(attached,area.View))return;
        var tabs=area.Tabs;int selected=tabs.SelectedIndex;var content=new Grid();var views=new List<FrameworkElement>();Grid.SetRow(content,1);
        foreach(TabItem item in tabs.Items){var view=(FrameworkElement)item.Content;Restyle.Apply(view);Polish(view,new[]{"Repair","Diagnostics","Console"}[views.Count]);item.Content=null;views.Add(view);content.Children.Add(view);}
        area.View.Children.Remove(tabs);tabs.Visibility=Visibility.Collapsed;area.View.Children.Add(tabs);area.View.Children.Add(content);
        foreach(var header in area.View.Children.OfType<StackPanel>())header.Visibility=Visibility.Collapsed;
        Action show=()=>{for(int i=0;i<views.Count;i++)views[i].Visibility=i==tabs.SelectedIndex?Visibility.Visible:Visibility.Collapsed;};
        tabs.SelectionChanged+=(s,e)=>{if(e.Source==tabs)show();};tabs.SelectedIndex=selected<0?0:selected;show();
    }
}
