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
 [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,IntPtr process);
 [DllImport("user32.dll")] static extern bool AttachThreadInput(uint from,uint to,bool attach);
 [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();

 [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] static extern int GetWindowStyle(IntPtr h,int index);
 [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p,IntPtr a,byte[] b,int size,out IntPtr read);
 [StructLayout(LayoutKind.Sequential)] struct Rect {public int L,T,R,B;}
 static int result;static string root,evidence,probeInput;static Process owned;static Dictionary<string,object> config;
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
 static void InputState(int key,bool down){
  string temporary=probeInput+".tmp";File.WriteAllText(temporary,key.ToString()+" "+(down?"1":"0")+"\n");
  var deadline=Stopwatch.StartNew();
  for(;;){try{if(File.Exists(probeInput))File.Replace(temporary,probeInput,null);else File.Move(temporary,probeInput);return;}catch(IOException){if(deadline.ElapsedMilliseconds>=500)throw;System.Threading.Thread.Sleep(2);}}
 }
 static void SetKey(IntPtr h,int key,bool down){
  if(probeInput!=null){InputState(key,down);return;}
  Foreground(h);Need(GetForegroundWindow()==h,"Game must own foreground before synthetic window-key input");
  PostMessage(h,down?0x100u:0x101u,new IntPtr(key),new IntPtr(down?1:unchecked((int)0xc0000001)));
 }
 static async Task Key(IntPtr h,int key,int ms){SetKey(h,key,true);await Task.Delay(ms);SetKey(h,key,false);}

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
 static void Foreground(IntPtr window){
  if(GetForegroundWindow()==window)return;
  uint current=GetCurrentThreadId(),foreground=GetWindowThreadProcessId(GetForegroundWindow(),IntPtr.Zero);
  bool attached=foreground!=0 && foreground!=current && AttachThreadInput(current,foreground,true);
  try{SetWindowPos(window,IntPtr.Zero,0,0,0,0,0x3);SetForegroundWindow(window);}
  finally{if(attached)AttachThreadInput(current,foreground,false);}
 }
 static void Capture(Process game,string path){
  game.Refresh();Need(GetForegroundWindow()==game.MainWindowHandle,"Game is not foreground; refusing an occluded desktop capture");
  Need(config.ContainsKey("capture_helper"),"No DXGI capture helper configured");
  string helper=(string)config["capture_helper"],expected=Path.GetFullPath(Path.Combine(root,"Bin/futureperfect_recomp.exe"));
  // Windows paths cannot contain quotes; all three paths are test-owned.
  Need(!helper.Contains("\"") && !expected.Contains("\"") && !path.Contains("\""),"Invalid capture path");
  var start=new ProcessStartInfo(helper,game.Id+" \""+expected+"\" \""+path+"\""){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
  using(var capture=Process.Start(start)){
   if(!capture.WaitForExit(12000)){capture.Kill();capture.WaitForExit();throw new Exception("DXGI capture timed out");}
   string output=capture.StandardOutput.ReadToEnd()+capture.StandardError.ReadToEnd();Console.WriteLine(output);
   Need(capture.ExitCode==0 && File.Exists(path),"DXGI capture failed, exit "+capture.ExitCode+": "+output);
  }
 }
 [STAThread] public static int Main(string[] args){if(args.Length==1 && args[0]=="--self-test-io")return CheckLiveLog();root=Path.GetFullPath(args[0]);evidence=Path.GetFullPath(args[1]);Directory.CreateDirectory(evidence);config=Json.Object(File.ReadAllText(args[2]));var app=new Application{ShutdownMode=ShutdownMode.OnExplicitShutdown};app.Startup+=async(s,e)=>{try{await Run((string)config["backend"]);}catch(Exception ex){Console.WriteLine(ex);result=1;}finally{if(owned!=null){try{if(!owned.HasExited){owned.Kill();owned.WaitForExit(10000);}}catch{}}app.Shutdown();}};app.Run();return result;}
 // Explicit developer probe only. Keep it in this opt-in test executable:
 // ordinary launcher settings and LaunchGame continue selecting Vulkan.
 static async Task DrainProbe(Stream source,Stream destination){
  byte[] buffer=new byte[8192];int remaining=4*1024*1024,count;
  while((count=await source.ReadAsync(buffer,0,buffer.Length))>0){int keep=Math.Min(count,remaining);if(keep>0){await destination.WriteAsync(buffer,0,keep);destination.Flush();remaining-=keep;}}
 }
 static async Task LaunchProbe(string backend){
  Need(backend=="opengl","Unknown developer renderer probe");
  var info=GameSettings.StartInfo(root);
  Need(info.EnvironmentVariables["RECOMP_D3D8_BACKEND"]=="vulkan","Probe changed the launcher default");
  info.EnvironmentVariables["RECOMP_D3D8_BACKEND"]=backend;
  probeInput=Path.Combine(root,"Settings/developer-input.txt");InputState(0,false);
  info.EnvironmentVariables["TSFP_PC_INPUT_STATE"]=probeInput;
  info.EnvironmentVariables["RECOMP_WINDOW_BACKGROUND"]=Flag("fullscreen")?"0":"1";
  string logs=Path.Combine(root,"Logs");Directory.CreateDirectory(logs);
  string tag=DateTime.Now.ToString("yyyyMMdd-HHmmss-fff",System.Globalization.CultureInfo.InvariantCulture)+"-developer";
  string errorPath=Path.Combine(logs,tag+".err");info.EnvironmentVariables["RECOMP_LOG_FILE"]=errorPath;
  using(var output=new FileStream(Path.Combine(logs,tag+".out"),FileMode.Create,FileAccess.Write,FileShare.ReadWrite))
  using(var errors=new FileStream(errorPath,FileMode.Create,FileAccess.Write,FileShare.ReadWrite))
  using(var process=new Process{StartInfo=info}){
   process.Start();owned=process;
   var stdout=DrainProbe(process.StandardOutput.BaseStream,output);var stderr=DrainProbe(process.StandardError.BaseStream,errors);
   while(!process.HasExited)await Task.Delay(50);
   await Task.WhenAll(stdout,stderr);
  }
 }
 static async Task Run(string backend){
  string expectedBackend=config.ContainsKey("expected_backend")?(string)config["expected_backend"]:backend;
  string recovery=config.ContainsKey("startup_recovery")?(string)config["startup_recovery"]:null;
  Need(expectedBackend==backend || (backend=="vulkan" && expectedBackend=="opengl" && (recovery=="opengl" || recovery=="both-fail")),"Unrequested renderer fallback");
  var initial=GameSettings.Load(root,null);initial.SkipStartupSplashes=true;initial.Width=Number("width",1280);initial.Height=Number("height",720);initial.Fullscreen=Flag("fullscreen");initial.Msaa=Number("msaa",1);initial.PostAntiAliasing=Number("aa",0);initial.Anisotropy=Number("anisotropy",1);initial.AmbientOcclusion=Number("ao",0);initial.Save();
  // Shared executable is unmodified; preview mode only suppresses network checks.
  var ui=new LauncherWindow(root,true);ui.Window.Show();ui.ShowSettings(1);
  var page=(SettingsPage)typeof(LauncherWindow).GetField("settings",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(ui);
  Need(backend=="vulkan" || backend=="opengl","Unknown renderer acceptance route");
  Need(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_D3D8_BACKEND"]=="vulkan","Launcher default is not Vulkan");
  Find<Button>(page.View,"SettingsSaveButton").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
  Console.WriteLine(backend=="vulkan"?"DEFAULT Vulkan through actual launcher settings and LaunchGame":"DEVELOPER OpenGL probe through launcher settings/StartInfo; child-only backend override");
  if(backend=="opengl")ui.Window.Hide();
  int duration=Number("seconds",120);Need(duration>=120 && duration<=600,"Duration outside 120..600 seconds");
  var settingsBefore=Directory.GetFiles(Path.Combine(root,"Settings")).ToDictionary(p=>p,p=>File.ReadAllBytes(p));
  var clock=Stopwatch.StartNew();
  var method=typeof(LauncherWindow).GetMethod("LaunchGame",BindingFlags.Instance|BindingFlags.NonPublic);
  var launch=backend=="vulkan"?(Task)method.Invoke(ui,new object[]{null}):LaunchProbe(backend);
  string liveLog=Directory.GetFiles(Path.Combine(root,"Logs"),"*.err").OrderBy(File.GetLastWriteTimeUtc).Last();
  if(recovery=="both-fail"){
   var done=await Task.WhenAny(launch,Task.Delay(30000));Need(done==launch,"Both-fail startup did not terminate within30s");await launch;
   string text=File.ReadAllText(liveLog);File.Copy(liveLog,Path.Combine(evidence,backend+".err"),true);
   Need(text.Contains("Vulkan and OpenGL initialization failed; no renderer is ready") && !text.Contains("[PRESENT]"),"Both-fail startup diagnostics/presentation incorrect");
   var session=Json.Object(File.ReadAllText(Path.Combine(root,".launcher/game-session.json")));
   Need(Convert.ToInt32(session["exit_code"])!=0,"Failed startup was reported as clean exit");
   foreach(var file in settingsBefore)Need(File.ReadAllBytes(file.Key).SequenceEqual(file.Value),"Failed startup changed saved settings: "+file.Key);
   Need(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_D3D8_BACKEND"]=="vulkan","Failed startup changed launcher default");
   File.WriteAllText(Path.Combine(evidence,backend+"-result.json"),Json.Encode(new {passed=true,backend=backend,launch_route="launcher",startup_outcome="both-fail",seconds=clock.Elapsed.TotalSeconds,exit_code=session["exit_code"],settings_preserved=true,source_log=liveLog}));
   Console.WriteLine("PASS actual launcher bounded both-fail startup and settings preservation");return;
  }
  Process game=null;
  while(game==null && clock.Elapsed.TotalSeconds<10){foreach(var p in Process.GetProcessesByName("futureperfect_recomp")){try{if(String.Equals(p.MainModule.FileName,Path.GetFullPath(Path.Combine(root,"Bin/futureperfect_recomp.exe")),StringComparison.OrdinalIgnoreCase)){game=p;break;}}catch{}p.Dispose();}await Task.Delay(100);}
  Need(game!=null,"Launcher did not create game process");owned=game;
  string pidPath=Path.Combine(evidence,"game-pid.txt");
  File.WriteAllText(pidPath+".tmp",game.Id.ToString());File.Move(pidPath+".tmp",pidPath);
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
   uint expectedScene=(uint)Number("expected_scene",2);
   int next=0;bool skipped=false,moving=false,released=false;int shot=0;int capture=0;uint firstTick=0,lastTick=0;var samples=new List<object>();var positions=new List<float[]>();int equipped=0;
   while(clock.Elapsed.TotalSeconds<duration){
    Need(!game.HasExited,"Game exited before bounded test ended");double t=clock.Elapsed.TotalSeconds;
    if(next<times.Length && t>=times[next]){await Key(hwnd,keys[next],holds[next]);Console.WriteLine(backend+" menu key "+keys[next]+" at "+t.ToString("F1"));next++;}
    if(firstPresentSeconds<0 && ReadLiveLog(liveLog).Contains("[PRESENT] backend="+expectedBackend))firstPresentSeconds=clock.Elapsed.TotalSeconds;
    if(Flag("display_cycle")){
     if(displayStage==0 && t>=60){await ToggleFullscreen(hwnd);displaySnapshots.Add(DisplaySnapshot(hwnd,"fullscreen",t));displayStage++;}
     else if(displayStage==1 && t>=70){await ToggleFullscreen(hwnd);Need(!Fullscreen(hwnd),"Window mode did not restore");displaySnapshots.Add(DisplaySnapshot(hwnd,"windowed",t));displayStage++;}
     else if(displayStage==2 && t>=100){Need(SetWindowPos(hwnd,IntPtr.Zero,0,0,1024,768,0x16),"Resize request failed");await Task.Delay(300);Rect client;Need(GetClientRect(hwnd,out client) && (client.R!=initialClient.R || client.B!=initialClient.B),"Actual client size did not change");displaySnapshots.Add(DisplaySnapshot(hwnd,"resized",t));displayStage++;}
     else if(displayStage==3 && t>=108){Need(SetWindowPos(hwnd,IntPtr.Zero,0,0,initialWindow.R-initialWindow.L,initialWindow.B-initialWindow.T,0x16),"Restore size request failed");await Task.Delay(300);Rect client;Need(GetClientRect(hwnd,out client) && client.R==initialClient.R && client.B==initialClient.B,"Client size did not restore");displaySnapshots.Add(DisplaySnapshot(hwnd,"restored",t));displayStage++;}
    }
    uint scene=Word(game,0x79094c),flags=Word(game,0x7de458),ticks=Word(game,0x7de318),player=Word(game,0x7b0c7c);
    if((!config.ContainsKey("skip_intro") || Flag("skip_intro")) && t>45 && (flags&0x20000000)!=0 && !skipped){await Key(hwnd,27,200);skipped=true;Console.WriteLine(backend+" skipped opening scene at "+t.ToString("F1"));}
    if(t>=85 && !moving){SetForegroundWindow(hwnd);SetKey(hwnd,87,true);moving=true;}
    if(t>=90 && !released){SetKey(hwnd,87,false);released=true;}
    if(t>=92+shot*4 && shot<5){if(probeInput!=null)await Key(hwnd,1,180);else {PostMessage(hwnd,0x201,new IntPtr(1),IntPtr.Zero);await Task.Delay(180);PostMessage(hwnd,0x202,IntPtr.Zero,IntPtr.Zero);}shot++;}
    if(t>=nextGpu){
     Need(new FileInfo(liveLog).Length<=4*1024*1024 && new FileInfo(Path.ChangeExtension(liveLog,".out")).Length<=4*1024*1024,"Game log exceeded per-stream 4 MiB budget");
     try{if(gpuCounters.Count==0)gpuCounters=GpuCounters(game.Id);if(gpuCounters.Count>0)gpuDedicated=gpuCounters.Sum(c=>(double)c.NextValue());}catch{gpuDedicated=null;}
     nextGpu=t+5;
    }
    float[] pos=null;uint weapon=0;game.Refresh();
    // Input holds, display changes and GPU-counter reads above can block.
    // Timestamp the observation itself, never the pre-input scheduler time.
    t=clock.Elapsed.TotalSeconds;
    double sampleQpcMs=Stopwatch.GetTimestamp()*1000.0/Stopwatch.Frequency;
    scene=Word(game,0x79094c);flags=Word(game,0x7de458);ticks=Word(game,0x7de318);player=Word(game,0x7b0c7c);
    if(scene==expectedScene && storySeconds<0)storySeconds=t;
    if(scene==expectedScene && player>=0x80000000 && player<0x84000000){pos=Position(game,player);weapon=Word(game,player+0x58c);if((expectedWeapon<0 || weapon==expectedWeapon) && (flags&0x20000000)==0 && gameplayReadySeconds<0)gameplayReadySeconds=t;if(t>=85 && t<=115 && (flags&0x20000000)==0){positions.Add(pos);if(expectedWeapon<0 || weapon==expectedWeapon)equipped++;if(firstTick==0)firstTick=ticks;lastTick=ticks;}}
    uint pausePointer=Word(game,0x761c48);bool paused=pausePointer!=0 && (Word(game,pausePointer)&1)!=0;
    samples.Add(new {seconds=Math.Round(t,2),clock_qpc_ms=sampleQpcMs,scene=scene,flags=flags,ticks=ticks,player=player,position=pos,weapon=weapon,paused=paused,cpu_total_ms=game.TotalProcessorTime.TotalMilliseconds,private_bytes=game.PrivateMemorySize64,working_set_bytes=game.WorkingSet64,handles=game.HandleCount,gpu_dedicated_bytes=gpuDedicated});
    if(Flag("capture") && capture<captureTimes.Length && t>=captureTimes[capture]){Foreground(hwnd);await Task.Delay(150);Capture(game,Path.Combine(evidence,backend+"-"+capture+".png"));capture++;}
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
   Need(GameSettings.StartInfo(root).EnvironmentVariables["RECOMP_D3D8_BACKEND"]=="vulkan","Game run changed launcher default");
   var model=GameSettings.Load(root,null);
   Need(model.Msaa==Number("msaa",1) && model.PostAntiAliasing==Number("aa",0) && model.Anisotropy==Number("anisotropy",1) && model.AmbientOcclusion==Number("ao",0),"Game run changed saved quality settings");
   string err=Directory.GetFiles(Path.Combine(root,"Logs"),"*.err").OrderBy(File.GetLastWriteTimeUtc).Last();string text=File.ReadAllText(err);File.Copy(err,Path.Combine(evidence,backend+".err"),true);
   Need(text.Contains("[PRESENT] backend="+expectedBackend),"Expected backend did not present");Need(!text.Contains("[CRASH]"),"Crash in game log");
   File.WriteAllText(Path.Combine(evidence,backend+"-result.json"),Json.Encode(new {passed=true,backend=backend,launch_route=backend=="vulkan"?"launcher":"developer-start-info",input_route=probeInput==null?"window-messages":"isolated-input-state",pid=game.Id,seconds=clock.Elapsed.TotalSeconds,window_seconds=windowSeconds,story_seconds=storySeconds,first_present_observed_seconds=firstPresentSeconds,gameplay_ready_seconds=gameplayReadySeconds,gpu_metric_available=gpuDedicated.HasValue,capture_enabled=Flag("capture"),display_cycle_completed=Flag("display_cycle") && displayStage==4,display_snapshots=displaySnapshots,settings=config,gameplay_samples=positions.Count,equipped_samples=equipped,tick_delta=lastTick-firstTick,exit_code=game.ExitCode,source_log=err}));
   Console.WriteLine("PASS "+backend+", observed "+expectedBackend+": "+(backend=="vulkan"?"actual launcher":"developer StartInfo probe")+", scenario gameplay, movement, weapon expectation, presents and clean exit; ticks="+(lastTick-firstTick));
  }finally{foreach(var counter in gpuCounters)counter.Dispose();if(!game.HasExited){game.Kill();game.WaitForExit(10000);}game.Dispose();owned=null;ui.Window.Close();}
 }
}
