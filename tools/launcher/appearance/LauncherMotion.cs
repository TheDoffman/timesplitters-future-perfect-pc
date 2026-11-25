using System;
using System.Linq;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Shapes;

// Decorative WPF clocks only: no timers, image generation or per-frame layout.
public sealed class LauncherMotion
{
    sealed class Loop {public Animatable Target;public DependencyProperty Property;public AnimationClock Clock;}
    readonly Window window;
    readonly Grid stage,aperture;
    readonly Image backdrop,art,logo;
    readonly GameChannelMenu channels;
    readonly List<Loop> loops=new List<Loop>();
    readonly List<RotateTransform> rotations=new List<RotateTransform>();
    readonly TranslateTransform backDrift=new TranslateTransform(),artDrift=new TranslateTransform();
    readonly List<Tuple<UIElement,double>> fades=new List<Tuple<UIElement,double>>();
    bool enabled=true,started,closed;
    public bool Enabled {get{return enabled;}set{if(enabled==value)return;enabled=value;Refresh();}}
    bool paused;
    public bool Paused {get{return paused;}set{if(paused==value)return;paused=value;Refresh();}}
    public LauncherMotion(Window owner,Grid home,Grid rings,Image background,Image keyart,Image title,GameChannelMenu selector){
        window=owner;stage=home;aperture=rings;backdrop=background;art=keyart;logo=title;channels=selector;
        var backgroundTransform=new TransformGroup();backgroundTransform.Children.Add(new ScaleTransform(1.035,1.035));backgroundTransform.Children.Add(backDrift);backdrop.RenderTransformOrigin=new Point(.5,.5);backdrop.RenderTransform=backgroundTransform;
        art.RenderTransform=artDrift;
        var ringShapes=aperture.Children.OfType<Ellipse>().ToArray();
        for(int i=0;i<2 && i<ringShapes.Length;i++){var ring=ringShapes[i];ring.RenderTransformOrigin=new Point(.5,.5);var spin=new RotateTransform();ring.RenderTransform=spin;rotations.Add(spin);if(i==1)ring.StrokeDashArray=new DoubleCollection {24,7,1,9};ring.CacheMode=new BitmapCache();}
        fades.Add(Tuple.Create((UIElement)backdrop,backdrop.Opacity));fades.Add(Tuple.Create((UIElement)art,art.Opacity));fades.Add(Tuple.Create((UIElement)aperture,aperture.Opacity));fades.Add(Tuple.Create((UIElement)logo,logo.Opacity));
        window.Loaded+=(s,e)=>{Refresh();if(Allowed)foreach(var item in fades){var fade=new DoubleAnimation(0,item.Item2,TimeSpan.FromMilliseconds(850)) {FillBehavior=FillBehavior.Stop,EasingFunction=new SineEase {EasingMode=EasingMode.EaseOut}};Timeline.SetDesiredFrameRate(fade,30);item.Item1.BeginAnimation(UIElement.OpacityProperty,fade);}};
        window.StateChanged+=(s,e)=>Refresh();window.IsVisibleChanged+=(s,e)=>Refresh();stage.IsEnabledChanged+=(s,e)=>Refresh();channels.OpenChanged+=Refresh;
        SystemParameters.StaticPropertyChanged+=PreferencesChanged;window.Closed+=(s,e)=>{closed=true;SystemParameters.StaticPropertyChanged-=PreferencesChanged;channels.OpenChanged-=Refresh;Stop();};
    }
    bool Allowed {get{return enabled && SystemParameters.ClientAreaAnimation && !SystemParameters.HighContrast;}}
    bool Quiet {get{return paused || !window.IsVisible || window.WindowState==WindowState.Minimized || !stage.IsEnabled || channels.IsOpen;}}
    void PreferencesChanged(object sender,PropertyChangedEventArgs e){if(e.PropertyName=="ClientAreaAnimation" || e.PropertyName=="HighContrast")Refresh();}
    void Animate(Animatable target,DependencyProperty property,double from,double to,double seconds,bool reverse){
        var animation=new DoubleAnimation(from,to,TimeSpan.FromSeconds(seconds)) {AutoReverse=reverse,RepeatBehavior=RepeatBehavior.Forever};
        if(reverse)animation.EasingFunction=new SineEase {EasingMode=EasingMode.EaseInOut};Timeline.SetDesiredFrameRate(animation,30);
        var clock=(AnimationClock)animation.CreateClock(true);target.ApplyAnimationClock(property,clock);loops.Add(new Loop {Target=target,Property=property,Clock=clock});
    }
    void Start(){if(started || closed)return;started=true;
        for(int i=0;i<rotations.Count;i++)Animate(rotations[i],RotateTransform.AngleProperty,0,i==0?360:-360,i==0?90:120,false);
        Animate(backDrift,TranslateTransform.XProperty,-6,6,32,true);Animate(backDrift,TranslateTransform.YProperty,-3,3,41,true);
        Animate(artDrift,TranslateTransform.XProperty,-3,3,37,true);Animate(artDrift,TranslateTransform.YProperty,1,-2,43,true);
    }
    void Stop(){foreach(var loop in loops){loop.Target.ApplyAnimationClock(loop.Property,null);loop.Clock.Controller.Remove();}loops.Clear();started=false;
        foreach(var item in fades)item.Item1.BeginAnimation(UIElement.OpacityProperty,null);
    }
    public void Refresh(){if(closed)return;if(!Allowed){Stop();return;}if(!started && !Quiet)Start();
        foreach(var loop in loops){if(Quiet)loop.Clock.Controller.Pause();else loop.Clock.Controller.Resume();}
        // Opening settings or minimizing also stops the one-time entrance fade.
        if(Quiet)foreach(var item in fades)item.Item1.BeginAnimation(UIElement.OpacityProperty,null);
    }
    // Deterministic verification of the actual controllable animation clocks.
    public string Verify(Action<string> render){
        if(!Allowed)return "Motion disabled by Windows animation/high-contrast preference; static layout retained.";
        // A connected, off-screen presentation source is required for WPF's
        // timing manager to process Pause/Resume during this render-only check.
        window.WindowStartupLocation=WindowStartupLocation.Manual;window.Left=-20000;window.Top=-20000;window.ShowActivated=false;window.Show();
        foreach(var item in fades)item.Item1.BeginAnimation(UIElement.OpacityProperty,null);
        Start();foreach(var loop in loops){loop.Clock.Controller.Begin();loop.Clock.Controller.SeekAlignedToLastTick(TimeSpan.Zero,TimeSeekOrigin.BeginTime);}render("motion-start");
        double before=rotations[0].Angle;foreach(var loop in loops)loop.Clock.Controller.SeekAlignedToLastTick(TimeSpan.FromSeconds(18),TimeSeekOrigin.BeginTime);render("motion-later");
        if(Math.Abs(rotations[0].Angle-before)<1)throw new Exception("Decorative animation clock did not advance.");
        foreach(var loop in loops)loop.Clock.Controller.Pause();
        // WPF applies Pause at its next timing tick, rather than synchronously.
        var frame=new System.Windows.Threading.DispatcherFrame();var tick=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromMilliseconds(60)};
        tick.Tick+=(s,e)=>{tick.Stop();frame.Continue=false;};tick.Start();System.Windows.Threading.Dispatcher.PushFrame(frame);
        if(loops.Any(l=>!l.Clock.IsPaused))throw new Exception("Motion pause failed.");
        Enabled=false;if(loops.Count!=0 || Math.Abs(rotations[0].Angle)>.001)throw new Exception("Disabling motion did not restore static layout.");render("motion-off");
        return "Motion clocks advanced, paused and removed correctly. Three animation-phase snapshots rendered.";
    }
}
