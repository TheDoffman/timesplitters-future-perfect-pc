#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
#else
using System.Windows;
using System.Windows.Controls;
#endif

namespace TimeSplitters.Launcher
{
    // Keep the two live status lines in measured rows, not overlapping offsets.
    public sealed class HomeStatusPage
    {
        public readonly Grid View=new Grid {VerticalAlignment=VerticalAlignment.Bottom};
        public HomeStatusPage(TextBlock kind,TextBlock status,TextBlock texture,ProgressBar progress)
        {
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            var row=new Grid {MinHeight=16};row.ColumnDefinitions.Add(new ColumnDefinition {Width=GridLength.Auto});row.ColumnDefinitions.Add(new ColumnDefinition());
            kind.Margin=new Thickness(0,0,16,0);kind.TextWrapping=TextWrapping.NoWrap;row.Children.Add(kind);
            status.Margin=new Thickness(0);status.TextWrapping=TextWrapping.NoWrap;status.TextTrimming=TextTrimming.CharacterEllipsis;status.TextAlignment=TextAlignment.Right;Grid.SetColumn(status,1);row.Children.Add(status);View.Children.Add(row);
            texture.Margin=new Thickness(0,2,0,0);texture.MinHeight=16;texture.TextWrapping=TextWrapping.NoWrap;texture.TextTrimming=TextTrimming.CharacterEllipsis;texture.VerticalAlignment=VerticalAlignment.Top;Grid.SetRow(texture,1);View.Children.Add(texture);
            progress.Height=3;progress.Margin=new Thickness(0,6,0,0);Grid.SetRow(progress,2);View.Children.Add(progress);
        }
    }
}
