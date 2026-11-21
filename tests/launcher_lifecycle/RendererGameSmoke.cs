// Bounded hardware acceptance harness. Run via test-launcher-renderers-windows.py.
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Drawing;
using TimeSplitters.Launcher;
public static class RendererGameSmoke {
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] static extern int GetWindowStyle(IntPtr h,int index);
 [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p,IntPtr a,byte[] b,int size,out IntPtr read);
 [StructLayout(LayoutKind.Sequential)] struct Rect {public int L,T,R,B;}
 static int result;static string root,evidence;static Process owned;static Dictionary<string,object> config;
 static int Number(string key,int fallback){object v;return config.TryGetValue(key,out v)?Convert.ToInt32(v):fallback;}
 static bool Flag(string key){object v;return config.TryGetValue(key,out v) && v is bool && (bool)v;}
 static void Need(bool v,string m){if(!v)throw new Exception(m);}
 static IEnumerable<DependencyObject> All(DependencyObject x){yield return x;foreach(var c in LogicalTreeHelper.GetChildren(x).OfType<DependencyObject>())foreach(var d in All(c))yield return d;}
 static T Find<T>(DependencyObject x,string name) where T:FrameworkElement {return All(x).OfType<T>().Single(c=>c.Name==name);}
 static List<PerformanceCounter> GpuCounters(int pid){
  try{return new PerformanceCounterCategory("GPU Process Memory").GetInstanceNames()
   .Where(n=>n.StartsWith("pid_"+pid+"_",StringComparison.OrdinalIgnoreCase))
   .Select(n=>new PerformanceCounter("GPU Process Memory","Dedicated Usage",n,true)).ToList();}
  catch{return new List<PerformanceCounter>();}
 }
 static string ReadLiveLog(string path){
  using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
  using(var reader=new StreamReader(stream))return reader.ReadToEnd();
 }
 static int CheckLiveLog(){
  string path=Path.GetTempFileName();
  try{using(var writer=new FileStream(path,FileMode.Create,FileAccess.Write,FileShare.ReadWrite)){
   byte[] bytes=System.Text.Encoding.UTF8.GetBytes("[PRESENT] backend=vulkan");writer.Write(bytes,0,bytes.Length);writer.Flush();
   Need(ReadLiveLog(path)=="[PRESENT] backend=vulkan","Live diagnostic reader conflicts with writer");
  }Console.WriteLine("PASS live log reader shares an active writer");return 0;}finally{File.Delete(path);}
 }
 static uint Word(Process p,uint a){var b=new byte[4];IntPtr n;return ReadProcessMemory(p.Handle,new IntPtr((long)a+0x10000),b,4,out n)?BitConverter.ToUInt32(b,0):0;}
 static float[] Position(Process p,uint player){var b=new byte[12];IntPtr n;if(!ReadProcessMemory(p.Handle,new IntPtr((long)player+0xb4+0x10000),b,12,out n))return new float[3];return new[]{BitConverter.ToSingle(b,0),BitConverter.ToSingle(b,4),BitConverter.ToSingle(b,8)};}
 static async Task Key(IntPtr h,int key,int ms){PostMessage(h,0x100,new IntPtr(key),new IntPtr(1));await Task.Delay(ms);PostMessage(h,0x101,new IntPtr(key),new IntPtr(unchecked((int)0xc0000001)));}
 static bool Fullscreen(IntPtr h){return (unchecked((uint)GetWindowStyle(h,-16))&0x80000000u)!=0;}
 static object DisplaySnapshot(IntPtr h,string label,double seconds){
  Rect client=new Rect(),window=new Rect();Need(GetClientRect(h,out client) && GetWindowRect(h,out window),"Cannot inspect game bounds");
  return new {label=label,seconds=seconds,client_width=client.R-client.L,client_height=client.B-client.T,
   window_width=window.R-window.L,window_height=window.B-window.T,fullscreen=Fullscreen(h)};
 }
 static async Task ToggleFullscreen(IntPtr h){
  bool before=Fullscreen(h);Need(PostMessage(h,0x104,new IntPtr(13),new IntPtr(1<<29)),"Alt+Enter could not be queued");
  for(int i=0;i<20 && Fullscreen(h)==before;i++)await Task.Delay(50);
  Need(Fullscreen(h)!=before,"Alt+Enter did not change actual window style");
 }
 static void Capture(IntPtr h,string path){Rect r;Need(GetWindowRect(h,out r),"No game rectangle");using(var b=new Bitmap(r.R-r.L,r.B-r.T)) {using(var g=Graphics.FromImage(b))g.CopyFromScreen(r.L,r.T,0,0,b.Size);b.Save(path,System.Drawing.Imaging.ImageFormat.Png);}}
 [STAThread] public static int Main(string[] args){if(args.Length==1 && args[0]=="--self-test-io")return CheckLiveLog();root=Path.GetFullPath(args[0]);evidence=Path.GetFullPath(args[1]);Directory.CreateDirectory(evidence);config=Json.Object(File.ReadAllText(args[2]));var app=new Application{ShutdownMode=ShutdownMode.OnExplicitShutdown};app.Startup+=async(s,e)=>{try{await Run((string)config["backend"]);}catch(Exception ex){Console.WriteLine(ex);result=1;}finally{if(owned!=null){try{if(!owned.HasExited){owned.Kill();owned.WaitForExit(10000);}}catch{}}app.Shutdown();}};app.Run();return result;}
 static async Task Run(string backend){
  var initial=GameSettings.Load(root,null);initial.SkipStartupSplashes=true;initial.Width=Number("width",1280);initial.Height=Number("height",720);initial.Fullscreen=Flag("fullscreen");initial.Msaa=Number("msaa",1);initial.PostAntiAliasing=Number("aa",0);initial.Anisotropy=Number("anisotropy",1);initial.Save();
  // Shared executable is unmodified; preview mode only suppresses network checks.
  var ui=new LauncherWindow(root,true);ui.Window.Show();ui.ShowSettings(1);
  var page=(SettingsPage)typeof(LauncherWindow).GetField("settings",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(ui);
  var combo=Find<ComboBox>(page.View,"GraphicsRenderer");Need(combo.IsEnabled,"Renderer selector disabled");
  combo.SelectedIndex=Array.IndexOf(GameSettings.Renderers,backend);
  Find<Button>(page.View,"SettingsSaveButton").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
  Need(GameSettings.Load(root,null).Renderer==backend,"UI choice not saved");
  Need(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_D3D8_BACKEND"]==backend,"Saved backend not mapped");
  Console.WriteLine("SAVED "+backend+" through GraphicsRenderer and SettingsSaveButton");
  int duration=Number("seconds",120);Need(duration>=120 && duration<=600,"Duration outside 120..600 seconds");
  var clock=Stopwatch.StartNew();
  var method=typeof(LauncherWindow).GetMethod("LaunchGame",BindingFlags.Instance|BindingFlags.NonPublic);
  var launch=(Task)method.Invoke(ui,new object[]{null});
  string liveLog=Directory.GetFiles(Path.Combine(root,"Logs"),"*.err").OrderBy(File.GetLastWriteTimeUtc).Last();
  Process game=null;
  while(game==null && clock.Elapsed.TotalSeconds<10){foreach(var p in Process.GetProcessesByName("futureperfect_recomp")){try{if(String.Equals(p.MainModule.FileName,Path.GetFullPath(Path.Combine(root,"Bin/futureperfect_recomp.exe")),StringComparison.OrdinalIgnoreCase)){game=p;break;}}catch{}p.Dispose();}await Task.Delay(100);}
  Need(game!=null,"Launcher did not create game process");owned=game;
  File.WriteAllText(Path.Combine(evidence,"game-pid.txt"),game.Id.ToString());
  Console.WriteLine("STARTED "+backend+" PID "+game.Id);
  var gpuCounters=new List<PerformanceCounter>();
  try{
   while(game.MainWindowHandle==IntPtr.Zero && clock.Elapsed.TotalSeconds<15){Need(!game.HasExited,"Game exited during startup");await Task.Delay(100);game.Refresh();}
   IntPtr hwnd=game.MainWindowHandle;Need(hwnd!=IntPtr.Zero,"No rendered game window");SetForegroundWindow(hwnd);double windowSeconds=clock.Elapsed.TotalSeconds;double storySeconds=-1,gameplayReadySeconds=-1,firstPresentSeconds=-1,nextGpu=5;double? gpuDedicated=null;
   if(Flag("fullscreen")){for(int i=0;i<20 && !Fullscreen(hwnd);i++)await Task.Delay(50);}
   Need(Fullscreen(hwnd)==Flag("fullscreen"),"Requested initial window mode not active");
   Rect initialWindow=new Rect(),initialClient=new Rect();Need(GetWindowRect(hwnd,out initialWindow) && GetClientRect(hwnd,out initialClient),"Cannot inspect initial window");
   var displaySnapshots=new List<object>{DisplaySnapshot(hwnd,"initial",clock.Elapsed.TotalSeconds)};int displayStage=0;
   double[] captureTimes=config.ContainsKey("capture_times")?((System.Collections.IEnumerable)config["capture_times"]).Cast<object>().Select(x=>Convert.ToDouble(x)).ToArray():new double[]{38,80,112};
   double[] times={5,10,14,18,22,26,30,34};int[] keys={69,69,40,69,69,69,69,69};int[] holds=Enumerable.Repeat(180,8).ToArray();
   if(config.ContainsKey("input")){
    var actions=((System.Collections.IEnumerable)config["input"]).Cast<object>().Select(x=>(Dictionary<string,object>)x).ToArray();
    times=actions.Select(a=>Convert.ToDouble(a["at"])).ToArray();keys=actions.Select(a=>Convert.ToInt32(a["key"])).ToArray();holds=actions.Select(a=>a.ContainsKey("hold_ms")?Convert.ToInt32(a["hold_ms"]):180).ToArray();
   }
   int expectedWeapon=Number("expected_weapon",14);
   int next=0;bool skipped=false,moving=false,released=false;int shot=0;int capture=0;uint firstTick=0,lastTick=0;var samples=new List<object>();var positions=new List<float[]>();int equipped=0;
   while(clock.Elapsed.TotalSeconds<duration){
    Need(!game.HasExited,"Game exited before bounded test ended");double t=clock.Elapsed.TotalSeconds;
    if(next<times.Length && t>=times[next]){await Key(hwnd,keys[next],holds[next]);Console.WriteLine(backend+" menu key "+keys[next]+" at "+t.ToString("F1"));next++;}
    if(firstPresentSeconds<0 && ReadLiveLog(liveLog).Contains("[PRESENT] backend="+backend))firstPresentSeconds=t;
    if(Flag("display_cycle")){
     if(displayStage==0 && t>=60){await ToggleFullscreen(hwnd);displaySnapshots.Add(DisplaySnapshot(hwnd,"fullscreen",t));displayStage++;}
     else if(displayStage==1 && t>=70){await ToggleFullscreen(hwnd);Need(!Fullscreen(hwnd),"Window mode did not restore");displaySnapshots.Add(DisplaySnapshot(hwnd,"windowed",t));displayStage++;}
     else if(displayStage==2 && t>=100){Need(SetWindowPos(hwnd,IntPtr.Zero,0,0,1024,768,0x16),"Resize request failed");await Task.Delay(300);Rect client;Need(GetClientRect(hwnd,out client) && (client.R!=initialClient.R || client.B!=initialClient.B),"Actual client size did not change");displaySnapshots.Add(DisplaySnapshot(hwnd,"resized",t));displayStage++;}
     else if(displayStage==3 && t>=108){Need(SetWindowPos(hwnd,IntPtr.Zero,0,0,initialWindow.R-initialWindow.L,initialWindow.B-initialWindow.T,0x16),"Restore size request failed");await Task.Delay(300);Rect client;Need(GetClientRect(hwnd,out client) && client.R==initialClient.R && client.B==initialClient.B,"Client size did not restore");displaySnapshots.Add(DisplaySnapshot(hwnd,"restored",t));displayStage++;}
    }
    uint scene=Word(game,0x79094c),flags=Word(game,0x7de458),ticks=Word(game,0x7de318),player=Word(game,0x7b0c7c);
    if((!config.ContainsKey("skip_intro") || Flag("skip_intro")) && t>45 && (flags&0x20000000)!=0 && !skipped){await Key(hwnd,27,200);skipped=true;Console.WriteLine(backend+" skipped opening scene at "+t.ToString("F1"));}
    if(t>=85 && !moving){SetForegroundWindow(hwnd);PostMessage(hwnd,0x100,new IntPtr(87),new IntPtr(1));moving=true;}
    if(t>=90 && !released){PostMessage(hwnd,0x101,new IntPtr(87),new IntPtr(unchecked((int)0xc0000001)));released=true;}
    if(t>=92+shot*4 && shot<5){PostMessage(hwnd,0x201,new IntPtr(1),IntPtr.Zero);await Task.Delay(180);PostMessage(hwnd,0x202,IntPtr.Zero,IntPtr.Zero);shot++;}
    if(scene==2 && storySeconds<0)storySeconds=t;
    if(t>=nextGpu){
     Need(new FileInfo(liveLog).Length<=4*1024*1024 && new FileInfo(Path.ChangeExtension(liveLog,".out")).Length<=4*1024*1024,"Game log exceeded per-stream 4 MiB budget");
     try{if(gpuCounters.Count==0)gpuCounters=GpuCounters(game.Id);if(gpuCounters.Count>0)gpuDedicated=gpuCounters.Sum(c=>(double)c.NextValue());}catch{gpuDedicated=null;}
     nextGpu=t+5;
    }
    float[] pos=null;uint weapon=0;game.Refresh();
    if(scene==2 && player>=0x80000000 && player<0x84000000){pos=Position(game,player);weapon=Word(game,player+0x58c);if((expectedWeapon<0 || weapon==expectedWeapon) && (flags&0x20000000)==0 && gameplayReadySeconds<0)gameplayReadySeconds=t;if(t>=85 && t<=115 && (flags&0x20000000)==0){positions.Add(pos);if(expectedWeapon<0 || weapon==expectedWeapon)equipped++;if(firstTick==0)firstTick=ticks;lastTick=ticks;}}
    samples.Add(new {seconds=Math.Round(t,2),clock_qpc_ms=Stopwatch.GetTimestamp()*1000.0/Stopwatch.Frequency,scene=scene,flags=flags,ticks=ticks,player=player,position=pos,weapon=weapon,cpu_total_ms=game.TotalProcessorTime.TotalMilliseconds,private_bytes=game.PrivateMemorySize64,working_set_bytes=game.WorkingSet64,handles=game.HandleCount,gpu_dedicated_bytes=gpuDedicated});
    if(Flag("capture") && capture<captureTimes.Length && t>=captureTimes[capture]){SetForegroundWindow(hwnd);await Task.Delay(150);Capture(hwnd,Path.Combine(evidence,backend+"-"+capture+".png"));capture++;}
    await Task.Delay(300);
   }
   File.WriteAllText(Path.Combine(evidence,backend+"-samples.json"),Json.Encode(samples));
   Need(!Flag("display_cycle") || displayStage==4,"Display cycle did not complete");
   File.WriteAllText(Path.Combine(evidence,"display.json"),Json.Encode(displaySnapshots));
   Need(positions.Count>20 && equipped>20,"Expected scenario gameplay / weapon not reached");
   Need(lastTick>firstTick+100,"Game tick clock did not advance in gameplay");
   Need(positions.Any(p=>p.Zip(positions[0],(a,b)=>(a-b)*(a-b)).Sum()>1),"Player did not move");
   PostMessage(hwnd,0x10,IntPtr.Zero,IntPtr.Zero);
   var done=await Task.WhenAny(launch,Task.Delay(15000));Need(done==launch,"Launcher did not finish after closing game");await launch;
   Need(game.HasExited && game.ExitCode==0,"Game did not close cleanly");
   var model=GameSettings.Load(root,null);Need(model.Renderer==backend,"Game run changed saved selection");
   Need(model.Msaa==Number("msaa",1) && model.PostAntiAliasing==Number("aa",0) && model.Anisotropy==Number("anisotropy",1),"Game run changed saved quality settings");
   string err=Directory.GetFiles(Path.Combine(root,"Logs"),"*.err").OrderBy(File.GetLastWriteTimeUtc).Last();string text=File.ReadAllText(err);File.Copy(err,Path.Combine(evidence,backend+".err"),true);
   Need(text.Contains("[PRESENT] backend="+backend),"Requested backend did not present");Need(!text.Contains("[CRASH]"),"Crash in game log");
   File.WriteAllText(Path.Combine(evidence,backend+"-result.json"),Json.Encode(new {passed=true,backend=backend,pid=game.Id,seconds=clock.Elapsed.TotalSeconds,window_seconds=windowSeconds,story_seconds=storySeconds,first_present_observed_seconds=firstPresentSeconds,gameplay_ready_seconds=gameplayReadySeconds,gpu_metric_available=gpuDedicated.HasValue,capture_enabled=Flag("capture"),display_cycle_completed=Flag("display_cycle") && displayStage==4,display_snapshots=displaySnapshots,settings=config,gameplay_samples=positions.Count,equipped_samples=equipped,tick_delta=lastTick-firstTick,exit_code=game.ExitCode,source_log=err}));
   Console.WriteLine("PASS "+backend+" actual launcher, scenario gameplay, movement, weapon expectation, presents and clean exit; ticks="+(lastTick-firstTick));
  }finally{foreach(var counter in gpuCounters)counter.Dispose();if(!game.HasExited){game.Kill();game.WaitForExit(10000);}game.Dispose();owned=null;ui.Window.Close();}
 }
}
