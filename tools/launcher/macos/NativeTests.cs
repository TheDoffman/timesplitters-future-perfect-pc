using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
namespace TimeSplitters.Launcher
{
    public static class NativeTests
    {
        static int count;static string sandbox;
        static void Need(bool test,string message){if(!test)throw new Exception(message);}
        static void Reject(Action action){bool rejected=false;try{action();}catch(InvalidDataException){rejected=true;}catch(IOException){rejected=true;}catch(UnauthorizedAccessException){rejected=true;}Need(rejected,"Expected rejection");}
        static void Test(string name,Action action){action();count++;Console.WriteLine("PASS "+name);}
        static string NewRoot(string name){var root=Path.Combine(sandbox,name);Directory.CreateDirectory(root);return root;}
        static string Hash(byte[] b){return Convert.ToHexString(SHA256.HashData(b)).ToLowerInvariant();}
        static void Write(string root,string name,string value){var path=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,value);}
        static string Archive(string root,string version,bool corrupt=false,bool launcher=false,bool textures=false)
        {
            var files=launcher?new Dictionary<string,string>{{Core.LauncherName,"native launcher"},{Core.AppPath,"plist"}}:new Dictionary<string,string>{{LauncherPlatform.GameBinary,"native game "+version},{LauncherPlatform.LaunchScript,"#!/bin/sh"},{"Settings/pc-controls.json","defaults"},{"Licenses/example.txt","license"}};
            if(textures) {files["Textures/0123456789abcdef.dds"]="new texture";files["Textures/0123456789abcdef.json"]="new metadata";files["Textures/abcdef0123456789.dds"]="second texture";}
            var hashes=files.ToDictionary(p=>p.Key,p=>(object)Hash(Encoding.UTF8.GetBytes(p.Value)));
            string manifest=Json.Encode(new {schema=1,version=version,files=hashes});var path=Path.Combine(root,Guid.NewGuid()+".zip");
            using(var zip=ZipFile.Open(path,ZipArchiveMode.Create)){foreach(var pair in files){using(var writer=new StreamWriter(zip.CreateEntry(pair.Key).Open()))writer.Write(pair.Value+(corrupt?"corrupt":""));}using(var writer=new StreamWriter(zip.CreateEntry(launcher?Core.SelfManifest:"BUILD.json").Open()))writer.Write(manifest);}
            return path;
        }
        static string Metadata(string suffix,string tag="v1.0.0",bool preview=false)
        {
            string name="TimeSplitters-Future-Perfect-PC-"+tag+"-"+suffix+".zip";
            return Json.Encode(new[]{new{tag_name=tag,prerelease=preview,draft=false,assets=new[]{new{name=name,state="uploaded",size=10,digest="sha256:"+new string('a',64),browser_download_url="https://github.com/"+ReleaseClient.Repository+"/releases/download/"+tag+"/"+name}}}});
        }
        sealed class Handler:HttpMessageHandler { public Func<HttpRequestMessage,HttpResponseMessage> Reply;protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage r,CancellationToken t){var response=Reply(r);response.RequestMessage=r;return Task.FromResult(response);} }
        static void GamePruningTest()
        {
            Need(Core.Allowed("Textures/0123456789abcdef.dds") && Core.PreserveGameFile("Textures/0123456789abcdef.dds"), "Bundled texture install/preservation missing");
            Need(!Core.Allowed("Textures/game.exe") && !Core.Allowed("Textures/sub/0123456789abcdef.dds"), "Unsafe texture path accepted");
            foreach (string name in new[] { Core.LauncherName, Core.AppPath, Core.SelfManifest })
                Need(Core.PreserveGameFile(name.ToLowerInvariant()) && Core.PreserveGameFile(name.ToUpperInvariant()), "Launcher casing lost cleanup protection");
            var root = NewRoot("game-pruning");
            Core.Apply(root, Core.Unpack(Archive(root, "v1.0.0"), Path.Combine(root, "initial"), "v1.0.0"), null);
            var metadata = Json.Object(File.ReadAllText(Core.PathIn(root, "BUILD.json")));
            var files = (Dictionary<string, object>)metadata["files"];
            var preserved = new[] { Core.LauncherName, Core.AppPath, Core.SelfManifest,
                "TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources",
                "TimeSplitters Launcher.app/Contents/MacOS/host.dylib",
                "TimeSplitters Launcher.app/Contents/Resources/Licenses/notice.txt" };
            foreach (var name in preserved.Concat(new[] { "Support/retired.txt" })) {
                Write(root, name, "old package file"); files[name] = Hash(Encoding.UTF8.GetBytes("old package file"));
            }
            Write(root, "BUILD.json", Json.Encode(metadata));
            string pending = Core.PathIn(root, ".launcher/pending"); Directory.CreateDirectory(pending);
            Core.Unpack(Archive(root, "v1.1.0"), Path.Combine(pending, "payload"), "v1.1.0");
            Write(root, ".launcher/pending/release.json", "{\"version\":\"v1.1.0\"}");
            Core.InstallPending(root);
            foreach (var name in preserved) Need(File.ReadAllText(Core.PathIn(root, name)) == "old package file", "Game cleanup removed launcher file: " + name);
            Need(!File.Exists(Core.PathIn(root, "Support/retired.txt")), "Retired game file retained");
            Core.Rollback(root);
            Need(Core.InstalledVersion(root) == "v1.0.0" && File.ReadAllText(Core.PathIn(root, "Support/retired.txt")) == "old package file", "Game cleanup rollback failed");
        }
        static void BundledTexturesTest()
        {
            var root=NewRoot("bundled-textures");
            Write(root,"Textures/0123456789abcdef.dds","artist texture");
            Write(root,"Settings/pc-controls.json","personal settings");
            var changes=Core.Unpack(Archive(root,"v1.0.0",textures:true),Path.Combine(root,".launcher/pending/payload"),"v1.0.0");
            Core.Apply(root,changes,null);
            Need(File.ReadAllText(Core.PathIn(root,"Textures/0123456789abcdef.dds"))=="artist texture","Existing DDS replaced");
            Need(!File.Exists(Core.PathIn(root,"Textures/0123456789abcdef.json")),"Existing pair received mismatched metadata");
            Need(File.ReadAllText(Core.PathIn(root,"Textures/abcdef0123456789.dds"))=="second texture","Missing bundled texture not installed");
            Need(File.ReadAllText(Core.PathIn(root,"Settings/pc-controls.json"))=="personal settings","Personal controls replaced");
            Core.DiscardGameStaging(root);
            Need(!Directory.Exists(Core.PathIn(root,".launcher/pending")),"Texture staging retained");
            Need(File.Exists(Core.PathIn(root,"Textures/abcdef0123456789.dds")),"Installed texture removed during cleanup");
        }
        static bool BundleSignatureValid(string root)
        {
            var start=new ProcessStartInfo("/usr/bin/codesign"){UseShellExecute=false};
            foreach(var arg in new[]{"--verify","--deep","--strict","--verbose=2",Path.Combine(root,"TimeSplitters Launcher.app")})start.ArgumentList.Add(arg);
            using(var process=Process.Start(start)){
                if(!process.WaitForExit(20000)){process.Kill(true);throw new Exception("Bundle signature verification timed out");}
                return process.ExitCode==0;
            }
        }
        public static void LocationUi()
        {
            string translocated = "/private/var/folders/test/T/AppTranslocation/" + Guid.NewGuid() + "/d";
            var window = NativeApplication.StartupWindow(translocated, Array.Empty<string>());
            try {
                Need(window is NativeLocationWindow, "Translocated startup reached the updater or main window");
                var content = (Avalonia.Controls.StackPanel)((Avalonia.Controls.Border)window.Content).Child;
                Need(content.Children.OfType<Avalonia.Controls.Button>().Any(b => b.IsEnabled &&
                    Convert.ToString(b.Content) == "Choose installation folder…"), "Recovery has no usable folder picker");
                Need(content.Children.OfType<Avalonia.Controls.TextBlock>().Any(t => t.Text == NativeInstallation.Guidance),
                    "Recovery instructions missing");
                Need(!Directory.Exists(translocated), "Recovery created a temporary installation");
                Console.WriteLine("PASS native UI: translocated startup offers recovery before any installation writes");
            } finally { window.Close(); }
        }
        static void LocationTests()
        {
            string translocated = "/private/var/folders/k1/test/T/AppTranslocation/" + Guid.NewGuid() + "/d";
            string bundle = Path.Combine(translocated, "TimeSplitters Launcher.app/Contents/MacOS");
            Need(Program.FindRoot(Array.Empty<string>(), bundle) == translocated, "Wrong translocated root");
            Need(NativeInstallation.Translocated(translocated) && NativeInstallation.Translocated(bundle), "Translocation not detected");
            Need(!NativeInstallation.Translocated("/Users/example/AppTranslocation/ordinary/d"), "Ordinary folder rejected");
            var portable = NewRoot("portable-location");
            Need(!NativeInstallation.Translocated(portable), "Portable root rejected");
            Need(Program.FindRoot(new[] { "--root", portable }, bundle) == portable, "Explicit root ignored");
            const string standalone = "/Users/example/TimeSplitters";
            Need(Program.FindRoot(Array.Empty<string>(), Path.Combine(standalone, "TimeSplitters Launcher.app/Contents/MacOS")) == standalone,
                "Portable package root changed");
            var checkout = NewRoot("checkout-location"); Write(checkout, "tools/play-native-mac.py", "fixture");
            Need(Program.FindRoot(Array.Empty<string>(), Path.Combine(checkout, "build/launcher/Test.app/Contents/MacOS")) == checkout,
                "Checkout discovery changed");
            Reject(() => NativeInstallation.ValidateFolder(translocated, Environment.ProcessPath));
            Need(!Directory.Exists(translocated), "Rejected translocation created files");

            var root = NewRoot("selected-location");
            var changes = SelfUpdate.Unpack(Archive(root, LauncherVersion.Value, launcher:true), Path.Combine(root, "payload"), LauncherVersion.Value);
            Core.ApplyLauncher(root, changes, null);
            Write(root, "Saves/keep", "save"); Write(root, "Settings/keep", "settings"); Write(root, "Data/keep", "game");
            var before = Directory.GetFiles(root, "*", SearchOption.AllDirectories).ToDictionary(p => p, Core.Hash);
            string executable = Core.PathIn(root, Core.LauncherName);
            Need(NativeInstallation.ValidateFolder(root, executable) == root, "Valid installation rejected");
            var after = Directory.GetFiles(root, "*", SearchOption.AllDirectories).ToDictionary(p => p, Core.Hash);
            Need(before.Count == after.Count && before.All(p => after[p.Key] == p.Value), "Selection changed existing files or left a probe");
            string unrelated = NewRoot("unrelated-location");
            Reject(() => NativeInstallation.ValidateFolder(unrelated, executable));
            Need(!Directory.EnumerateFileSystemEntries(unrelated).Any(), "Rejected folder was modified");
            string other = Path.Combine(unrelated, "different-build"); File.WriteAllText(other, "different binary");
            Reject(() => NativeInstallation.ValidateFolder(root, other));
            File.AppendAllText(Core.PathIn(root, Core.AppPath), "changed");
            Reject(() => NativeInstallation.ValidateFolder(root, executable));
            File.WriteAllText(Core.PathIn(root, Core.AppPath), "plist");
            string metadata = Core.PathIn(root, ".launcher");
            var rootMode = File.GetUnixFileMode(root);
            try {
                File.SetUnixFileMode(root, UnixFileMode.UserRead | UnixFileMode.UserExecute);
                Reject(() => NativeInstallation.ValidateFolder(root, executable));
            } finally { File.SetUnixFileMode(root, rootMode); }
            var mode = File.GetUnixFileMode(metadata);
            try {
                File.SetUnixFileMode(metadata, UnixFileMode.UserRead | UnixFileMode.UserExecute);
                Reject(() => NativeInstallation.ValidateFolder(root, executable));
            } finally { File.SetUnixFileMode(metadata, mode); }
            string linked = Path.Combine(sandbox, "linked-location"); Directory.CreateSymbolicLink(linked, root);
            try { Reject(() => NativeInstallation.ValidateFolder(linked, executable)); }
            finally { Directory.Delete(linked); }
            Need(File.ReadAllText(Core.PathIn(root, "Saves/keep")) == "save" &&
                File.ReadAllText(Core.PathIn(root, "Settings/keep")) == "settings" &&
                File.ReadAllText(Core.PathIn(root, "Data/keep")) == "game", "Recovery changed personal data");
        }
        public static int VerifyPackage(string archive,string version)
        {
            string test=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(archive)),"package-check-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(test);
            try {
                var changes=SelfUpdate.Unpack(archive,Path.Combine(test,"payload"),version);string install=Path.Combine(test,"install");Directory.CreateDirectory(install);Write(install,"BUILD.json","preserved game metadata");Write(install,"Saves/keep","preserved save");Core.ApplyLauncher(install,changes,null);Need(SelfUpdate.Complete(install),"Packaged app did not verify after installation");
                Need(NativeInstallation.ValidateFolder(install, Environment.ProcessPath) == install, "Packaged installation cannot be selected from translocation recovery");
                Need(BundleSignatureValid(install),"Installed app bundle signature is invalid");
                string seal=Core.PathIn(install,"TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources");
                byte[] sealBytes=File.ReadAllBytes(seal);File.Delete(seal);
                Need(!BundleSignatureValid(install),"Bundle signature accepted a missing resource seal");
                File.WriteAllBytes(seal,sealBytes);
                string pending=Core.PathIn(install,".launcher/self-pending");Directory.CreateDirectory(pending);SelfUpdate.Unpack(archive,Path.Combine(pending,"payload"),version);Write(install,".launcher/self-pending/release.json",Json.Encode(new{version=version,repair=true}));
                string helper=Core.PathIn(install,".launcher/mac-self-helper");File.Copy(Core.PathIn(install,Core.LauncherName),helper);File.SetUnixFileMode(helper,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
                File.AppendAllText(Core.PathIn(install,Core.AppPath),"modified fixture");
                Need(!BundleSignatureValid(install),"Bundle signature accepted a modified Info.plist");
                using(var parent=Process.Start(new ProcessStartInfo("/bin/sleep","1"){UseShellExecute=false})){
                    var start=new ProcessStartInfo(helper){UseShellExecute=false};start.ArgumentList.Add("--test-self-helper");start.ArgumentList.Add(install);start.ArgumentList.Add(parent.Id.ToString());
                    using(var child=Process.Start(start)){Need(child.WaitForExit(40000),"Self-update helper timed out");Need(child.ExitCode==0,"Self-update helper failed");}
                }
                Need(SelfUpdate.Complete(install),"Self repair did not restore the modified bundle");Need(File.Exists(Core.PathIn(install,"self-update-preview.png")),"Updated app did not open and render");Need(!Directory.Exists(pending),"Completed staging retained");
                Need(BundleSignatureValid(install),"Self repair did not restore the app bundle signature");
                Need(File.ReadAllText(Path.Combine(install,"Saves/keep"))=="preserved save"&&File.ReadAllText(Path.Combine(install,"BUILD.json"))=="preserved game metadata","Package touched personal/game files");
                Console.WriteLine("Packaged Mac app installed, repaired through a copied helper, restarted and rendered; game metadata and saves preserved.");

                Core.Apply(install,Core.Unpack(Archive(test,"v1.0.0"),Path.Combine(test,"initial-game"),"v1.0.0"),null);
                Write(install,"Settings/pc-controls.json","personal bindings");Write(install,"Data/keep","personal data");
                var bundled=changes.ToDictionary(change=>change.Name,change=>File.ReadAllBytes(change.Source));
                LauncherRestartChecks.Stage(install,"v1.1.0",bundled);
                // An incomplete equal-version launcher must be replaced by the
                // bundled app, reproducing the game-update replacement boundary.
                File.AppendAllText(Core.PathIn(install,Core.AppPath),"old installed fixture");
                string gameHelper=Core.PathIn(install,".launcher/mac-update-helper");
                LauncherPlatform.CopyFile(Core.PathIn(install,Core.LauncherName),gameHelper);
                File.SetUnixFileMode(gameHelper,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
                using(var parent=Process.Start(new ProcessStartInfo("/bin/sleep","2"){UseShellExecute=false})){
                    var start=new ProcessStartInfo(gameHelper){UseShellExecute=false};
                    foreach(var arg in new[]{"--test-game-helper",install,parent.Id.ToString()})start.ArgumentList.Add(arg);
                    using(var child=Process.Start(start)){
                        Thread.Sleep(250);
                        Need(!parent.HasExited&&Core.InstalledVersion(install)=="v1.0.0","Game update did not wait for the old process");
                        if(!child.WaitForExit(40000)){child.Kill(true);child.WaitForExit(3000);throw new Exception("Game-update helper timed out");}
                        Need(child.ExitCode==0,"Game-update helper failed");
                    }
                }
                Need(Core.InstalledVersion(install)=="v1.1.0"&&SelfUpdate.Complete(install)&&BundleSignatureValid(install),"Game update did not replace the bundled launcher");
                Need(File.Exists(Core.PathIn(install,"game-update-preview.png")),"Game update did not reopen the installed app");
                Need(!Directory.Exists(Core.PathIn(install,".launcher/pending")),"Game update retained completed staging");
                Need(File.ReadAllText(Core.PathIn(install,"Settings/pc-controls.json"))=="personal bindings"&&File.ReadAllText(Core.PathIn(install,"Data/keep"))=="personal data"&&File.ReadAllText(Core.PathIn(install,"Saves/keep"))=="preserved save","Game update changed personal files");
                Console.WriteLine("Packaged Mac game update waited for the parent, replaced the bundled launcher, restarted and rendered with personal files preserved.");return 0;
            }
            catch(Exception e){Console.Error.WriteLine(e);return 1;}
            finally{Directory.Delete(test,true);}
        }
        public static int VerifyGamePackage(string archive,string version)
        {
            string test=Path.Combine(Path.GetDirectoryName(Path.GetFullPath(archive)),"game-package-check-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(test);
            try {
                foreach(bool preserve in new[]{false,true}){
                    string root=Path.Combine(test,preserve?"existing":"fresh"),pending=Path.Combine(root,".launcher/pending");Directory.CreateDirectory(pending);
                    if(preserve){Write(root,"Data/keep","game data");Write(root,"Saves/keep","save");Write(root,"Settings/pc-controls.json","personal controls");Write(root,"Settings/pc-display.ini","personal display");}
                    Core.Unpack(archive,Path.Combine(pending,"payload"),version);Write(pending,"release.json",Json.Encode(new{version=version}));
                    Core.InstallPending(root);
                    var manifest=NativeHost.CheckBuild(root);var files=(Dictionary<string,object>)manifest["files"];
                    foreach(var pair in files){if(preserve&&pair.Key=="Settings/pc-controls.json")continue;Need(Core.Hash(Core.PathIn(root,pair.Key))==(string)pair.Value,"Installed package file changed: "+pair.Key);}
                    Need(SelfUpdate.Complete(root)&&BundleSignatureValid(root),"Bundled launcher failed installation/signature checks");
                    if(preserve)Need(File.ReadAllText(Core.PathIn(root,"Data/keep"))=="game data"&&File.ReadAllText(Core.PathIn(root,"Saves/keep"))=="save"&&File.ReadAllText(Core.PathIn(root,"Settings/pc-controls.json"))=="personal controls"&&File.ReadAllText(Core.PathIn(root,"Settings/pc-display.ini"))=="personal display","Personal files changed");
                    Console.WriteLine("Mac game package installed and verified "+files.Count+" files (preserve personal files: "+preserve+").");
                }
                return 0;
            }catch(Exception e){Console.Error.WriteLine(e);return 1;}
            finally{Directory.Delete(test,true);}
        }
        public static int Run()
        {
            sandbox=Path.Combine(AppContext.BaseDirectory,"tsfp-mac-launcher-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(sandbox);
            try{
                Test("shared update restart and failure recovery", LauncherRestartChecks.Run);
                LauncherTextureChecks.Run();
                Test("bundled game textures install and preserve existing replacements",BundledTexturesTest);
                Test("translocation recovery validates the real writable installation and preserves personal files", LocationTests);
                Test("game cleanup preserves launcher files and legacy casing while retired game files roll back", GamePruningTest);
                Test("Mac release selection rejects Windows packages",()=>{Need(ReleaseClient.Select(Metadata("windows-x64"),true)==null,"Windows package selected");Need(ReleaseClient.Select(Metadata("macos-arm64"),false)!=null,"Native release missing");Need(ReleaseClient.Select(Metadata("macos-arm64","v1.1.0-alpha.1",true),false)==null,"Preview selected on stable");});
                Test("JSON adapter round-trips nested fields and journal records",()=>{var map=Json.Object("{\"nested\":{\"flag\":true},\"list\":[1,2],\"n\":9223372036854775807}");Need(map["nested"] is Dictionary<string,object> && map["list"] is object[] && (long)map["n"]==long.MaxValue,"JSON types lost");var journal=Json.Serializer().Deserialize<List<Original>>(Json.Encode(new[]{new Original{Name="Bin/test",Existed=true}}));Need(journal[0].Name=="Bin/test"&&journal[0].Existed,"Journal fields lost");});
                Test("settings drafts, backups, unknown fields and external edit protection",()=>{var r=NewRoot("settings");var model=GameSettings.Load(r,null);Need(!Directory.Exists(Path.Combine(r,"Settings")),"Load wrote settings");model.Width=1280;model.Height=720;model.Save();var file=Core.PathIn(r,"Settings/pc-controls.json");var map=Json.Object(File.ReadAllText(file));map["future_option"]="preserve";File.WriteAllText(file,Json.Encode(map));var before=File.ReadAllBytes(file);model=GameSettings.Load(r,null);model.Sensitivity=.2;model.Save();Need(File.ReadAllBytes(file+".bak").SequenceEqual(before),"Backup differs");Need(Json.String(Json.Object(File.ReadAllText(file)),"future_option")=="preserve","Unknown setting lost");var a=GameSettings.Load(r,null);File.AppendAllText(file,"\n");Reject(()=>a.Save());});
                Test("multi-file settings failure rolls back committed files",()=>{var r=NewRoot("settings-failure");var m=GameSettings.Load(r,null);m.Save();var files=LauncherPlatform.Settings(r).Select(p=>Core.PathIn(r,p)).ToArray();var old=files.Select(File.ReadAllBytes).ToArray();Directory.CreateDirectory(files[2]+".bak");m.Sensitivity=.3;Reject(()=>m.Save());for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(old[i]),"Partial settings commit");});
                Test("retired presentation preferences migrate during native settings edits",()=>{
                    var r=NewRoot("pc-presentation");
                    Write(r,"tools/play-native-mac.py","fixture");
                    Write(r,"private/native-settings/runtime.conf","frame_interp = 1\nframe_target = 144\nrefresh_rate = 144\nframe_cap = 30\nfuture_setting = retained\n");
                    var m=GameSettings.Load(r,null);
                    m.Sensitivity=.2;m.Save();
                    var saved=GameSettings.Load(r,null);
                    Need(saved.PresentationFps==0 && saved.RefreshRate==0 && saved.Interpolation==1 && saved.FrameCap=="60","Retired timing preference not migrated");
                    Need(saved.Sensitivity==.2,"Native edit not saved");
                    Need(File.ReadAllText(Path.Combine(r,"private/native-settings/runtime.conf")).Contains("future_setting = retained"),"Unrelated setting lost during migration");
                    var fresh=GameSettings.Load(NewRoot("native-defaults"),null);
                    Need(fresh.Interpolation==1 && fresh.PresentationFps==0,"Native 60 FPS default incorrect");
                });
                Test("checkout settings and launch routing preserve user defaults",()=>{var r=NewRoot("checkout");Write(r,"tools/play-native-mac.py","fixture");Write(r,"private/native-settings/runtime.conf","frame_interp = 1\nfuture_setting = retained\n");var m=GameSettings.Load(r,null);Need(m.Interpolation==1,"60 FPS preference lost");m.Save();Need(!Directory.Exists(Path.Combine(r,"Settings")),"Created package settings in checkout");Environment.SetEnvironmentVariable("RECOMP_TEST_STALE","bad");var info=GameSettings.StartInfo(r);Need(info.FileName==Path.Combine(r,"build/macos/futureperfect_recomp"),"Wrong native executable");Need(info.EnvironmentVariables["RECOMP_SAVE_DIR"]==Path.Combine(r,"private/saves"),"Wrong save route");Need(info.EnvironmentVariables["RECOMP_DISPLAY_CONFIG"]==Path.Combine(r,"private/native-settings/runtime.conf")&&info.EnvironmentVariables["TSFP_DISPLAY_CONFIG"]==Path.Combine(r,"config/pc-display.ini"),"Display routes wrong");Need(!info.EnvironmentVariables.ContainsKey("RECOMP_TEST_STALE"),"Inherited override leaked");Need(File.ReadAllText(Path.Combine(r,"private/native-settings/runtime.conf")).Contains("future_setting = retained"),"Unsupported setting lost");Environment.SetEnvironmentVariable("RECOMP_TEST_STALE",null);});
                Test("binding conflicts, reserved keys and clear are shared",()=>{Need(GameSettings.WheelUpKey==134 && GameSettings.WheelDownKey==135 && GameSettings.BindingLabel("key:F24")=="Wheel down" && GameSettings.CaptureKey(162)==17,"Native capture differs from shared bindings");var m=GameSettings.Load(NewRoot("bindings"),null);m.Assign(0,0,65,true);Need(m.Conflict(1,65)==0,"Conflict missing");Reject(()=>m.Assign(1,0,65,false));m.Assign(1,0,65,true);Need(m.Binding(0)[0]=="none","Move left old key");Reject(()=>m.Assign(1,0,113,true));m.Assign(1,0,0,true);Need(m.Binding(1)[0]=="none","Clear failed");});
                Test("native update, failed transaction recovery and rollback preserve personal files",()=>{var r=NewRoot("updates");Write(r,"Saves/keep","save");Write(r,"Data/keep","data");Write(r,"Settings/pc-controls.json","personal");var a=Core.Unpack(Archive(r,"v1.0.0"),Path.Combine(r,"one"),"v1.0.0");Core.Apply(r,a,null);Need(File.ReadAllText(Path.Combine(r,"Settings/pc-controls.json"))=="personal","Controls overwritten");Need((File.GetUnixFileMode(Path.Combine(r,LauncherPlatform.GameBinary))&UnixFileMode.UserExecute)!=0,"Execute bit absent");var b=Core.Unpack(Archive(r,"v1.1.0"),Path.Combine(r,"two"),"v1.1.0");Reject(()=>Core.Apply(r,b,i=>{if(i==1)throw new IOException("synthetic failure");}));Need(Core.InstalledVersion(r)=="v1.0.0","Failure did not recover");Core.Apply(r,b,null);Core.Rollback(r);Need(Core.InstalledVersion(r)=="v1.0.0","Rollback failed");Need(File.ReadAllText(Path.Combine(r,"Saves/keep"))=="save"&&File.ReadAllText(Path.Combine(r,"Data/keep"))=="data","Personal files changed");});
                Test("corrupt packages and unsafe paths are rejected",()=>{var r=NewRoot("invalid");Reject(()=>Core.Unpack(Archive(r,"v1.0.0",true),Path.Combine(r,"bad"),"v1.0.0"));foreach(var name in new[]{"../data","/outside","foo:bar","a\\b","CON","a/../b"})Reject(()=>Core.PathIn(r,name));});
                Test("linked installation directories cannot redirect writes",()=>{var r=NewRoot("links");var other=NewRoot("link-target");Directory.CreateSymbolicLink(Path.Combine(r,"Bin"),other);Reject(()=>Core.PathIn(r,"Bin/test"));Need(!Directory.EnumerateFileSystemEntries(other).Any(),"Link target changed");});
                Test("native launcher manifest verifies and installs independently",()=>{var r=NewRoot("self");Write(r,"BUILD.json","keep game");var changes=SelfUpdate.Unpack(Archive(r,"1.2.18",false,true),Path.Combine(r,"stage"),"1.2.18");Core.ApplyLauncher(r,changes,null);Need(SelfUpdate.Complete(r),"Self manifest incomplete");Need(File.ReadAllText(Path.Combine(r,"BUILD.json"))=="keep game","Self update changed game");});
                Test("legacy game manifests preserve launcher files regardless of casing",()=>{
                    foreach(var name in new[]{Core.LauncherName,Core.AppPath,Core.SelfManifest})
                        Need(Core.PreserveGameFile(name.ToLowerInvariant()),"Legacy launcher path not protected");
                    Need(Core.PreserveGameFile("TimeSplitters Launcher.app/Contents/Resources/launcher.icns"),"Native icon not protected");
                    Need(!Core.PreserveGameFile(LauncherPlatform.GameBinary),"Game binary incorrectly protected from repair");
                });
                Test("combined release selects only the native launcher and manifest",()=>{
                    string version="1.2.19",prefix="https://github.com/"+ReleaseClient.Repository+"/releases/download/launcher-v"+version+"/";
                    var names=new[]{"TimeSplitters-Launcher-v"+version+"-windows-x64.zip","launcher-update.json",
                        "TimeSplitters-Launcher-v"+version+"-macos-arm64.zip","launcher-update-macos-arm64.json",
                        "TimeSplitters-Launcher-windows-x64.zip","TimeSplitters-Launcher-macos-arm64.zip"};
                    foreach(int count in new[]{4,6}) {
                        var assets=names.Take(count).Select(n=>new{name=n,state="uploaded",size=100,digest="",browser_download_url=prefix+n}).ToArray();
                        var candidate=ReleaseClient.SelectLauncher(Json.Encode(new[]{new{tag_name="launcher-v"+version,draft=false,prerelease=false,assets=assets}}),false);
                        Need(candidate.Package.AssetName==names[2]&&candidate.ManifestUrl==prefix+names[3],"Wrong host selected from combined release");
                        var metadata=Json.Encode(new{schema=1,version=version,size=100,sha256=new string('a',64),asset_name=names[2]});
                        Need(ReleaseClient.VerifyLauncherMetadata(candidate,metadata).Tag==version,"Combined release metadata rejected");
                    }
                });
                Test("SDK helper caps/drains one pipe and rejects failure/timeout",()=>{
                    ProcessStartInfo Helper(string script){var i=new ProcessStartInfo("/bin/sh"){UseShellExecute=false,RedirectStandardOutput=true};i.ArgumentList.Add("-c");i.ArgumentList.Add(script);return i;}
                    string output=NativeHost.ReadGpuHelper(Helper("printf '{\"driver\":\"test\"}'"));
                    Need(Json.String(Json.Object(output),"driver")=="test","Helper output was lost");
                    Need(NativeHost.ReadGpuHelper(Helper("head -c 100000 /dev/zero")).Length==16384,"Unbounded or blocked output");
                    Reject(()=>NativeHost.ReadGpuHelper(Helper("printf rejected; exit 7")));
                    Reject(()=>NativeHost.ReadGpuHelper(Helper("sleep 5"),50));
                });
                Test("portable Vulkan uses only hashed bundled libraries without a checkout",()=>{
                    var r=NewRoot("portable-vulkan");var hashes=new Dictionary<string,object>();
                    foreach(var name in new[]{"Bin/libvulkan.1.dylib","Bin/libMoltenVK.dylib","Bin/libdxcompiler.dylib","Bin/libcrypto.3.dylib","Support/Vulkan/MoltenVK_icd.json"}){
                        Write(r,name,name.EndsWith(".json")?"{\"ICD\":{\"library_path\":\"../../Bin/libMoltenVK.dylib\"}}":"library");hashes[name]=Core.Hash(Core.PathIn(r,name));
                    }
                    var manifest=new Dictionary<string,object>{{"vulkan_driver","moltenvk"},{"files",hashes}};
                    var env=NativeHost.PortableGpuEnvironment(r,manifest);
                    Need((string)env["SDL_VULKAN_LIBRARY"]==Core.PathIn(r,"Bin/libvulkan.1.dylib")&&(string)env["VK_DRIVER_FILES"]==Core.PathIn(r,"Support/Vulkan/MoltenVK_icd.json"),"Wrong packaged GPU paths");
                    Need(!Directory.Exists(Path.Combine(r,"Settings")),"Read-only GPU check wrote settings");
                    Write(r,"Bin/libcrypto.3.dylib","altered");Reject(()=>NativeHost.PortableGpuEnvironment(r,manifest));
                    hashes["Bin/libcrypto.3.dylib"]=Core.Hash(Core.PathIn(r,"Bin/libcrypto.3.dylib"));
                    Write(r,"Support/Vulkan/MoltenVK_icd.json","{\"ICD\":{\"library_path\":\"/tmp/untrusted.dylib\"}}");hashes["Support/Vulkan/MoltenVK_icd.json"]=Core.Hash(Core.PathIn(r,"Support/Vulkan/MoltenVK_icd.json"));Reject(()=>NativeHost.PortableGpuEnvironment(r,manifest));
                    manifest["vulkan_driver"]="unknown";Reject(()=>NativeHost.PortableGpuEnvironment(r,manifest));
                });
                Test("wrong XBE and altered native executable fail read-only checks",()=>{var r=NewRoot("checks");Write(r,"Data/default.xbe","invalid");Reject(()=>NativeHost.CheckData(r));Write(r,LauncherPlatform.GameBinary,"invalid");Write(r,"BUILD.json",Json.Encode(new{version="v1.0.0",executable_sha256=new string('a',64)}));Reject(()=>NativeHost.CheckBuild(r));Need(!Directory.Exists(Path.Combine(r,"Saves")),"Check created saves");});
                Test("verified download and cached replay use shared integrity policy",()=>{var r=NewRoot("download");byte[] bytes=Encoding.UTF8.GetBytes("verified archive");var release=new Release{AssetUrl="https://github.com/"+ReleaseClient.Repository+"/releases/download/v1.0.0/a.zip",Digest="sha256:"+Hash(bytes),Size=bytes.Length};int calls=0;using(var c=new ReleaseClient(new Handler{Reply=q=>{calls++;return new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(bytes)};}})){c.Download(release,Path.Combine(r,"one.zip"),null,CancellationToken.None,Path.Combine(r,"cache")).GetAwaiter().GetResult();c.Download(release,Path.Combine(r,"two.zip"),null,CancellationToken.None,Path.Combine(r,"cache")).GetAwaiter().GetResult();}Need(calls==1&&File.ReadAllBytes(Path.Combine(r,"two.zip")).SequenceEqual(bytes),"Cache replay failed");});
                Test("known issues exclude pull requests and closed bugs",()=>{bool more;var issues=KnownIssues.ParsePage("[{\"number\":1,\"title\":\"Live bug\",\"state\":\"open\",\"labels\":[{\"name\":\"bug\"}]},{\"number\":2,\"state\":\"closed\"},{\"pull_request\":{}}]",out more);Need(issues.Length==1&&issues[0].Number==1&&!more,"Wrong issue snapshot");});
                Test("shared rich Markdown preserves formatting and rejects unsafe links",()=>{var doc=ReleaseNotesDocument.Parse("# Heading\n\nParagraph **bold** and *italic* and ~~old~~ and `code`. [Safe](https://example.com) [Unsafe](file:///tmp/no)\n\n3. Third\n4. Fourth\n\n> Quote\n\n```cs\n<literal>\n```\n\n---");Need(doc.Count(b=>b.Kind=="item")==2&&doc.First(b=>b.Kind=="item").Marker=="3.","Numbered list lost");Need(doc.Any(b=>b.Kind=="code"&&b.Text=="<literal>")&&doc.Any(b=>b.Kind=="quote")&&doc.Any(b=>b.Kind=="rule"),"Block formatting lost");var nodes=doc.SelectMany(b=>b.Inlines).ToArray();Need(nodes.Any(n=>n.Kind=="bold")&&nodes.Any(n=>n.Kind=="italic")&&nodes.Any(n=>n.Kind=="strike")&&nodes.Any(n=>n.Kind=="code"),"Inline formatting lost");Need(nodes.Count(n=>n.Kind=="link")==1,"Unsafe link accepted");Uri u;Need(!ReleaseNotesDocument.TryLink("https://user:secret@example.com",out u),"Credentials allowed");Need(ReleaseNotesDocument.Parse(new string('x',70000)).Last().Text.Contains("shortened"),"Unbounded notes");});
                Test("native crash export includes bounded context and preserves unrelated files",()=>{var r=NewRoot("native-report");string name="crash-20261019-123456789-0000000001-123456";string folder="Logs/Crashes/"+name+"/";Write(r,folder+"capture.lock","");Write(r,folder+"report.json",Json.Encode(new{schema=1,kind="unhandled_exception",platform="macos-arm64",signal=6,native_capture_saved=true}));Write(r,folder+"native-stack.txt","native symbols");Write(r,folder+"secret-save","never exported");Need(CrashReports.Latest(r)==name&&CrashReports.Summary(r).Contains("Fatal native signal 6"),"Native metadata not recognized");string zip=Path.Combine(r,"report.zip");CrashReports.Export(r,name,zip);using(var z=ZipFile.OpenRead(zip))Need(z.GetEntry("native-stack.txt")!=null&&z.GetEntry("secret-save")==null,"Wrong export contents");File.WriteAllBytes(Core.PathIn(r,folder+"native-stack.txt"),new byte[262145]);Reject(()=>CrashReports.Export(r,name,Path.Combine(r,"oversized.zip")));Need(!File.Exists(Path.Combine(r,"oversized.zip")),"Partial export leaked");});
                Test("native unexpected exit includes the current bounded log tail",()=>{var r=NewRoot("session-exit");Write(r,"logs/native-play.err",new string('x',70000)+"current session tail");string id=GameSessions.Begin(r,"20261019-120000-000");using(var game=Process.Start(new ProcessStartInfo("/bin/sh","-c \"exit 7\""){UseShellExecute=false})){game.WaitForExit();string name=GameSessions.Complete(r,id,game);Need(CrashReports.IsUnexpectedExit(r,name),"Unexpected exit was not recorded");string tail=File.ReadAllText(Core.PathIn(r,"Logs/Crashes/"+name+"/game-log-tail.txt"));Need(tail.Length<=65536&&tail.EndsWith("current session tail"),"Wrong native session log");}Need(GameSessions.Recover(r)==null,"Completed session reported twice");});
                Console.WriteLine(count+" native launcher regression groups passed.");return 0;
            }catch(Exception ex){Console.Error.WriteLine(ex);return 1;}finally{if(Directory.Exists(sandbox))Directory.Delete(sandbox,true);}
        }
    }
}
