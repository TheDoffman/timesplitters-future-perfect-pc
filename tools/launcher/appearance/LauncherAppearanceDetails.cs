using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Media.Animation;
using TimeSplitters.Launcher;

// Launcher placement and presentation; game preferences remain owned by SettingsPage.
public static class LauncherAppearanceDetails
{
    static bool Mark(System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> cache,DependencyObject key){object value;if(cache.TryGetValue(key,out value))return false;cache.Add(key,new object());return true;}
    public sealed class Placement {public double Left,Top,Width,Height;public bool Maximized;}
    static IEnumerable<DependencyObject> Tree(DependencyObject root){yield return root;foreach(var c in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())foreach(var d in Tree(c))yield return d;}
    static Brush Ink(string c){return (Brush)new BrushConverter().ConvertFromString(c);}
    static readonly System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object> attached=new System.Runtime.CompilerServices.ConditionalWeakTable<DependencyObject,object>();
    public static Size LayoutSize(Size client){double scale=Math.Min(1,Math.Min(client.Width/940,client.Height/640));return scale>0?new Size(client.Width/scale,client.Height/scale):new Size(940,640);}
    public static void FitWindow(Window window,Grid body){
        var viewport=new Viewbox {Stretch=Stretch.Uniform,StretchDirection=StretchDirection.DownOnly};window.Content=null;viewport.Child=body;window.Content=viewport;
        Action resize=()=>{var size=LayoutSize(new Size(Math.Max(1,window.ActualWidth),Math.Max(1,window.ActualHeight)));body.Width=size.Width;body.Height=size.Height;};
        Action fit=()=>{var source=PresentationSource.FromVisual(window) as System.Windows.Interop.HwndSource;if(source==null)return;var screen=System.Windows.Forms.Screen.FromHandle(source.Handle);var work=source.CompositionTarget.TransformFromDevice.Transform(new Vector(screen.WorkingArea.Width,screen.WorkingArea.Height));window.MinWidth=Math.Min(640,work.X);window.MinHeight=Math.Min(400,work.Y);window.Width=Math.Min(window.Width,work.X);window.Height=Math.Min(window.Height,work.Y);resize();};
        window.SourceInitialized+=(s,e)=>fit();window.SizeChanged+=(s,e)=>resize();window.DpiChanged+=(s,e)=>fit();resize();
    }
    public static Rect Fit(Rect saved,Rect work){
        double width=Math.Min(work.Width,Math.Max(940,saved.Width)),height=Math.Min(work.Height,Math.Max(640,saved.Height));
        return new Rect(Math.Max(work.Left,Math.Min(saved.Left,work.Right-width)),Math.Max(work.Top,Math.Min(saved.Top,work.Bottom-height)),width,height);
    }
    static bool Valid(Placement p){return p!=null && new[]{p.Left,p.Top,p.Width,p.Height}.All(v=>!Double.IsNaN(v)&&!Double.IsInfinity(v)&&Math.Abs(v)<100000) && p.Width>0 && p.Height>0;}
    public static void Remember(Window window,string path){
        bool maximized=false;
        window.SourceInitialized+=(s,e)=>{
            try{
                if(!File.Exists(path)||new FileInfo(path).Length>8192)return;
                var saved=Json.Serializer().Deserialize<Placement>(File.ReadAllText(path));if(!Valid(saved))return;
                var source=PresentationSource.FromVisual(window);var transform=source.CompositionTarget.TransformFromDevice;
                var screens=System.Windows.Forms.Screen.AllScreens.Select(screen=>{var a=screen.WorkingArea;var origin=transform.Transform(new Point(a.Left,a.Top));var size=transform.Transform(new Vector(a.Width,a.Height));return new Rect(origin,new Size(size.X,size.Y));}).ToArray();
                var rect=new Rect(saved.Left,saved.Top,saved.Width,saved.Height);
                var work=screens.OrderByDescending(area=>{var intersection=Rect.Intersect(rect,area);return intersection.IsEmpty?0:intersection.Width*intersection.Height;}).First();
                if(!screens.Any(area=>area.IntersectsWith(rect)))work=SystemParameters.WorkArea;
                rect=Fit(rect,work);window.WindowStartupLocation=WindowStartupLocation.Manual;window.Width=rect.Width;window.Height=rect.Height;window.Left=rect.Left;window.Top=rect.Top;
                maximized=saved.Maximized;if(maximized)window.WindowState=WindowState.Maximized;
            }catch(IOException){}catch(UnauthorizedAccessException){}catch(ArgumentException){}catch(InvalidOperationException){}
        };
        window.StateChanged+=(s,e)=>{if(window.WindowState!=WindowState.Minimized)maximized=window.WindowState==WindowState.Maximized;};
        window.Closing+=(s,e)=>{
            try{
                var rect=window.WindowState==WindowState.Normal?new Rect(window.Left,window.Top,window.ActualWidth,window.ActualHeight):window.RestoreBounds;
                var value=new Placement {Left=rect.Left,Top=rect.Top,Width=rect.Width,Height=rect.Height,Maximized=maximized};if(!Valid(value))return;
                Directory.CreateDirectory(Path.GetDirectoryName(path));string temporary=path+".tmp";File.WriteAllText(temporary,Json.Serializer().Serialize(value));
                if(File.Exists(path))File.Replace(temporary,path,null);else File.Move(temporary,path);
            }catch(IOException){}catch(UnauthorizedAccessException){}
        };
    }
    public static void Fade(UIElement content,bool allowed){
        content.BeginAnimation(UIElement.OpacityProperty,null);
        if(!allowed || !SystemParameters.ClientAreaAnimation || SystemParameters.HighContrast)return;
        var fade=new DoubleAnimation(.35,1,TimeSpan.FromMilliseconds(160)){FillBehavior=FillBehavior.Stop,EasingFunction=new SineEase {EasingMode=EasingMode.EaseOut}};
        Timeline.SetDesiredFrameRate(fade,30);content.BeginAnimation(UIElement.OpacityProperty,fade,HandoffBehavior.SnapshotAndReplace);
    }
    public static void EmptyStates(DependencyObject root,string page,string installRoot){
        if(page=="Saves"){
            var text=Tree(root).OfType<TextBlock>().FirstOrDefault(t=>t.Text.StartsWith("No backups yet."));if(text!=null)text.Text="No backups yet. Create your first backup here before updating or trying another game version.";
            var choices=Tree(root).OfType<ComboBox>().First(c=>c.Name=="SaveBackupChoices");
            if(Mark(attached,choices)){
                var parent=(Panel)choices.Parent;int at=parent.Children.IndexOf(choices);parent.Children.Remove(choices);
                var holder=new Grid();var hint=new TextBlock {Text="No backups available — create one above to enable restore.",Foreground=Ink("#9FB5C9"),FontSize=11,Margin=new Thickness(11,0,32,0),VerticalAlignment=VerticalAlignment.Center,IsHitTestVisible=false,TextWrapping=TextWrapping.Wrap};
                holder.Children.Add(choices);holder.Children.Add(hint);parent.Children.Insert(at,holder);
                Action update=()=>{bool empty=choices.Items.Count==0;hint.Visibility=empty?Visibility.Visible:Visibility.Collapsed;choices.IsEnabled=!empty;};
                ((INotifyCollectionChanged)choices.Items).CollectionChanged+=(s,e)=>update();update();
            }
        }
        if(page=="Console"){
            var output=Tree(root).OfType<TimeSplitters.Launcher.ConsoleOutput>().First(t=>t.Name=="GameConsoleOutput");
            if(Mark(attached,output)){
                var border=(Border)output.Parent;border.Child=null;
                var holder=new Grid();holder.Children.Add(output);
                var note=new StackPanel {Margin=new Thickness(24),HorizontalAlignment=HorizontalAlignment.Left,VerticalAlignment=VerticalAlignment.Top,IsHitTestVisible=false};
                note.Children.Add(new TextBlock {Text="No game output yet",FontSize=17,Foreground=Ink("#DFEAF4"),Margin=new Thickness(0,0,0,8)});
                note.Children.Add(new TextBlock {Text="Game logs and errors appear here after you launch the game.\nPause keeps the display still; Clear view clears only this display.\nThe original logs remain saved in the installation’s Logs folder.",FontSize=12,Foreground=Ink("#9FB5C9"),TextWrapping=TextWrapping.Wrap});
                note.Visibility=String.IsNullOrWhiteSpace(output.Text)?Visibility.Visible:Visibility.Collapsed;holder.Children.Add(note);border.Child=holder;output.TextChanged+=(s,e)=>note.Visibility=String.IsNullOrWhiteSpace(output.Text)?Visibility.Visible:Visibility.Collapsed;
            }
        }
        if(page=="Diagnostics"){
            var disclosure=Tree(root).OfType<Expander>().First(e=>e.Name=="DiagnosticsSupportOptions");var options=(StackPanel)disclosure.Content;
            var note=options.Children.OfType<TextBlock>().FirstOrDefault(t=>t.Name=="NoGameReports");
            if(note==null){note=new TextBlock {Name="NoGameReports",Text="No game reports yet. Crash and diagnostic reports appear here when a game session creates one. Nothing is uploaded automatically.",FontSize=11,Foreground=Ink("#9FB5C9"),TextWrapping=TextWrapping.Wrap,Margin=new Thickness(0,8,0,4)};options.Children.Insert(0,note);}
            bool available=CrashReports.Latest(installRoot)!=null;note.Visibility=available?Visibility.Collapsed:Visibility.Visible;
            foreach(var button in Tree(options).OfType<Button>().Where(b=>b.Name=="OpenCrashReport"||b.Name=="ExportCrashReport"))button.IsEnabled=available;
        }
    }
}
