using System;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
#endif

namespace TimeSplitters.Launcher
{
    // Shared presentation only; hosts retain update, cancellation and recovery ownership.
    public sealed class UpdateWindowPage
    {
        public readonly Grid View=new Grid();
        public readonly TextBlock Heading,Status,Note;
        public readonly ProgressBar Progress;
        public readonly UniformGrid Actions=new UniformGrid {Rows=1,Columns=1};
        public UpdateWindowPage(string heading,string status,string note)
        {
            Ui.Theme(View);View.Background=Ui.Brush("#071321");
            var frame=new DockPanel {Margin=new Thickness(28,24,28,24)};View.Children.Add(frame);
            var brand=new Border {BorderBrush=Ui.Brush("#294E70"),BorderThickness=new Thickness(0,0,0,1),Padding=new Thickness(0,0,0,14),Child=PageTheme.Text("TIMESPLITTERS  /  FUTURE PERFECT",11,"#85D4FF")};DockPanel.SetDock(brand,Dock.Top);frame.Children.Add(brand);
            var title=new StackPanel {Margin=new Thickness(0,20,0,16)};Heading=PageTheme.Text(heading,25);title.Children.Add(Heading);DockPanel.SetDock(title,Dock.Top);frame.Children.Add(title);
            var footer=new StackPanel();DockPanel.SetDock(footer,Dock.Bottom);frame.Children.Add(footer);
            Progress=new ProgressBar {Height=3,Minimum=0,Maximum=100,IsIndeterminate=true,Foreground=Ui.Brush("#65C7FF"),Background=Ui.Brush("#204566")};footer.Children.Add(Progress);
            Note=PageTheme.Text(note,12,"#9FB5C9");Note.Margin=new Thickness(0,16,0,0);footer.Children.Add(Note);
            Actions.Margin=new Thickness(0,16,0,0);footer.Children.Add(Actions);
            Status=PageTheme.Text(status,13,"#D5E2ED");
            frame.Children.Add(new ScrollViewer {Content=Status,Margin=new Thickness(0,0,0,20),VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled});
        }
        public Button AddAction(string label,Action action,bool primary=false)
        {
            var button=new Button {Content=label,MinHeight=38,Padding=new Thickness(12,9,12,9),HorizontalAlignment=HorizontalAlignment.Stretch,HorizontalContentAlignment=HorizontalAlignment.Center};
            if(primary)PageTheme.Primary(button);else PageTheme.Secondary(button);
            button.Click+=(s,e)=>action();Actions.Children.Add(button);return button;
        }
    }
}
