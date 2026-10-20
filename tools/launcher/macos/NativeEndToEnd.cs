using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
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
                string runtime=Path.GetFullPath(Json.String(plan,"runtime_root"));string scope=Core.PathIn(runtime,"private/test-saves/native-mac");
                Core.Require(Path.GetFullPath(root)==Core.PathIn(scope,"launcher-session"),"Launcher tests require the isolated native runner root.");
                output=Core.PathIn(root,"result.json");NativeHost.CheckData(runtime);NativeHost.CheckBuild(runtime);
                var info=GameSettings.StartInfo(runtime);var env=(Dictionary<string,object>)plan["environment"];
                // Validate production launcher routing before applying isolated test paths.
                foreach(var key in new[]{"VK_DRIVER_FILES","SDL_VULKAN_LIBRARY","VK_LAYER_PATH","DYLD_LIBRARY_PATH"})
                    if(env.TryGetValue(key,out var expected))Core.Require(info.Environment.TryGetValue(key,out var actual)&&actual==Convert.ToString(expected),"Launcher GPU routing differs from manifest: "+key);
                foreach(var key in info.Environment.Keys.ToArray())if(key.StartsWith("RECOMP_")||key.StartsWith("TSFP_")||key.StartsWith("XDG_"))info.Environment.Remove(key);
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
                Core.Require(lastWindowObserved,"Native game window was never observed.");Core.Require(minimized&&restored,"Launcher window did not minimize and restore.");Core.Require(exit==(mode=="quit"?0:134),"Unexpected game exit: "+exit);
                Core.Write(output,Json.Encode(new{passed=true,mode,exit,minimized,restored,gameWindowObserved=lastWindowObserved,driver=info.Environment.TryGetValue("VK_DRIVER_FILES",out var icd)?icd:null,report=latest}));Close();
            }catch(Exception e){if(output!=null)Core.Write(output,Json.Encode(new{passed=false,error=e.ToString()}));Console.Error.WriteLine(e);Environment.ExitCode=1;gameRunning=false;Close();}
        }
    }
}
