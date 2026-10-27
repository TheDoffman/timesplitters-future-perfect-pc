using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
namespace TimeSplitters.Launcher
{
    public static class NativeHost
    {
        public const string XbeHash="3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc";
        const string CG="/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics";
        const string CF="/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation";
        [DllImport(CG)]static extern uint CGMainDisplayID();
        [DllImport(CG)]static extern IntPtr CGDisplayCopyAllDisplayModes(uint display,IntPtr options);
        [DllImport(CG)]static extern nuint CGDisplayModeGetPixelWidth(IntPtr mode);
        [DllImport(CG)]static extern nuint CGDisplayModeGetPixelHeight(IntPtr mode);
        [DllImport(CF)]static extern nint CFArrayGetCount(IntPtr array);
        [DllImport(CF)]static extern IntPtr CFArrayGetValueAtIndex(IntPtr array,nint index);
        [DllImport(CF)]static extern void CFRelease(IntPtr value);
        [DllImport(CG)]static extern IntPtr CGWindowListCopyWindowInfo(uint options,uint relativeToWindow);
        [DllImport(CF)]static extern IntPtr CFStringCreateWithCString(IntPtr allocator,string text,uint encoding);
        [DllImport(CF)]static extern IntPtr CFDictionaryGetValue(IntPtr dictionary,IntPtr key);
        [DllImport(CF)][return:MarshalAs(UnmanagedType.I1)]static extern bool CFNumberGetValue(IntPtr number,int type,out int value);
        public static bool GameWindowReady(int pid){
            IntPtr windows=CGWindowListCopyWindowInfo(17,0);if(windows==IntPtr.Zero)return false;
            IntPtr owner=CFStringCreateWithCString(IntPtr.Zero,"kCGWindowOwnerPID",0x08000100),layer=CFStringCreateWithCString(IntPtr.Zero,"kCGWindowLayer",0x08000100);
            try{for(nint i=0;i<CFArrayGetCount(windows);i++){var item=CFArrayGetValueAtIndex(windows,i);var p=CFDictionaryGetValue(item,owner);var l=CFDictionaryGetValue(item,layer);int process,level;if(p!=IntPtr.Zero&&l!=IntPtr.Zero&&CFNumberGetValue(p,3,out process)&&CFNumberGetValue(l,3,out level)&&process==pid&&level==0)return true;}return false;}
            finally{CFRelease(owner);CFRelease(layer);CFRelease(windows);}
        }
        public static IEnumerable<DisplayMode> DisplayModes()
        {
            var modes=new List<DisplayMode>();var array=CGDisplayCopyAllDisplayModes(CGMainDisplayID(),IntPtr.Zero);
            if(array==IntPtr.Zero)return modes;
            try { for(nint i=0;i<CFArrayGetCount(array);i++){var m=CFArrayGetValueAtIndex(array,i);modes.Add(new DisplayMode((int)CGDisplayModeGetPixelWidth(m),(int)CGDisplayModeGetPixelHeight(m)));} }
            finally{CFRelease(array);}return modes;
        }
        public static string Graphics() { return "Apple Silicon / native " + RuntimeInformation.OSDescription; }
        public static string Version(string root)
        {
            string value=Core.InstalledVersion(root);if(!String.IsNullOrEmpty(value))return value;
            var path=Core.PathIn(root,LauncherPlatform.ManifestPath(root));
            if(!File.Exists(path))return "Not installed";
            var m=Json.Object(File.ReadAllText(path));var revision=Json.String(m,"source_revision");
            return "Local build "+revision.Substring(0,Math.Min(7,revision.Length));
        }
        public static void CheckData(string root)
        {
            string data=Core.PathIn(root,LauncherPlatform.Data(root));var xbe=Core.PathIn(data,"default.xbe");
            Core.Require(File.Exists(xbe),"Import your supported US Xbox game data first.");
            Core.Require(Core.Hash(xbe)==XbeHash,"default.xbe does not match the supported US Xbox release.");
            foreach(var name in new[]{"pak","xmv"}) { var path=Core.PathIn(data,name);Core.Require(Directory.Exists(path)&&Directory.EnumerateFileSystemEntries(path).Any(),"Missing or empty game assets: "+name); }
        }
        public static Dictionary<string,object> CheckBuild(string root)
        {
            var manifest=Core.PathIn(root,LauncherPlatform.ManifestPath(root));Core.Require(File.Exists(manifest),"Native Mac build manifest is missing. Build the native runtime first.");
            var m=Json.Object(File.ReadAllText(manifest));var exe=Core.PathIn(root,LauncherPlatform.GamePath(root));Core.Require(File.Exists(exe),"Native Mac game executable is missing.");
            string expected=Json.String(m,"executable_sha256");
            if(expected.Length==0 && m.TryGetValue("files",out var files) && files is Dictionary<string,object> map)expected=Json.String(map,LauncherPlatform.GameBinary);
            Core.Require(expected.Length==64 && Core.Hash(exe)==expected,"Native executable differs from its build manifest. Rebuild or repair before playing.");
            using(var stream=File.OpenRead(exe)){var header=new byte[8];Core.Require(stream.Read(header,0,8)==8 && BitConverter.ToUInt32(header,0)==0xfeedfacf && BitConverter.ToUInt32(header,4)==0x0100000c,"This launcher requires a native Apple Silicon game executable.");}
            if(Json.String(m,"renderer")=="vulkan")GpuEnvironment(root,m);
            return m;
        }
        static Dictionary<string,object> GpuEnvironment(string root,Dictionary<string,object> manifest)
        {
            if(!LauncherPlatform.Checkout(root))return PortableGpuEnvironment(root,manifest);
            string python=Path.Combine(root,".venv/bin/python");
            Core.Require(LauncherPlatform.Checkout(root) && File.Exists(python) && File.Exists(Core.PathIn(root,"tools/build/vulkan_sdk.py")),"This Vulkan build needs its pinned checkout SDK and verification helper.");
            var info=new ProcessStartInfo(python){UseShellExecute=false,RedirectStandardOutput=true,WorkingDirectory=root};
            // The helper needs driver metadata only, not source hashes or build logs.
            var selection=new Dictionary<string,object>();
            if(manifest.TryGetValue("vulkan_driver",out var driver))selection["vulkan_driver"]=driver;
            info.ArgumentList.Add("-c");info.ArgumentList.Add("import pathlib,sys,json; sys.dont_write_bytecode=True; sys.path.insert(0,str(pathlib.Path(sys.argv[1])/'tools/build'))\ntry:\n import vulkan_sdk; print(json.dumps(vulkan_sdk.launch_environment(pathlib.Path(sys.argv[1]),json.loads(sys.argv[2]))))\nexcept Exception as error:\n print(str(error)); sys.exit(1)");info.ArgumentList.Add(root);info.ArgumentList.Add(Json.Encode(selection));
            return Json.Object(ReadGpuHelper(info));
        }
        internal static Dictionary<string,object> PortableGpuEnvironment(string root,Dictionary<string,object> manifest)
        {
            Core.Require(Json.String(manifest,"vulkan_driver")=="moltenvk","Unsupported packaged Vulkan driver.");
            Core.Require(manifest.TryGetValue("files",out var files)&&files is Dictionary<string,object>,"Packaged runtime hashes are missing.");
            var hashes=(Dictionary<string,object>)files;
            var required=new[]{"Bin/libvulkan.1.dylib","Bin/libMoltenVK.dylib","Bin/libdxcompiler.dylib","Support/Vulkan/MoltenVK_icd.json"};
            foreach(var name in required.Concat(hashes.Keys.Where(n=>n.StartsWith("Bin/",StringComparison.Ordinal)&&n.EndsWith(".dylib",StringComparison.Ordinal))).Distinct()){
                string path=Core.PathIn(root,name),expected=Json.String(hashes,name);
                Core.Require(expected.Length==64&&File.Exists(path)&&Core.Hash(path)==expected,"Packaged Vulkan runtime is missing or changed: "+name);
            }
            var icd=Json.Object(File.ReadAllText(Core.PathIn(root,required[3])));
            Core.Require(icd.TryGetValue("ICD",out var entry)&&entry is Dictionary<string,object> && Json.String((Dictionary<string,object>)entry,"library_path")=="../../Bin/libMoltenVK.dylib","Packaged Vulkan driver path is invalid.");
            return new Dictionary<string,object>{{"DYLD_LIBRARY_PATH",Core.PathIn(root,"Bin")},{"SDL_VULKAN_LIBRARY",Core.PathIn(root,required[0])},{"VK_DRIVER_FILES",Core.PathIn(root,required[3])},{"RECOMP_SHADER_CACHE","0"}};
        }
        internal static string ReadGpuHelper(ProcessStartInfo info,int timeout=30000)
        {
            // One redirected pipe avoids the ARM64 dual-pipe initialization crash.
            // The controlled Python helper reports failures on stdout as well.
            Core.Require(info.RedirectStandardOutput&&!info.RedirectStandardError,"SDK helper requires one output pipe.");
            using(var process=Process.Start(info)){
                string output="";Exception readError=null;
                var reader=new Thread(()=>{try{
                    var text=new System.Text.StringBuilder();var buffer=new char[2048];int count;
                    while((count=process.StandardOutput.Read(buffer,0,buffer.Length))>0)
                        if(text.Length<16384)text.Append(buffer,0,Math.Min(count,16384-text.Length));
                    output=text.ToString();
                }catch(Exception e){readError=e;}}){IsBackground=true};
                reader.Start();bool exited=process.WaitForExit(timeout);
                if(!exited){process.Kill(true);process.WaitForExit(3000);}
                bool drained=reader.Join(3000);
                Core.Require(exited,"Pinned Vulkan SDK verification timed out.");
                Core.Require(drained&&readError==null,"Could not read pinned Vulkan SDK verification output.");
                Core.Require(process.ExitCode==0,"Pinned Vulkan SDK verification failed: "+output);
                return output;
            }
        }

        public static void Configure(ProcessStartInfo info,string root)
        {
            info.WorkingDirectory=root;
            foreach(var key in info.EnvironmentVariables.Keys.Cast<string>().ToArray())if(key.StartsWith("VK_")||key.StartsWith("SDL_VULKAN")||key.StartsWith("DYLD_"))info.EnvironmentVariables.Remove(key);
            string manifest=Core.PathIn(root,LauncherPlatform.ManifestPath(root));
            if(File.Exists(manifest)){
                var build=Json.Object(File.ReadAllText(manifest));
                if(Json.String(build,"renderer")=="vulkan")
                    foreach(var pair in GpuEnvironment(root,build))info.EnvironmentVariables[pair.Key]=(string)pair.Value;
            }
            string settings=LauncherPlatform.Checkout(root)?"private/native-settings":"Settings";
            info.EnvironmentVariables["XDG_CONFIG_HOME"]=Core.PathIn(root,settings+"/xdg-config");
            info.EnvironmentVariables["XDG_DATA_HOME"]=Core.PathIn(root,settings+"/xdg-data");
            info.EnvironmentVariables["TSFP_ONLINE_SETTINGS"]=Core.PathIn(root,LauncherPlatform.Checkout(root)?"config/pc-online.json":"Settings/pc-online.json");
            info.EnvironmentVariables.Remove("RECOMP_FFMPEG_DIR");
            info.EnvironmentVariables["RECOMP_LOG_FILE"]=Core.PathIn(root,"logs/native-play.err");
        }
        public static void Prepare(string root)
        {
            var files=LauncherPlatform.Settings(root);
            foreach(var path in new[]{LauncherPlatform.Saves(root),Path.GetDirectoryName(files[2]),"logs"})Directory.CreateDirectory(Core.PathIn(root,path));
            string controls=Core.PathIn(root,files[0]);Directory.CreateDirectory(Path.GetDirectoryName(controls));
            if(!File.Exists(controls))using(var src=typeof(NativeHost).Assembly.GetManifestResourceStream("launcher-controls-default.json"))using(var dst=new FileStream(controls,FileMode.CreateNew))src.CopyTo(dst);
            string runtime=Core.PathIn(root,files[2]);if(!File.Exists(runtime))using(var writer=new StreamWriter(new FileStream(runtime,FileMode.CreateNew)))writer.WriteLine("# Native runtime settings\nframe_interp = 1\nframe_target = 0\nframe_cap = 60\nrefresh_rate = 0");
        }
        public static async Task Drain(Stream input,string path)
        {
            using(var output=new FileStream(path,FileMode.Create,FileAccess.Write,FileShare.Read)){var buffer=new byte[65536];long budget=4*1024*1024;int count;
                while((count=await input.ReadAsync(buffer,0,buffer.Length))>0){int keep=(int)Math.Min(budget,count);if(keep>0)await output.WriteAsync(buffer,0,keep);budget-=keep;}}
        }
        public static VerificationResult Verify(string root,CancellationToken token)
        {
            if(!LauncherPlatform.Checkout(root))return InstallationMaintenance.Verify(root,false,null,token);
            var result=new VerificationResult{Component="Native game",Version=Version(root)};
            try { token.ThrowIfCancellationRequested();CheckBuild(root);result.Checked=1;result.ManifestHash=Core.Hash(Core.PathIn(root,LauncherPlatform.ManifestPath(root))); }
            catch(Exception ex){result.Issues.Add(ex.Message);}return result;
        }
        public static void Open(string target) { var info=new ProcessStartInfo("/usr/bin/open"){UseShellExecute=false};info.ArgumentList.Add(target);Process.Start(info); }
    }
}
