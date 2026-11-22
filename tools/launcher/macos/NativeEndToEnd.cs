using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.LogicalTree;
namespace TimeSplitters.Launcher
{
    public sealed partial class NativeWindow
    {
        // Invoked only by the bounded isolated runner. It exercises the normal
        // monitor and UI lifecycle, never Prepare or personal saves/settings.
        async Task TestSession()
        {
            string output=null;
            try{
                int option=Array.IndexOf(args,"--test-session");var plan=Json.Object(File.ReadAllText(args[option+1]));
                string runtime=Path.GetFullPath(Json.String(plan,"runtime_root"));
#if LINUX
                string scope=Core.PathIn(runtime,"private/test-saves/native-linux");
#else
                string scope=Core.PathIn(runtime,"private/test-saves/native-mac");
#endif
                Core.Require(Path.GetFullPath(root)==Core.PathIn(scope,"launcher-session"),"Launcher tests require the isolated native runner root.");
                output=Core.PathIn(root,"result.json");string renderer=Json.String(plan,"renderer");bool selector=renderer.Length!=0;
                string launchRoot=selector?root:runtime;
                NativeHost.CheckData(launchRoot);
                if(selector){
                    Core.Require(GameSettings.Renderers.Contains(renderer),"Invalid test renderer.");
                    ShowSettings();settings.SelectTab(1);
                    var picker=settings.View.GetLogicalDescendants().OfType<ComboBox>().Single(c=>c.Name=="GraphicsRenderer");
                    int selected=Array.IndexOf(GameSettings.Renderers,renderer);
                    Core.Require(picker.SelectedIndex!=selected,"Selector test must switch from the opposite saved renderer.");
                    picker.SelectedIndex=selected;
                    var save=settings.View.GetLogicalDescendants().OfType<Button>().Single(b=>b.Name=="SettingsSaveButton");
                    save.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                    Core.Require(GameSettings.Load(root,null).Renderer==renderer&&!save.IsEnabled,"Renderer selection was not saved.");
                }
                NativeHost.CheckBuild(launchRoot,selector?renderer:null);
                var info=GameSettings.StartInfo(launchRoot);var env=(Dictionary<string,object>)plan["environment"];
                if(selector)Core.Require(info.Environment["RECOMP_D3D8_BACKEND"]==renderer&&!env.ContainsKey("RECOMP_D3D8_BACKEND"),"Test environment must not replace the launcher renderer.");
                // Validate production launcher routing before applying isolated test paths.
                foreach(var key in new[]{"VK_DRIVER_FILES","SDL_VULKAN_LIBRARY","VK_LAYER_PATH","DYLD_LIBRARY_PATH"})
                    if(!selector&&env.TryGetValue(key,out var expected))Core.Require(info.Environment.TryGetValue(key,out var actual)&&actual==Convert.ToString(expected),"Launcher GPU routing differs from manifest: "+key);
                foreach(var key in info.Environment.Keys.ToArray())if((key.StartsWith("RECOMP_")||key.StartsWith("TSFP_")||key.StartsWith("XDG_"))&&!(selector&&key=="RECOMP_D3D8_BACKEND"))info.Environment.Remove(key);
                foreach(var pair in env)info.Environment[pair.Key]=Convert.ToString(pair.Value);
                foreach(var key in new[]{"RECOMP_SAVE_DIR","RECOMP_INPUT_CONFIG","RECOMP_DISPLAY_CONFIG","TSFP_DISPLAY_CONFIG","TSFP_DISCORD_CONFIG","TSFP_CRASH_DIR","XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME","TSFP_NATIVE_TEST_KEYS","RECOMP_LOG_FILE"}){
                    string value=Path.GetFullPath(info.Environment[key]);Core.Require(value.StartsWith(scope+Path.DirectorySeparatorChar,StringComparison.Ordinal)||value==scope,"Test route escaped isolation: "+key);Core.PathIn(scope,Path.GetRelativePath(scope,value));
                }
                Core.Require(File.Exists(Core.PathIn(scope,"run.lock")),"The bounded native runner must hold the test lock.");
                string mode=Json.String(plan,"mode");Core.Require(mode=="quit"||mode=="crash","Invalid test termination mode.");int seconds=Convert.ToInt32(plan["seconds"]);Core.Require(seconds>=3 && seconds<=120,"Invalid test duration.");
                var monitor=MonitorGame(info,root);await Task.Delay(700);bool minimized=WindowState==WindowState.Minimized;
                await Task.Delay(seconds*1000);File.AppendAllText(info.Environment["TSFP_NATIVE_TEST_KEYS"],mode+"\n");
                int exit=await monitor.WaitAsync(TimeSpan.FromSeconds(20));bool restored=WindowState==WindowState.Normal;
                string latest=CrashReports.Latest(root);if(mode=="crash"){Core.Require(latest!=null&&!CrashReports.IsUnexpectedExit(root,latest),"Missing native fatal report.");CrashReports.Export(root,latest,Core.PathIn(root,"export.zip"));}
                bool requireWindowObservation=true;
#if LINUX
                // Wayland cannot query another process's windows. This new
                // selector probe proves saved routing/rendering/lifecycle;
                // it does not approve update recovery or visual acceptance.
                if(selector)requireWindowObservation=false;
#endif
                if(requireWindowObservation)Core.Require(lastWindowObserved,"Native game window was never observed.");
                bool rendered=!selector||File.ReadAllText(Core.PathIn(root,"logs/native-play.err")).Contains("[PRESENT] backend="+renderer);
                Core.Require(rendered,"Selected renderer never presented a game frame.");
                Core.Require(minimized&&restored,"Launcher window did not minimize and restore.");Core.Require(exit==(mode=="quit"?0:134),"Unexpected game exit: "+exit);
                if(selector)Core.Require(GameSettings.Load(root,null).Renderer==renderer,"Saved renderer changed during gameplay.");
                Core.Write(output,Json.Encode(new{passed=true,mode,exit,minimized,restored,renderer,selector,rendered,gameWindowObservationRequired=requireWindowObservation,gameWindowObserved=lastWindowObserved,driver=info.Environment.TryGetValue("VK_DRIVER_FILES",out var icd)?icd:null,report=latest}));Close();
            }catch(Exception e){if(output!=null)Core.Write(output,Json.Encode(new{passed=false,error=e.ToString()}));Console.Error.WriteLine(e);Environment.ExitCode=1;gameRunning=false;Close();}
        }
    }
}
