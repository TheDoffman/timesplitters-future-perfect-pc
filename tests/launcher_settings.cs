using System;
using System.Collections;
using System.IO;
using System.Diagnostics;
using System.Linq;
using TimeSplitters.Launcher;

public static class LauncherSettingsChecks
{
    static int count;
    [System.Runtime.InteropServices.DllImport("dwmapi.dll")]
    static extern int DwmGetWindowAttribute(IntPtr window,int attribute,out int value,int size);
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wp,IntPtr lp);
    static void CheckTitleTheme()
    {
        bool dark=false;
        var window=new System.Windows.Window { Width=320,Height=160,Title="Title bar theme regression",ShowInTaskbar=false };
        WindowTheme.Attach(window,()=>dark);
        try
        {
            var hwnd=new System.Windows.Interop.WindowInteropHelper(window).EnsureHandle();
            int actual;
            if(DwmGetWindowAttribute(hwnd,20,out actual,sizeof(int))<0)
            { Console.WriteLine("SKIP: optional DWM theme attribute unavailable");return; }
            Require(actual==0,"Initial title bar must use light mode");
            dark=true;SendMessage(hwnd,0x001A,IntPtr.Zero,IntPtr.Zero);
            Require(DwmGetWindowAttribute(hwnd,20,out actual,sizeof(int))>=0 && actual==1,"Settings change must select dark mode");
            dark=false;SendMessage(hwnd,0x031A,IntPtr.Zero,IntPtr.Zero);
            Require(DwmGetWindowAttribute(hwnd,20,out actual,sizeof(int))>=0 && actual==0,"Theme change must restore light mode");
        }
        finally { window.Close(); }
    }
    static string temp;
    static void Require(bool condition, string message = "Settings assertion failed.") { if (!condition) throw new Exception(message); }
    static void Reject(Action action) { bool rejected=false; try { action(); } catch { rejected=true; } Require(rejected); }
    static string Root(string name) { string root=Path.Combine(temp,name);Directory.CreateDirectory(root);return root; }
    static void Put(string root,string file,string data) { string path=Path.Combine(root,file);Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,data); }
    static string Read(string root,string file) { return File.ReadAllText(Path.Combine(root,file)); }
    static void Test(string name,Action action)
    {
        Console.WriteLine("START "+name);
        var elapsed=Stopwatch.StartNew();
        try { action(); }
        catch(Exception failure) { throw new Exception("Launcher settings check failed: "+name,failure); }
        count++;Console.WriteLine("PASS "+name+" ("+elapsed.ElapsedMilliseconds+" ms)");
    }
    static void STA(Action action)
    {
        Exception failure=null;
        var thread=new System.Threading.Thread(()=> { try {action();} catch(Exception ex){failure=ex;} });
        thread.IsBackground=true;thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();
        // The first WPF control initializes fonts, themes and the graphics
        // stack. Cold hosted Windows runners can take more than ten seconds.
        // Keep the wait bounded, but distinguish startup timeouts from failed
        // assertions and preserve the original UI-thread stack for diagnosis.
        if(!thread.Join(60000)) throw new TimeoutException("Launcher settings UI thread did not finish within 60 seconds.");
        if(failure!=null) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure).Throw();
    }
    static void Click(System.Windows.Controls.Button button)
    { button.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent)); }
    static System.Collections.Generic.IEnumerable<System.Windows.DependencyObject> Descendants(System.Windows.DependencyObject parent)
    {
        foreach(var child in System.Windows.LogicalTreeHelper.GetChildren(parent).OfType<System.Windows.DependencyObject>())
        { yield return child; foreach(var descendant in Descendants(child)) yield return descendant; }
    }
    public static int Child()
    {
        string controls=Environment.GetEnvironmentVariable("RECOMP_INPUT_CONFIG");
        string display=Environment.GetEnvironmentVariable("TSFP_DISPLAY_CONFIG");
        string runtime=Environment.GetEnvironmentVariable("RECOMP_DISPLAY_CONFIG");
        var mouse=(System.Collections.Generic.Dictionary<string,object>)Json.Object(File.ReadAllText(controls))["mouse"];
        Console.WriteLine(Convert.ToString(mouse["sensitivity"],System.Globalization.CultureInfo.InvariantCulture));
        Console.WriteLine(File.ReadAllText(display).Trim());
        Console.WriteLine(GameSettings.ParseRuntime(File.ReadAllText(runtime))["anisotropy"]);
        Console.WriteLine("XMV_PLAY="+Environment.GetEnvironmentVariable("RECOMP_XMV_PLAY"));
        return 0;
    }
    // Invoked only by the synthetic copy of this test binary in a disposable Bin.
    public static int LaunchProbe()
    {
        Console.WriteLine("DIRECT_GAME_STARTED");
        Console.WriteLine("CWD=" + Environment.CurrentDirectory);
        foreach (string key in new[] { "RECOMP_GAME_DIR", "RECOMP_SAVE_DIR", "RECOMP_INPUT_CONFIG",
            "RECOMP_FFMPEG_DIR", "RECOMP_DISPLAY_CONFIG", "TSFP_DISPLAY_CONFIG", "TSFP_ONLINE_SETTINGS",
            "RECOMP_LOG_FILE", "RECOMP_MOUSE_SENSITIVITY", "RECOMP_MOUSE_INVERT_Y", "RECOMP_PC_MOUSE",
            "RECOMP_XMV_PLAY", "RECOMP_UNWANTED", "TSFP_UNWANTED" })
            Console.WriteLine(key + "=" + Environment.GetEnvironmentVariable(key));
        Console.Error.WriteLine("synthetic game diagnostic");
        return Int32.Parse(File.ReadAllText(Path.Combine(Environment.CurrentDirectory, "probe-exit.txt")));
    }
    static void PlayProbe(string root, int exitCode)
    {
        Directory.CreateDirectory(Path.Combine(root,"Bin"));
        File.Copy(typeof(LauncherSettingsChecks).Assembly.Location,Path.Combine(root,"Bin/futureperfect_recomp.exe"));
        Put(root,"Bin/probe-exit.txt",exitCode.ToString());
        // Reproduce the reported restricted script operation if Play regresses
        // to using PowerShell. No machine policy or personal settings are changed.
        Put(root,"Launch.ps1","$ErrorActionPreference = 'Stop'\n" +
            "$ExecutionContext.SessionState.LanguageMode = 'ConstrainedLanguage'\n" +
            "$info = New-Object System.Diagnostics.ProcessStartInfo\nexit 23\n");
        var settings = Directory.GetFiles(Path.Combine(root,"Settings"))
            .ToDictionary(p=>p,p=>Convert.ToBase64String(File.ReadAllBytes(p)));
        string oldRecomp=Environment.GetEnvironmentVariable("RECOMP_UNWANTED"),oldTsfp=Environment.GetEnvironmentVariable("TSFP_UNWANTED");
        Environment.SetEnvironmentVariable("RECOMP_UNWANTED","must not inherit");
        Environment.SetEnvironmentVariable("TSFP_UNWANTED","must not inherit");
        try {
            STA(()=>{
                var ui=new LauncherWindow(root,true);
                var dispatcher=System.Windows.Threading.Dispatcher.CurrentDispatcher;
                var previous=System.Threading.SynchronizationContext.Current;
                var timer=new System.Windows.Threading.DispatcherTimer { Interval=TimeSpan.FromSeconds(15) };
                var frame=new System.Windows.Threading.DispatcherFrame();bool timedOut=false;
                try {
                    System.Threading.SynchronizationContext.SetSynchronizationContext(new System.Windows.Threading.DispatcherSynchronizationContext(dispatcher));
                    var task=ui.LaunchGame();
                    timer.Tick+=(sender,args)=>{timedOut=true;frame.Continue=false;};timer.Start();
                    task.ContinueWith(done=>dispatcher.BeginInvoke(new Action(()=>frame.Continue=false)));
                    System.Windows.Threading.Dispatcher.PushFrame(frame);
                    Require(!timedOut,"Play did not finish within 15 seconds.");task.GetAwaiter().GetResult();
                    string expected=exitCode==0?"Game closed.":"The game closed unexpectedly. No crash dump was captured; session report saved - see Diagnostics.";
                    string actual=((System.Windows.Controls.TextBlock)ui.Window.FindName("StatusText")).Text;
                    Require(actual==expected,"Unexpected game exit status: "+actual+"\n"+(File.Exists(Path.Combine(root,".launcher/launcher.log"))?File.ReadAllText(Path.Combine(root,".launcher/launcher.log")):"No launcher log."));
                } finally {timer.Stop();System.Threading.SynchronizationContext.SetSynchronizationContext(previous);ui.Window.Close();}
            });
        } finally {
            Environment.SetEnvironmentVariable("RECOMP_UNWANTED",oldRecomp);
            Environment.SetEnvironmentVariable("TSFP_UNWANTED",oldTsfp);
        }
        string output=File.ReadAllText(Directory.GetFiles(Path.Combine(root,"Logs"),"*.out").Single());
        string errors=Directory.GetFiles(Path.Combine(root,"Logs"),"*.err").Single();
        Require(output.Contains("DIRECT_GAME_STARTED") && output.Contains("CWD="+Path.Combine(root,"Bin")));
        foreach(var pair in new[]{new[]{"RECOMP_GAME_DIR","Data"},new[]{"RECOMP_SAVE_DIR","Saves"},
            new[]{"RECOMP_INPUT_CONFIG","Settings/pc-controls.json"},new[]{"RECOMP_FFMPEG_DIR","Bin"},
            new[]{"RECOMP_DISPLAY_CONFIG","Settings/runtime.conf"},new[]{"TSFP_DISPLAY_CONFIG","Settings/pc-display.ini"},
            new[]{"TSFP_ONLINE_SETTINGS","Settings/pc-online.json"}})
            Require(output.Contains(pair[0]+"="+Core.PathIn(root,pair[1])));
        Require(output.Contains("RECOMP_LOG_FILE="+errors) && File.ReadAllText(errors).Contains("synthetic game diagnostic"));
        if(exitCode!=0)Require(File.ReadAllText(Path.Combine(CrashReports.Folder(root,CrashReports.Latest(root)),"game-log-tail.txt")).Contains("synthetic game diagnostic"),"Unexpected-exit report must include a short buffered stderr diagnostic.");
        Require(output.Contains("RECOMP_MOUSE_SENSITIVITY=0.37") && output.Contains("RECOMP_MOUSE_INVERT_Y=1") &&
            output.Contains("RECOMP_PC_MOUSE=0") && output.Contains("RECOMP_XMV_PLAY=0"));
        Require(output.Contains("RECOMP_UNWANTED=\r\n") && output.Contains("TSFP_UNWANTED=\r\n"));
        Require(settings.Count==Directory.GetFiles(Path.Combine(root,"Settings")).Length);
        foreach(var pair in settings)Require(Convert.ToBase64String(File.ReadAllBytes(pair.Key))==pair.Value);
    }
    public static int Run()
    {
        temp=Path.Combine(Path.GetDirectoryName(typeof(LauncherSettingsChecks).Assembly.Location),"settings-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try
        {
            Test("title bar follows initial theme and live setting changes",()=>STA(CheckTitleTheme));
            Test("published alpha.4 retains V1 resolution settings and rejects custom FOV atomically",()=>{
                string root=Root("legacy-fov");Put(root,"BUILD.json","{\"version\":\"v0.2.0-alpha.4\"}");
                var model=GameSettings.Load(root,null);Require(!model.CanAdjustFov);
                model.Width=1920;model.Height=1080;model.Save();
                string original=Read(root,"Settings/pc-display.ini");
                Require(original=="TSFP_DISPLAY_V1 1920 1080\n");
                model.VerticalFov=75;Reject(()=>model.Save());Require(Read(root,"Settings/pc-display.ini")==original);
                Put(root,"BUILD.json","{\"version\":\"v0.2.0-alpha.5\"}");
                Require(model.CanAdjustFov);model.Save();Require(Read(root,"Settings/pc-display.ini")=="TSFP_DISPLAY_V2 1920 1080 75\n");
            });
            Test("published resolution bounds reject unsupported writes and preserve newer modes",()=>{
                string root=Root("legacy-resolution");Put(root,"BUILD.json","{\"version\":\"v0.2.0-alpha.4\"}");
                var model=GameSettings.Load(root,null);model.Width=7680;model.Height=2160;model.Save();
                string original=Read(root,"Settings/pc-display.ini");
                Require(model.SupportsResolution(0,0) && model.SupportsResolution(3840,2160));
                model.Width=7680;model.Height=4320;Reject(()=>model.Save());
                Require(Read(root,"Settings/pc-display.ini")==original);
                Put(root,"BUILD.json","{\"version\":\"v0.2.0-alpha.5\"}");model.Save();
                Require(GameSettings.Load(root,null).Height==4320);
                Put(root,"BUILD.json","{\"version\":\"invalid\"}");Require(!model.SupportsResolution(7680,4320));
            });
            Test("alpha.4 FOV controls explain unavailable capability while resolution saves",()=>STA(()=>{
                string root=Root("legacy-fov-ui");Put(root,"BUILD.json","{\"version\":\"v0.2.0-alpha.4\"}");
                var owner=new System.Windows.Window();var editor=new SettingsPage(owner.Resources,root,true);editor.SelectTab(1);
                var nodes=Descendants(editor.View).ToList();
                var picker=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.Items.OfType<DisplayMode>().Any());
                Require(picker.Items.Cast<DisplayMode>().All(m=>m.Height<=2160),"Legacy presets exceed the installed game's resolution limit.");
                Require(picker.Items.Cast<DisplayMode>().Any(m=>m.Width==3840 && m.Height==2160),"Legacy 4K must remain available.");
                Require(!nodes.OfType<System.Windows.Controls.Slider>().Single(c=>c.Name=="VerticalFovSlider").IsEnabled,"Legacy FOV slider must be disabled.");
                Require(!nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="ResetFovButton").IsEnabled,"Legacy FOV reset must be disabled.");
                Require(nodes.OfType<System.Windows.Controls.TextBlock>().Any(c=>c.Text.Contains("newer than v0.2.0-alpha.4")),"Missing capability explanation.");
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsSaveButton"));
                Require(Read(root,"Settings/pc-display.ini")=="TSFP_DISPLAY_V1 0 0\n","Legacy UI resolution output: "+Read(root,"Settings/pc-display.ini"));owner.Close();
            }));
            Test("FOV legacy defaults, range checks and shared V2 round trip",()=>{
                string root=Root("fov-settings");Put(root,"Settings/pc-display.ini","TSFP_DISPLAY_V1 1920 1080\n");
                var model=GameSettings.Load(root,null);Require(model.VerticalFov==55);
                model.VerticalFov=75;model.Save();
                Require(Read(root,"Settings/pc-display.ini")=="TSFP_DISPLAY_V2 1920 1080 75\n");
                var loaded=GameSettings.Load(root,null);Require(loaded.Width==1920 && loaded.Height==1080 && loaded.VerticalFov==75);
                loaded.VerticalFov=101;Reject(()=>loaded.Save());loaded.VerticalFov=39;Reject(()=>loaded.Save());
                Require(GameSettings.Load(root,null).VerticalFov==75);
                loaded.VerticalFov=55;loaded.Save();Require(Read(root,"Settings/pc-display.ini")=="TSFP_DISPLAY_V1 1920 1080\n");
                foreach(string invalid in new[]{"TSFP_DISPLAY_V2 1920 1080", "TSFP_DISPLAY_V1 1920 1080 75", "TSFP_DISPLAY_V2 1920 1080 999", "TSFP_DISPLAY_V2 1920 1080 NaN"}) {
                    Put(root,"Settings/pc-display.ini",invalid);Reject(()=>GameSettings.Load(root,null));
                }
            });
            Test("FOV slider number, reset, cancel and save",()=>STA(()=>{
                string root=Root("fov-slider");var owner=new System.Windows.Window();
                var editor=new SettingsPage(owner.Resources,root,true);editor.SelectTab(1);
                var nodes=Descendants(editor.View).ToList();
                var slider=nodes.OfType<System.Windows.Controls.Slider>().Single(c=>c.Name=="VerticalFovSlider");
                var value=nodes.OfType<System.Windows.Controls.TextBlock>().Single(c=>c.Name=="VerticalFovValue");
                Require(slider.Value==55 && slider.Minimum==40 && slider.Maximum==100 && slider.IsSnapToTickEnabled);
                slider.Value=80;Require(value.Text=="80°");
                bool cancelled=false;editor.Cancelled+=()=>cancelled=true;
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsCancelButton"));
                Require(cancelled && !File.Exists(Path.Combine(root,"Settings/pc-display.ini")));
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="ResetFovButton"));Require(slider.Value==55);
                slider.Value=75;Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsSaveButton"));
                Require(GameSettings.Load(root,null).VerticalFov==75);owner.Close();
            }));
            Test("primary mode list deduplicates, sorts, includes default and preserves saved custom",()=>{
                var modes=DisplayModes.Choices(new[]{new DisplayMode(1920,1080),new DisplayMode(640,480),new DisplayMode(1920,1080),new DisplayMode(3840,2160),new DisplayMode(1080,1920),new DisplayMode(0,720),new DisplayMode(7680,4320)},3000,1400);
                Require(modes.Count>40 && modes[0].Width==0 && modes[0].Height==0);
                Require(modes[1].Width==640 && modes.Any(m=>m.Width==7680 && m.Height==4320));
                Require(modes.GroupBy(m=>m.Width+"x"+m.Height).All(g=>g.Count()==1));
                Require(modes.Last().Width==3000 && modes.Last().SavedCustom);
                Require(!modes.Any(m=>m.Width==1080 && m.Height==1920));
                var empty=DisplayModes.Choices(new DisplayMode[0],0,0);Require(empty.Count>40);
                Require(DisplayModes.Choices(new DisplayMode[0],0,1080).Count==empty.Count);
                var actual=DisplayModes.Primary(0,0);Require(actual.Count>=1 && actual[0].Width==0);
                Console.WriteLine("Primary display modes: "+String.Join(", ",actual));
            });
            Test("aspect groups and launcher filtering preserve proportional resolutions",()=>STA(()=>{
                Require(DisplayModes.Aspects.Length==7);
                foreach(var size in DisplayModes.Presets())Require(DisplayMode.Valid(size.Width,size.Height));
                Require(DisplayModes.AspectIndex(1366,768)==3 && DisplayModes.AspectIndex(3440,1440)==5);
                Require(DisplayModes.AspectIndex(5120,1440)==6 && DisplayModes.AspectIndex(1280,1024)==2);
                string root=Root("aspect-filter");var owner=new System.Windows.Window();var editor=new SettingsPage(owner.Resources,root,true);editor.SelectTab(1);
                var nodes=Descendants(editor.View).ToList();
                var aspect=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.Name=="AspectRatio");
                var picker=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.SelectedItem is DisplayMode);
                var label=nodes.OfType<System.Windows.Controls.TextBlock>().Single(c=>c.Name=="CurrentAspectRatio");
                for(int i=1;i<DisplayModes.Aspects.Length;i++) {
                    aspect.SelectedIndex=i;Require(picker.Items.Count>0);
                    Require(picker.Items.Cast<DisplayMode>().All(m=>DisplayModes.AspectIndex(m.Width,m.Height)==i));
                    Require(label.Text=="Current ratio: "+DisplayModes.Aspects[i].Label);
                }
                aspect.SelectedIndex=3;
                picker.SelectedItem=picker.Items.Cast<DisplayMode>().Single(m=>m.Width==1920 && m.Height==1080);
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsSaveButton"));
                var saved=GameSettings.Load(root,null);Require(saved.Width==1920 && saved.Height==1080);
                aspect.SelectedIndex=0;Require(picker.Items.Count>40);owner.Close();
            }));
            Test("lower supported modes and original resolution persist without changing other settings",()=>{
                string root=Root("lower-resolution");var model=GameSettings.Load(root,null);model.Width=800;model.Height=600;model.Save();
                var loaded=GameSettings.Load(root,null);Require(loaded.Width==800 && loaded.Height==600);
                loaded.Width=0;loaded.Height=0;loaded.Save();Require(Read(root,"Settings/pc-display.ini")=="TSFP_DISPLAY_V1 0 0\n");
            });
            Test("launcher resolution arrows select both directions, wrap and leave files untouched",()=>{
                Exception failure=null;string root=Root("arrow-controls");
                var thread=new System.Threading.Thread(()=>{try {
                    var owner=new System.Windows.Window();var editor=new SettingsPage(owner.Resources,root,true);owner.Content=editor.View;
                    var tabs=Descendants(editor.View).OfType<System.Windows.Controls.TabControl>().Single();tabs.SelectedIndex=1;
                    var nodes=Descendants(editor.View).ToList();
                    var startup=nodes.OfType<System.Windows.Controls.CheckBox>().Single(c=>Convert.ToString(c.Content)=="Skip startup splash screens");
                    Require(startup.IsChecked==false);startup.IsChecked=true;
                    var picker=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.SelectedItem is DisplayMode);
                    var buttons=nodes.OfType<System.Windows.Controls.Button>().ToList();
                    var previous=buttons.Single(b=>System.Windows.Automation.AutomationProperties.GetName(b)=="Previous resolution");
                    var next=buttons.Single(b=>System.Windows.Automation.AutomationProperties.GetName(b)=="Next resolution");
                    Require(picker.SelectedIndex==0);
                    previous.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(picker.SelectedIndex==picker.Items.Count-1);
                    next.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(picker.SelectedIndex==0);
                    next.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(picker.SelectedIndex==1%picker.Items.Count);
                    previous.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(picker.SelectedIndex==0);
                    owner.Close();
                } catch(Exception ex) {failure=ex;} });
                thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();thread.Join();
                if(failure!=null) throw failure;
                Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
            });
            Test("settings stay in the main window, preserve the sidebar and retain drafts across navigation",()=>{
                string root=Root("inline-navigation");STA(()=>{
                    var ui=new LauncherWindow(root,true);
                    try {
                        int windows=System.Windows.Application.Current==null?0:System.Windows.Application.Current.Windows.Count;
                        var sidebar=(System.Windows.Controls.Border)ui.Window.FindName("SidebarPanel");
                        var host=(System.Windows.Controls.ContentControl)ui.Window.FindName("SettingsHost");
                        var home=(System.Windows.Controls.Grid)ui.Window.FindName("CommandCenter");
                        Click((System.Windows.Controls.Button)ui.Window.FindName("SettingsNav"));
                        Require(host.Visibility==System.Windows.Visibility.Visible && home.Visibility==System.Windows.Visibility.Collapsed && sidebar.Visibility==System.Windows.Visibility.Visible && sidebar.IsEnabled);
                        Require(System.Windows.Window.GetWindow((System.Windows.DependencyObject)host.Content)==ui.Window);
                        var shell=(System.Windows.Controls.Panel)ui.Window.Content;
                        foreach(var size in new[]{new System.Windows.Size(1000,720),new System.Windows.Size(1152,772)}) {
                            shell.Measure(size);shell.Arrange(new System.Windows.Rect(new System.Windows.Point(),size));shell.UpdateLayout();
                            Require(sidebar.ActualWidth==204 && host.ActualWidth==size.Width-204);
                            Require(((System.Windows.FrameworkElement)host.Content).ActualWidth>600);
                        }
                        var input=Descendants(host).OfType<System.Windows.Controls.TextBox>().Single(t=>t.Name=="MouseSensitivity");input.Text="0.42";
                        var draft=host.Content;
                        Click((System.Windows.Controls.Button)ui.Window.FindName("HomeNav"));Require(home.Visibility==System.Windows.Visibility.Visible && host.Visibility==System.Windows.Visibility.Collapsed);
                        Click((System.Windows.Controls.Button)ui.Window.FindName("SettingsNav"));Require(ReferenceEquals(host.Content,draft) && input.Text=="0.42");
                        Require(System.Windows.Application.Current==null || System.Windows.Application.Current.Windows.Count==windows);
                        Click(Descendants(host).OfType<System.Windows.Controls.Button>().Single(b=>b.Name=="SettingsCancelButton"));
                        Require(host.Content==null && home.Visibility==System.Windows.Visibility.Visible);
                        Click((System.Windows.Controls.Button)ui.Window.FindName("SettingsNav"));
                        Require(Descendants(host).OfType<System.Windows.Controls.TextBox>().Single(t=>t.Name=="MouseSensitivity").Text=="0.1");
                    } finally {ui.Window.Close();}
                });Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
            });
            Test("inline binding capture, conflict cancellation and reassignment save without another window",()=>{
                string root=Root("inline-bindings");STA(()=>{
                    var ui=new LauncherWindow(root,true);
                    try {
                        ui.ShowSettings();var host=(System.Windows.Controls.ContentControl)ui.Window.FindName("SettingsHost");
                        Func<string,System.Windows.Controls.Button> button=label=>Descendants(host).OfType<System.Windows.Controls.Button>().Single(b=>b.IsEnabled && Convert.ToString(b.Content)==label);
                        var binding=Descendants(host).OfType<System.Windows.Controls.Button>().First(b=>Convert.ToString(b.ToolTip)==GameSettings.Labels[0]+", binding 1");
                        Click(binding);Require(Descendants(host).OfType<System.Windows.Controls.Border>().Any(b=>b.Name=="BindingCaptureArea"));
                        Click(button("Bind Escape"));Click(button("Cancel"));Require(Convert.ToString(binding.Content)=="E");
                        Click(binding);Click(button("Bind Escape"));Click(button("Move binding"));Require(Convert.ToString(binding.Content)=="Escape");
                        Click(Descendants(host).OfType<System.Windows.Controls.Button>().Single(b=>b.Name=="SettingsSaveButton"));
                        var saved=GameSettings.Load(root,null);Require(GameSettings.KeyCode(saved.Binding(0)[0])==27 && saved.Binding(6).All(t=>GameSettings.KeyCode(t)!=27));
                        Require(host.Visibility==System.Windows.Visibility.Visible);
                        Click(binding);Click((System.Windows.Controls.Button)ui.Window.FindName("HomeNav"));ui.ShowSettings();
                        Require(!Descendants(host).OfType<System.Windows.Controls.Border>().Any(b=>b.Name=="BindingCaptureArea"));
                    } finally {ui.Window.Close();}
                });
            });
            Test("inline save validates input and keeps the page open after a successful save",()=>{
                string root=Root("inline-save");STA(()=>{
                    var ui=new LauncherWindow(root,true);
                    try {
                        ui.ShowSettings(1);var host=(System.Windows.Controls.ContentControl)ui.Window.FindName("SettingsHost");
                        var nodes=Descendants(host).ToList();
                        var sensitivity=nodes.OfType<System.Windows.Controls.TextBox>().Single(t=>t.Name=="MouseSensitivity");
                        var save=nodes.OfType<System.Windows.Controls.Button>().Single(b=>b.Name=="SettingsSaveButton");
                        sensitivity.Text="invalid";Click(save);Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
                        Require(Descendants(host).OfType<System.Windows.Controls.TextBlock>().Any(t=>t.Text.Contains("numeric sensitivity")));
                        sensitivity.Text="0.33";var skip=nodes.OfType<System.Windows.Controls.CheckBox>().Single(c=>Convert.ToString(c.Content)=="Skip startup splash screens");skip.IsChecked=true;Click(save);
                        var saved=GameSettings.Load(root,null);Require(saved.SkipStartupSplashes && saved.Sensitivity==.33 && host.Visibility==System.Windows.Visibility.Visible);
                    } finally {ui.Window.Close();}
                });
            });
            Test("opening settings does not create files; defaults have PC bindings",()=>{
                string root=Root("cancel");var model=GameSettings.Load(root,null);
                Require(model.Sensitivity==.10 && model.Binding(14)[0]=="key:W" && !model.SkipStartupSplashes);
                Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
            });
            Test("Discord defaults, validation, persistence, launch routing and concurrent edits",()=>{
                string root=Root("discord");var model=GameSettings.Load(root,null);
                Require(!model.DiscordEnabled && model.DiscordApplicationId=="1561627743353962566");
                model.DiscordEnabled=true;
                foreach(string invalid in new[]{"", "bot-token", "+1561627743353962566", "01561627743353962566", "18446744073709551616", "1561627743353962566\n"}) {
                    model.DiscordApplicationId=invalid;Reject(()=>model.Save());
                    Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
                }
                model.DiscordApplicationId="1561627743353962566";model.Save();
                Require(Read(root,"Settings/pc-discord.ini")=="TSFP_DISCORD_V1 1 1561627743353962566\n");
                Require(GameSettings.Load(root,null).DiscordEnabled);
                Require(GameSettings.StartInfo(root).EnvironmentVariables["TSFP_DISCORD_CONFIG"]==Path.Combine(root,"Settings","pc-discord.ini"));
                model=GameSettings.Load(root,null);model.DiscordEnabled=false;model.Save();
                Require(!GameSettings.Load(root,null).DiscordEnabled && Read(root,"Settings/pc-discord.ini.bak").Contains("V1 1"));
                model=GameSettings.Load(root,null);string before=Read(root,"Settings/pc-controls.json");
                Put(root,"Settings/pc-discord.ini","TSFP_DISCORD_V1 1 1561627743353962566\n");
                model.Sensitivity=.9;Reject(()=>model.Save());Require(Read(root,"Settings/pc-controls.json")==before);
                Put(root,"Settings/pc-discord.ini","TSFP_DISCORD_V1 1 0\n");Reject(()=>GameSettings.Load(root,null));
            });
            Test("Discord settings UI saves and cancels without altering unrelated preferences",()=>STA(()=>{
                string root=Root("discord-ui");var owner=new System.Windows.Window();
                var editor=new SettingsPage(owner.Resources,root,true);editor.SelectTab(2);owner.Content=editor.View;
                var nodes=Descendants(editor.View).ToList();
                var enabled=nodes.OfType<System.Windows.Controls.CheckBox>().Single(c=>c.Name=="DiscordPresenceEnabled");
                var id=nodes.OfType<System.Windows.Controls.TextBox>().Single(c=>c.Name=="DiscordApplicationId");
                Require(enabled.IsChecked==false && id.Text=="1561627743353962566");
                enabled.IsChecked=true;id.Text="wrong";
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsSaveButton"));
                Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
                id.Text="1561627743353962566";
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsCancelButton"));
                Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
                Click(nodes.OfType<System.Windows.Controls.Button>().Single(c=>c.Name=="SettingsSaveButton"));
                var saved=GameSettings.Load(root,null);Require(saved.DiscordEnabled && saved.Sensitivity==.10 && !saved.SkipStartupSplashes);
                owner.Close();
            }));
            Test("startup preference round-trips, preserves other launcher keys and can be disabled",()=>{
                string root=Root("startup");var model=GameSettings.Load(root,null);model.Save();
                var json=Json.Object(Read(root,"Settings/pc-controls.json"));
                ((System.Collections.Generic.Dictionary<string,object>)json["launcher"])["future_option"]="keep";
                Put(root,"Settings/pc-controls.json",Json.Encode(json));
                model=GameSettings.Load(root,null);model.SkipStartupSplashes=true;model.Save();
                Require(GameSettings.Load(root,null).SkipStartupSplashes);
                Require(Read(root,"Settings/pc-controls.json").Contains("future_option"));
                string inherited=Environment.GetEnvironmentVariable("RECOMP_XMV_PLAY");
                try {
                    Environment.SetEnvironmentVariable("RECOMP_XMV_PLAY","1");
                    Require(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_XMV_PLAY"]=="0");
                    model=GameSettings.Load(root,null);model.SkipStartupSplashes=false;model.Save();
                    Environment.SetEnvironmentVariable("RECOMP_XMV_PLAY","0");
                    Require(!GameSettings.Load(root,null).SkipStartupSplashes);
                    Require(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_XMV_PLAY"]=="1");
                } finally {Environment.SetEnvironmentVariable("RECOMP_XMV_PLAY",inherited);}
            });
            Test("invalid startup preference is refused without altering settings",()=>{
                string root=Root("bad-startup");var model=GameSettings.Load(root,null);model.Save();
                var json=Json.Object(Read(root,"Settings/pc-controls.json"));
                ((System.Collections.Generic.Dictionary<string,object>)json["launcher"])["skip_startup_splashes"]="false";
                Put(root,"Settings/pc-controls.json",Json.Encode(json));string before=Read(root,"Settings/pc-controls.json");
                Reject(()=>GameSettings.Load(root,null));Require(Read(root,"Settings/pc-controls.json")==before);
            });
            Test("save round-trips controls and graphics; preserves other profiles and unknown fields",()=>{
                string root=Root("preserve"); var first=GameSettings.Load(root,null);first.Save();
                var json=Json.Object(Read(root,"Settings/pc-controls.json"));json["future_setting"]="keep";
                var profiles=(IList)json["controllers"];((System.Collections.Generic.Dictionary<string,object>)profiles[1])["future_pad"]="keep pad";
                Put(root,"Settings/pc-controls.json",Json.Encode(json));
                Put(root,"Settings/runtime.conf","# Keep this comment\nfuture_graphics = keep\nframe_interp = 2\n");
                var model=GameSettings.Load(root,null);model.Width=3440;model.Height=1440;model.Sensitivity=.35;model.Anisotropy=16;model.Fullscreen=true;
                model.Assign(14,1,90,false);model.Save();var saved=GameSettings.Load(root,null);
                Require(saved.Binding(14)[0]=="key:W" && GameSettings.KeyCode(saved.Binding(14)[1])==90);
                Require(saved.Width==3440 && saved.Height==1440 && saved.Fullscreen && saved.Interpolation==1 && saved.Anisotropy==16 && saved.Sensitivity==.35);
                Require(Read(root,"Settings/pc-controls.json").Contains("keep pad") && Read(root,"Settings/pc-controls.json").Contains("future_setting"));
                Require(Read(root,"Settings/runtime.conf").Contains("# Keep this comment") && Read(root,"Settings/runtime.conf").Contains("future_graphics = keep"));
                Require(File.Exists(Path.Combine(root,"Settings/pc-controls.json.bak")));
            });
            Test("legacy timing settings migrate to 60 FPS without losing unrelated preferences",()=>{
                foreach(int target in new[]{0,120,144}) {
                    string root=Root("retired-fps-"+target);
                    Put(root,"Settings/runtime.conf","frame_interp = 4\nframe_target = "+target+"\nrefresh_rate = 144\nframe_cap = 0\nfuture_graphics = keep\nfullscreen = 1\n");
                    var model=GameSettings.Load(root,null);
                    Require(model.PresentationFps==0 && model.Interpolation==1 && model.FrameCap=="60" && model.RefreshRate==0);
                    var info=GameSettings.StartInfo(root);
                    Require(info.EnvironmentVariables["RECOMP_FRAME_INTERP"]=="1" && info.EnvironmentVariables["RECOMP_FRAME_TARGET"]=="0" && info.EnvironmentVariables["RECOMP_FPS_CAP"]=="60");
                    model.Save();
                    var values=GameSettings.ParseRuntime(Read(root,"Settings/runtime.conf"));
                    Require(values["frame_interp"]=="1" && values["frame_target"]=="0" && values["frame_cap"]=="60" && values["refresh_rate"]=="0");
                    Require(values["future_graphics"]=="keep" && model.Fullscreen);
                }
            });
            Test("graphics page keeps retired interpolation and presentation controls removed",()=>STA(()=>{
                var page=new SettingsPage(new System.Windows.Window().Resources,Root("fixed-fps-ui"),true);page.SelectTab(1);
                var names=Descendants(page.View).OfType<System.Windows.FrameworkElement>().Select(n=>n.Name).ToList();
                foreach(string name in new[]{"PresentationTarget","LegacyInterpolation","GameFramePacing","MonitorRefreshRate","PerformancePreset120","PerformancePreset144"})
                    Require(!names.Contains(name),"Retired control remains: "+name);
            }));
            Test("performance guidance tracks actual resolution, VRR and fullscreen choices",()=>STA(()=>{
                string root=Root("performance-guidance");var model=GameSettings.Load(root,null);model.Scale=4;model.Save();
                var owner=new System.Windows.Window();var page=new SettingsPage(owner.Resources,root,true);page.SelectTab(1);
                var nodes=Descendants(page.View).ToList();
                var advice=nodes.OfType<System.Windows.Controls.TextBlock>().Single(t=>t.Name=="PerformanceAdvice");
                var scale=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.Name=="ConsoleRenderScale");
                var resolution=nodes.OfType<System.Windows.Controls.ComboBox>().Single(c=>c.Name=="OutputResolution");
                Require(scale.IsEnabled && advice.Text.Contains("16× the scene pixels"));
                resolution.SelectedItem=resolution.Items.OfType<DisplayMode>().First(m=>m.Width==1920 && m.Height==1080);
                Require(!scale.IsEnabled && advice.Text.Contains("console scale is inactive"));
                Require(Convert.ToString(scale.SelectedItem)=="4");
                Require(advice.Text.Contains("60 FPS"));
                string preview=Environment.GetEnvironmentVariable("TSFP_PERFORMANCE_PREVIEW");
                if(!String.IsNullOrEmpty(preview)) {
                    page.View.Measure(new System.Windows.Size(1000,900));
                    page.View.Arrange(new System.Windows.Rect(0,0,1000,900));page.View.UpdateLayout();
                    var bitmap=new System.Windows.Media.Imaging.RenderTargetBitmap(1000,900,96,96,System.Windows.Media.PixelFormats.Pbgra32);bitmap.Render(page.View);
                    var encoder=new System.Windows.Media.Imaging.PngBitmapEncoder();encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));
                    using(var stream=File.Create(preview))encoder.Save(stream);
                }
            }));
            Test("conflicting key requires move and removes only that key",()=>{
                var model=GameSettings.Load(Root("conflict"),null);Reject(()=>model.Assign(14,1,69,false));
                model.Assign(14,1,69,true);Require(GameSettings.KeyCode(model.Binding(14)[1])==69);
                Require(model.Binding(0).All(t=>GameSettings.KeyCode(t)!=69));Require(model.Binding(0).Contains("key:SPACE"));
                Reject(()=>model.Assign(0,0,112,true));Reject(()=>model.Assign(0,0,122,true));
                model.Assign(14,1,0,true);Require(model.Binding(14)[1]=="none");
            });
            Test("invalid settings are rejected before any writes",()=>{
                string root=Root("invalid");var model=GameSettings.Load(root,null);model.Sensitivity=Double.NaN;Reject(()=>model.Save());
                model.Sensitivity=.1;model.Width=7680;model.Height=720;Reject(()=>model.Save());
                Require(Directory.GetFiles(root,"*",SearchOption.AllDirectories).Length==0);
            });
            Test("malformed existing JSON is preserved",()=>{
                string root=Root("malformed");Put(root,"Settings/pc-controls.json","{broken");Reject(()=>GameSettings.Load(root,null));Require(Read(root,"Settings/pc-controls.json")=="{broken");
            });
            Test("external changes reject the entire stale save",()=>{
                string root=Root("external");var model=GameSettings.Load(root,null);Put(root,"Settings/pc-display.ini","TSFP_DISPLAY_V1 1920 1080\n");Reject(()=>model.Save());Require(!File.Exists(Path.Combine(root,"Settings/pc-controls.json")));Require(Read(root,"Settings/pc-display.ini").Contains("1920 1080"));
            });
            Test("read-only settings prevent partial changes",()=>{
                string root=Root("readonly");var model=GameSettings.Load(root,null);model.Save();model=GameSettings.Load(root,null);
                string path=Path.Combine(root,"Settings/runtime.conf");string before=Read(root,"Settings/pc-controls.json");File.SetAttributes(path,FileAttributes.ReadOnly);
                try {model.Sensitivity=.5;Reject(()=>model.Save());Require(Read(root,"Settings/pc-controls.json")==before);}finally{File.SetAttributes(path,FileAttributes.Normal);}
            });
            Test("late write failure rolls back already replaced files",()=>{
                string root=Root("rollback");var model=GameSettings.Load(root,null);model.Save();model=GameSettings.Load(root,null);
                string before=Read(root,"Settings/pc-controls.json"),display=Read(root,"Settings/pc-display.ini");
                Directory.CreateDirectory(Path.Combine(root,"Settings/runtime.conf.bak"));model.Sensitivity=.7;model.Width=1920;model.Height=1080;
                Reject(()=>model.Save());Require(Read(root,"Settings/pc-controls.json")==before && Read(root,"Settings/pc-display.ini")==display);
            });
            Test("inherited runtime settings are copied without modifying their source",()=>{
                string root=Root("inherit");Put(root,"old.conf","frame_interp = 2\nfuture = untouched\n");
                var model=GameSettings.Load(root,Path.Combine(root,"old.conf"));Require(model.Interpolation==1);model.Save();Require(Read(root,"old.conf")=="frame_interp = 2\nfuture = untouched\n");Require(Read(root,"Settings/runtime.conf").Contains("future = untouched"));
            });
            foreach(bool configured in new[]{false,true}) foreach(int exitCode in new[]{0,7})
            {
                bool saved=configured;int expectedExit=exitCode;
                Test("Play directly launches " + (saved?"configured":"fresh") + " installation, exit " + expectedExit,()=>{
                    string root=Root("Play in folder with spaces "+saved+expectedExit);
                    var model=GameSettings.Load(root,null);model.Sensitivity=.37;model.InvertY=true;model.MouseEnabled=false;model.SkipStartupSplashes=true;model.Save();
                    if(!saved) {File.Delete(Path.Combine(root,"Settings/runtime.conf"));File.Delete(Path.Combine(root,"Settings/pc-display.ini"));}
                    PlayProbe(root,expectedExit);
                });
            }
            Test("configured launch passes portable settings directly to runtime",()=>{
                string root=Root("launch");var model=GameSettings.Load(root,null);model.Sensitivity=.25;model.Save();
                string old=Environment.GetEnvironmentVariable("RECOMP_SETTINGS_TEST");Environment.SetEnvironmentVariable("RECOMP_SETTINGS_TEST","bad");
                try {var info=GameSettings.StartInfo(root);Require(info.FileName==Path.Combine(root,"Bin","futureperfect_recomp.exe"));Require(info.EnvironmentVariables["RECOMP_DISPLAY_CONFIG"]==Path.Combine(root,"Settings","runtime.conf"));Require(info.EnvironmentVariables["TSFP_DISPLAY_CONFIG"]==Path.Combine(root,"Settings","pc-display.ini"));Require(info.EnvironmentVariables["RECOMP_MOUSE_SENSITIVITY"]=="0.25");Require(info.EnvironmentVariables["RECOMP_SETTINGS_TEST"]==null);}finally{Environment.SetEnvironmentVariable("RECOMP_SETTINGS_TEST",old);}
            });
            Test("child process receives and reads the selected portable settings",()=>{
                string root=Root("child");var model=GameSettings.Load(root,null);model.Sensitivity=.45;model.Width=2560;model.Height=1440;model.Anisotropy=8;model.SkipStartupSplashes=true;model.Save();
                Directory.CreateDirectory(Path.Combine(root,"Bin"));File.Copy(typeof(LauncherSettingsChecks).Assembly.Location,Path.Combine(root,"Bin/futureperfect_recomp.exe"));
                var info=GameSettings.StartInfo(root);info.Arguments="--settings-child";
                using(var child=Process.Start(info)) {
                    try {var output=child.StandardOutput.ReadToEndAsync();var errors=child.StandardError.ReadToEndAsync();Require(child.WaitForExit(5000));string text=output.GetAwaiter().GetResult();string error=errors.GetAwaiter().GetResult();Require(child.ExitCode==0 && text.Contains("0.45") && text.Contains("TSFP_DISPLAY_V1 2560 1440") && text.Contains("8") && text.Contains("XMV_PLAY=0"));Require(error.Length==0);}
                    finally{if(!child.HasExited)child.Kill();}
                }
            });
            Console.WriteLine("PASS: "+count+" isolated launcher settings checks.");return 0;
        }
        catch(Exception error){Console.WriteLine(error);return 1;}
        finally {Directory.Delete(temp,true);}
    }
}
