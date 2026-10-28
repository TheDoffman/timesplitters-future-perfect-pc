
#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Automation;
using UIElement = Avalonia.Controls.Control;
using Brush = Avalonia.Media.IBrush;
using FontWeights = Avalonia.Media.FontWeight;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Markup;
#endif

namespace TimeSplitters.Launcher
{
    // Scoped to the embedded pages; native controls keep their keyboard behavior.
    public enum StatusTone { Neutral, Info, Success, Warning }
    public static class PageTheme
    {
        public static Brush Brush(string value) { return Ui.Brush(value); }
        public static TextBlock Text(string text,int size,string color="#E4EDF5")
        { return new TextBlock { Text=text,FontSize=size,Foreground=Brush(color),TextWrapping=TextWrapping.Wrap }; }
        public static Border Card(UIElement child)
        { return new Border { Background=Brush("#122233"),BorderBrush=Brush("#2D455C"),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(7),Padding=new Thickness(16),Child=child }; }
        public static StackPanel Section(Panel parent,string title,string description)
        {
            var body=new StackPanel();var heading=Text(title,17);heading.FontWeight=FontWeights.SemiBold;body.Children.Add(heading);
            if(description.Length>0){var hint=Text(description,13,"#B4C7D8");hint.Margin=new Thickness(0,4,0,10);body.Children.Add(hint);}
            var card=Card(body);card.Margin=new Thickness(0,0,0,14);parent.Children.Add(card);return body;
        }
        public static Expander Details(Panel parent,string title,params UIElement[] items)
        {
            var body=new StackPanel {Margin=new Thickness(0,8,0,4)};
            foreach(var item in items)body.Children.Add(item);
            var details=new Expander {Header=title,Content=body,IsExpanded=false,Foreground=Brush("#B4C7D8"),FontSize=12,Margin=new Thickness(0,10,0,0)};
            parent.Children.Add(details);return details;
        }
        public static StackPanel Header(string category,string title,string description)
        {
            var header=new StackPanel {Margin=new Thickness(0,0,0,10)};
            var eyebrow=Text(category,11,"#86CDFF");eyebrow.FontWeight=FontWeights.SemiBold;header.Children.Add(eyebrow);
            var heading=Text(title,28);heading.FontWeight=FontWeights.SemiBold;heading.Margin=new Thickness(0,5,0,7);header.Children.Add(heading);
            header.Children.Add(Text(description,13,"#AFC3D5"));
            header.Children.Add(new Border {Height=1,Background=Brush("#2D455C"),Margin=new Thickness(0,16,0,12)});
            return header;
        }
        public static Border Badge(string label,StatusTone tone=StatusTone.Info)
        {
            var badge=new Border {CornerRadius=new CornerRadius(4),BorderThickness=new Thickness(1),Padding=new Thickness(9,4,9,4),
                HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Center,Child=Text(label,11)};
            ((TextBlock)badge.Child).FontWeight=FontWeights.SemiBold;SetBadge(badge,label,tone);return badge;
        }
        public static void SetBadge(Border badge,string label,StatusTone tone)
        {
            var text=(TextBlock)badge.Child;text.Text=label;
            string ink=tone==StatusTone.Success?"#A6E6D5":tone==StatusTone.Warning?"#FFDAA6":tone==StatusTone.Info?"#B4DFFF":"#B9CAD9";
            text.Foreground=Brush(ink);badge.Background=Brush(tone==StatusTone.Success?"#173D3B":tone==StatusTone.Warning?"#423322":tone==StatusTone.Info?"#1B3852":"#213245");
            badge.BorderBrush=Brush(tone==StatusTone.Success?"#3D7069":tone==StatusTone.Warning?"#806441":tone==StatusTone.Info?"#3B6281":"#40556B");
        }
        public static void Primary(Button button)
        { button.Background=Brush("#A7D9FA");button.Foreground=Brush("#0B2337");button.BorderBrush=Brush("#D6EBFA"); }
        public static void Secondary(Button button)
        { button.Background=Brush("#1C3349");button.Foreground=Brush("#DFEAF4");button.BorderBrush=Brush("#405D76"); }
#if !MACOS
        public static ResourceDictionary Tabs() { return Ui.LoadTheme("TabsTheme.xaml"); }
#endif
#if !MACOS
        public static ResourceDictionary Resources() { return Ui.LoadTheme("PageTheme.xaml"); }
#endif
    }
}
