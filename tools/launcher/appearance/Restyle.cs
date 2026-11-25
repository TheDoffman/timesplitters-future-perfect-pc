using System;
using System.Linq;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

// Restyles the original controls. No replacement models or dummy settings.
public static class Restyle
{
    static bool Mark(System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> cache,DependencyObject key){object value;if(cache.TryGetValue(key,out value))return false;cache.Add(key,new object());return true;}
    static readonly System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> done=new System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object>();
    static Brush B(string c){return (Brush)new BrushConverter().ConvertFromString(c);}
    static Brush Convert(Brush brush){return brush;}
    public static void Apply(DependencyObject root){if(root==null)return;if(Mark(done,root)){
        var ctrl=root as Control;var before=ctrl==null?null:ctrl.Background as SolidColorBrush;bool wasPrimary=before!=null && before.Color.R>100 && before.Color.G>170 && before.Color.B>before.Color.R+12;
        if(ctrl!=null){ctrl.Foreground=Convert(ctrl.Foreground);ctrl.Background=Convert(ctrl.Background);ctrl.BorderBrush=Convert(ctrl.BorderBrush);}
        var panel=root as Panel;if(panel!=null)panel.Background=Convert(panel.Background);
        var border=root as Border;if(border!=null){var old=border.Background as SolidColorBrush;bool section=border.CornerRadius.TopLeft>=7 && old!=null && old.Color.B>old.Color.R+8;border.Background=Convert(border.Background);border.BorderBrush=Convert(border.BorderBrush);border.CornerRadius=new CornerRadius(0);if(section){border.Background=B("#122233");border.BorderBrush=B("#2D455C");border.BorderThickness=new Thickness(0,0,0,1);border.Padding=new Thickness(14,14,14,18);border.Margin=new Thickness(0,0,0,4);}}
        var text=root as TextBlock;if(text!=null && text.Name!="MinimalCategoryTitle"){text.Foreground=Convert(text.Foreground);if(text.FontSize>=25){text.FontSize=21;text.FontWeight=FontWeights.Light;}else if(text.FontSize>=16){text.FontSize=14;text.FontWeight=FontWeights.Normal;}else if(text.FontSize==13)text.FontSize=12;else if(text.FontSize==12)text.FontSize=11;}
        var button=root as Button;if(button!=null){if(wasPrimary){button.Background=B("#A7D9FA");button.Foreground=B("#091C2C");button.BorderBrush=B("#D6EDFF");}button.FontSize=Math.Min(11,button.FontSize);}
        var box=root as TextBox;if(box!=null){box.Background=Brushes.Transparent;box.BorderThickness=new Thickness(0,0,0,1);box.BorderBrush=B("#3B5268");}
        var combo=root as ComboBox;if(combo!=null){combo.Background=Brushes.Transparent;combo.BorderThickness=new Thickness(0,0,0,1);combo.BorderBrush=B("#3B5268");}
        var grid=root as Grid;if(grid!=null && grid.ColumnDefinitions.Count==2 && grid.RowDefinitions.Count==0 && grid.Children.Count==2 && grid.Children.OfType<StackPanel>().Count()==2){grid.ColumnDefinitions.Clear();int row=0;foreach(FrameworkElement child in grid.Children){grid.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});Grid.SetColumn(child,0);Grid.SetRow(child,row++);child.Margin=new Thickness(0);}}
        var stack=root as StackPanel;if(stack!=null && stack.Children.Count==2 && stack.Children[0] is TextBlock && (stack.Children[1] is ComboBox || stack.Children[1] is TextBox)){var label=(FrameworkElement)stack.Children[0];var input=(FrameworkElement)stack.Children[1];stack.Children.Clear();var row=new Grid();row.ColumnDefinitions.Add(new ColumnDefinition {Width=new GridLength(180)});row.ColumnDefinitions.Add(new ColumnDefinition());label.Margin=new Thickness(0,0,24,0);label.VerticalAlignment=VerticalAlignment.Center;input.Margin=new Thickness(0);Grid.SetColumn(input,1);row.Children.Add(label);row.Children.Add(input);stack.Children.Add(row);stack.Margin=new Thickness(0,8,0,8);}
        // The outer dialog already supplies the title. Retain each page's help
        // text and toolbar, and reclaim the duplicate heading's vertical space.
        if(stack!=null && stack.Children.Count>=3 && stack.Children[0] is TextBlock && stack.Children[1] is TextBlock && ((TextBlock)stack.Children[1]).FontSize>=25){stack.Children[0].Visibility=Visibility.Collapsed;stack.Children[1].Visibility=Visibility.Collapsed;((FrameworkElement)stack.Children[2]).Margin=new Thickness(0,0,0,6);foreach(var line in stack.Children.OfType<Border>()){line.Margin=new Thickness(0,6,0,4);}stack.Margin=new Thickness(0,0,0,4);}
    }foreach(var child in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())Apply(child);}
}
