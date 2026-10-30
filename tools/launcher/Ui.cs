using System;
using System.IO;
#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Documents;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Markup.Xaml;
using Avalonia.Styling;
using Control = Avalonia.Controls.Control;
using Brush = Avalonia.Media.IBrush;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using Control = System.Windows.FrameworkElement;
#endif

namespace TimeSplitters.Launcher
{
    // Framework adapters only. Page geometry, controls, labels and interactions
    // live in the shared *Page.cs files compiled by both launcher builds.
    public static class Ui
    {
#if MACOS
        public static FontFamily Monospace { get { return new FontFamily("Menlo"); } }
        public static Brush Brush(string value) { return Avalonia.Media.Brush.Parse(value); }
        public static void Freeze(SolidColorBrush brush) { }
        public static void Theme(Control view) { view.Styles.Add((Styles)AvaloniaXamlLoader.Load(new Uri("avares://TimeSplitters.Launcher/PageTheme.axaml"))); }
        public static void Tabs(Control view) { view.Styles.Add((Styles)AvaloniaXamlLoader.Load(new Uri("avares://TimeSplitters.Launcher/TabsTheme.axaml"))); }
        public static void Resources(Control view,ResourceDictionary resources) { view.Resources.MergedDictionaries.Add(resources); }
        public static void Show(Control view,bool visible) { view.IsVisible=visible; }
        public static void Tip(Control view,object text) { ToolTip.SetTip(view,text); }
        public static void TextChanged(TextBox view,Action action)
        {
            // Avalonia routes TextChanged through the dispatcher; draft state must
            // follow the value immediately, like the WPF adapter.
            view.PropertyChanged+=(s,e)=>{if(e.Property==TextBox.TextProperty)action();};
        }
        public static void ValueChanged(Slider view,Action action) { view.PropertyChanged+=(s,e)=> { if(e.Property==Slider.ValueProperty)action(); }; }
        public static bool From(RoutedEventArgs e,Control view) { return e.Source==view; }
        public static void ScrollHome(ScrollViewer view) { view.Offset=new Vector(0,0); }
        public static void ScrollBars(TextBox view) { ScrollViewer.SetVerticalScrollBarVisibility(view,ScrollBarVisibility.Auto);ScrollViewer.SetHorizontalScrollBarVisibility(view,ScrollBarVisibility.Disabled); }
        public static void CheckedChanged(CheckBox view,Action action) { view.IsCheckedChanged+=(s,e)=>action(); }
        public static string InheritedSettings(bool preview) { return null; }
        public static void FocusWhenReady(Control view) { view.Loaded+=(s,e)=>view.Focus();view.Focus(); }
        public static void Capture(Control area,Action<int> accept,Action cancel)
        {
            area.KeyDown+=(s,e)=> { if(e.Key==Key.Escape){e.Handled=true;cancel();return;}int key=VirtualKey(e.Key);if(key>=0){e.Handled=true;accept(GameSettings.CaptureKey(key));} };
            area.PointerPressed+=(s,e)=> { var p=e.GetCurrentPoint(area).Properties;int key=p.IsRightButtonPressed?2:p.IsMiddleButtonPressed?4:p.IsXButton1Pressed?5:p.IsXButton2Pressed?6:p.IsLeftButtonPressed?1:-1;if(key>=0){e.Handled=true;accept(key);} };
            area.PointerWheelChanged+=(s,e)=> { e.Handled=true;accept(e.Delta.Y>0?GameSettings.WheelUpKey:GameSettings.WheelDownKey); };
        }
        static int VirtualKey(Key key)
        {
            if(key>=Key.A&&key<=Key.Z)return 'A'+(key-Key.A);if(key>=Key.D0&&key<=Key.D9)return '0'+(key-Key.D0);if(key>=Key.NumPad0&&key<=Key.NumPad9)return 96+(key-Key.NumPad0);if(key>=Key.F1&&key<=Key.F24)return 112+(key-Key.F1);
            switch(key){case Key.Multiply:return 106;case Key.Add:return 107;case Key.Subtract:return 109;case Key.Decimal:return 110;case Key.Divide:return 111;case Key.NumLock:return 144;case Key.Scroll:return 145;case Key.Pause:return 19;case Key.Up:return 38;case Key.Down:return 40;case Key.Left:return 37;case Key.Right:return 39;case Key.Enter:return 13;case Key.Back:return 8;case Key.Space:return 32;case Key.Tab:return 9;case Key.Escape:return 27;case Key.LeftShift:return 160;case Key.RightShift:return 161;case Key.LeftCtrl:return 162;case Key.RightCtrl:return 163;case Key.LeftAlt:return 164;case Key.RightAlt:return 165;case Key.Insert:return 45;case Key.Delete:return 46;case Key.CapsLock:return 20;case Key.OemMinus:return 189;case Key.OemPlus:return 187;case Key.OemComma:return 188;case Key.OemPeriod:return 190;case Key.OemQuestion:return 191;case Key.OemSemicolon:return 186;case Key.OemQuotes:return 222;case Key.OemOpenBrackets:return 219;case Key.OemCloseBrackets:return 221;case Key.OemPipe:return 220;case Key.OemTilde:return 192;case Key.Home:return 36;case Key.End:return 35;case Key.PageUp:return 33;case Key.PageDown:return 34;default:return -1;}
        }
        public static void Link(TextBlock view,string label,string url,Action<string> open)
        {
            var link=new Button { Content=label,Padding=new Thickness(0),Margin=new Thickness(0),MinHeight=0,Background=Avalonia.Media.Brushes.Transparent,BorderThickness=new Thickness(0),Foreground=Brush("#A7D9FA") };
            Tip(link,url);link.Click+=(s,e)=> { if(open!=null)open(url); };view.Inlines.Add(new InlineUIContainer(link));
        }
        public static void ForwardWheel(ReleaseNotesMarkdown notes,ScrollViewer parent)
        { notes.PointerWheelChanged+=(s,e)=> { parent.Offset=new Vector(parent.Offset.X,parent.Offset.Y-e.Delta.Y*60);e.Handled=true; }; }
#else
        public static FontFamily Monospace { get { return new FontFamily("Consolas"); } }
        public static Brush Brush(string value) { return (Brush)new BrushConverter().ConvertFromString(value); }
        public static void Freeze(SolidColorBrush brush) { brush.Freeze(); }
        public static ResourceDictionary LoadTheme(string name) { using(var stream=typeof(Ui).Assembly.GetManifestResourceStream(name))return (ResourceDictionary)System.Windows.Markup.XamlReader.Load(stream); }
        public static void Theme(Control view) { view.Resources.MergedDictionaries.Add(PageTheme.Resources()); }
        public static void Tabs(Control view) { view.Resources.MergedDictionaries.Add(PageTheme.Tabs()); }
        public static void Resources(Control view,ResourceDictionary resources) { view.Resources.MergedDictionaries.Add(resources); }
        public static void Show(Control view,bool visible) { view.Visibility=visible?Visibility.Visible:Visibility.Collapsed; }
        public static void Tip(Control view,object text) { view.ToolTip=text; }
        public static void TextChanged(TextBox view,Action action) { view.TextChanged+=(s,e)=>action(); }
        public static void ValueChanged(Slider view,Action action) { view.ValueChanged+=(s,e)=>action(); }
        public static bool From(RoutedEventArgs e,Control view) { return e.OriginalSource==view; }
        public static void ScrollHome(ScrollViewer view) { view.ScrollToHome(); }
        public static void ScrollBars(TextBox view) { view.VerticalScrollBarVisibility=ScrollBarVisibility.Auto;view.HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled; }
        public static void CheckedChanged(CheckBox view,Action action) { view.Checked+=(s,e)=>action();view.Unchecked+=(s,e)=>action(); }
        public static string InheritedSettings(bool preview) { return preview?null:Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"xboxrecomp","titles","45410066.conf"); }
        public static void FocusWhenReady(Control view) { view.Loaded+=(s,e)=>view.Focus();view.Focus(); }
        public static void Capture(Control area,Action<int> accept,Action cancel)
        {
            area.PreviewKeyDown+=(s,e)=> { if(e.Key==Key.Escape){e.Handled=true;cancel();return;}int key=GameSettings.CaptureKey(KeyInterop.VirtualKeyFromKey(e.Key==Key.System?e.SystemKey:e.Key));if(key>0){e.Handled=true;accept(key);} };
            area.PreviewMouseDown+=(s,e)=> { e.Handled=true;accept(e.ChangedButton==MouseButton.Left?1:e.ChangedButton==MouseButton.Right?2:e.ChangedButton==MouseButton.Middle?4:e.ChangedButton==MouseButton.XButton1?5:6); };
            area.PreviewMouseWheel+=(s,e)=> { e.Handled=true;accept(e.Delta>0?GameSettings.WheelUpKey:GameSettings.WheelDownKey); };
        }
        public static void Link(TextBlock view,string label,string url,Action<string> open)
        { var link=new Hyperlink(new Run(label)){NavigateUri=new Uri(url),ToolTip=url,Foreground=Brush("#A7D9FA")};link.RequestNavigate+=(s,e)=>{e.Handled=true;if(open!=null)open(url);};view.Inlines.Add(link); }
        public static void ForwardWheel(ReleaseNotesMarkdown notes,ScrollViewer parent)
        { notes.PreviewMouseWheel+=(s,e)=>{double step=SystemParameters.WheelScrollLines<0?parent.ViewportHeight:SystemParameters.WheelScrollLines*20;parent.ScrollToVerticalOffset(parent.VerticalOffset-e.Delta/120.0*step);e.Handled=true;}; }
#endif
    }
}
