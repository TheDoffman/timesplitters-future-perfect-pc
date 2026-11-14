using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using TimeSplitters.Launcher;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.LogicalTree;
using Element = Avalonia.Controls.Control;
#else
using System.Windows;
using System.Windows.Controls;
using Element = System.Windows.FrameworkElement;
#endif

// Exactly the same behavioral contracts run against the real WPF and Avalonia pages.
public static class LauncherSharedUiChecks
{
    static int passed;
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Test(string name,Action action){action();passed++;Console.WriteLine("PASS shared UI: "+name);}
    static void Click(Button button){button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));}
    static IEnumerable<Element> All(Element root)
    {
#if MACOS || LINUX
        return root.GetLogicalDescendants().OfType<Element>();
#else
        return LogicalTreeHelper.GetChildren(root).OfType<Element>().SelectMany(c=>new[]{c}.Concat(All(c)));
#endif
    }
    static T Find<T>(Element root,string name) where T:Element { return All(root).OfType<T>().Single(c=>c.Name==name); }
    public static int Run()
    {
#if MACOS || LINUX
        return RunCore();
#else
        Exception failure=null;var thread=new System.Threading.Thread(()=>{try{RunCore();}catch(Exception e){failure=e;}});
        thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.IsBackground=true;thread.Start();
        Need(thread.Join(60000),"Shared UI tests timed out");if(failure!=null)throw failure;return 0;
#endif
    }
    static string Tip(Button button)
    {
#if MACOS || LINUX
        return Convert.ToString(ToolTip.GetTip(button));
#else
        return Convert.ToString(button.ToolTip);
#endif
    }
    static string TestBase()
    {
#if MACOS || LINUX
        // macOS's /var temp alias is a symlink; production intentionally rejects it.
        return Path.GetFullPath(".");
#else
        return Path.GetTempPath();
#endif
    }
    static Dictionary<string,object> ChannelRecord(string tag,bool preview=false)
    {
        string asset="TimeSplitters-Future-Perfect-PC-"+tag+"-"+LauncherPlatform.AssetSuffix+".zip";
        return Json.Object(Json.Encode(new {tag_name=tag,prerelease=preview,draft=false,body="Notes",assets=new[]{new {
            name=asset,state="uploaded",size=100,digest="sha256:"+new string('a',64),browser_download_url=ReleaseClient.ReleasesPage+"/download/"+tag+"/"+asset}}}));
    }
    sealed class WorkshopHandler : System.Net.Http.HttpMessageHandler
    {
        readonly Func<System.Net.Http.HttpRequestMessage,System.Net.Http.HttpResponseMessage> reply;
        public WorkshopHandler(Func<System.Net.Http.HttpRequestMessage,System.Net.Http.HttpResponseMessage> reply){this.reply=reply;}
        protected override System.Threading.Tasks.Task<System.Net.Http.HttpResponseMessage> SendAsync(System.Net.Http.HttpRequestMessage request,System.Threading.CancellationToken token)
        {token.ThrowIfCancellationRequested();return System.Threading.Tasks.Task.FromResult(reply(request));}
    }
    static string WorkshopCatalog(string sha,long scanned,bool available=true,string download=null)
    {
        object release=new {sha256=sha,download=download??"/v1/packages/"+sha,
            manifest=new {id="community-demo",name="Community Demo",version="1.0.0",author="Map Maker",description="A packaging example",game="tsfp-us-45410066",api="tsfp-mod-v1-draft"},
            scan=new {engine="TEST ONLY",time=scanned}};
        return Json.Encode(new {schema=1,releases=available?new[]{release}:new object[0]});
    }
    static void WorkshopReject(Action action,string message){bool rejected=false;try{action();}catch{rejected=true;}Need(rejected,message);}
    static void WorkshopChecks(string root)
    {
        long now=DateTimeOffset.UtcNow.ToUnixTimeSeconds();string sha=new string('a',64);
        Test("workshop catalog validates identities and freshness",()=>{
            var entries=ModWorkshopClient.Parse(WorkshopCatalog(sha,now-1),DateTime.UtcNow);
            Need(entries.Length==1 && entries[0].Author=="Map Maker","Mod metadata lost");
            Need(ModWorkshopClient.Parse(WorkshopCatalog(sha,now-90000),DateTime.UtcNow).Length==0,"Stale scan appeared");
            Need(ModWorkshopClient.Parse(WorkshopCatalog(sha,now+3600),DateTime.UtcNow).Length==0,"Future scan appeared");
            WorkshopReject(()=>ModWorkshopClient.Parse(WorkshopCatalog(sha,now,true,"https://attacker.invalid/payload"),DateTime.UtcNow),"External package URL accepted");
            WorkshopReject(()=>ModWorkshopClient.Parse(WorkshopCatalog("../escape",now),DateTime.UtcNow),"Invalid digest accepted");
            WorkshopReject(()=>ModWorkshopClient.Parse(new string('x',ModWorkshopClient.MaxCatalogBytes+1),DateTime.UtcNow),"Oversized catalog accepted");
        });
        Test("workshop transport requires HTTPS except explicit loopback",()=>{
            Need(ModWorkshopClient.ServiceUri("https://workshop.example.ts.net").Host=="workshop.example.ts.net","HTTPS service rejected");
            Need(ModWorkshopClient.ServiceUri("http://127.0.0.1:8765").IsLoopback,"Loopback preview rejected");
            foreach(var url in new[]{"http://example.com","https://user:secret@example.com","https://example.com/subpath","https://example.com/?redirect=1","file:///tmp/package"})
                WorkshopReject(()=>ModWorkshopClient.ServiceUri(url),"Unsafe service URL accepted");
        });
        Test("workshop shared page searches creators and previews stay offline",()=>{
            int requests=0;
            using(var page=new ModWorkshopPage(root,true,new ModWorkshopClient(new WorkshopHandler(request=>{requests++;throw new Exception("Preview made a request");})))) {
                page.SetReleases(ModWorkshopClient.Parse(WorkshopCatalog(sha,now-1),DateTime.UtcNow));
                Need(page.VisibleCount==1,"Mod card missing");page.Search.Text="map maker";Need(page.VisibleCount==1,"Creator search failed");
                page.Search.Text="no matching mod";Need(page.VisibleCount==0,"Search did not filter");
                page.Load().GetAwaiter().GetResult();Need(requests==0 && page.StatusText.Contains("Preview"),"Preview attempted network access");
            }
        });
        Test("workshop downloads verify hashes and recheck removal before fetching",()=>{
            string folder=Path.Combine(root,"workshop-downloads");byte[] bytes=System.Text.Encoding.UTF8.GetBytes("package fixture bytes");string hash;
            using(var algorithm=System.Security.Cryptography.SHA256.Create())hash=BitConverter.ToString(algorithm.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();
            bool available=true,corrupt=false;int packageRequests=0;
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/catalog")return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=new System.Net.Http.StringContent(WorkshopCatalog(hash,now-1,available))};
                packageRequests++;return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=new System.Net.Http.ByteArrayContent(corrupt?new byte[]{0}:bytes)};
            }))) {
                var release=client.Read("https://workshop.example.ts.net",System.Threading.CancellationToken.None).GetAwaiter().GetResult().Single();
                string path=client.Download("https://workshop.example.ts.net",release,folder,System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                Need(File.ReadAllBytes(path).SequenceEqual(bytes),"Verified mod bytes changed");
                Need(!Directory.Exists(Path.Combine(folder,"Mods")),"Download activated or installed a mod");
                available=false;int before=packageRequests;
                WorkshopReject(()=>client.Download("https://workshop.example.ts.net",release,folder,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Removed mod downloaded");
                Need(packageRequests==before,"Removed package was requested");
                available=true;corrupt=true;
                WorkshopReject(()=>client.Download("https://workshop.example.ts.net",release,folder,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Corrupt package accepted");
                Need(File.ReadAllBytes(path).SequenceEqual(bytes),"Failed download damaged the cached package");
                Need(Directory.GetFiles(Path.GetDirectoryName(path),"*.partial").Length==0,"Failed download left partial files");
            }
        });
        Test("workshop redirects and excessive response lengths fail closed",()=>{
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.Redirect))))
                WorkshopReject(()=>client.Read("https://workshop.example.ts.net",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Redirect accepted");
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>{
                var content=new System.Net.Http.ByteArrayContent(new byte[0]);content.Headers.ContentLength=ModWorkshopClient.MaxCatalogBytes+1;
                return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=content};})))
                WorkshopReject(()=>client.Read("https://workshop.example.ts.net",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Excessive catalog length accepted");
        });
    }
    static byte[] WorkshopDds(byte marker)
    {
        var bytes=new byte[152];foreach(var pair in new Dictionary<int,uint>{{0,0x20534444},{4,124},{12,4},{16,4},{28,3},{76,32},{80,4},{84,0x31545844}})
            Array.Copy(BitConverter.GetBytes(pair.Value),0,bytes,pair.Key,4);
        for(int i=128;i<bytes.Length;i++)bytes[i]=marker;return bytes;
    }
    static string LibraryPackage(string root,string id,string version,byte marker,string key="0123456789abcdef",Dictionary<string,string> dependencies=null,bool script=false,string extra=null)
    {
        string file=script?"scripts/main.lua":"textures/"+key+".dds";byte[] data=script?System.Text.Encoding.UTF8.GetBytes("return true\n"):WorkshopDds(marker);string hash;
        using(var sha=System.Security.Cryptography.SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();
        string json=Json.Encode(new {schema=1,id=id,version=version,name=id,author="Test creator",description="Synthetic original fixture",game="tsfp-us-45410066",api="tsfp-mod-v1-draft",dependencies=dependencies??new Dictionary<string,string>(),files=new Dictionary<string,object>{{file,new {sha256=hash,size=data.Length}}}});
        string package=Path.Combine(root,Guid.NewGuid().ToString("N")+".zip");
        using(var archive=System.IO.Compression.ZipFile.Open(package,System.IO.Compression.ZipArchiveMode.Create)) {
            using(var output=archive.CreateEntry("mod.json").Open()){byte[] text=System.Text.Encoding.UTF8.GetBytes(json);output.Write(text,0,text.Length);}
            using(var output=archive.CreateEntry(file).Open())output.Write(data,0,data.Length);
            if(extra!=null)using(var output=archive.CreateEntry(extra).Open())output.Write(data,0,data.Length);
        }
        return package;
    }
    static void LibraryChecks(string root)
    {
        Test("native mod metadata rejects allocation and numeric amplification",()=>{
            string package=LibraryPackage(root,"metadata-budget","1.0.0",1);byte[] original=File.ReadAllBytes(package);int end=original.Length-22;
            Need(BitConverter.ToUInt32(original,end)==0x06054b50,"Fixture ZIP end changed");
            foreach(var change in new[]{new[]{8,65535},new[]{8,3},new[]{4,1}}) {
                byte[] bad=(byte[])original.Clone();Array.Copy(BitConverter.GetBytes((ushort)change[1]),0,bad,end+change[0],2);
                if(change[0]==8)Array.Copy(BitConverter.GetBytes((ushort)change[1]),0,bad,end+10,2);
                string path=Path.Combine(root,Guid.NewGuid().ToString("N")+".zip");File.WriteAllBytes(path,bad);
                WorkshopReject(()=>ModLibrary.ValidateZipMetadata(path),"Forged ZIP directory passed preflight");
            }
            string values=String.Join(",",Enumerable.Repeat("0",4096));
            string large="{\"items\":["+String.Join(",",Enumerable.Repeat("["+values+"]",16))+"]}";
            WorkshopReject(()=>ModJson.Read(large,4*1024*1024),"JSON token budget was not enforced");
            WorkshopReject(()=>ModJson.Read("{\"n\":1e400}"),"JSON numeric overflow accepted");
            string valid="{\"items\":["+String.Join(",",Enumerable.Repeat("["+values+"]",8))+"]}";
            Need(ModJson.Read(valid,4*1024*1024).ContainsKey("items"),"Bounded numeric data failed");
        });
        Test("native mod installation enables verified texture overlays and preserves official files",()=>{
            string folder=Path.Combine(root,"mod-install");Directory.CreateDirectory(folder);Directory.CreateDirectory(Path.Combine(folder,"Textures"));File.WriteAllText(Path.Combine(folder,"Textures/official.dds"),"preserve");
            var library=new ModLibrary(folder,()=>false);string package=LibraryPackage(root,"texture-demo","1.0.0",7);var mod=library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);
            Need(library.List().Single().Key=="texture-demo@1.0.0" && library.Enabled().Length==0,"Install enabled the package automatically");
            library.Enable(mod.Key,System.Threading.CancellationToken.None);var info=new System.Diagnostics.ProcessStartInfo();library.Configure(info);string overlay=info.EnvironmentVariables["TSFP_MOD_TEXTURE_DIR"];
            Need(overlay!=null && File.ReadAllBytes(Path.Combine(overlay,"0123456789abcdef.dds"))[128]==7,"Activated DDS was not mounted");
            Need(File.ReadAllText(Path.Combine(folder,"Textures/official.dds"))=="preserve","Official pack was modified");
            WorkshopReject(()=>library.Uninstall(mod.Key),"Active mod removed");library.DisableAll(System.Threading.CancellationToken.None);library.Uninstall(mod.Key);
            Need(library.List().Length==0 && File.ReadAllText(Path.Combine(folder,"Textures/official.dds"))=="preserve","Uninstall damaged unrelated files");
        });
        Test("native mod activation rejects conflicts, missing dependencies and cycles",()=>{
            string folder=Path.Combine(root,"mod-deps");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            foreach(var record in new[]{new[]{"base","1.0.0","0123456789abcdef"},new[]{"conflict","1.0.0","0123456789abcdef"}}) {
                string package=LibraryPackage(root,record[0],record[1],1,record[2]);library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);
            }
            library.Enable("base@1.0.0",System.Threading.CancellationToken.None);
            string original=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));WorkshopReject(()=>library.Enable("conflict@1.0.0",System.Threading.CancellationToken.None),"Conflicting texture enabled");
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==original,"Failed activation changed the old profile");
            string addon=LibraryPackage(root,"addon","1.0.0",3,"aaaaaaaaaaaaaaaa",new Dictionary<string,string>{{"base","2.0.0"}});library.Install(addon,Core.Hash(addon),System.Threading.CancellationToken.None);
            WorkshopReject(()=>library.Enable("addon@1.0.0",System.Threading.CancellationToken.None),"Missing exact dependency enabled");
            foreach(var item in new[]{new[]{"cycle-a","cycle-b","bbbbbbbbbbbbbbbb"},new[]{"cycle-b","cycle-a","cccccccccccccccc"}}){string package=LibraryPackage(root,item[0],"1.0.0",4,item[2],new Dictionary<string,string>{{item[1],"1.0.0"}});library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);}
            WorkshopReject(()=>library.SetEnabled(new[]{"cycle-a@1.0.0","cycle-b@1.0.0"},System.Threading.CancellationToken.None),"Dependency cycle enabled");
            Need(library.Enabled().SequenceEqual(new[]{"base@1.0.0"}),"Dependency failure changed the active set");
        });
        Test("native mod upgrades retain rollback and do not accumulate overlay generations",()=>{
            string folder=Path.Combine(root,"mod-versions");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            foreach(string version in new[]{"1.0.0","2.0.0"}){string package=LibraryPackage(root,"versioned",version,version=="1.0.0"?(byte)1:(byte)2);library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);}
            foreach(string version in new[]{"1.0.0","2.0.0","1.0.0"}){library.Enable("versioned@"+version,System.Threading.CancellationToken.None);Need(library.Enabled().Single()=="versioned@"+version,"Version switch failed");}
            Need(library.List().Length==2 && Directory.GetDirectories(Path.Combine(folder,"Mods/Active")).Length==1,"Rollback version lost or active cache copies accumulated");
        });
        Test("native mod validation rejects archive traversal and duplicate metadata",()=>{
            string folder=Path.Combine(root,"mod-hostile");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            foreach(string extra in new[]{"../outside.dds","scripts/payload.exe","Textures/0123456789abcdef.dds"}) {
                string package=LibraryPackage(root,"hostile","1.0.0",1,extra:extra);WorkshopReject(()=>library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None),"Hostile archive installed");
            }
            Need(library.List().Length==0 && Directory.GetDirectories(Path.Combine(folder,"Mods"),".install-*").Length==0,"Failed install left a partial library");
            string mismatched=LibraryPackage(root,"actual-identity","1.0.0",1);
            WorkshopReject(()=>library.Install(mismatched,Core.Hash(mismatched),System.Threading.CancellationToken.None,"claimed-identity","1.0.0"),"Package identity differed from its catalog listing");
            WorkshopReject(()=>ModLibrary.Manifest("{\"schema\":1,\"schema\":1}"),"Duplicate manifest fields accepted");
            byte[] bad=WorkshopDds(1);bad[28]=2;WorkshopReject(()=>ModLibrary.ValidateDds(bad),"Incomplete DDS mip chain accepted");
            string script=LibraryPackage(root,"script-only","1.0.0",1,script:true);library.Install(script,Core.Hash(script),System.Threading.CancellationToken.None);
            WorkshopReject(()=>library.Enable("script-only@1.0.0",System.Threading.CancellationToken.None),"A script was enabled without a sandbox");
        });
        Test("native mod tampering, cancellation and game-running guards retain the active profile",()=>{
            string folder=Path.Combine(root,"mod-preservation");Directory.CreateDirectory(folder);bool running=false;var library=new ModLibrary(folder,()=>running);
            string package=LibraryPackage(root,"preserved","1.0.0",1);var mod=library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);library.Enable(mod.Key,System.Threading.CancellationToken.None);
            string profile=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));running=true;
            WorkshopReject(()=>library.DisableAll(System.Threading.CancellationToken.None),"Profile changed while game was running");running=false;
            using(var cancellation=new System.Threading.CancellationTokenSource()){cancellation.Cancel();WorkshopReject(()=>library.DisableAll(cancellation.Token),"Cancelled activation committed");}
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==profile,"A rejected operation changed the active profile");
            var info=new System.Diagnostics.ProcessStartInfo();library.Configure(info);File.WriteAllText(Path.Combine(info.EnvironmentVariables["TSFP_MOD_TEXTURE_DIR"],"0123456789abcdef.dds"),"tampered");
            WorkshopReject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Changed active payload passed launch checks");
            library.Enable(mod.Key,System.Threading.CancellationToken.None);File.WriteAllText(Path.Combine(mod.Directory,"content/mod.json"),"{}");
            WorkshopReject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Changed installed manifest passed launch checks");
            library.DisableAll(System.Threading.CancellationToken.None);Need(library.Enabled().Length==0,"Disable-all could not recover a broken library");
        });
        Test("native mod uninstall preserves unexpected personal files",()=>{
            string folder=Path.Combine(root,"mod-extra-files");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            string package=LibraryPackage(root,"personal-guard","1.0.0",1);var mod=library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);
            string personal=Path.Combine(mod.Directory,"my-original.iso");File.WriteAllText(personal,"keep this original");
            WorkshopReject(()=>library.Uninstall(mod.Key),"Uninstall deleted an unexpected personal file");Need(File.ReadAllText(personal)=="keep this original","Personal bytes were changed");
        });
        Test("launch without mods bypasses a broken profile without changing it",()=>{
            string folder=Path.Combine(root,"mod-launch-recovery");Directory.CreateDirectory(folder);GameSettings.StartInfo(folder,true);
            Directory.CreateDirectory(Path.Combine(folder,"Mods"));string path=Path.Combine(folder,"Mods/profile.json");File.WriteAllText(path,"broken profile");
            WorkshopReject(()=>GameSettings.StartInfo(folder),"Broken mod profile was silently ignored");
            var info=GameSettings.StartInfo(folder,true);Need(!info.EnvironmentVariables.ContainsKey("TSFP_MOD_TEXTURE_DIR") && File.ReadAllText(path)=="broken profile","Recovery changed the profile or inherited a mod overlay");
        });
    }
    public static int RunWorkshop()
    {
        Action run=()=>{string root=Path.Combine(TestBase(),"tsfp-workshop-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);try{WorkshopChecks(root);LibraryChecks(root);}finally{Directory.Delete(root,true);}};
#if MACOS || LINUX
        run();return 0;
#else
        Exception failure=null;var thread=new System.Threading.Thread(()=>{try{run();}catch(Exception e){failure=e;}});
        thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.Start();Need(thread.Join(60000),"Workshop checks timed out");if(failure!=null)throw failure;return 0;
#endif
    }
    static int RunCore()
    {
        string root=Path.Combine(TestBase(),"tsfp-shared-ui-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            WorkshopChecks(root);
            LibraryChecks(root);
            Test("main menu update notice follows version and selected channel",()=>{
                var release=new Release {Tag="v1.2.4",Channel="stable"};
                Need(GameUpdateNotice.Encode("v1.2.3",release,"stable",123)=="TSFP_GAME_UPDATE_V1 123 v1.2.3 v1.2.4\n","Newer release missing");
                Need(GameUpdateNotice.Encode("v1.2.4",release,"stable",123)=="","Current release advertised");
                Need(GameUpdateNotice.Encode("v1.3.0",release,"stable",123)=="","Downgrade advertised");
                Need(GameUpdateNotice.Encode("development",release,"stable",123)=="","Unknown local version advertised");
                Need(GameUpdateNotice.Encode("v1.2.3",release,"beta",123)=="","Wrong channel advertised");
                Need(GameUpdateNotice.Encode("v1.2.3",null,"stable",123)=="","Failed check advertised");
                release.Tag="v1.2.4-beta.1";release.Channel="beta";
                Need(GameUpdateNotice.Encode("v1.2.4-beta.0",release,"beta",123)!="","Preview upgrade missing");
                string installation=Path.Combine(root,"notice-install");Directory.CreateDirectory(installation);
                Core.Write(Core.PathIn(installation,LauncherPlatform.ManifestPath(installation)),Json.Encode(new {version="v1.2.3"}));
                release.Tag="v1.2.4";release.Channel="stable";GameUpdateNotice.Publish(installation,release,"stable");
                string published=File.ReadAllText(GameUpdateNotice.Path(installation));
                Need(published.StartsWith("TSFP_GAME_UPDATE_V1 ") && published.EndsWith(" v1.2.3 v1.2.4\n"),"Verified result not published");
                GameUpdateNotice.Publish(installation,null,"stable");
                Need(File.ReadAllText(GameUpdateNotice.Path(installation))=="","Failed check retained notice");
                string unwritable=Path.Combine(root,"notice-blocked");Directory.CreateDirectory(unwritable);
                File.WriteAllText(Path.Combine(unwritable,".launcher"),"Not a directory");
                GameUpdateNotice.Publish(unwritable,release,"stable"); // Optional notice failure must not block launch.
                GameUpdateNotice.Publish(root,null,"stable");
                Need(File.ReadAllText(GameUpdateNotice.Path(root))=="","Empty result did not clear old notice");
                var info=GameSettings.StartInfo(root);
                Need(info.EnvironmentVariables["TSFP_GAME_UPDATE_NOTICE"]==GameUpdateNotice.Path(root),"Notice path missing from launch");
                Need(info.EnvironmentVariables["TSFP_GAME_INSTALLED_VERSION"]==Core.InstalledVersion(root),"Installed version missing from launch");
            });
            Test("game texture preference survives launcher reload and detects concurrent edits",()=>{
                string folder=Path.Combine(root,"textures-game-save");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                string preference=Core.PathIn(folder,LauncherPlatform.Settings(folder)[8]);
                File.WriteAllText(preference,"TSFP_TEXTURES_V1 1\n");
                Need(GameSettings.Load(folder,null).EnhancedTextures,"Game-saved ON did not reach launcher");
                Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_TEXTURE_MODE"]=="4k","Game-saved choice lost at launch");
                bool rejected=false;try {model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(preference)=="TSFP_TEXTURES_V1 1\n","Stale launcher overwrote game preference");
                model=GameSettings.Load(folder,null);model.VerticalFov=70;model.Save();
                Need(GameSettings.Load(folder,null).EnhancedTextures,"Unrelated launcher save reset game choice");
                File.WriteAllText(preference,"TSFP_TEXTURES_V1 0\n");
                Need(!GameSettings.Load(folder,null).EnhancedTextures,"Game-saved OFF did not override legacy JSON");
                model=GameSettings.Load(folder,null);model.EnhancedTextures=true;model.Save();
                Need(File.ReadAllText(preference)=="TSFP_TEXTURES_V1 1\n","Launcher could not update shared preference");
                File.WriteAllText(preference,"TSFP_TEXTURES_V1 1 trailing");
                rejected=false;try {GameSettings.Load(folder,null);}catch{rejected=true;}
                Need(rejected && File.ReadAllText(preference).EndsWith("trailing"),"Malformed shared preference was rewritten");
            });
            Test("antivirus error codes respect the host and preserve unrelated IO failures",()=>{
                var native=new System.ComponentModel.Win32Exception(225,"localized detection");
                var io=new IOException("localized detection",unchecked((int)0x800700e2));
#if MACOS || LINUX
                Need(AntivirusErrors.Code(native)==0 && AntivirusErrors.Code(io)==0,"Windows detection codes leaked into a native host");
#else
                Need(AntivirusErrors.Code(native)==225 && AntivirusErrors.Code(io)==226,"Windows detection codes were lost");
#endif
                var denied=new UnauthorizedAccessException("access denied");
                try { AntivirusErrors.At("fixture",()=>{throw denied;}); }
                catch(Exception actual) { Need(ReferenceEquals(actual,denied),"Unrelated access failure was replaced"); }
                Need(RecoveryInfo.From(denied,false).Title!="Antivirus blocked a file","Permissions were diagnosed as antivirus");
            });
#if MACOS || LINUX
            Test("launcher fits small and scaled desktop work areas",()=>{
                foreach(var desktop in new[]{new Size(1280,800),new Size(1024,640),new Size(853,533),new Size(1920,1080)}) {
                    var client=NativeApplication.LauncherClientArea(desktop);
                    Need(client.Width+32<=desktop.Width&&client.Height+64<=desktop.Height,"Window exceeds desktop work area");
                    var layout=NativeApplication.LauncherLayoutArea(client);
                    Need(layout.Width>=999.9&&layout.Height>=719.9,"Layout fell below supported content size");
                    Need(Math.Abs(layout.Width/layout.Height-client.Width/client.Height)<0.001,"Layout would clip instead of scaling uniformly");
                }
            });
#endif
#if LINUX
            Test("launcher windows retain embedded icons",()=>{
                using(var resource=typeof(NativeApplication).Assembly.GetManifestResourceStream("launcher.ico")) {
                    if(resource==null) { Need(Environment.GetEnvironmentVariable("TSFP_TEST_REQUIRE_ICON")!="1","Required launcher icon was not embedded"); Console.WriteLine("Developer build without optional icon artwork."); return; }
                }
                foreach(var window in new Window[]{new NativeStartup(root,Array.Empty<string>()),
                    new NativeWindow(root,new[]{"--render-preview"}),new NativeLocationWindow(Array.Empty<string>())}) {
                    Need(window.Icon!=null,"Launcher window has no application icon");
                    using(var bytes=new MemoryStream()){window.Icon.Save(bytes);Need(bytes.Length>0,"Empty window icon");}
                    window.Close();
                }
            });
#endif
            Test("graphics controls persist across shared launcher hosts",()=>{
                string install=Path.Combine(root,"graphics-controls");Directory.CreateDirectory(install);
                foreach(int aa in new[]{2,1,0}) {
                    bool enabled=aa!=0;
                    var page=new SettingsPage(new ResourceDictionary(),install,true);page.SelectTab(1);
                    Find<ComboBox>(page.View,"MultisampleAntialiasing").SelectedItem=enabled?"8x":"Off";
                    Find<ComboBox>(page.View,"PostAntiAliasing").SelectedIndex=aa;
                    Find<ComboBox>(page.View,"Supersampling").SelectedItem=enabled?"200%":"100% (Off)";
                    Find<ComboBox>(page.View,"AnisotropicFiltering").SelectedItem=enabled?"16":"1";
                    Find<CheckBox>(page.View,"AmbientOcclusion").IsChecked=enabled;
                    Find<CheckBox>(page.View,"ShaderCache").IsChecked=enabled;
                    Find<CheckBox>(page.View,"FpsPingCounter").IsChecked=enabled;
                    Need(Find<Button>(page.View,"ClearShaderCache").IsEnabled,"Cache clear is unavailable");
                    Click(Find<Button>(page.View,"SettingsSaveButton"));
                    var saved=GameSettings.Load(install,null);
                    Need(saved.Msaa==(enabled?8:1) && saved.PostAntiAliasing==aa && saved.Smaa==(aa==2) && saved.EdgeSmoothing==(aa==1?1:0) && saved.Anisotropy==(enabled?16:1),"Graphics selection not saved");
                    Need(saved.Supersampling==(enabled?3:0),"Supersampling not saved");
                    Need(saved.AmbientOcclusion==(enabled?1:0) && saved.ShaderCache==enabled && saved.ShowCounter==enabled,"Graphics toggles not saved");
                    var reopened=new SettingsPage(new ResourceDictionary(),install,true);reopened.SelectTab(1);
                    Need(Convert.ToString(Find<ComboBox>(reopened.View,"MultisampleAntialiasing").SelectedItem)==(enabled?"8x":"Off"),"MSAA choice not restored");
                    Need(Convert.ToString(Find<ComboBox>(reopened.View,"Supersampling").SelectedItem)==(enabled?"200%":"100% (Off)"),"Supersampling choice not restored");
                    Need(Find<CheckBox>(reopened.View,"AmbientOcclusion").IsChecked==enabled && Find<CheckBox>(reopened.View,"FpsPingCounter").IsChecked==enabled,"Toggles not restored");
                }
            });
            SaveBackupChecks.Run(root,Test);
            Test("complete language packs save, cancel, launch and restore English",()=>{
                string folder=Path.Combine(root,"languages");Directory.CreateDirectory(folder);
                File.WriteAllText(Path.Combine(folder,"BUILD.json"),"{\"language_packs\":true}");
                string pak=Core.PathIn(folder,"Data/pak");Directory.CreateDirectory(pak);
                foreach(string suffix in new[]{"e","f","g","i","s"})File.WriteAllText(Path.Combine(pak,"sounds_"+suffix+".pak"),"P5CK fixture "+suffix);
                var before=Directory.GetFiles(pak).ToDictionary(f=>f,File.ReadAllBytes);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var picker=Find<ComboBox>(page.View,"LanguagePack");var save=Find<Button>(page.View,"SettingsSaveButton");
                Need(picker.Items.Count==5 && picker.SelectedIndex==0,"Language default or choices differ");
                picker.SelectedIndex=1;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!File.Exists(Core.PathIn(folder,LauncherPlatform.Settings(folder)[0])),"Cancelled language was saved");
                foreach(int i in new[]{1,2,3,4,0}) {
                    picker.SelectedIndex=i;Click(save);
                    Need(!save.IsEnabled,"Language draft remains dirty");
                    Need(GameSettings.Load(folder,null).LanguagePack==GameSettings.LanguagePacks[i],"Language did not round trip");
                    var info=GameSettings.StartInfo(folder);
                    Need(info.EnvironmentVariables["TSFP_GAME_LANGUAGE"]==GameSettings.LanguagePacks[i],"Launch lost selected language");
                    picker.SelectedIndex=(i+1)%5;Need(save.IsEnabled,"Language edit did not enable Save");
                    picker.SelectedIndex=i;Need(!save.IsEnabled,"Reverted language remains dirty");
                }
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);
                Need(Find<ComboBox>(reopened.View,"LanguagePack").SelectedIndex==0,"Return to English was not retained");
                foreach(var entry in before)Need(File.ReadAllBytes(entry.Key).SequenceEqual(entry.Value),"Language selection changed original pack bytes");
            });
            Test("language launch rejects missing packs and unsupported game versions",()=>{
                string folder=Path.Combine(root,"language-errors");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.LanguagePack="fr";model.Save();
                bool rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("missing");}
                Need(rejected,"Missing French archive was silently ignored");
                string pak=Core.PathIn(folder,"Data/pak/sounds_f.pak");Directory.CreateDirectory(Path.GetDirectoryName(pak));File.WriteAllText(pak,"invalid");
                rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("invalid");}
                Need(rejected && File.ReadAllText(pak)=="invalid","Invalid pack was accepted or modified");
                File.WriteAllText(pak,"P5CK fixture");File.WriteAllText(Core.PathIn(folder,"BUILD.json"),"{\"version\":\"v0.4.0\",\"auto_display_resolution\":true}");
                Need(GameSettings.Load(folder,null).LanguagePack=="fr","Older release erased saved choice");
                rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("Update the game");}
                Need(rejected,"Old game silently ignored language choice");
                model=GameSettings.Load(folder,null);model.LanguagePack="en";model.Save();
                Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_GAME_LANGUAGE"]=="en","English failed on an older game");
            });
            Test("language preferences retain concurrent edits and reject unknown codes",()=>{
                string folder=Path.Combine(root,"language-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();var concurrent=GameSettings.Load(folder,null);concurrent.LanguagePack="de";concurrent.Save();
                model.LanguagePack="es";bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected && GameSettings.Load(folder,null).LanguagePack=="de","Concurrent language was overwritten");
                string file=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                string bad=File.ReadAllText(file).Replace("\"de\"","\"../fr\"");File.WriteAllText(file,bad);
                rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                Need(rejected && File.ReadAllText(file)==bad,"Invalid language was accepted or rewritten");
            });
            Test("save manager lists dated restore points and requires restore confirmation",()=>{
                string folder=Path.Combine(root,"saves-page");Directory.CreateDirectory(folder);
                string saves=Core.PathIn(folder,LauncherPlatform.Saves(folder));Directory.CreateDirectory(saves);File.WriteAllText(Path.Combine(saves,"profile"),"original");
                var backup=SaveBackups.Create(folder,"Manual",null,System.Threading.CancellationToken.None);File.WriteAllText(Path.Combine(saves,"profile"),"current");
                var page=new SavesPage(folder,false);
                Need(Find<Button>(page.View,"ChooseSaveFolder")!=null && Find<Button>(page.View,"DefaultSaveFolder")!=null,"Save location controls missing");
                Need(Find<TextBlock>(page.View,"ActiveSaveLocation").Text.Contains(saves),"Active save folder missing");
                Click(Find<Button>(page.View,"DefaultSaveFolder"));
                Need(Find<Button>(page.View,"UseSaveLocation")!=null && Find<Button>(page.View,"CopySaveLocation")!=null,"Folder selection must offer use or copy");
                Click(Find<Button>(page.View,"CancelSaveLocation"));
                Need(page.Choices.Items.Count==1 && ((SaveBackup)page.Choices.SelectedItem).Id==backup.Id && page.Restore.IsEnabled,"Backup list or restore selection missing");
                Click(page.Restore);Need(Find<Button>(page.View,"ConfirmSaveRestore")!=null,"Restore did not ask for confirmation");
                Click(Find<Button>(page.View,"CancelSaveRestore"));Need(File.ReadAllText(Path.Combine(saves,"profile"))=="current","Cancelled restore changed saves");
                var preview=new SavesPage(folder,true);Click(preview.Backup);Need(SaveBackups.List(folder).Length==1,"Preview created a real backup");
            });
            Test("texture verification reports missing and changed parts without modifying custom files",()=>{
                string folder=Path.Combine(root,"texture-health");Directory.CreateDirectory(Path.Combine(folder,"Textures"));
                string texture=Core.PathIn(folder,"Textures/0123456789abcdef.dds"),meta=Path.ChangeExtension(texture,".json");
                File.WriteAllText(texture,"texture fixture");File.WriteAllText(meta,"metadata fixture");File.WriteAllText(Core.PathIn(folder,"Textures/custom.dds"),"custom");
                var pack=new TextureRelease {Version="0.1.0-beta.1",Count=2};pack.Parts.Add(new Release{AssetName="textures-001.zip",Size=111});pack.Parts.Add(new Release{AssetName="textures-002.zip",Size=222});
                pack.Files.Add("textures-001.zip",new Dictionary<string,string>{{Path.GetFileName(texture),Core.Hash(texture)},{Path.GetFileName(meta),Core.Hash(meta)}});
                pack.Files.Add("textures-002.zip",new Dictionary<string,string>{{"fedcba9876543210.dds",Core.Hash(texture)},{"fedcba9876543210.json",Core.Hash(meta)}});
                var report=TextureDownload.Verify(folder,pack,System.Threading.CancellationToken.None);
                Need(report.Matched==2 && report.Missing==2 && report.Changed==0 && report.DownloadBytes==222,"Missing-file verification or remaining size differs");
                File.WriteAllText(texture,"edited");report=TextureDownload.Verify(folder,pack,System.Threading.CancellationToken.None);
                Need(report.Changed==1 && report.DownloadBytes==333 && File.ReadAllText(texture)=="edited" && File.ReadAllText(Core.PathIn(folder,"Textures/custom.dds"))=="custom","Verification changed files or missed modified content");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),"{\"version\":\"0.1.0-beta.1\"}");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var details=Find<TextBlock>(page.View,"TexturePackDetails");Need(details.Text.Contains("Installed pack: 0.1.0-beta.1") && details.Text.Contains("Original textures"),"Installed version or saved mode missing");
                Find<CheckBox>(page.View,"EnhancedTextures").IsChecked=true;Need(details.Text.Contains("unsaved changes"),"Unsaved texture selection not identified");
                Click(Find<Button>(page.View,"SettingsSaveButton"));Need(details.Text.Contains("Next launch (saved): Enhanced textures (Beta)") && !details.Text.Contains("unsaved changes"),"Saved texture mode not refreshed");
                Click(Find<Button>(page.View,"VerifyTexturePack"));Need(Find<TextBlock>(page.View,"TexturePackStatus").Text.Contains("Preview only"),"Preview verification performed live work");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),"broken receipt");
                Need(TextureDownload.InstalledVersion(folder).Contains("attention") && File.ReadAllText(Core.PathIn(folder,".launcher/texture-pack.json"))=="broken receipt","Malformed receipt blocked settings or was overwritten");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),new string('x',5000));Need(TextureDownload.InstalledVersion(folder).Contains("attention"),"Damaged receipt blocked settings or hid the error");
            });
            Test("Preview groups named feature tracks and remembers the selected test across Stable",()=>{
                var picker=new GameChannelPickerPage();int changes=0;picker.Changed+=()=>changes++;
                var releases=new[]{new Release {Tag="v0.4.0-beta.1",Channel="beta",ChannelName="Beta"},
                    new Release {Tag="v0.4.0-story-coop.1",Channel="story-coop",ChannelName="Story Co-op Preview"}};
                picker.SetReleases(releases,"stable","story-coop");
                Need(picker.Channel.Items.Count==2&&picker.Selected=="stable"&&!picker.ShowingTracks&&changes==0,"Top-level channels must be Stable and Preview only");
                picker.Channel.SelectedIndex=1;
                Need(picker.ShowingTracks&&picker.Selected=="story-coop"&&changes==1,"Preview did not restore the remembered feature track");
                Need(picker.Track.Items.Cast<GameChannelChoice>().Select(c=>c.Name).SequenceEqual(new[]{"Beta","Story Co-op"}),"Named tests were not grouped under Preview");
                picker.Track.SelectedItem=picker.Track.Items.Cast<GameChannelChoice>().First(c=>c.Id=="beta");
                Need(picker.Selected=="beta"&&changes==2,"Beta did not select its own track");
                picker.Channel.SelectedIndex=0;picker.Channel.SelectedIndex=1;
                Need(picker.Selected=="beta"&&changes==4,"Switching through Stable lost the selected preview track");
                picker.SetReleases(new Release[0],"story-coop");
                Need(picker.Selected=="story-coop"&&picker.ShowingTracks&&changes==4,"Unavailable tracks must remain selected without triggering checks");
                picker.SetEnabled(false);Need(!picker.Channel.IsEnabled&&!picker.Track.IsEnabled,"Busy state must lock both selectors");
            });
            Test("game channels select independent series and permit explicit return to Stable",()=>{
                var records=new[]{ChannelRecord("v0.4.1"),ChannelRecord("v0.5.0-story-coop.2",true),ChannelRecord("v0.6.0-other.8",true),ChannelRecord("v0.5.0-beta.1",true),ChannelRecord("v0.3.1-alpha.2",true),ChannelRecord("v0.6.0-preview.99",true)};
                var releases=ReleaseClient.GameVersions(Json.Encode(records)).Releases;
                Need(GameChannels.Select(releases,"story-coop").Tag=="v0.5.0-story-coop.2","Experiment escaped its channel");
                Need(GameChannels.Select(releases,"beta").Tag=="v0.5.0-beta.1","Beta absorbed experiments");
                var stable=GameChannels.Select(releases,"stable");Need(stable.Tag=="v0.4.1","Stable absorbed preview");
                Need(GameChannels.CanInstall(stable,"v0.5.0-story-coop.2"),"Returning to Stable must allow a lower version");
                Need(!GameChannels.CanInstall(stable,"v0.4.2"),"Same-channel downgrade must remain explicit history selection");
                Need(releases.Any(r=>r.Tag=="v0.3.1-alpha.2"&&r.Channel=="legacy"&&r.ChannelName=="Legacy Preview"),"Historical alpha was relabeled Beta");
                Need(releases.Any(r=>r.Tag=="v0.6.0-preview.99"&&r.Channel=="legacy"),"Historical preview was relabeled Beta");
                Need(!GameChannels.Choices(releases,"beta").Any(c=>c.Id=="legacy"),"Legacy versions must stay in history, outside testing tracks");
                Need(GameChannels.Select(releases,"withdrawn")==null,"Missing experiment must not substitute Stable");
                Need(GameChannels.Select(releases,"legacy").Channel=="legacy","Exact legacy selection was redirected to Beta");
                Need(GameChannels.Choices(releases,"withdrawn").Any(c=>c.Id=="withdrawn"),"Unavailable selected channel was lost");
            });
            Test("Story co-op beta joins its existing track while retaining exact tag metadata",()=>{
                var record=ChannelRecord("v0.5.0-story-coop-beta.1",true);
                var metadata=new Dictionary<string,object>{{"schema",1},{"channel","story-coop-beta"},{"name","Story Co-op Beta Preview"},{"source_commit",new string('a',40)},{"minimum_launcher","1.3.0"}};
                record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->\nBeta notes";
                var releases=ReleaseClient.GameVersions(Json.Encode(new[]{record,ChannelRecord("v0.4.0-story-coop.1",true),ChannelRecord("v0.5.0-beta.1",true),ChannelRecord("v0.6.0-other-beta.1",true)})).Releases;
                var selected=GameChannels.Select(releases,"story-coop");
                Need(selected.Tag=="v0.5.0-story-coop-beta.1"&&selected.Channel=="story-coop"&&selected.ChannelName=="Story Co-op Preview","Beta was split from the existing Story track");
                Need(GameChannels.Select(releases,"story-coop-beta")==selected,"Saved beta track did not resolve to Story co-op");
                var choices=GameChannels.Choices(releases,"story-coop-beta");
                Need(choices.Count(c=>c.Id=="story-coop")==1&&!choices.Any(c=>c.Id=="story-coop-beta"),"Duplicate Story tracks were offered");
                Need(GameChannels.Select(releases,"beta").Tag=="v0.5.0-beta.1"&&GameChannels.Select(releases,"other-beta").Tag=="v0.6.0-other-beta.1","Unrelated beta tracks were merged");
                Need(GameChannels.FromTag(selected.Tag)=="story-coop"&&!GameChannels.CanInstall(selected,selected.Tag),"Installed beta identity was lost");
                metadata["channel"]="story-coop";record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Alias accepted metadata that did not match its immutable tag");
            });
            Test("Story beta preferences migrate without writing on read or losing unrelated settings",()=>{
                string folder=Path.Combine(root,"story-beta-channels");Directory.CreateDirectory(folder);
                string path=Core.PathIn(folder,".launcher/preferences.json");
                Core.Write(path,Json.Encode(new {game_channel="story-coop-beta",game_preview_track="story-coop-beta",launcher_preview=true,custom="keep"}));
                string original=File.ReadAllText(path);
                Need(GameChannels.Read(folder)=="story-coop"&&GameChannels.ReadPreviewTrack(folder)=="story-coop"&&File.ReadAllText(path)==original,"Reading beta preferences changed the file or lost its track");
                GameChannels.Save(folder,"story-coop-beta");var prefs=Json.Object(File.ReadAllText(path));
                Need(Json.String(prefs,"game_channel")=="story-coop"&&Json.String(prefs,"game_preview_track")=="story-coop"&&Json.String(prefs,"custom")=="keep"&&SelfUpdate.PreviewChannel(folder),"Migrating Story beta lost preferences");
                GameChannels.Save(folder,"stable","story-coop-beta");
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="story-coop","Stable lost the remembered Story track");
            });
            Test("game preview preferences migrate without enrolling launcher previews or losing unrelated preferences",()=>{
                string folder=Path.Combine(root,"channels");Directory.CreateDirectory(folder);
                Need(GameChannels.Read(folder)=="stable","Fresh installs must use Stable");
                string path=Core.PathIn(folder,".launcher/preferences.json");
                Core.Write(path,Json.Encode(new {preview=true,custom="keep"}));
                Need(GameChannels.Read(folder)=="beta"&&!SelfUpdate.PreviewChannel(folder),"Old Preview must migrate to Beta without enrolling launcher previews");
                Core.Write(path,Json.Encode(new {game_channel="preview",game_preview_track="preview",custom="keep"}));
                Need(GameChannels.Read(folder)=="beta"&&GameChannels.ReadPreviewTrack(folder)=="beta","Explicit general-preview preferences were not migrated");
                Core.Write(path,Json.Encode(new {game_channel="stable",game_preview_track="preview",custom="keep"}));
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="beta","Stable must retain a migrated Beta track");
                GameChannels.Save(folder,"story-coop");var prefs=Json.Object(File.ReadAllText(path));
                Need(GameChannels.Read(folder)=="story-coop"&&GameChannels.ReadPreviewTrack(folder)=="story-coop"&&Json.String(prefs,"custom")=="keep"&&!Json.Flag(prefs,"preview"),"Channel save lost preferences");
                GameChannels.Save(folder,"stable");
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="story-coop","Stable lost the remembered preview track");
                bool rejected=false;try{GameChannels.Save(folder,"../outside");}catch(InvalidDataException){rejected=true;}Need(rejected,"Unsafe channel ID accepted");
            });
            Test("release channel metadata is checked and hidden from visible notes",()=>{
                var record=ChannelRecord("v0.5.0-story-coop.1",true);
                var metadata=new Dictionary<string,object>{{"schema",1},{"channel","story-coop"},{"name","Story Co-op Preview"},{"source_commit",new string('a',40)},{"minimum_launcher","1.3.0"}};
                record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->\nHuman release notes";
                var release=ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Single();
                Need(release.ChannelName=="Story Co-op Preview"&&release.MinimumLauncher=="1.3.0"&&!release.Notes.Contains("tsfp-release"),"Metadata was lost or displayed");
                record["body"]="<!-- tsfp-release {bad json} -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Malformed metadata was accepted");
                metadata["channel"]="other";record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Mismatched metadata was accepted");
            });
            Test("optional texture beta toggle saves a launch preference without changing textures",()=>{
                string folder=Path.Combine(root,"textures-toggle");Directory.CreateDirectory(Path.Combine(folder,"Textures"));
                string texture=Path.Combine(folder,"Textures/0123456789abcdef.dds");File.WriteAllText(texture,"untouched texture fixture");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var toggle=Find<CheckBox>(page.View,"EnhancedTextures");
                Need(toggle.IsChecked==false && Convert.ToString(toggle.Content).Contains("Beta"),"Classic default or Beta label missing");
                Need(!Find<Button>(page.View,"DownloadTexturePack").IsEnabled,"Download needs a checked manifest first");
                Need(Find<CheckBox>(page.View,"TextureUpdateChecks").IsChecked==false,"Fresh install unexpectedly opted in");
                Click(Find<Button>(page.View,"CheckTexturePack"));
                Need(Find<TextBlock>(page.View,"TexturePackStatus").Text.Contains("Preview only"),"Preview initiated a network check");
                Need(Find<TextBlock>(page.View,"EnhancedTexturesHint").Text.Contains("incomplete"),"Missing beta limitation");
                toggle.IsChecked=true;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!Directory.Exists(Path.Combine(folder,"Settings")),"Cancelled texture draft was saved");
                foreach(bool enabled in new[]{true,false}) {
                    toggle.IsChecked=enabled;Click(Find<Button>(page.View,"SettingsSaveButton"));
                    Need(GameSettings.Load(folder,null).EnhancedTextures==enabled,"Texture preference did not persist");
                    Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_TEXTURE_MODE"]==(enabled?"4k":"classic"),"Runtime texture mode differs from saved choice");
                    Need(File.ReadAllText(texture)=="untouched texture fixture","Toggle modified texture assets");
                    var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(1);
                    Need(Find<CheckBox>(reopened.View,"EnhancedTextures").IsChecked==enabled,"Reopened toggle lost saved choice");
                }
                var saved=GameSettings.Load(folder,null);saved.EnhancedTextures=true;saved.NativeFps=120;saved.Save();
                saved=GameSettings.Load(folder,null);saved.VerticalFov=70;saved.Save();
                Need(GameSettings.Load(folder,null).EnhancedTextures && GameSettings.Load(folder,null).NativeFps==120,"Unrelated save reset preferences");
            });
            Test("texture preference works without a pack and rejects malformed saved choices",()=>{
                string folder=Path.Combine(root,"textures-no-pack");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.EnhancedTextures=true;model.Save();
                Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_TEXTURE_MODE"]=="4k","Missing pack blocked selection instead of runtime fallback");
                Need(!Directory.Exists(Path.Combine(folder,"Textures")),"Saving tried to install textures");
                string controls=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                var doc=Json.Object(File.ReadAllText(controls));((Dictionary<string,object>)doc["launcher"])["enhanced_textures"]="yes";
                string invalid=Json.Encode(doc);File.WriteAllText(controls,invalid);bool rejected=false;
                try {GameSettings.Load(folder,null);}catch{rejected=true;}
                Need(rejected && File.ReadAllText(controls)==invalid,"Malformed preference was accepted or rewritten");
            });
            Test("saved settings disable Save until a persisted draft changes",()=>{
                string folder=Path.Combine(root,"save-button");Directory.CreateDirectory(folder);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);
                var save=Find<Button>(page.View,"SettingsSaveButton");int saves=0;page.Saved+=()=>saves++;
                Click(save);Need(!save.IsEnabled && saves==1,"Successful save did not disable Save");
                Click(save);Need(saves==1,"Disabled Save rewrote settings");
                for(int tab=0;tab<3;tab++){page.SelectTab(tab);Need(!save.IsEnabled,"Tab navigation enabled Save");}
                var flags=System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic;
                Func<string,object> field=name=>page.GetType().GetField(name,flags).GetValue(page);
                foreach(string name in new[]{"sensitivity","width","height","discordId"}) {
                    var input=(TextBox)field(name);string before=input.Text;
                    input.Text=before+"0";Need(save.IsEnabled,name+" did not enable Save");
                    input.Text=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                foreach(string name in new[]{"enabled","invert","fullscreen","vrr","fps","skipSplashes","discordEnabled","enhancedTextures"}) {
                    var check=(CheckBox)field(name);bool? before=check.IsChecked;
                    check.IsChecked=before!=true;Need(save.IsEnabled,name+" did not enable Save");
                    check.IsChecked=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                foreach(string name in new[]{"device","scale","filtering","frameRate"}) {
                    var choice=(ComboBox)field(name);int before=choice.SelectedIndex;
                    choice.SelectedIndex=(before+1)%choice.Items.Count;Need(save.IsEnabled,name+" did not enable Save");
                    choice.SelectedIndex=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                var fov=(Slider)field("fov");double original=fov.Value;
                fov.Value=original+1;Need(save.IsEnabled,"FOV did not enable Save");
                fov.Value=original;Need(!save.IsEnabled,"Reverted FOV remained dirty");
                var automatic=(CheckBox)field("textureUpdates");automatic.IsChecked=automatic.IsChecked!=true;
                Need(!save.IsEnabled,"Immediate texture-check preference enabled deferred Save");
                var inputSensitivity=(TextBox)field("sensitivity");string valid=inputSensitivity.Text;
                inputSensitivity.Text="invalid";Click(save);
                Need(save.IsEnabled && saves==1,"Failed validation disabled Save or reported success");
                inputSensitivity.Text=valid;Need(!save.IsEnabled,"Restored saved input remained dirty");
                page.SelectTab(0);var binding=All(page.View).OfType<Button>().First(b=>Tip(b)==GameSettings.Labels[0]+", binding 1");
                Click(binding);Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Clear binding"));
                Need(save.IsEnabled,"Binding change did not enable Save");Click(save);
                Need(!save.IsEnabled && saves==2 && GameSettings.Load(folder,null).Binding(0)[0]=="none","Binding save did not disable Save or persist");
                Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Restore default controls"));
                Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Restore defaults"));
                Need(save.IsEnabled,"Restoring default controls did not enable Save");Click(save);
                Need(!save.IsEnabled && saves==3,"Saving defaults did not disable Save");
                string controls=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                string external=File.ReadAllText(controls)+" ";File.WriteAllText(controls,external);
                fov.Value=original+2;Click(save);
                Need(save.IsEnabled && saves==3 && File.ReadAllText(controls)==external,"Conflicting save disabled retry or overwrote external settings");
            });
            Test("controller deadzones preserve legacy and tune each player",()=>{
                string folder=Path.Combine(root,"deadzone-settings");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);
                Need(model.ControllerDeadzone(0,false)==7849 && model.ControllerDeadzone(0,true)==7849,"Default deadzone changed");
                model.SetControllerDeadzone(1,true,1638);model.SetControllerDeadzone(2,false,0);model.Save();
                model=GameSettings.Load(folder,null);
                Need(model.ControllerDeadzone(1,true)==1638 && model.ControllerDeadzone(1,false)==7849 && model.ControllerDeadzone(2,false)==0,"Independent deadzones did not persist");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);
                var aim=Find<Slider>(page.View,"Player2AimDeadzone");var save=Find<Button>(page.View,"SettingsSaveButton");
                for(int player=1;player<=4;player++)
                    Need(Find<Expander>(page.View,"Player"+player+"DeadzoneSection").IsExpanded==(player==1),"Wrong default expanded deadzone player");
                bool dirty=save.IsEnabled;
                var firstPlayer=Find<Expander>(page.View,"Player1DeadzoneSection");
                var secondPlayer=Find<Expander>(page.View,"Player2DeadzoneSection");
                firstPlayer.IsExpanded=false;secondPlayer.IsExpanded=true;
                Need(save.IsEnabled==dirty,"Expanding or collapsing a player changed the settings draft");
                Need(aim.Value==5,"Aim percentage incorrect");
                Need(aim.Minimum==0 && aim.Maximum==100 && aim.TickFrequency==1 && aim.IsSnapToTickEnabled,"Deadzone slider bounds/steps changed");
                Need(Find<TextBlock>(page.View,"Player2AimDeadzoneValue").Text=="5%","Initial percentage missing");
                aim.Value=8;Need(Find<TextBlock>(page.View,"Player2AimDeadzoneValue").Text=="8%","Live percentage missing");Need(save.IsEnabled,"Deadzone edit not dirty");
                secondPlayer.IsExpanded=false;secondPlayer.IsExpanded=true;
                Need(aim.Value==8,"Collapsing a player lost a draft deadzone");Click(save);
                model=GameSettings.Load(folder,null);Need(model.ControllerDeadzone(1,true)==2621 && model.ControllerDeadzone(1,false)==7849,"UI deadzone save failed");
                model.Sensitivity=.15;model.Save();model=GameSettings.Load(folder,null);
                Need(model.ControllerDeadzone(1,true)==2621 && model.ControllerDeadzone(2,false)==0,"Unrelated save lost deadzones");
            });
            Test("settings drafts, FOV reset, cancel and save",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);page.SelectTab(1);
                var fov=Find<Slider>(page.View,"VerticalFovSlider");var number=Find<TextBlock>(page.View,"VerticalFovValue");
                fov.Value=80;Need(number.Text=="80°","FOV label did not follow slider");
                bool cancelled=false;page.Cancelled+=()=>cancelled=true;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(cancelled&&!Directory.Exists(Path.Combine(root,"Settings")),"Cancel saved a draft");
                Click(Find<Button>(page.View,"ResetFovButton"));Need(fov.Value==55,"FOV reset differs");
                fov.Value=75;Click(Find<Button>(page.View,"SettingsSaveButton"));Need(GameSettings.Load(root,null).VerticalFov==75,"Shared page did not save FOV");
            });
            Test("binding capture, conflict cancel, reassignment and clear",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);

                var binding=All(page.View).OfType<Button>().First(b=>Tip(b)==GameSettings.Labels[0]+", binding 1");
                Func<string,Button> action=label=>All(page.View).OfType<Button>().Single(b=>b.Name!="SettingsCancelButton"&&Convert.ToString(b.Content)==label);
                string original=Convert.ToString(binding.Content);Click(binding);Need(Find<Border>(page.View,"BindingCaptureArea")!=null,"No capture prompt");
                Click(action("Bind Escape"));Click(action("Cancel"));Need(Convert.ToString(binding.Content)==original,"Cancelled conflict changed binding");
                Click(binding);Click(action("Bind Escape"));Click(action("Move binding"));Need(Convert.ToString(binding.Content)=="Escape","Conflict move failed");
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(root,null);
                Need(GameSettings.KeyCode(saved.Binding(0)[0])==27&&saved.Binding(6).All(t=>GameSettings.KeyCode(t)!=27),"Conflict was not removed from previous action");
                Click(binding);Click(action("Clear binding"));Need(Convert.ToString(binding.Content)=="Unbound","Clear did not update draft");
            });
            Test("Discord uses the same settings model and preserves other settings",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);page.SelectTab(2);
                Find<CheckBox>(page.View,"DiscordPresenceEnabled").IsChecked=true;Find<TextBox>(page.View,"DiscordApplicationId").Text="123456789012345678";
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(root,null);
                Need(saved.DiscordEnabled&&saved.DiscordApplicationId=="123456789012345678"&&saved.VerticalFov==75,"Discord save changed other settings");
            });
            Test("both launchers migrate retired timing preferences",()=>{
                string shared=Path.Combine(root,"high-refresh");Directory.CreateDirectory(Path.Combine(shared,"Settings"));
                File.WriteAllText(Path.Combine(shared,"Settings/runtime.conf"),"frame_interp = 2\nframe_target = 144\nrefresh_rate = 120\nframe_cap = 0\n");
                var page=new SettingsPage(new ResourceDictionary(),shared,true);page.SelectTab(1);
                Find<Slider>(page.View,"VerticalFovSlider").Value=70;
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(shared,null);
                Need(saved.VerticalFov==70 && saved.PresentationFps==0 && saved.RefreshRate==0 && saved.Interpolation==1 && saved.FrameCap=="60","Retired timing settings survived migration");
            });
            Test("Developer Tools opt-in saves with an inline warning and configured shortcut",()=>{
                string folder=Path.Combine(root,"developer-tools");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Assign(34,0,114,true);model.Save();
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var toggle=Find<CheckBox>(page.View,"DeveloperToolsEnabled");
                var save=Find<Button>(page.View,"SettingsSaveButton");
                Click(save);Need(toggle.IsChecked==false && !save.IsEnabled,"Tools must default OFF with a clean saved draft");
                var warning=Find<TextBlock>(page.View,"DeveloperToolsWarning");
                var section=All(page.View).OfType<StackPanel>().Single(p=>p.Children.Contains(toggle));
                Need(section.Children.Contains(warning)&&warning.Text.Contains("Use with caution."),"Warning must be inline beside the setting before opt-in");
                Need(Find<TextBlock>(page.View,"DeveloperToolsShortcut").Text.Contains("Press F3 to open"),"Inline shortcut ignores configured binding");
                toggle.IsChecked=true;
                Need(!All(page.View).OfType<Button>().Any(b=>String.Equals(b.Content,"Continue")),"Developer Tools still opens a warning prompt");
                Need(toggle.IsEnabled&&save.IsEnabled,"Inline opt-in must leave settings available and enable Save");Click(save);
                Need(GameSettings.Load(folder,null).DeveloperToolsEnabled && !save.IsEnabled,"Tools ON not saved");
                toggle.IsChecked=false;Need(save.IsEnabled,"Tools OFF did not enable Save");Click(save);
                var saved=GameSettings.Load(folder,null);
                Need(!saved.DeveloperToolsEnabled && GameSettings.KeyCode(saved.Binding(34)[0])==114,"Tools OFF lost shortcut");
            });
            Test("beta audio opt-in, draft state and game-menu synchronization",()=>{
                string folder=Path.Combine(root,"enhanced-audio");Directory.CreateDirectory(folder);
                string sidecar=Path.Combine(folder,"Settings/pc-audio.ini");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var toggle=Find<CheckBox>(page.View,"EnhancedAudio");
                Need(toggle.IsChecked==false,"Enhanced Audio must default OFF");
                Need(Convert.ToString(toggle.Content).Contains("Beta"),"Audio toggle lacks beta label");
                Need(Find<TextBlock>(page.View,"EnhancedAudioBetaNote").Text.Contains("game build"),"Compatibility hint missing");
                toggle.IsChecked=true;Click(Find<Button>(page.View,"SettingsSaveButton"));
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 1\n" && GameSettings.Load(folder,null).EnhancedAudio,"Launcher/game formats differ");
                var save=Find<Button>(page.View,"SettingsSaveButton");Need(!save.IsEnabled,"Saved audio draft remains dirty");
                toggle.IsChecked=false;Need(save.IsEnabled,"Audio edit did not enable Save");Click(save);
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 0\n","Audio OFF did not save");
                File.WriteAllText(sidecar,"TSFP_AUDIO_V1 1\n"); // Simulate the in-game menu.
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(3);
                Need(Find<CheckBox>(reopened.View,"EnhancedAudio").IsChecked==true,"In-game audio choice not reflected");
                var model=GameSettings.Load(folder,null);model.Sensitivity=.2;model.Save();
                Need(GameSettings.Load(folder,null).EnhancedAudio,"Unrelated save reset the audio choice");
            });
            Test("individual audio controls retain their choices across the master switch",()=>{
                string folder=Path.Combine(root,"audio-features");Directory.CreateDirectory(folder);
                string sidecar=Path.Combine(folder,"Settings/pc-audio.ini");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var master=Find<CheckBox>(page.View,"EnhancedAudio");
                var range=Find<CheckBox>(page.View,"ExtendedGunfire");
                var environment=Find<CheckBox>(page.View,"EnvironmentalAudio");
                Need(!range.IsEnabled && range.IsChecked==true,"Default child preference was lost while OFF");
                master.IsChecked=true;range.IsChecked=false;environment.IsChecked=false;
                Click(Find<Button>(page.View,"SettingsSaveButton"));
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V2 1 1 0 0 1\n","Custom audio format differs from game");
                master.IsChecked=false;Click(Find<Button>(page.View,"SettingsSaveButton"));
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V2 0 1 0 0 1\n" && !range.IsEnabled,"Master OFF reset children");
                var model=GameSettings.Load(folder,null);model.Sensitivity=.2;model.Save();
                Need(!model.ExtendedGunfire && !model.EnvironmentalAudio && model.SpatialAudio && model.AmbientAudio,"Unrelated save lost custom choices");
                File.WriteAllText(sidecar,"TSFP_AUDIO_V2 1 0 1 1 0\n");
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(3);
                Need(Find<CheckBox>(reopened.View,"SpatialAudio").IsChecked==false && Find<CheckBox>(reopened.View,"AmbientAudio").IsChecked==false,"Game-side custom settings were not loaded");
            });
            Test("beta audio invalid and concurrent preferences are preserved",()=>{
                string folder=Path.Combine(root,"enhanced-audio-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                string sidecar=Path.Combine(folder,"Settings/pc-audio.ini");
                File.WriteAllText(sidecar,"TSFP_AUDIO_V1 1\n");
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 1\n","Concurrent in-game change was overwritten");
                foreach(string invalid in new[]{"TSFP_AUDIO_V1 2","TSFP_AUDIO_V2 1","TSFP_AUDIO_V1 1 extra", "TSFP_AUDIO_V2 1 0 1 0 2","TSFP_AUDIO_V2 1 0 1 0 1 extra"}) {
                    File.WriteAllText(sidecar,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(sidecar)==invalid,"Invalid audio preference was accepted or rewritten");
                }
            });
            Test("beta audio failure rolls back the settings transaction",()=>{
                string folder=Path.Combine(root,"enhanced-audio-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[5]+".bak");model.EnhancedAudio=true;model.VerticalFov=75;
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected,"Blocked audio backup should reject Save");
                for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(before[i]),"Partial settings commit survived audio failure");
            });
            Test("audio volume sliders, category round trips and unchanged enhancement preference",()=>{
                string folder=Path.Combine(root,"audio-mix");Directory.CreateDirectory(folder);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var save=Find<Button>(page.View,"SettingsSaveButton");
                for(int i=0;i<6;i++) {
                    var slider=Find<Slider>(page.View,"Audio"+GameSettings.AudioVolumeLabels[i]+"Volume");
                    Need(slider.Value==100 && slider.IsEnabled,"Volume must default to 100 and work with enhancement OFF");
                    slider.Value=i*17;Need(save.IsEnabled,"Volume edit was not tracked");
                }
                Click(save);Need(!save.IsEnabled,"Saved volume draft is dirty");
                var model=GameSettings.Load(folder,null);
                Need(!model.EnhancedAudio,"Volumes enabled the enhancement master");
                Need(model.AudioVolumes.SequenceEqual(new[]{0,17,34,51,68,85}),"Volume order/values lost");
                string file=Core.PathIn(folder,LauncherPlatform.Settings(folder)[7]);
                Need(File.ReadAllText(file)=="TSFP_AUDIO_MIX_V1 0 17 34 51 68 85\n","Game/launcher mix format mismatch");
                model.Sensitivity=.2;model.Save();
                Need(GameSettings.Load(folder,null).AudioVolumes.SequenceEqual(new[]{0,17,34,51,68,85}),"Unrelated save reset volume");
                model.AudioVolumes[0]=101;bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected,"Out-of-range gain was saved");
                model=GameSettings.Load(folder,null);File.WriteAllText(file,"TSFP_AUDIO_MIX_V1 100 100 100 100 100 100\n");
                rejected=false;try{model.Save();}catch{rejected=true;}Need(rejected,"External mix edit was overwritten");
                foreach(string invalid in new[]{"", "TSFP_AUDIO_MIX_V2 1 1 1 1 1 1", "TSFP_AUDIO_MIX_V1 01 1 1 1 1 1",
                    "TSFP_AUDIO_MIX_V1 101 1 1 1 1 1", "TSFP_AUDIO_MIX_V1 1 1 1 1 1 1 extra",
                    "TSFP_AUDIO_MIX_V1 1 1 1 1 1", "TSFP_AUDIO_MIX_V1 1 1 1 1 1 1\0", "\ufeffTSFP_AUDIO_MIX_V1 1 1 1 1 1 1"}) {
                    File.WriteAllText(file,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllBytes(file).SequenceEqual(System.Text.Encoding.UTF8.GetBytes(invalid)),"Malformed mix settings were changed or accepted: "+invalid);
                }
            });
            Test("dialogue focus and impact range migrate, retain volumes and remain optional",()=>{
                string folder=Path.Combine(root,"audio-listening");Directory.CreateDirectory(folder);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var focus=Find<CheckBox>(page.View,"DialogueFocus");var range=Find<ComboBox>(page.View,"ImpactRange");
                var save=Find<Button>(page.View,"SettingsSaveButton");
                Need(focus.IsChecked==false && range.SelectedIndex==0,"Listening changes must be opt-in");
                for(int duck=0;duck<=1;duck++)for(int mode=0;mode<=2;mode++) {
                    focus.IsChecked=duck!=0;range.SelectedIndex=mode;
                    if(save.IsEnabled)Click(save);
                    var model=GameSettings.Load(folder,null);
                    Need(model.DialogueFocus==(duck!=0) && model.ImpactRange==mode,"Listening choices not saved");
                    Need(model.AudioVolumes.All(v=>v==100) && !model.EnhancedAudio,"Listening changed gain or enhancements");
                }
                var retained=GameSettings.Load(folder,null);retained.Sensitivity=.2;retained.Save();
                Need(GameSettings.Load(folder,null).DialogueFocus && GameSettings.Load(folder,null).ImpactRange==2,"Unrelated save lost listening choices");
                string file=Core.PathIn(folder,LauncherPlatform.Settings(folder)[7]);
                Need(File.ReadAllText(file)=="TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 1 2\n","Native/launcher listening formats differ");
                retained.DialogueFocus=false;retained.ImpactRange=0;retained.Save();
                Need(File.ReadAllText(file)=="TSFP_AUDIO_MIX_V1 100 100 100 100 100 100\n","Original choices did not retain V1 compatibility");
                foreach(string invalid in new[]{"TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 2 0","TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 1 3",
                    "TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 01 0", "TSFP_AUDIO_MIX_V2 100 100 100 100 100 100 1 2 extra"}) {
                    File.WriteAllText(file,invalid);bool rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(file)==invalid,"Malformed listening settings accepted or changed");
                }
            });
            Test("late audio mix failure restores every earlier settings file",()=>{
                string folder=Path.Combine(root,"audio-mix-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[7]+".bak");model.AudioVolumes[3]=40;model.VerticalFov=75;
                bool rejected=false;try{model.Save();}catch{rejected=true;}Need(rejected,"Mix backup failure was ignored");
                for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(before[i]),"Mix failure left a partial save");
            });
            Test("native frame-rate defaults, beta selection and game-menu synchronization",()=>{
                string folder=Path.Combine(root,"native-fps");Directory.CreateDirectory(Path.Combine(folder,"Settings"));
                string sidecar=Path.Combine(folder,"Settings/pc-frame-rate.ini");
                File.WriteAllText(Path.Combine(folder,"Settings/runtime.conf"),"native_fps = 144\nfuture_setting = retained\n");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var picker=Find<ComboBox>(page.View,"NativeFrameRate");
                Need(picker.Items.Count==4 && picker.SelectedIndex==0,"High refresh must remain opt-in");
                Need(Find<TextBlock>(page.View,"NativeFrameRateBetaNote").Text.Contains("Arcade"),"Missing beta limitations");
                picker.SelectedIndex=2;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!File.Exists(sidecar),"Cancelled draft changed the game preference");
                foreach(int choice in new[]{3,2,1,0}) {
                    picker.SelectedIndex=choice;Click(Find<Button>(page.View,"SettingsSaveButton"));
                    int rate=GameSettings.NativeFrameRates[choice];
                    Need(GameSettings.Load(folder,null).NativeFps==rate,"Saved rate did not round trip");
                    Need(File.ReadAllText(sidecar)=="TSFP_FRAME_RATE_V1 "+rate+"\n","Game-menu sidecar format differs");
                    var info=GameSettings.StartInfo(folder);
                    Need(info.EnvironmentVariables["TSFP_NATIVE_FPS"]==rate.ToString(),"Launch did not select the saved timing mode");
                    Need(info.EnvironmentVariables["RECOMP_FRAME_INTERP"]=="1" && info.EnvironmentVariables["RECOMP_FRAME_TARGET"]=="0" && info.EnvironmentVariables["RECOMP_FPS_CAP"]=="60","Retired presentation settings were reactivated");
                }
                Need(File.ReadAllText(Path.Combine(folder,"Settings/runtime.conf")).Contains("future_setting = retained"),"Unrelated setting lost");
                File.WriteAllText(sidecar,"TSFP_FRAME_RATE_V1 120\n"); // Simulate in-game settings.
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(1);
                Need(Find<ComboBox>(reopened.View,"NativeFrameRate").SelectedIndex==1,"In-game choice not reflected");
                Find<Slider>(reopened.View,"VerticalFovSlider").Value=70;Click(Find<Button>(reopened.View,"SettingsSaveButton"));
                Need(GameSettings.Load(folder,null).NativeFps==120,"Unrelated edit reset frame rate");
            });
            Test("native frame-rate rejects invalid or concurrently changed preferences",()=>{
                string folder=Path.Combine(root,"native-fps-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                string sidecar=Path.Combine(folder,"Settings/pc-frame-rate.ini");
                File.WriteAllText(sidecar,"TSFP_FRAME_RATE_V1 144\n");
                Need(GameSettings.Load(folder,null).NativeFps==120,"Legacy 144 preference did not migrate to 120");
                bool rejected=false;try{model.NativeFps=120;model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(sidecar)=="TSFP_FRAME_RATE_V1 144\n","Concurrent game preference was overwritten");
                foreach(string invalid in new[]{"TSFP_FRAME_RATE_V1 60","TSFP_FRAME_RATE_V1 300","TSFP_FRAME_RATE_V2 144","TSFP_FRAME_RATE_V1 144 extra"}) {
                    File.WriteAllText(sidecar,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(sidecar)==invalid,"Invalid sidecar was accepted or rewritten");
                }
            });
            Test("native frame-rate failure rolls back all earlier settings writes",()=>{
                string folder=Path.Combine(root,"native-fps-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[4]+".bak");model.NativeFps=240;model.VerticalFov=75;
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected,"Blocked backup should reject save");
                for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(before[i]),"Partial settings commit survived");
            });
            Test("news selection, stale notes and toolbar spacing",()=>{
                var page=new WhatsNewPage();page.SetVersions("v1.0.0","1.0.0",false);
                var game=new Release{Tag="v1.1.0",Url=ReleaseClient.ReleasesPage,Notes="## Changes\n- **Shared** release"};
                var launcher=new Release{Tag="1.1.0",Url=ReleaseClient.ReleasesPage+"/tag/launcher-v1.1.0"};
                page.SetNews(new ReleaseNews{Game=game,Launcher=launcher,CheckedUtc=DateTime.UtcNow});
                Need(page.SelectedUrl==game.Url&&page.OnGitHub.IsEnabled,"Game release selection failed");Click(page.LauncherTab);
                Need(page.SelectedUrl==launcher.Url,"Launcher tab did not switch notes");page.SetLoading(true);Need(!page.Refresh.IsEnabled,"Refresh was not locked");page.Failed();
                Need(page.Refresh.IsEnabled&&page.SelectedUrl==launcher.Url&&page.StatusText.Contains("Showing notes checked"),"Failed refresh lost previous notes");
                Need(page.GameTab.Margin.Right>=10&&page.LauncherTab.Margin.Right>=10&&page.Refresh.Margin.Right>=10,"Shared toolbar spacing regressed");
            });
            Test("installed-only notes remain selectable and issue errors retain their age",()=>{
                var page=new WhatsNewPage();page.SetVersions("v1.0.0","1.0.0",false);
                page.SetNews(new ReleaseNews{InstalledGame=new Release{Tag="v1.0.0",Url=ReleaseClient.ReleasesPage},CheckedUtc=DateTime.UtcNow});
                Need(page.Edition.SelectedIndex==1&&page.SelectedUrl!=null,"Installed-only notes were lost");
                page.SetIssues(new KnownIssues{Items=new[]{new KnownIssue{Number=12,Title="Literal <Button>"}},CheckedUtc=DateTime.UtcNow});page.IssuesFailed();
                Need(page.RefreshIssues.IsEnabled&&page.IssuesStatusText.Contains("Showing the list checked"),"Issue refresh failure lost stale status");
            });
            Test("version selection persists across pagination and locks while busy",()=>{
                var page=new GameVersionsPage();page.SetInstalled("v1.0.0");
                page.SetReleases(new GameReleaseList{Releases=new[]{new Release{Tag="v1.0.0",Url=ReleaseClient.ReleasesPage}},HasMore=true},true);
                page.SetReleases(new GameReleaseList{Releases=new[]{new Release{Tag="v1.1.0",Url=ReleaseClient.ReleasesPage}}},false);
                Need(page.Count==2&&page.Selected.Tag=="v1.0.0"&&page.Install.Content.ToString().StartsWith("Reinstall"),"Pagination changed selected version");
                page.SetBusy(true,false);Need(!page.Install.IsEnabled&&!page.Refresh.IsEnabled,"Busy version page accepted changes");page.SetBusy(false,true);Need(!page.Install.IsEnabled,"Running game could be replaced");
                page.SetBusy(false,false);page.SetLoading(true);page.Failed();Need(page.Count==2&&page.Install.IsEnabled,"Failure discarded loaded releases");
            });
            Test("maintenance locks navigation while retaining cancellation",()=>{
                var repair=new MaintenancePage(false,true);var diagnostics=new MaintenancePage(true,true);var area=new MaintenanceArea(repair,diagnostics);
                repair.SetBusy(true,false);area.SetBusy(true);area.Tabs.SelectedIndex=1;
                Need(area.Tabs.SelectedIndex==0&&!repair.VerifyGame.IsEnabled&&repair.Cancel.IsEnabled,"Maintenance did not keep cancel available and lock navigation");
                area.SetBusy(false);area.Tabs.SelectedIndex=1;Need(area.Tabs.SelectedIndex==1,"Maintenance stayed locked");
                repair.SetVerification(new VerificationResult{Version="v1.0.0"},new VerificationResult{Version=LauncherVersion.Value});repair.SetBusy(false,false);
                Need(repair.RepairGame.IsEnabled&&repair.RepairLauncher.IsEnabled,"Verified files cannot be repaired");repair.SetBusy(false,true);Need(!repair.RepairGame.IsEnabled&&!repair.RepairLauncher.IsEnabled,"Repair allowed during play");
            });
            Test("capture health distinguishes current, stale, missing and blocked capture",()=>{
                string folder=Path.Combine(root,"capture-health");Directory.CreateDirectory(folder);
                Need(CrashReports.CaptureHealth(folder).Contains("Game not installed"),"Missing game claimed support");
                string game=Core.PathIn(folder,LauncherPlatform.GamePath(folder));Directory.CreateDirectory(Path.GetDirectoryName(game));File.WriteAllText(game,"fixture");
#if !MACOS && !LINUX
                Need(CrashReports.CaptureHealth(folder).Contains("helper missing"),"Missing helper claimed support");
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(game),"tsfp_crash_helper.exe"),"fixture");
#endif
                Need(CrashReports.CaptureHealth(folder).Contains("no tracked game"),"Idle helper presence claimed live health");
                string id=GameSessions.Begin(folder,"20261024-120000-000");
                using(var process=System.Diagnostics.Process.GetCurrentProcess()) {
                    GameSessions.Started(folder,id,process);
                    Need(CrashReports.CaptureHealth(folder).Contains("has not confirmed"),"Old game claimed a handshake");
                    string path=Core.PathIn(folder,"Logs/Crashes/capture-health.json");
                    Action<string,int,bool> record=(session,pid,armed)=>File.WriteAllText(path,Json.Encode(new {schema=1,session_id=session,pid=process.Id,helper_pid=pid,armed=armed}));
                    record(id,process.Id,true);Need(CrashReports.CaptureHealth(folder).Contains("Helper responding"),"Valid current heartbeat rejected");
                    Need(InstallationMaintenance.Report(folder,null,null,"fixture").Contains("Freeze monitoring armed"),"Diagnostics omitted health");
                    record(id,process.Id,false);Need(CrashReports.CaptureHealth(folder).Contains("Waiting for guest"),"Unarmed capture called armed");
                    record(new string('e',32),process.Id,true);Need(CrashReports.CaptureHealth(folder).Contains("no matching"),"Old session heartbeat accepted");
                    record(id,process.Id,true);File.SetLastWriteTimeUtc(path,DateTime.UtcNow.AddSeconds(-30));Need(CrashReports.CaptureHealth(folder).Contains("stale"),"Stale helper accepted");
                    record(id,process.Id,true);File.SetLastWriteTimeUtc(path,DateTime.UtcNow.AddMinutes(2));Need(CrashReports.CaptureHealth(folder).Contains("stale"),"Future-dated helper accepted");
                    record(id,Int32.MaxValue,true);Need(CrashReports.CaptureHealth(folder).Contains("Degraded"),"Dead helper accepted");
                    record(id,process.Id,true);
                    string marker=Core.PathIn(folder,".launcher/game-session.json");var current=Json.Object(File.ReadAllText(marker));
                    current["game_started_ticks"]=(process.StartTime.ToUniversalTime().Ticks+1).ToString();File.WriteAllText(marker,Json.Encode(current));
                    Need(!CrashReports.CaptureHealth(folder).Contains("Helper responding"),"Reused game PID accepted");
                    current["game_started_ticks"]=process.StartTime.ToUniversalTime().Ticks.ToString();File.WriteAllText(marker,Json.Encode(current));
                    File.WriteAllText(path,"private path and malformed JSON");Need(!CrashReports.CaptureHealth(folder).Contains("private path"),"Health leaked raw metadata");
                    File.WriteAllText(path,new string('x',2049));Need(CrashReports.CaptureHealth(folder).Contains("invalid"),"Oversized record accepted");
                    File.Delete(path);Directory.Delete(Path.GetDirectoryName(path));File.WriteAllText(Path.GetDirectoryName(path),"blocked storage");
                    Need(CrashReports.CaptureHealth(folder).Contains("storage unavailable"),"Blocked storage claimed writable");
                }
            });
            Test("breadcrumb exports are bounded and preserve unknown files",()=>{
                string folder=Path.Combine(root,"breadcrumb-export");Directory.CreateDirectory(folder);
                string name="crash-20261024-120000000-0000000001-123456";
                string report=CrashReports.Folder(folder,name);Directory.CreateDirectory(report);
                File.WriteAllText(Path.Combine(report,"capture.lock"),"");
                File.WriteAllText(Path.Combine(report,"report.json"),"{\"schema\":1,\"kind\":\"suspected_hang\"}");
                File.WriteAllText(Path.Combine(report,"breadcrumbs.txt"),"1 0 scene-id 4\n");
                File.WriteAllText(Path.Combine(report,"private-file"),"private");
                string zip=Path.Combine(folder,"export.zip");CrashReports.Export(folder,name,zip);
                using(var archive=System.IO.Compression.ZipFile.OpenRead(zip))Need(archive.GetEntry("breadcrumbs.txt")!=null && archive.GetEntry("private-file")==null,"Wrong breadcrumb export contents");
                File.WriteAllText(Path.Combine(report,"breadcrumbs.txt"),new string('x',8193));bool rejected=false;
                try{CrashReports.Export(folder,name,Path.Combine(folder,"large.zip"));}catch(System.IO.InvalidDataException){rejected=true;}
                Need(rejected && !File.Exists(Path.Combine(folder,"large.zip")),"Oversized breadcrumb file escaped export cap");
            });
            Console.WriteLine(passed+" shared launcher UI groups passed.");return 0;
        }finally{Directory.Delete(root,true);}
    }
}
