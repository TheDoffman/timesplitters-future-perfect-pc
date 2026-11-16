using System;
using System.IO;
using System.Linq;
using System.Text;
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
    static void WithUiContext(Action action)
    {
#if MACOS || LINUX
        action();
#else
        var previous=System.Threading.SynchronizationContext.Current;
        System.Threading.SynchronizationContext.SetSynchronizationContext(new System.Windows.Threading.DispatcherSynchronizationContext(System.Windows.Threading.Dispatcher.CurrentDispatcher));
        try{action();}finally{System.Threading.SynchronizationContext.SetSynchronizationContext(previous);}
#endif
    }
    static void PumpUntil(Func<bool> done,string message)
    {
        var elapsed=System.Diagnostics.Stopwatch.StartNew();
#if MACOS || LINUX
        while(!done() && elapsed.Elapsed<TimeSpan.FromSeconds(10)){Avalonia.Threading.Dispatcher.UIThread.RunJobs();System.Threading.Thread.Sleep(1);}
#else
        if(!done()) {
            var frame=new System.Windows.Threading.DispatcherFrame();
            var timer=new System.Windows.Threading.DispatcherTimer {Interval=TimeSpan.FromMilliseconds(5)};
            timer.Tick+=(s,e)=>{if(done() || elapsed.Elapsed>=TimeSpan.FromSeconds(10))frame.Continue=false;};
            timer.Start();try{System.Windows.Threading.Dispatcher.PushFrame(frame);}finally{timer.Stop();}
        }
#endif
        Need(done(),message);
    }
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
            manifest=new {schema=1,id="community-demo",name="Community Demo",version="1.0.0",author="Map Maker",description="A packaging example",game="tsfp-us-45410066",api="tsfp-mod-v1-draft",
                dependencies=new Dictionary<string,string>{{"base-textures","1.0.0"}},files=new Dictionary<string,object>{{"textures/0123456789abcdef.dds",new {sha256=new string('a',64),size=152}}}},
            scan=new {engine="TEST ONLY",time=scanned}};
        return Json.Encode(new {schema=1,releases=available?new[]{release}:new object[0]});
    }
    static void WorkshopReject(Action action,string message){bool rejected=false;try{action();}catch{rejected=true;}Need(rejected,message);}
    static WorkshopRelease PlannedRelease(string id,string version,string key,Dictionary<string,string> dependencies=null,long bytes=152)
    {
        long now=DateTimeOffset.UtcNow.ToUnixTimeSeconds();string sha=ModTrust.Hash(System.Text.Encoding.UTF8.GetBytes(id+"@"+version));
        var manifest=new {schema=1,id=id,name=id,version=version,author="Fixture creator",description="Dependency planning fixture",game="tsfp-us-45410066",api="tsfp-mod-v1-draft",
            dependencies=dependencies??new Dictionary<string,string>(),files=new Dictionary<string,object>{{"textures/"+key+".dds",new {sha256=new string('a',64),size=bytes}}}};
        return ModWorkshopClient.Parse(Json.Encode(new {schema=1,releases=new[]{new {sha256=sha,download="/v1/packages/"+sha,manifest=manifest,scan=new {engine="TEST",time=now}}}}),DateTime.UtcNow).Single();
    }
    static void WorkshopChecks(string root)
    {
        Test("trusted game updates accept only exact immutable mod-runtime companions",()=>{
            string prefix="Bin/ModRuntime/"+new string('a',64)+"/";
            foreach(string file in new[]{"runtime.json","Lua.txt","tsfp-mod-worker","tsfp-mod-worker.exe","tsfp-mod-guardian"})
                Need(Core.Allowed(prefix+file),"Bundled game helper refused: "+file);
            foreach(string file in new[]{"other.exe","install.sh","main.lua","nested/Lua.txt","lua.txt","runtime.json.exe"})
                Need(!Core.Allowed(prefix+file),"Unsupported bundled helper accepted: "+file);
            foreach(string folder in new[]{new string('a',63),new string('a',65),new string('A',64),new string('g',64)})
                Need(!Core.Allowed("Bin/ModRuntime/"+folder+"/Lua.txt"),"Invalid helper identity accepted");
            Need(!Core.Allowed("Bin/modruntime/"+new string('a',64)+"/Lua.txt"),"Noncanonical helper folder accepted");
            string upload=LibraryPackage(root,"runtime-boundary","1.0.0",1,extra:prefix+"tsfp-mod-worker");
            WorkshopReject(()=>ModLibrary.InspectPackage(upload,System.Threading.CancellationToken.None),"Uploaded mod gained trusted-game helper permissions");
        });
        WorkshopTrustChecks.Run(root,Test);
        Test("script source capsules match the native wire and isolated-save identity",()=>{
            Dictionary<string,object> corpus;
            using(var input=typeof(LauncherSharedUiChecks).Assembly.GetManifestResourceStream("mod-launch-vectors.json"))
            using(var memory=new MemoryStream()){input.CopyTo(memory);corpus=ModJson.Read(System.Text.Encoding.UTF8.GetString(memory.ToArray()));}
            foreach(Dictionary<string,object> vector in (object[])corpus["vectors"]) {
                var source=Convert.FromBase64String((string)vector["source"]);
                var dependencies=((object[])vector["dependencies"]).Cast<string>().ToArray();
                var capsule=ModScriptCapsule.Create((string)vector["package"],dependencies.Reverse(),source);
                Need(Convert.ToBase64String(capsule.Bytes)==(string)vector["wire"],"Source capsule bytes differ from native contract");
                Need(capsule.Sha256==(string)vector["sha256"] && capsule.ProfileSha256==(string)vector["profile"],"Capsule/save identity mismatch");
                WorkshopReject(()=>ModScriptCapsule.Create((string)vector["package"],new[]{(string)vector["package"]},source),"Repeated primary accepted");
                WorkshopReject(()=>ModScriptCapsule.Create((string)vector["package"],new[]{new string('b',64),new string('b',64)},source),"Duplicate dependency accepted");
            }
            foreach(byte[] bad in new[]{new byte[0],new byte[]{0},new byte[]{0xc0,0x80},new byte[]{27,76,117,97},new byte[1024*1024+1]})
                WorkshopReject(()=>ModScriptCapsule.Create(new string('a',64),new string[0],bad),"Invalid source accepted");
            WorkshopReject(()=>ModScriptCapsule.Create(new string('A',64),new string[0],new byte[]{32}),"Uppercase digest accepted");
            WorkshopReject(()=>ModScriptCapsule.Create(new string('a',64),Enumerable.Range(0,32).Select(i=>i.ToString("x64")),new byte[]{32}),"Dependency budget exceeded");
        });
        Test("audio capsules match native vectors and reject ambiguous resource authority",()=>{
            Dictionary<string,object> corpus;
            using(var input=typeof(LauncherSharedUiChecks).Assembly.GetManifestResourceStream("mod-launch-vectors.json"))
            using(var memory=new MemoryStream()){input.CopyTo(memory);corpus=ModJson.Read(System.Text.Encoding.UTF8.GetString(memory.ToArray()));}
            foreach(Dictionary<string,object> vector in (object[])corpus["audioVectors"]) {
                var owners=((object[])vector["packages"]).Cast<string>().ToArray();
                var pins=((object[])vector["assets"]).Cast<Dictionary<string,object>>().Select(a=>new ModAudioPin{
                    Key=Convert.ToUInt32(a["key"]),Package=(string)a["package"],Sha256=(string)a["sha256"],Bytes=Convert.ToUInt64(a["bytes"])}).ToArray();
                var capsule=ModAudioCapsule.Create(owners.Reverse(),pins.Reverse());
                Need(Convert.ToBase64String(capsule.Bytes)==(string)vector["wire"] && capsule.Sha256==(string)vector["sha256"],"Audio wire differs from native contract");
                WorkshopReject(()=>ModAudioCapsule.Create(new[]{owners[0]},pins),"Foreign audio owner accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(new[]{owners[0],owners[0]},pins),"Repeated audio authority accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(new[]{new string('A',64)},pins),"Noncanonical audio authority accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(owners,new[]{pins[0],pins[0]}),"Repeated audio key accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(owners,new ModAudioPin[0]),"Empty audio handoff accepted");
                foreach(ulong size in new[]{0UL,64UL*1024*1024+1,UInt64.MaxValue})
                    WorkshopReject(()=>ModAudioCapsule.Create(owners,new[]{new ModAudioPin{Key=0,Package=owners[0],Sha256=pins[0].Sha256,Bytes=size}}),"Invalid audio length accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(owners,new[]{new ModAudioPin{Key=UInt32.MaxValue,Package=owners[0],Sha256=pins[0].Sha256,Bytes=1}}),"Reserved audio key accepted");
                WorkshopReject(()=>ModAudioCapsule.Create(owners,Enumerable.Range(0,5).Select(i=>new ModAudioPin{Key=(uint)i,Package=owners[0],Sha256=pins[0].Sha256,Bytes=64UL*1024*1024})),"Aggregate audio budget exceeded");
            }
        });
        long now=DateTimeOffset.UtcNow.ToUnixTimeSeconds();string sha=new string('a',64);
        Test("workshop catalog validates identities and freshness",()=>{
            var entries=ModWorkshopClient.Parse(WorkshopCatalog(sha,now-1),DateTime.UtcNow);
            Need(entries.Single().Categories.SequenceEqual(new[]{"textures"}) && entries.Single().PayloadBytes==152 && entries.Single().Dependencies.Single()=="base-textures @ 1.0.0","Browse details differ from validated package metadata");
            Need(entries.Length==1 && entries[0].Author=="Map Maker","Mod metadata lost");
            Need(ModWorkshopClient.Parse(WorkshopCatalog(sha,now-90000),DateTime.UtcNow).Length==0,"Stale scan appeared");
            Need(ModWorkshopClient.Parse(WorkshopCatalog(sha,now+3600),DateTime.UtcNow).Length==0,"Future scan appeared");
            WorkshopReject(()=>ModWorkshopClient.Parse(WorkshopCatalog(sha,now,true,"https://attacker.invalid/payload"),DateTime.UtcNow),"External package URL accepted");
            WorkshopReject(()=>ModWorkshopClient.Parse(WorkshopCatalog("../escape",now),DateTime.UtcNow),"Invalid digest accepted");
            WorkshopReject(()=>ModWorkshopClient.Parse(new string('x',ModWorkshopClient.MaxCatalogBytes+1),DateTime.UtcNow),"Oversized catalog accepted");
        });
        Test("workshop transport requires HTTPS",()=>{
            Need(ModWorkshopClient.ServiceUri("https://workshop.example.ts.net").Host=="workshop.example.ts.net","HTTPS service rejected");
            foreach(var url in new[]{"http://127.0.0.1:8765","http://example.com","https://user:secret@example.com","https://example.com/subpath","https://example.com/?redirect=1","file:///tmp/package"})
                WorkshopReject(()=>ModWorkshopClient.ServiceUri(url),"Unsafe service URL accepted");
            foreach(int port in new[]{8443,10000})Need(ModWorkshopClient.ServiceUri("https://workshop.example.ts.net:"+port).Port==port,"Supported HTTPS port rejected");
            foreach(var authority in new[]{"https://workshop.example.ts.net:444","https://workshop.example.ts.net:10001"})
                WorkshopReject(()=>ModWorkshopClient.ServiceUri(authority),"Unapproved HTTPS port accepted");
            foreach(var authority in new[]{"https://workshop.example.ts.net:443","https://workshop.example.ts.net:08443","HTTPS://workshop.example.ts.net:8443"})
                Need(!ModTrust.CanonicalOrigin(authority),"Noncanonical signed authority accepted");
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
        Test("workshop browse bounds cards and resets pages after filtering or refresh",()=>{
            using(var page=new ModWorkshopPage(root,true)) {
                var releases=Enumerable.Range(0,23).SelectMany(i=>new[]{"1.0.0","2.0.0"}.Select(version=>new WorkshopRelease {
                    Id="mod-"+i.ToString("00"),Name="Mod "+i.ToString("00"),Version=version,Author="Example creator",Description="Example",Categories=new[]{"textures"},ScanEngine="TEST",ScannedUtc=DateTime.UtcNow})).ToArray();
                page.SetReleases(releases);
                Need(page.MatchingCount==23 && page.VisibleCount==20 && page.PageCount==2 && !page.Previous.IsEnabled && page.Next.IsEnabled,"Latest-release paging failed");
                Click(page.Next);Need(page.PageNumber==2 && page.VisibleCount==3 && page.Previous.IsEnabled && !page.Next.IsEnabled,"Last page is incomplete or unbounded");
                page.Search.Text="mod-00";Need(page.PageNumber==1 && page.VisibleCount==1 && !page.Next.IsEnabled,"Search left an empty old page");
                page.Search.Text="";page.OlderVersions.IsChecked=true;Need(page.MatchingCount==46 && page.PageCount==3 && page.VisibleCount==20,"Older versions were lost or unbounded");
                Click(page.Next);Click(page.Next);Need(page.PageNumber==3 && page.VisibleCount==6,"History paging failed");
                page.SetReleases(releases.Take(2).ToArray());Need(page.PageNumber==1 && page.VisibleCount==2 && !page.Next.IsEnabled,"Refresh retained an invalid page");
            }
        });
        Test("workshop content and creator filters use latest metadata and numeric version order",()=>{
            var releases=new[]{
                new WorkshopRelease {Id="mode",Name="Mode",Version="2.9.0",Author="Old Maker",Description="Old release",Categories=new[]{"maps"}},
                new WorkshopRelease {Id="mode",Name="Mode",Version="2.10.0",Author="New Maker",Description="Current mode",Categories=new[]{"scripts","textures"}},
                new WorkshopRelease {Id="large",Name="Large",Version="99999999999999999999.0.0",Author="New Maker",Description="Large version",Categories=new[]{"audio"}}};
            Need(WorkshopRelease.Select(releases,"","old","maps",false,"name").Length==0,"An old match replaced latest release metadata");
            Need(WorkshopRelease.Select(releases,"","old","maps",true,"name").Single().Version=="2.9.0","Older release filter failed");
            Need(WorkshopRelease.Select(releases,"","new","textures",false,"name").Single().Version=="2.10.0","Mixed-content filter failed");
            Need(WorkshopRelease.Select(releases,"","","",true,"version").Select(r=>r.Version).SequenceEqual(new[]{"99999999999999999999.0.0","2.10.0","2.9.0"}),"Version sorting overflowed or compared lexically");
            using(var page=new ModWorkshopPage(root,true)) {
                page.SetReleases(releases);page.Creator.Text="OLD";page.Category.SelectedIndex=3;Need(page.VisibleCount==0,"Page ignored creator/content filters");
                page.OlderVersions.IsChecked=true;Need(page.VisibleCount==1,"History filter did not refresh immediately");
                page.Creator.Text="new";page.Category.SelectedIndex=2;Need(page.VisibleCount==1,"Gameplay category did not match a mixed package");
            }
        });
        Test("workshop dependency plans order exact versions and reuse only origin-bound installed packages",()=>{
            const string origin="https://workshop.example.test";
            var baseline=PlannedRelease("base","1.0.0","0000000000000001");
            var middle=PlannedRelease("middle","1.0.0","0000000000000002",new Dictionary<string,string>{{"base","1.0.0"}});
            var selected=PlannedRelease("selected","1.0.0","0000000000000003",new Dictionary<string,string>{{"base","1.0.0"},{"middle","1.0.0"}});
            var catalog=new[]{selected,middle,baseline};var installed=new InstalledMod {Id="base",Version="1.0.0",Sha256=baseline.Sha256,WorkshopOrigin=origin};
            var plan=WorkshopInstallPlan.Create(selected,catalog,new[]{installed},origin);
            Need(plan.Ordered.Select(r=>r.Id).SequenceEqual(new[]{"base","middle","selected"}) && plan.Downloads.Select(r=>r.Id).SequenceEqual(new[]{"middle","selected"}),"Dependencies were repeated, skipped or ordered after their dependent");
            Need(plan.PayloadBytes==456 && plan.NewPayloadBytes==304,"Dependency payload preview is inaccurate");
            WorkshopReject(()=>WorkshopInstallPlan.Create(selected,new[]{selected,middle},new[]{installed},origin),"An installed but unavailable dependency bypassed catalog availability");
            installed.WorkshopOrigin=null;WorkshopReject(()=>WorkshopInstallPlan.Create(selected,catalog,new[]{installed},origin),"A local receipt silently bypassed future workshop revocation");
            installed.WorkshopOrigin=origin;installed.Sha256=new string('b',64);WorkshopReject(()=>WorkshopInstallPlan.Create(selected,catalog,new[]{installed},origin),"A different installed archive was reused");
            var changed=PlannedRelease("selected","1.0.0","0000000000000003");changed.Sha256=new string('c',64);
            WorkshopReject(()=>WorkshopInstallPlan.Create(changed,catalog,new InstalledMod[0],origin),"An old selection hash bypassed the current catalog");
        });
        Test("workshop dependency plans reject cycles version conflicts payload conflicts and oversized closures",()=>{
            const string origin="https://workshop.example.test";var empty=new InstalledMod[0];
            var a=PlannedRelease("cycle-a","1.0.0","0000000000000001",new Dictionary<string,string>{{"cycle-b","1.0.0"}});
            var b=PlannedRelease("cycle-b","1.0.0","0000000000000002",new Dictionary<string,string>{{"cycle-a","1.0.0"}});
            WorkshopReject(()=>WorkshopInstallPlan.Create(a,new[]{a,b},empty,origin),"Cyclic download plan accepted");
            var baseOne=PlannedRelease("base","1.0.0","0000000000000001");var baseTwo=PlannedRelease("base","2.0.0","0000000000000002");
            var middle=PlannedRelease("middle","1.0.0","0000000000000003",new Dictionary<string,string>{{"base","2.0.0"}});
            var selected=PlannedRelease("selected","1.0.0","0000000000000004",new Dictionary<string,string>{{"base","1.0.0"},{"middle","1.0.0"}});
            WorkshopReject(()=>WorkshopInstallPlan.Create(selected,new[]{selected,middle,baseOne,baseTwo},empty,origin),"Incompatible exact dependency versions accepted");
            var conflict=PlannedRelease("conflict","1.0.0","0000000000000001",new Dictionary<string,string>{{"base","1.0.0"}});
            WorkshopReject(()=>WorkshopInstallPlan.Create(conflict,new[]{conflict,baseOne},empty,origin),"Conflicting payloads accepted");
            var upper=PlannedRelease("upper","1.0.0","0000000000000005");var lower=PlannedRelease("lower","1.0.0","0000000000000006",new Dictionary<string,string>{{"upper","1.0.0"}});
            upper.Package.Files.Clear();upper.Package.Files.Add("maps/Arena.json",new ModPayload {Sha256=new string('a',64),Size=1});
            lower.Package.Files.Clear();lower.Package.Files.Add("maps/arena.json",new ModPayload {Sha256=new string('b',64),Size=1});
            WorkshopReject(()=>WorkshopInstallPlan.Create(lower,new[]{upper,lower},empty,origin),"Cross-platform case-colliding payloads accepted");
            foreach(int count in new[]{5,33}) {
                var chain=Enumerable.Range(0,count).Select(i=>PlannedRelease("package-"+i.ToString("00"),"1.0.0",i.ToString("x16"),
                    i+1<count?new Dictionary<string,string>{{"package-"+(i+1).ToString("00"),"1.0.0"}}:null,count==5?64L*1024*1024:1)).ToArray();
                WorkshopReject(()=>WorkshopInstallPlan.Create(chain[0],chain,empty,origin),count==5?"Expanded payload budget bypassed":"Package-count budget bypassed");
            }
        });
        Test("workshop install preview shows exact dependencies and preview actions stay offline",()=>{
            int requests=0;
            using(var page=new ModWorkshopPage(root,true,new ModWorkshopClient(new WorkshopHandler(_=>{requests++;throw new Exception("Preview accessed network");})))) {
                var releases=WorkshopTrustChecks.DependencyReleases();var plan=WorkshopInstallPlan.Create(releases.Single(r=>r.Id=="addon"),releases,new InstalledMod[0],WorkshopTrustChecks.Origin);
                page.ShowInstallPlan(plan);
                Need(All(page.View).OfType<TextBlock>().Any(text=>(text.Text??"").Contains("base @ 1.0.0")) && All(page.View).OfType<TextBlock>().Any(text=>(text.Text??"").Contains("middle @ 1.0.0")),"Exact required versions missing from confirmation");
                Need(!page.Search.IsEnabled,"Search remained active over an install preview");
                Click(Find<Button>(page.View,"ConfirmModInstall"));Need(requests==0 && page.StatusText.Contains("Preview only"),"Preview installed or connected");
                Click(Find<Button>(page.View,"CancelModInstallPlan"));Need(page.Search.IsEnabled && page.VisibleCount==2,"Back did not restore browse");
            }
        });
        Test("workshop updates use numeric versions exact origins and the newest installed version",()=>{
            string origin=WorkshopTrustChecks.Origin;
            var current=new InstalledMod {Id="update-demo",Version="1.9.0",WorkshopOrigin=origin};
            var catalog=new[]{PlannedRelease("update-demo","1.8.0","0000000000000001"),PlannedRelease("update-demo","1.10.0","0000000000000001"),PlannedRelease("other","9.0.0","0000000000000002")};
            Need(WorkshopRelease.UpdateFor(current,catalog,new[]{current},origin).Version=="1.10.0","Numeric newer release was missed");
            Need(WorkshopRelease.UpdateFor(current,catalog,new[]{current},origin+":8443")==null,"An offer crossed workshop authorities");
            Need(WorkshopRelease.UpdateFor(current,catalog,new[]{current},"")==null,"An unloaded workshop offered an update");
            var local=new InstalledMod {Id=current.Id,Version=current.Version};
            Need(WorkshopRelease.UpdateFor(local,catalog,new[]{local},origin)==null,"An unbound local package was treated as a workshop package");
            var newer=new InstalledMod {Id=current.Id,Version="1.10.0",WorkshopOrigin=origin};
            Need(WorkshopRelease.UpdateFor(current,catalog,new[]{current,newer},origin)==null && WorkshopRelease.UpdateFor(newer,catalog,new[]{current,newer},origin)==null,"Already installed update was offered again");
            var elsewhere=new InstalledMod {Id=current.Id,Version="2.0.0",WorkshopOrigin=origin+":8443"};
            Need(WorkshopRelease.UpdateFor(current,catalog,new[]{current,elsewhere},origin)!=null,"Another authority hid a valid update");
            Need(WorkshopRelease.UpdateFor(current,new WorkshopRelease[0],new[]{current},origin)==null,"A removed release remained an update offer");
        });
        Test("update review names the retained version and returns to installed without downloading",()=>{
            int requests=0;using(var page=new ModWorkshopPage(root,true,new ModWorkshopClient(new WorkshopHandler(_=>{requests++;throw new Exception("Update preview accessed network");})))) {
                var releases=WorkshopTrustChecks.DependencyReleases();var plan=WorkshopInstallPlan.Create(releases.Single(r=>r.Id=="addon"),releases,new InstalledMod[0],WorkshopTrustChecks.Origin);
                page.ShowInstallPlan(plan,"addon@0.9.0");
                Need(All(page.View).OfType<TextBlock>().Any(t=>(t.Text??"").Contains("addon@0.9.0")),"Update review hid the previous version");
                Need(Convert.ToString(Find<Button>(page.View,"CancelModInstallPlan").Content)=="Back to installed","Update back action lost its context");
                Click(Find<Button>(page.View,"ConfirmModInstall"));Need(requests==0 && page.StatusText.Contains("Preview only"),"Update preview downloaded content");
                Click(Find<Button>(page.View,"CancelModInstallPlan"));Need(Convert.ToString(page.Refresh.Content)=="Check for updates" && page.VisibleCount==0,"Back did not restore the installed view");
            }
        });
        Test("workshop downloads verify hashes and recheck removal before fetching",()=>{
            string folder=Path.Combine(root,"workshop-downloads");WorkshopTrustChecks.Pin(folder);byte[] bytes=WorkshopTrustChecks.Bytes("package");string hash;
            using(var algorithm=System.Security.Cryptography.SHA256.Create())hash=BitConverter.ToString(algorithm.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();
            bool available=true,corrupt=false;int packageRequests=0;
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>{
                int version=!available?2:corrupt?3:1;
                if(request.RequestUri.AbsolutePath=="/v1/trust/catalog")return WorkshopTrustChecks.Response("metadata"+version);
                if(request.RequestUri.AbsolutePath=="/v1/catalog")return WorkshopTrustChecks.Response("catalog"+version);
                packageRequests++;return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=new System.Net.Http.ByteArrayContent(corrupt?new byte[]{0}:bytes)};
            }),folder,()=>WorkshopTrustChecks.Now)) {
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
            string folder=Path.Combine(root,"trust-transport");WorkshopTrustChecks.Pin(folder);
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.Redirect)),folder,()=>WorkshopTrustChecks.Now))
                WorkshopReject(()=>client.Read("https://workshop.example.ts.net",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Redirect accepted");
            using(var client=new ModWorkshopClient(new WorkshopHandler(request=>{
                var content=new System.Net.Http.ByteArrayContent(new byte[0]);content.Headers.ContentLength=ModWorkshopClient.MaxCatalogBytes+1;
                return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=content};}),folder,()=>WorkshopTrustChecks.Now))
                WorkshopReject(()=>client.Read("https://workshop.example.ts.net",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Excessive catalog length accepted");
        });
    }
    static byte[] WorkshopDds(byte marker)
    {
        var bytes=new byte[152];foreach(var pair in new Dictionary<int,uint>{{0,0x20534444},{4,124},{12,4},{16,4},{28,3},{76,32},{80,4},{84,0x31545844}})
            Array.Copy(BitConverter.GetBytes(pair.Value),0,bytes,pair.Key,4);
        for(int i=128;i<bytes.Length;i++)bytes[i]=marker;return bytes;
    }
    static string LibraryPackage(string root,string id,string version,byte marker,string key="0123456789abcdef",Dictionary<string,string> dependencies=null,bool script=false,string extra=null,string author="Test creator",string sourceText=null,Dictionary<string,byte[]> additionalPayloads=null)
    {
        string file=script?"scripts/main.lua":"textures/"+key+".dds";byte[] data=script?System.Text.Encoding.UTF8.GetBytes(sourceText??"return true\n"):WorkshopDds(marker);string hash;
        using(var sha=System.Security.Cryptography.SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();
        var files=new Dictionary<string,object>{{file,new {sha256=hash,size=data.Length}}};
        foreach(var pair in additionalPayloads??new Dictionary<string,byte[]>())files.Add(pair.Key,new {sha256=ModTrust.Hash(pair.Value),size=pair.Value.Length});
        string json=Json.Encode(new {schema=1,id=id,version=version,name=id,author=author,description="Synthetic original fixture",game="tsfp-us-45410066",api="tsfp-mod-v1-draft",dependencies=dependencies??new Dictionary<string,string>(),files=files});
        string package=Path.Combine(root,Guid.NewGuid().ToString("N")+".zip");
        using(var archive=System.IO.Compression.ZipFile.Open(package,System.IO.Compression.ZipArchiveMode.Create)) {
            using(var output=archive.CreateEntry("mod.json").Open()){byte[] text=System.Text.Encoding.UTF8.GetBytes(json);output.Write(text,0,text.Length);}
            using(var output=archive.CreateEntry(file).Open())output.Write(data,0,data.Length);
            if(extra!=null)using(var output=archive.CreateEntry(extra).Open())output.Write(data,0,data.Length);
            foreach(var pair in additionalPayloads??new Dictionary<string,byte[]>())using(var output=archive.CreateEntry(pair.Key).Open())output.Write(pair.Value,0,pair.Value.Length);
        }
        return package;
    }
    static System.Net.Http.HttpResponseMessage CreatorJson(object value,int status=200)
    {return new System.Net.Http.HttpResponseMessage((System.Net.HttpStatusCode)status){Content=new System.Net.Http.StringContent(Json.Encode(value),System.Text.Encoding.UTF8,"application/json")};}
    static object CreatorLogin()
    {return new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role="creator"}};}
    static Dictionary<string,object> CreatorRow(CreatorPackagePlan plan,string state,long cursor=0)
    {
        var data=new Dictionary<string,object>{{"id",new string('a',32)},{"mod_id",plan.Id},{"version",plan.Version},{"sha256",plan.Sha256},{"size",plan.Size},{"created",WorkshopTrustChecks.Unix},{"expires",WorkshopTrustChecks.Unix+900},{"state",state},{"retry_at",0},{"error",""}};
        if(cursor>0)data.Add("cursor",cursor);return data;
    }
    static void CreatorConfig(string folder)
    {WorkshopTrustChecks.Pin(folder);Core.Write(Core.PathIn(folder,"Support/Mods/service.json"),Json.Encode(new {service=WorkshopTrustChecks.Origin}));}
    static Dictionary<string,object> ModeratorRow(string zip,CreatorPackagePlan plan,string state="pending-review")
    {
        string manifest;using(var archive=System.IO.Compression.ZipFile.OpenRead(zip))using(var reader=new StreamReader(archive.GetEntry("mod.json").Open()))manifest=reader.ReadToEnd();
        return new Dictionary<string,object>{{"cursor",1L},{"id",new string('a',32)},{"mod_id",plan.Id},{"version",plan.Version},{"sha256",plan.Sha256},{"state",state},{"manifest",manifest}};
    }
    static Dictionary<string,object> ModeratorScan(CreatorPackagePlan plan,string state="pending-review",bool fresh=true)
    {return new Dictionary<string,object>{{"id",new string('a',32)},{"sha256",plan.Sha256},{"state",state},{"scan",new Dictionary<string,object>{{"engine","ClamAV"},{"version","ClamAV fixture only"},{"status","clean"},{"time",WorkshopTrustChecks.Unix},{"isolation","macos-seatbelt-v3"},{"database_max_age_days",2L},{"fresh",fresh}}}};}
    static Dictionary<string,object> ModeratorSource(string zip,CreatorPackagePlan plan)
    {
        string text;using(var archive=System.IO.Compression.ZipFile.OpenRead(zip))using(var reader=new StreamReader(archive.GetEntry("scripts/main.lua").Open()))text=reader.ReadToEnd();
        var payload=ModLibrary.Manifest((string)ModeratorRow(zip,plan)["manifest"]).Files["scripts/main.lua"];
        return new Dictionary<string,object>{{"id",new string('a',32)},{"sha256",plan.Sha256},{"path","scripts/main.lua"},{"payload_sha256",payload.Sha256},{"size",payload.Size},{"offset",0L},{"next_offset",payload.Size},{"text",text}};
    }
    static Dictionary<string,object> ModeratorTexture(string zip,CreatorPackagePlan plan)
    {
        var payload=ModLibrary.InspectPackage(zip,System.Threading.CancellationToken.None).Files["textures/0123456789abcdef.dds"];
        return new Dictionary<string,object>{{"id",new string('a',32)},{"sha256",plan.Sha256},{"path","textures/0123456789abcdef.dds"},{"payload_sha256",payload.Sha256},{"size",payload.Size},{"width",4},{"height",4},{"format","BC1"},{"preview_width",4},{"preview_height",4},{"rgba",Convert.ToBase64String(Enumerable.Range(0,64).Select(i=>(byte)(i%4==3?255:64)).ToArray())}};
    }
    static byte[] ModeratorWave(int channels=1)
    {
        using(var bytes=new MemoryStream())using(var writer=new BinaryWriter(bytes)) {
            writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));writer.Write(36+16002*channels*2);writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt "));writer.Write(16);
            writer.Write((short)1);writer.Write((short)channels);writer.Write(8000);writer.Write(16000*channels);writer.Write((short)(channels*2));writer.Write((short)16);
            writer.Write(System.Text.Encoding.ASCII.GetBytes("data"));writer.Write(16002*channels*2);
            for(int i=0;i<16002*channels;i++)writer.Write((short)(i%2==0?-32768:32767));return bytes.ToArray();
        }
    }
    static Dictionary<string,object> ModeratorAudio(CreatorPackagePlan plan,long offset=0,int channels=1)
    {
        byte[] wave=ModeratorWave(channels);long next=Math.Min(16002,offset+16000);
        return new Dictionary<string,object>{{"id",new string('a',32)},{"sha256",plan.Sha256},{"path","audio/00000001.wav"},{"payload_sha256",ModTrust.Hash(wave)},{"size",wave.Length},{"rate",8000},{"channels",channels},{"frames",16002},{"offset",offset},{"next_offset",next},{"pcm",Convert.ToBase64String(wave.Skip(44+(int)offset*channels*2).Take((int)(next-offset)*channels*2).ToArray())}};
    }
    static void SetCreatorSecret(Element view,string name,string secret)
    {
#if MACOS || LINUX
        Find<TextBox>(view,name).Text=secret;
#else
        Find<PasswordBox>(view,name).Password=secret;
#endif
    }
    static Dictionary<string,object> PlayerReport(CreatorPackagePlan plan)
    {return new Dictionary<string,object>{{"cursor",1L},{"id",new string('d',32)},{"sha256",plan.Sha256},{"mod_id",plan.Id},{"version",plan.Version},{"reporter","fixture-creator"},{"reason","other"},{"description","Original report for review"},{"created",WorkshopTrustChecks.Unix},{"updated",WorkshopTrustChecks.Unix},{"state","open"},{"resolution",""}};}
    sealed class FixtureSessionVault : IModSessionVault
    {
        internal readonly Dictionary<string,byte[]> Values=new Dictionary<string,byte[]>();
        internal bool FailWrite,FailDelete;internal int Writes,Deletes;
        public byte[] Read(string binding){lock(Values){byte[] value;return Values.TryGetValue(binding,out value)?(byte[])value.Clone():null;}}
        public void Write(string binding,byte[] secret){if(FailWrite)throw new Exception("Do not expose private provider details");lock(Values){Values[binding]=(byte[])secret.Clone();Writes++;}}
        public void Delete(string binding){if(FailDelete)throw new Exception("Do not expose private provider details");lock(Values){Values.Remove(binding);Deletes++;}}
    }
    static void CreatorVaultChecks(string root)
    {
        Test("remembered sessions opt in bind authority and recheck the current server role",()=>{
            string folder=Path.Combine(root,"session-vault-roundtrip");CreatorConfig(folder);var vault=new FixtureSessionVault();int accounts=0;
            Func<System.Net.Http.HttpRequestMessage,System.Net.Http.HttpResponseMessage> respond=request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                Need(request.RequestUri.AbsolutePath=="/v1/account" && request.Method==System.Net.Http.HttpMethod.Get && request.Headers.Authorization.Parameter==new string('t',43),"Saved sign-in was not revalidated with its bearer session");accounts++;return CreatorJson(new {username="fixture-creator",role="creator"});
            };
            using(var first=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(respond),()=>WorkshopTrustChecks.Now,vault)) {
                first.SignIn("fixture-creator","not-a-real-password",System.Threading.CancellationToken.None).GetAwaiter().GetResult();Need(vault.Writes==0,"Remembering was enabled by default");
                first.SignIn("fixture-creator","not-a-real-password",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();Need(vault.Writes==1 && vault.Values.Count==1,"Opt-in did not persist the session");
                string saved=Encoding.UTF8.GetString(vault.Values.Values.Single());Need(!saved.Contains("not-a-real-password") && !saved.Contains("role"),"Password or stale privileges entered the credential store");
            }
            Need(vault.Values.Count==1,"Disposal removed an opted-in session");
            using(var resumed=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(respond),()=>WorkshopTrustChecks.Now,vault)) {
                Need(resumed.Resume(System.Threading.CancellationToken.None).GetAwaiter().GetResult() && resumed.SignedIn && !resumed.CanModerate && accounts==1,"Saved sign-in trusted stale roles or failed to resume");
                resumed.SignIn("fixture-creator","not-a-real-password",System.Threading.CancellationToken.None).GetAwaiter().GetResult();Need(vault.Values.Count==0,"Unchecking Remember retained an earlier saved sign-in");
            }
        });
        Test("expired changed-authority malformed and revoked remembered sessions fail closed",()=>{
            foreach(string mode in new[]{"expired","pin","schema","token","revoked","identity","role"}) {
                string folder=Path.Combine(root,"session-vault-"+mode);CreatorConfig(folder);var vault=new FixtureSessionVault();int requests=0;
                using(var first=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>CreatorJson(CreatorLogin())),()=>WorkshopTrustChecks.Now,vault))first.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();
                string binding=vault.Values.Keys.Single();var saved=ModJson.Read(Encoding.UTF8.GetString(vault.Values[binding]));
                if(mode=="expired")saved["expires"]=WorkshopTrustChecks.Unix-1;else if(mode=="pin")saved["pin"]=new string('f',64);else if(mode=="schema")saved["schema"]=true;else if(mode=="token")saved["token"]="invalid";
                vault.Values[binding]=Encoding.UTF8.GetBytes(Json.Encode(saved));
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{requests++;return mode=="revoked"?new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.Unauthorized):CreatorJson(new {username=mode=="identity"?"another-account":"fixture-creator",role=mode=="role"?"unknown":"creator"});}),()=>WorkshopTrustChecks.Now,vault)) {
                    WorkshopReject(()=>client.Resume(System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Invalid saved sign-in resumed");Need(!client.SignedIn && !client.CanModerate && vault.Values.Count==0,"Invalid saved credentials were retained");Need(requests==(new[]{"revoked","identity","role"}.Contains(mode)?1:0),"Invalid local session reached the network");
                }
            }
        });
        Test("failed remote sign-out clears remembered session and vault failures stay visible",()=>{
            string folder=Path.Combine(root,"session-vault-signout");CreatorConfig(folder);var vault=new FixtureSessionVault();
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>request.Method==System.Net.Http.HttpMethod.Delete?new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.ServiceUnavailable):CreatorJson(CreatorLogin())),()=>WorkshopTrustChecks.Now,vault)) {
                client.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();
                WorkshopReject(()=>client.SignOut(System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Remote logout failure was hidden");Need(!client.SignedIn && vault.Values.Count==0,"Remote failure retained remembered credentials");
                vault.FailWrite=true;client.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();Need(client.SignedIn && client.VaultNotice.Contains("not saved") && !client.VaultNotice.Contains("provider details"),"Vault failure broke memory-only sign-in or exposed provider details");
                vault.FailWrite=false;client.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();vault.FailDelete=true;
                WorkshopReject(()=>client.ForgetSaved(System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Locked vault claimed successful deletion");Need(client.VaultNotice.Contains("could not be removed"),"Failed credential deletion was not visible");vault.FailDelete=false;client.ForgetSaved(System.Threading.CancellationToken.None).GetAwaiter().GetResult();
            }
        });
        Test("remember sign-in UI is opt in and exposes explicit resume and forget",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"session-vault-ui");CreatorConfig(folder);var vault=new FixtureSessionVault();
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>CreatorJson(CreatorLogin())),()=>WorkshopTrustChecks.Now,vault))using(var page=new ModCreatorPage(folder,false,client)) {
                var remember=Find<CheckBox>(page.View,"RememberCreatorSignIn");Need(remember.IsChecked!=true && Find<Button>(page.View,"ResumeCreatorSignIn")!=null && Find<Button>(page.View,"ForgetCreatorSignIn")!=null,"Remember UI missing or enabled by default");remember.IsChecked=true;
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-only");Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignOut" && b.IsEnabled),"Remembered UI sign-in did not finish");Need(vault.Writes==1,"Remember checkbox did not reach the client");Click(Find<Button>(page.View,"ForgetCreatorSignIn"));PumpUntil(()=>page.StatusText=="Saved sign-in forgotten.","Explicit Forget did not finish");Need(vault.Values.Count==0 && client.SignedIn,"Forgetting the OS entry changed the active memory session");
            }
        }));
        Test("restoring a saved sign-in clears unsent password and invitation drafts",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"session-vault-resume-ui");CreatorConfig(folder);var vault=new FixtureSessionVault();
            using(var first=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>CreatorJson(CreatorLogin())),()=>WorkshopTrustChecks.Now,vault))first.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None,true).GetAwaiter().GetResult();
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>CreatorJson(new {username="fixture-creator",role="creator"})),()=>WorkshopTrustChecks.Now,vault))using(var page=new ModCreatorPage(folder,false,client)) {
                SetCreatorSecret(page.View,"CreatorPassword","unsent-password-draft");SetCreatorSecret(page.View,"CreatorInvitation","unsent-invitation-draft");
#if MACOS || LINUX
                var password=Find<TextBox>(page.View,"CreatorPassword");var invitation=Find<TextBox>(page.View,"CreatorInvitation");
#else
                var password=Find<PasswordBox>(page.View,"CreatorPassword");var invitation=Find<PasswordBox>(page.View,"CreatorInvitation");
#endif
                Click(Find<Button>(page.View,"ResumeCreatorSignIn"));PumpUntil(()=>page.StatusText=="Signed in using the system credential store.","Saved sign-in did not resume through the UI");
#if MACOS || LINUX
                Need(password.Text=="" && invitation.Text=="","Restored sign-in retained secret drafts");
#else
                Need(password.Password=="" && invitation.Password=="","Restored sign-in retained secret drafts");
#endif
            }
        }));
        if(Environment.GetEnvironmentVariable("TSFP_TEST_OS_VAULT")=="1")Test("actual OS credential vault roundtrip update isolation and removal",()=>{
            var vault=new ModSessionVault();string binding=ModTrust.Hash(Encoding.UTF8.GetBytes("disposable-vault-fixture-"+Guid.NewGuid().ToString("N"))),other=ModTrust.Hash(Encoding.UTF8.GetBytes("other-fixture-"+Guid.NewGuid().ToString("N")));byte[] first=Encoding.UTF8.GetBytes("Disposable session fixture; no actual account"),second=Encoding.UTF8.GetBytes("Updated disposable session fixture");
            try{Need(vault.Read(binding)==null,"Fresh random fixture already existed");vault.Write(binding,first);Need(vault.Read(binding).SequenceEqual(first) && vault.Read(other)==null,"Credential bytes or scope isolation failed");vault.Write(binding,second);Need(vault.Read(binding).SequenceEqual(second),"Credential update failed");vault.Delete(binding);Need(vault.Read(binding)==null,"Credential removal failed");vault.Delete(binding);}finally{vault.Delete(binding);}
        });
    }
    static void CreatorChecks(string root)
    {
        CreatorVaultChecks(root);
        Test("audio waveform keeps stereo channels separate and renders final short portions",()=>{
            string folder=Path.Combine(root,"moderator-audio-stereo");CreatorConfig(folder);
            string zip=LibraryPackage(folder,"audio-stereo","1.0.0",1,additionalPayloads:new Dictionary<string,byte[]>{{"audio/00000001.wav",ModeratorWave(2)}});
            var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var review=new ModeratorUpload(ModeratorRow(zip,plan));
            foreach(long offset in new[]{0L,16000L}) {
                var clip=new ModeratorAudioPreview(ModJson.Read(Json.Encode(ModeratorAudio(plan,offset,2))),review,"audio/00000001.wav",offset);byte[] pixels=clip.WaveformPixels();
                Need(pixels.Length==256*128*4 && pixels[0]==38 && pixels[63*256*4]==250 && pixels[64*256*4]==250 && pixels[127*256*4]==38,"Stereo waveform mixed channels or lost short-portion samples");
            }
        });
        Test("moderator audio binds PCM source pagination size and role",()=>{
            string folder=Path.Combine(root,"moderator-audio-binding");CreatorConfig(folder);
            string zip=LibraryPackage(folder,"audio-review","1.0.0",1,additionalPayloads:new Dictionary<string,byte[]>{{"audio/00000001.wav",ModeratorWave()}});
            var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var review=new ModeratorUpload(ModeratorRow(zip,plan));
            foreach(string mode in new[]{"valid","digest","payload","path","offset","end","channels","rate","frames","size","short","oversize","extra","creator","cancel"}) {
                int requests=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    requests++;Need(request.RequestUri.AbsolutePath=="/v1/moderation/"+review.Id+"/audio" && request.Method==System.Net.Http.HttpMethod.Post,"Audio inspection used an unexpected route");
                    var data=ModeratorAudio(plan);
                    if(mode=="digest")data["sha256"]=new string('f',64);if(mode=="payload")data["payload_sha256"]=new string('f',64);if(mode=="path")data["path"]="audio/00000002.wav";
                    if(mode=="offset")data["offset"]=1;if(mode=="end")data["next_offset"]=16001;if(mode=="channels")data["channels"]=3;if(mode=="rate")data["rate"]=48001;
                    if(mode=="frames")data["frames"]=48000*600;if(mode=="size")data["size"]=44;if(mode=="short")data["pcm"]=Convert.ToBase64String(new byte[2]);
                    if(mode=="oversize")data["pcm"]=new string('A',512004);if(mode=="extra")data["local_path"]="private";return CreatorJson(data);
                }),()=>WorkshopTrustChecks.Now))using(var cancel=new System.Threading.CancellationTokenSource()) {
                    client.SignIn("fixture-creator","fixture-only",System.Threading.CancellationToken.None).GetAwaiter().GetResult();if(mode=="cancel")cancel.Cancel();
                    Action read=()=>{var clip=client.Audio(review,"audio/00000001.wav",0,cancel.Token).GetAwaiter().GetResult();byte[] pixels=clip.WaveformPixels();Need(pixels.Length==256*64*4 && pixels[0]==250 && pixels[2]==167,"PCM extrema were not rendered at full scale");};
                    if(mode=="valid")read();else WorkshopReject(read,"Invalid audio preview was accepted");Need(requests==(mode=="creator" || mode=="cancel"?0:1),"Unauthorized/cancelled audio request reached the server");
                }
            }
        });
        Test("moderator audio UI pages exact PCM and clears images on sign-out",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"moderator-audio-ui");CreatorConfig(folder);
            string zip=LibraryPackage(folder,"audio-review-ui","1.0.0",1,additionalPayloads:new Dictionary<string,byte[]>{{"audio/00000001.wav",ModeratorWave()}});
            var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int previews=0;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role="moderator"}});
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(request.RequestUri.AbsolutePath.EndsWith("/evidence"))return CreatorJson(ModeratorScan(plan,"pending-review",true));
                if(request.RequestUri.AbsolutePath.EndsWith("/audio")) {
                    var body=ModJson.Read(request.Content.ReadAsStringAsync().GetAwaiter().GetResult());long offset=ModTrust.Number(body,"offset",0,16000);
                    Need(ModTrust.Text(body,"sha256")==plan.Sha256 && ModTrust.Text(body,"path")=="audio/00000001.wav" && offset==previews*16000,"Audio pagination changed identity or skipped frames");previews++;return CreatorJson(ModeratorAudio(plan,offset));
                }
                Need(request.Method==System.Net.Http.HttpMethod.Get,"Audio inspection sent an approval");return CreatorJson(new {uploads=new[]{ModeratorRow(zip,plan)}});
            }),()=>WorkshopTrustChecks.Now))using(var page=new ModCreatorPage(folder,false,client)) {
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-only");Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorUploads" && b.IsEnabled),"Audio moderator sign-in failed");
                Click(Find<Button>(page.View,"RefreshModeratorUploads"));PumpUntil(()=>page.StatusText=="Moderation queue refreshed.","Audio queue failed");Click(Find<Button>(page.View,"ModeratorUpload"+new string('a',32)));PumpUntil(()=>page.StatusText=="Scan evidence refreshed.","Audio evidence failed");Need(previews==0,"Audio fetched automatically");
                Click(Find<Button>(page.View,"InspectModeratorAudio"));PumpUntil(()=>previews==1 && All(page.View).OfType<Image>().Any(i=>i.Name=="ModeratorAudioWaveform"),"Audio waveform did not render");var image=Find<Image>(page.View,"ModeratorAudioWaveform");Need(image.Source!=null && image.Width==512 && image.Height==128,"Waveform image is unbounded");
                Click(Find<Button>(page.View,"NextModeratorAudio"));PumpUntil(()=>previews==2 && !All(page.View).OfType<Button>().Any(b=>b.Name=="NextModeratorAudio"),"Audio final portion did not finish");
                Click(Find<Button>(page.View,"CreatorSignOut"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Audio sign-out failed");Need(!All(page.View).OfType<Image>().Any(i=>i.Name=="ModeratorAudioWaveform") && !Directory.Exists(Path.Combine(folder,"Mods")),"Audio inspection retained image or installed content");
            }
        }));
        Test("owner invitation responses bind username expiry and one-time secret",()=>{
            string folder=Path.Combine(root,"creator-invitation-binding");CreatorConfig(folder);
            foreach(string mode in new[]{"valid","username","expiry","token","extra"}) {
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-owner",role="admin"}});
                    var row=new Dictionary<string,object>{{"username",mode=="username"?"different-author":"guest-author"},{"invitation",mode=="token"?new string('!',43):new string('i',43)},{"expires",WorkshopTrustChecks.Unix+(mode=="expiry"?86401:86400)}};
                    if(mode=="extra")row.Add("role","admin");return CreatorJson(row,201);
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-owner","fixture-only",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    Action issue=()=>{var invitation=client.InviteCreator("guest-author",System.Threading.CancellationToken.None).GetAwaiter().GetResult();Need(invitation.Username=="guest-author" && invitation.Service==WorkshopTrustChecks.Origin,"Invitation lost trusted authority binding");};
                    if(mode=="valid")issue();else WorkshopReject(issue,"Malformed invitation response was accepted");
                }
            }
        });
        Test("owner invitations conceal secrets and require cancellation confirmation",()=>WithUiContext(()=>{
            foreach(string role in new[]{"admin","moderator","creator"}) {
                string folder=Path.Combine(root,"creator-invitation-"+role);CreatorConfig(folder);int issued=0,cancelled=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    string path=request.RequestUri.AbsolutePath;
                    if(path=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-owner",role=role}});
                    if(path=="/v1/sessions/current")return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                    Need(role=="admin" && request.Headers.Authorization.Parameter==new string('t',43),"Invitation request lost owner authentication");
                    if(request.Method==System.Net.Http.HttpMethod.Post){Need(path=="/v1/creator-invitations","Wrong invitation endpoint");issued++;return CreatorJson(new {username="guest-author",invitation=new string('i',43),expires=WorkshopTrustChecks.Unix+86400},201);}
                    Need(request.Method==System.Net.Http.HttpMethod.Delete && path=="/v1/creator-invitations/guest-author","Cancellation targeted the wrong invitation");cancelled++;return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                }),()=>WorkshopTrustChecks.Now))
                using(var page=new ModCreatorPage(folder,false,client)) {
                    Find<TextBox>(page.View,"CreatorUsername").Text="fixture-owner";SetCreatorSecret(page.View,"CreatorPassword","fixture-only");
                    Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignOut"),"Owner sign-in did not finish");
                    Need(All(page.View).OfType<Button>().Any(b=>b.Name=="CreateCreatorInvitation")== (role=="admin"),"Non-owner UI exposed invitation controls");
                    if(role!="admin") {
                        WorkshopReject(()=>client.InviteCreator("guest-author",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"A non-owner invited a creator");
                        Need(issued==0 && cancelled==0,"A non-owner request reached the server");continue;
                    }
                    Find<TextBox>(page.View,"InviteCreatorUsername").Text="Bad/Name";Click(Find<Button>(page.View,"CreateCreatorInvitation"));
                    PumpUntil(()=>page.StatusText.Contains("lowercase"),"Invalid creator name was not rejected");Need(issued==0,"Invalid name reached the server");
                    Find<TextBox>(page.View,"InviteCreatorUsername").Text="guest-author";Click(Find<Button>(page.View,"CreateCreatorInvitation"));
                    PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RevealCreatorInvitation"),"Invitation did not return to its UI");
                    Need(issued==1 && !All(page.View).OfType<TextBox>().Any(t=>t.Name=="CreatedCreatorInvitationSecret"),"Invitation secret was revealed automatically");
                    Click(Find<Button>(page.View,"RevealCreatorInvitation"));Need(Find<TextBox>(page.View,"CreatedCreatorInvitationSecret").Text==new string('i',43),"Explicit reveal lost invitation");
                    Click(Find<Button>(page.View,"HideCreatorInvitation"));Need(!All(page.View).OfType<TextBox>().Any(t=>t.Name=="CreatedCreatorInvitationSecret") && issued==1,"Hide retained secret or repeated issuance");
                    Click(Find<Button>(page.View,"ReviewCancelCreatorInvitation"));Need(cancelled==0,"Cancellation happened before confirmation");
                    Click(Find<Button>(page.View,"BackCreatorInvitation"));Need(cancelled==0,"Back cancelled an invitation");
                    Click(Find<Button>(page.View,"ReviewCancelCreatorInvitation"));Click(Find<Button>(page.View,"ConfirmCancelCreatorInvitation"));
                    PumpUntil(()=>page.StatusText=="Pending creator invitation cancelled.","Cancellation did not finish");Need(cancelled==1,"Cancellation was duplicated");
                    Click(Find<Button>(page.View,"CreateCreatorInvitation"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RevealCreatorInvitation"),"Replacement invitation did not finish");
                    Click(Find<Button>(page.View,"RevealCreatorInvitation"));Click(Find<Button>(page.View,"CreatorSignOut"));
                    PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Owner sign-out did not finish");
                    Need(!All(page.View).OfType<TextBox>().Any(t=>t.Name=="CreatedCreatorInvitationSecret"),"Sign-out retained a visible invitation");
                }
            }
        }));
        Test("reported release inspection rejects changed digests versions and creator roles",()=>{
            string folder=Path.Combine(root,"reported-release-binding");CreatorConfig(folder);string zip=LibraryPackage(folder,"reported-lookup","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var report=new WorkshopReport(PlayerReport(plan));
            foreach(string mode in new[]{"valid","digest","version","creator"}) {
                int lookups=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    lookups++;Need(request.Method==System.Net.Http.HttpMethod.Get && request.RequestUri.AbsolutePath=="/v1/moderation/by-digest/"+plan.Sha256,"Reported release used an unexpected route");
                    var row=ModeratorRow(zip,plan,"approved");if(mode=="digest")row["sha256"]=new string('f',64);if(mode=="version")row["version"]="2.0.0";return CreatorJson(row);
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    if(mode=="valid")Need(client.ReportedRelease(report,System.Threading.CancellationToken.None).GetAwaiter().GetResult().Sha256==plan.Sha256,"Reported release lost its archive binding");else WorkshopReject(()=>client.ReportedRelease(report,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Unbound/unauthorized reported release accepted");
                    Need(lookups==(mode=="creator"?0:1),"A creator sent an inspection request");
                }
            }
        });
        Test("moderator report UI inspects exact release and confirms close reopen without package decisions",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"moderator-report-ui");CreatorConfig(folder);string zip=LibraryPackage(folder,"report-review","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int decisions=0,packageDecisions=0;string state="open",resolution="";
            Func<Dictionary<string,object>> record=()=>{var row=PlayerReport(plan);row["state"]=state;row["resolution"]=resolution;row["updated"]=WorkshopTrustChecks.Unix+decisions;return row;};
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                string path=request.RequestUri.AbsolutePath;
                if(path=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role="moderator"}});
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(path=="/v1/moderation/reports" || path=="/v1/moderation/report-history")return CreatorJson(new {reports=state=="open" || path.EndsWith("report-history")?new[]{record()}:new Dictionary<string,object>[0]});
                if(path=="/v1/moderation/by-digest/"+plan.Sha256)return CreatorJson(ModeratorRow(zip,plan,"approved"));
                if(path.EndsWith("/evidence"))return CreatorJson(ModeratorScan(plan,"approved"));
                if(path=="/v1/moderation/reports/"+new string('d',32) && request.Method==System.Net.Http.HttpMethod.Post){decisions++;var body=ModJson.Read(request.Content.ReadAsStringAsync().GetAwaiter().GetResult());state=ModTrust.Text(body,"action")=="close"?"closed":"open";resolution=ModTrust.Text(body,"note");return CreatorJson(record());}
                if(request.Method==System.Net.Http.HttpMethod.Post)packageDecisions++;throw new Exception("Unexpected moderator report request");
            }),()=>WorkshopTrustChecks.Now))using(var page=new ModCreatorPage(folder,false,client)) {
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-password-not-a-real-account");Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorReports" && b.IsEnabled),"Report moderator sign-in did not finish");
                Click(Find<Button>(page.View,"RefreshModeratorReports"));PumpUntil(()=>page.StatusText=="Moderator reports refreshed.","Open reports did not load");Click(Find<Button>(page.View,"ModeratorReport"+new string('d',32)));
                Click(Find<Button>(page.View,"InspectReportedRelease"));PumpUntil(()=>page.StatusText.StartsWith("Reported release loaded"),"Reported release inspection did not finish");Need(All(page.View).OfType<TextBlock>().Any(t=>(t.Text??"").Contains(plan.Sha256)) && packageDecisions==0,"Report inspection changed a package");
                Click(Find<Button>(page.View,"ReviewReportDecision"));Need(!All(page.View).OfType<Button>().Any(b=>b.Name=="ConfirmReportDecision"),"Blank resolution reached confirmation");Find<TextBox>(page.View,"ModeratorReportResolution").Text="Investigated original issue";Click(Find<Button>(page.View,"ReviewReportDecision"));Need(decisions==0,"Report review submitted early");Click(Find<Button>(page.View,"CancelReportDecision"));Need(decisions==0,"Report Back submitted a decision");Click(Find<Button>(page.View,"ReviewReportDecision"));Click(Find<Button>(page.View,"ConfirmReportDecision"));PumpUntil(()=>page.StatusText.StartsWith("Report decision recorded"),"Report close did not finish");Need(state=="closed" && decisions==1,"Report close duplicated or changed state");
                Click(Find<Button>(page.View,"ModeratorReportHistory"));PumpUntil(()=>page.StatusText=="Moderator reports refreshed.","Report history did not load");Click(Find<Button>(page.View,"ModeratorReport"+new string('d',32)));Find<TextBox>(page.View,"ModeratorReportResolution").Text="Reopen for further review";Click(Find<Button>(page.View,"ReviewReportDecision"));Click(Find<Button>(page.View,"ConfirmReportDecision"));PumpUntil(()=>page.StatusText.StartsWith("Report decision recorded"),"Report reopen did not finish");Need(state=="open" && decisions==2 && packageDecisions==0,"Report reopen changed package decisions");
                Click(Find<Button>(page.View,"CreatorSignOut"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Report moderator sign-out did not finish");Need(!All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorReports"),"Sign-out retained moderator reports");
            }
        }));
        Test("player report receipts and private history bind release account state and cursor",()=>{
            string folder=Path.Combine(root,"player-report-client");CreatorConfig(folder);var plan=CreatorPackagePlan.Review(LibraryPackage(folder,"reported-demo","1.0.0",1),System.Threading.CancellationToken.None);
            foreach(string mode in new[]{"valid","digest","reporter","state","history-owner","duplicate"}) {
                int submissions=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                    var row=PlayerReport(plan);
                    if(request.Method==System.Net.Http.HttpMethod.Post) {submissions++;if(mode=="digest")row["sha256"]=new string('f',64);if(mode=="reporter")row["reporter"]="another-account";if(mode=="state")row["state"]="closed";return CreatorJson(row,201);}
                    if(mode=="history-owner")row["reporter"]="another-account";
                    return CreatorJson(new {reports=mode=="duplicate"?new[]{row,row}:new[]{row}});
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    Action create=()=>client.Report(plan.Sha256,"other","Original report for review",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    if(new[]{"digest","reporter","state"}.Contains(mode))WorkshopReject(create,"Forged report receipt accepted");else {create();if(mode=="valid")Need(client.Reports(0,false,System.Threading.CancellationToken.None).GetAwaiter().GetResult().Length==1,"Own report disappeared");else WorkshopReject(()=>client.Reports(0,false,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Private/duplicate report history accepted");}
                    Need(submissions==1,"Report creation retried automatically");
                    WorkshopReject(()=>client.Reports(0,true,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"A creator listed moderator reports");
                }
            }
        });
        Test("player report UI reviews exact release and sends only after confirmation",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"player-report-ui");CreatorConfig(folder);var plan=CreatorPackagePlan.Review(LibraryPackage(folder,"reported-ui","1.0.0",1),System.Threading.CancellationToken.None);int submissions=0;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(request.Method==System.Net.Http.HttpMethod.Post){submissions++;var body=ModJson.Read(request.Content.ReadAsStringAsync().GetAwaiter().GetResult());Need(ModTrust.Text(body,"sha256")==plan.Sha256 && ModTrust.Text(body,"description")=="Original report for review","Report changed reviewed release/text");return CreatorJson(PlayerReport(plan),201);}
                return CreatorJson(new {reports=new[]{PlayerReport(plan)}});
            }),()=>WorkshopTrustChecks.Now))using(var page=new ModCreatorPage(folder,false,client)) {
                page.BeginReport(plan.Id,plan.Version,plan.Sha256);Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-password-not-a-real-account");Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignOut"),"Report sign-in did not finish");
                Find<TextBox>(page.View,"PlayerReportDescription").Text="Original report for review";Click(Find<Button>(page.View,"ReviewPlayerReport"));Need(submissions==0,"Review sent a report");Click(Find<Button>(page.View,"CancelPlayerReport"));Need(submissions==0,"Back sent a report");Click(Find<Button>(page.View,"ReviewPlayerReport"));Click(Find<Button>(page.View,"ConfirmPlayerReport"));PumpUntil(()=>page.StatusText.StartsWith("Report recorded"),"Report submission did not return to UI");Need(submissions==1 && Find<TextBox>(page.View,"PlayerReportDescription").Text=="","Report duplicated or retained submitted draft");
                Click(Find<Button>(page.View,"RefreshPlayerReports"));PumpUntil(()=>page.StatusText=="Report status refreshed.","Report history did not return to UI");Need(!Directory.Exists(Path.Combine(folder,"Mods")),"Reporting installed or enabled a package");
            }
            using(var workshop=new ModWorkshopPage(root,true)){Need(All(workshop.View).OfType<Button>().Where(b=>b.Name=="ReportModRelease").All(b=>!b.IsEnabled),"Hashless interface examples offered a report");workshop.SetReleases(new[]{new WorkshopRelease {Id=plan.Id,Version=plan.Version,Sha256=plan.Sha256,Name="Original report fixture",Author="fixture-creator",Description="Synthetic interface fixture"}});Click(All(workshop.View).OfType<Button>().First(b=>b.Name=="ReportModRelease"));Need(All(workshop.View).OfType<Button>().Any(b=>b.Name=="ReviewPlayerReport"),"Browse did not open the bound report form");}
            string installedRoot=Path.Combine(root,"report-installed");CreatorConfig(installedRoot);var library=new ModLibrary(installedRoot,()=>false,()=>WorkshopTrustChecks.Now);
            using(var signed=new ModWorkshopClient(new WorkshopHandler(request=>WorkshopTrustChecks.Response(request.RequestUri.AbsolutePath=="/v1/catalog"?"catalog1":"metadata1")),installedRoot,()=>WorkshopTrustChecks.Now))signed.Read(WorkshopTrustChecks.Origin,System.Threading.CancellationToken.None).GetAwaiter().GetResult();
            string remoteZip=Path.Combine(installedRoot,"original-signed-fixture.zip");File.WriteAllBytes(remoteZip,WorkshopTrustChecks.Bytes("package"));var remote=library.Install(remoteZip,WorkshopTrustChecks.Sha,System.Threading.CancellationToken.None,workshopOrigin:WorkshopTrustChecks.Origin);
            string localZip=LibraryPackage(installedRoot,"local-unbound","1.0.0",1);library.Install(localZip,Core.Hash(localZip),System.Threading.CancellationToken.None);
            using(var workshop=new ModWorkshopPage(installedRoot,false,new ModWorkshopClient(new WorkshopHandler(_=>{throw new Exception("Installed report selection accessed the network");})))){Click(workshop.InstalledTab);var buttons=All(workshop.View).OfType<Button>().Where(b=>b.Name=="ReportInstalledMod").ToArray();Need(buttons.Length==1,"Installed reporting lost origin binding");Click(buttons[0]);Need(All(workshop.View).OfType<TextBlock>().Any(t=>(t.Text??"").Contains(remote.Id+" @ "+remote.Version)),"Installed reporting selected another release");Need(library.Enabled().Length==0 && library.List().Length==2,"Report selection changed installed/active mods");}
        }));
        Test("moderator texture preview binds identity format shape pixels and role",()=>{
            string folder=Path.Combine(root,"moderator-texture");CreatorConfig(folder);string zip=LibraryPackage(folder,"texture-preview","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var review=new ModeratorUpload(ModeratorRow(zip,plan));
            foreach(string mode in new[]{"valid","digest","path","width","shape","format","pixels","base64","size","archive","creator","cancel"}) {
                int requests=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    requests++;Need(request.Method==System.Net.Http.HttpMethod.Post && request.RequestUri.AbsolutePath.EndsWith("/texture"),"Unexpected texture route");
                    var row=ModeratorTexture(zip,plan);if(mode=="digest")row["payload_sha256"]=new string('f',64);if(mode=="path")row["path"]="../secret.dds";if(mode=="width")row["width"]=3;if(mode=="shape")row["preview_width"]=128;if(mode=="format")row["format"]="DX10";if(mode=="pixels")row["rgba"]="AAAA";if(mode=="base64")row["rgba"]=(string)row["rgba"]+"\n";if(mode=="size")row["size"]=128;if(mode=="archive")row["sha256"]=new string('f',64);return CreatorJson(row);
                }),()=>WorkshopTrustChecks.Now))using(var stop=new System.Threading.CancellationTokenSource()) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();if(mode=="cancel")stop.Cancel();
                    if(mode=="valid"){var image=client.Texture(review,"textures/0123456789abcdef.dds",stop.Token).GetAwaiter().GetResult();Need(image.Width==4 && image.PreviewWidth==4 && image.Format=="BC1","Texture metadata changed");}
                    else WorkshopReject(()=>client.Texture(review,"textures/0123456789abcdef.dds",stop.Token).GetAwaiter().GetResult(),"Invalid texture preview accepted");
                    Need(requests==(mode=="creator" || mode=="cancel"?0:1),"Forbidden texture preview reached transport");
                    WorkshopReject(()=>client.Texture(review,"../private.dds",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Undeclared texture requested");
                }
            }
        });
        Test("moderator texture display preserves color and composites transparency",()=>{
            string folder=Path.Combine(root,"moderator-alpha");Directory.CreateDirectory(folder);string zip=LibraryPackage(folder,"texture-alpha","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var row=ModeratorTexture(zip,plan);var pixels=new byte[64];pixels[4]=255;pixels[7]=128;row["rgba"]=Convert.ToBase64String(pixels);
            byte[] display=new ModeratorTexturePreview(ModJson.Read(Json.Encode(row)),new ModeratorUpload(ModeratorRow(zip,plan)),"textures/0123456789abcdef.dds").DisplayPixels();
            Need(display.Take(8).SequenceEqual(new byte[]{72,72,72,255,36,36,164,255}),"Texture transparency/color compositing changed");
        });
        Test("moderator texture UI renders bounded pixels and clears them on sign-out",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"moderator-texture-ui");CreatorConfig(folder);string zip=LibraryPackage(folder,"texture-review-ui","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int previews=0;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role="moderator"}});
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(request.RequestUri.AbsolutePath.EndsWith("/evidence"))return CreatorJson(ModeratorScan(plan,"pending-review",true));
                if(request.RequestUri.AbsolutePath.EndsWith("/texture")){previews++;return CreatorJson(ModeratorTexture(zip,plan));}
                Need(request.Method==System.Net.Http.HttpMethod.Get,"Texture preview sent a package decision");return CreatorJson(new {uploads=new[]{ModeratorRow(zip,plan)}});
            }),()=>WorkshopTrustChecks.Now))using(var page=new ModCreatorPage(folder,false,client)) {
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-password-not-a-real-account");Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorUploads" && b.IsEnabled),"Texture moderator sign-in failed");
                Click(Find<Button>(page.View,"RefreshModeratorUploads"));PumpUntil(()=>page.StatusText=="Moderation queue refreshed.","Texture queue failed");Click(Find<Button>(page.View,"ModeratorUpload"+new string('a',32)));PumpUntil(()=>page.StatusText=="Scan evidence refreshed.","Texture evidence failed");
                Click(Find<Button>(page.View,"InspectModeratorTexture"));PumpUntil(()=>page.StatusText=="Texture preview loaded.","Texture preview did not render");var image=Find<Image>(page.View,"ModeratorTextureImage");Need(image.Source!=null && image.Width==8 && image.Height==8 && previews==1,"Texture image is missing or unbounded");
                Click(Find<Button>(page.View,"CreatorSignOut"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Texture sign-out failed");Need(!All(page.View).OfType<Image>().Any(i=>i.Name=="ModeratorTextureImage") && !Directory.Exists(Path.Combine(folder,"Mods")),"Preview survived logout or installed content");
            }
        }));
        Test("moderator source preview binds bytes path offsets manifest and role",()=>{
            string folder=Path.Combine(root,"moderator-source");CreatorConfig(folder);string zip=LibraryPackage(folder,"source-demo","1.0.0",1,script:true);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var review=new ModeratorUpload(ModeratorRow(zip,plan));
            foreach(string mode in new[]{"valid","digest","path","offset","body","creator","cancel"}) {
                int requests=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    requests++;Need(request.Method==System.Net.Http.HttpMethod.Post && request.RequestUri.AbsolutePath.EndsWith("/source"),"Source preview used an unexpected route");
                    var row=ModeratorSource(zip,plan);if(mode=="digest")row["payload_sha256"]=new string('f',64);if(mode=="path")row["path"]="../private.lua";if(mode=="offset")row["offset"]=1L;if(mode=="body")row["text"]="return evil\n";
                    return CreatorJson(row);
                }),()=>WorkshopTrustChecks.Now))using(var stop=new System.Threading.CancellationTokenSource()) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();if(mode=="cancel")stop.Cancel();
                    if(mode=="valid"){var page=client.Source(review,"scripts/main.lua",0,stop.Token).GetAwaiter().GetResult();Need(page.Text=="return true\n" && page.NextOffset==page.Size,"Source preview changed fixture bytes");}
                    else WorkshopReject(()=>client.Source(review,"scripts/main.lua",0,stop.Token).GetAwaiter().GetResult(),"Invalid source preview accepted");
                    Need(requests==(mode=="creator" || mode=="cancel"?0:1),"Forbidden source request reached the transport");
                    WorkshopReject(()=>client.Source(review,"../private.lua",0,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Undeclared source path accepted");
                }
            }
            string hiddenZip=LibraryPackage(folder,"hidden-source","1.0.0",1,script:true,sourceText:"return {text=\"x\u202ey\"}\n");var hiddenPlan=CreatorPackagePlan.Review(hiddenZip,System.Threading.CancellationToken.None);var hiddenReview=new ModeratorUpload(ModeratorRow(hiddenZip,hiddenPlan));var hiddenPage=new ModeratorSourcePage(ModeratorSource(hiddenZip,hiddenPlan),hiddenReview,"scripts/main.lua",0);
            Need(hiddenPage.DisplayText().Contains("\\u202e") && !hiddenPage.DisplayText().Contains("\u202e"),"Hidden source direction character was rendered invisibly");
        });
        Test("moderator scan evidence binds digest state types and role without exposing raw output",()=>{
            string folder=Path.Combine(root,"moderator-evidence");CreatorConfig(folder);string zip=LibraryPackage(folder,"scan-evidence","1.0.0",1);var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);var review=new ModeratorUpload(ModeratorRow(zip,plan));
            foreach(string mode in new[]{"valid","stale","digest","state","type","status","extra","creator"}) {
                int requests=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    requests++;Need(request.RequestUri.AbsolutePath.EndsWith("/"+review.Id+"/evidence") && request.Method==System.Net.Http.HttpMethod.Get,"Scan evidence used an unexpected route");
                    var data=ModeratorScan(plan,fresh:mode!="stale");var scan=(Dictionary<string,object>)data["scan"];
                    if(mode=="digest")data["sha256"]=new string('f',64);if(mode=="state")data["state"]="approved";if(mode=="type")scan["fresh"]="true";if(mode=="status")scan["status"]="blocked";if(mode=="extra")scan["detail"]="private raw scanner output";
                    return CreatorJson(data);
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    if(mode=="valid" || mode=="stale"){var evidence=client.Evidence(review,System.Threading.CancellationToken.None).GetAwaiter().GetResult();Need(evidence.Fresh==(mode=="valid") && evidence.Engine=="ClamAV","Scan evidence lost freshness or engine");}
                    else WorkshopReject(()=>client.Evidence(review,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Invalid/private scan evidence accepted");
                    Need(requests==(mode=="creator"?0:1),"A creator sent a privileged evidence request");
                }
            }
        });
        Test("moderator decisions bind exact digest identity state role and note",()=>{
            string folder=Path.Combine(root,"moderator-client");CreatorConfig(folder);string zip=LibraryPackage(folder,"moderator-demo","1.0.0",1,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);
            foreach(string mode in new[]{"valid","creator","duplicate","manifest","state","digest","note","cancel"}) {
                int decisions=0,reads=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role=mode=="creator"?"creator":"moderator"}});
                    Need(request.Headers.Authorization!=null && request.Headers.Authorization.Parameter==new string('t',43),"Moderator request lost authentication");
                    if(request.Method==System.Net.Http.HttpMethod.Get){reads++;var row=ModeratorRow(zip,plan);if(mode=="manifest")row["version"]="2.0.0";if(mode=="state")row["state"]="queued";return CreatorJson(new {uploads=mode=="duplicate"?new[]{row,row}:new[]{row}});}
                    decisions++;Need(request.RequestUri.AbsolutePath=="/v1/moderation/"+new string('a',32),"Decision addressed another upload");
                    var body=ModJson.Read(request.Content.ReadAsStringAsync().GetAwaiter().GetResult());Need(ModTrust.Text(body,"note")=="Reviewed original source" && ModTrust.Text(body,"action")=="approve","Decision changed note or action");
                    return CreatorJson(new {id=new string('a',32),sha256=mode=="digest"?new string('f',64):plan.Sha256,state="approved"});
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    if(new[]{"creator","duplicate","manifest","state"}.Contains(mode)) {
                        WorkshopReject(()=>client.Reviews(0,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Invalid moderator queue accepted");
                        Need(decisions==0 && (mode!="creator" || reads==0),"Creator issued a moderator request");continue;
                    }
                    var review=client.Reviews(0,System.Threading.CancellationToken.None).GetAwaiter().GetResult().Single();
                    using(var stop=new System.Threading.CancellationTokenSource()) {
                        if(mode=="cancel")stop.Cancel();
                        Action submit=()=>client.Moderate(review,"approve",mode=="note"?" ":"Reviewed original source",stop.Token).GetAwaiter().GetResult();
                        if(mode=="valid")submit();else WorkshopReject(submit,"Invalid or cancelled moderation decision accepted");
                    }
                    Need(decisions==(mode=="valid" || mode=="digest"?1:0),"Invalid moderation input reached the server");
                }
            }
        });
        Test("moderator UI requires note review and confirmation and Back makes no decision",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"moderator-ui");CreatorConfig(folder);string zip=LibraryPackage(folder,"moderator-ui","1.0.0",1,script:true,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int decisions=0;string state="pending-review";bool scanFresh=false;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix+43200,account=new {username="fixture-creator",role="moderator"}});
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(request.RequestUri.AbsolutePath.EndsWith("/evidence"))return CreatorJson(ModeratorScan(plan,state,scanFresh));
                if(request.RequestUri.AbsolutePath.EndsWith("/source"))return CreatorJson(ModeratorSource(zip,plan));
                if(request.Method==System.Net.Http.HttpMethod.Get)return CreatorJson(new {uploads=new[]{ModeratorRow(zip,plan,state)}});
                decisions++;var body=ModJson.Read(request.Content.ReadAsStringAsync().GetAwaiter().GetResult());state=ModTrust.Text(body,"action")=="approve"?"approved":"revoked";
                return CreatorJson(new {id=new string('a',32),sha256=plan.Sha256,state=state});
            }),()=>WorkshopTrustChecks.Now))
            using(var page=new ModCreatorPage(folder,false,client)) {
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-password-not-a-real-account");Click(Find<Button>(page.View,"CreatorSignIn"));
                PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorUploads" && b.IsEnabled),"Moderator sign-in did not render queue controls");
                Action refresh=()=>{Click(Find<Button>(page.View,"RefreshModeratorUploads"));PumpUntil(()=>page.StatusText=="Moderation queue refreshed.","Moderation refresh did not finish");Click(Find<Button>(page.View,"ModeratorUpload"+new string('a',32)));PumpUntil(()=>page.StatusText=="Scan evidence refreshed.","Scan evidence did not return to UI");};
                refresh();Need(!Find<Button>(page.View,"ReviewModeratorApproval").IsEnabled && Find<Button>(page.View,"ReviewModeratorRevocation").IsEnabled,"Stale scan allowed approval or blocked revocation");
                scanFresh=true;refresh();Click(Find<Button>(page.View,"ReviewModeratorApproval"));Need(!All(page.View).OfType<Button>().Any(b=>b.Name=="ConfirmModeratorDecision") && decisions==0,"Blank note bypassed review");
                Click(Find<Button>(page.View,"InspectModeratorSource"));PumpUntil(()=>page.StatusText.StartsWith("Lua source loaded"),"Source inspection did not return to UI");Need(Find<TextBox>(page.View,"ModeratorSourceText").IsReadOnly && Find<TextBox>(page.View,"ModeratorSourceText").Text=="return true\n" && decisions==0,"Source inspection changed bytes or sent a decision");
                Find<TextBox>(page.View,"ModeratorNote").Text="Reviewed original source";Click(Find<Button>(page.View,"ReviewModeratorApproval"));Need(decisions==0,"Review approved without confirmation");
                Click(Find<Button>(page.View,"CancelModeratorDecision"));Need(decisions==0,"Back approved the package");
                Click(Find<Button>(page.View,"ReviewModeratorApproval"));Click(Find<Button>(page.View,"ConfirmModeratorDecision"));PumpUntil(()=>page.StatusText.StartsWith("Moderation decision recorded"),"Approval did not return to UI");Need(decisions==1,"Approval duplicated requests");
                refresh();Need(!Find<Button>(page.View,"ReviewModeratorApproval").IsEnabled,"Approved package offered another approval");Find<TextBox>(page.View,"ModeratorNote").Text="Withdraw reviewed fixture";
                Click(Find<Button>(page.View,"ReviewModeratorRevocation"));Click(Find<Button>(page.View,"ConfirmModeratorDecision"));PumpUntil(()=>page.StatusText.StartsWith("Moderation decision recorded"),"Revocation did not return to UI");Need(decisions==2 && state=="revoked","Revocation did not use confirmed action");
                Need(!Directory.Exists(Path.Combine(folder,"Mods")),"Moderator UI installed or enabled quarantine");
                Click(Find<Button>(page.View,"CreatorSignOut"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Moderator sign-out did not finish");Need(!All(page.View).OfType<Button>().Any(b=>b.Name=="RefreshModeratorUploads"),"Signed-out UI retained moderator controls");
            }
        }));
        Test("creator package review validates all bytes without installing or extracting",()=>{
            string folder=Path.Combine(root,"creator-review");Directory.CreateDirectory(folder);
            string zip=LibraryPackage(folder,"creator-demo","1.0.0",1,author:"fixture-creator");var before=Directory.GetFileSystemEntries(folder);
            var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);
            Need(plan.Id=="creator-demo" && plan.Author=="fixture-creator" && plan.Sha256==Core.Hash(zip) && plan.Size==new FileInfo(zip).Length,"Package review differs from source");
            Need(Directory.GetFileSystemEntries(folder).SequenceEqual(before),"Creator review extracted or installed content");
            var stop=new System.Threading.CancellationTokenSource();stop.Cancel();WorkshopReject(()=>CreatorPackagePlan.Review(zip,stop.Token),"Cancelled review completed");
            string bad=LibraryPackage(folder,"creator-bad","1.0.0",1,extra:"../escape.dds",author:"fixture-creator");WorkshopReject(()=>CreatorPackagePlan.Review(bad,System.Threading.CancellationToken.None),"Traversal package passed creator review");
        });
        Test("creator transport pins the authority before sending credentials and rejects redirects",()=>{
            foreach(bool wrong in new[]{true,false}) {
                string folder=Path.Combine(root,"creator-pin-"+wrong);CreatorConfig(folder);int requests=0;
                if(wrong)Core.Write(Core.PathIn(folder,"Support/Mods/service.json"),Json.Encode(new {service=WorkshopTrustChecks.Origin+":8443"}));
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{requests++;var response=new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.Redirect);response.Headers.Location=new Uri("https://elsewhere.example.test/");return response;}),()=>WorkshopTrustChecks.Now)) {
                    WorkshopReject(()=>client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Unsafe creator authority accepted");
                    Need(requests==(wrong?0:1) && !client.SignedIn,"Creator credentials crossed an authority or redirect");
                }
            }
        });
        Test("creator upload rechecks reviewed bytes binds reservations and retains private status",()=>{
            string folder=Path.Combine(root,"creator-transfer");CreatorConfig(folder);string zip=LibraryPackage(folder,"creator-demo","1.0.0",1,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int transfers=0;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                string path=request.RequestUri.AbsolutePath;Need(request.RequestUri.GetLeftPart(UriPartial.Authority)==WorkshopTrustChecks.Origin,"Creator request escaped its origin");
                if(path=="/v1/sessions"){Need(request.Headers.Authorization==null,"Previous bearer leaked into sign-in");return CreatorJson(CreatorLogin());}
                Need(request.Headers.Authorization.Scheme=="Bearer" && request.Headers.Authorization.Parameter==new string('t',43),"Creator request lost authentication");
                if(path=="/v1/uploads" && request.Method==System.Net.Http.HttpMethod.Post)return CreatorJson(CreatorRow(plan,"open"),201);
                if(path=="/v1/uploads/"+new string('a',32)) {transfers++;Need(request.Content.Headers.ContentType.MediaType=="application/zip" && request.Content.Headers.ContentLength==plan.Size,"Upload framing changed");Need(request.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult().SequenceEqual(File.ReadAllBytes(zip)),"Upload bytes differ from reviewed package");return CreatorJson(CreatorRow(plan,"queued"),202);}
                if(path=="/v1/uploads")return CreatorJson(new {uploads=new[]{CreatorRow(plan,"pending-review",1)}});
                if(path=="/v1/sessions/current")return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                throw new Exception("Unexpected creator route");
            }),()=>WorkshopTrustChecks.Now)) {
                client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                Need(client.Upload(plan,System.Threading.CancellationToken.None).GetAwaiter().GetResult().State=="queued" && transfers==1,"Upload was auto-approved or duplicated");
                Need(client.History(0,System.Threading.CancellationToken.None).GetAwaiter().GetResult().Single().State=="pending-review","Review state was not preserved");
                client.SignOut(System.Threading.CancellationToken.None).GetAwaiter().GetResult();Need(!client.SignedIn && client.Username==null && !Directory.Exists(Path.Combine(folder,"Mods")),"Sign-out retained authentication or upload enabled a mod");
                Need(!Directory.GetFiles(folder,"*.json",SearchOption.AllDirectories).Any(p=>File.ReadAllText(p).Contains(new string('t',43)) || File.ReadAllText(p).Contains("fixture-password-not-a-real-account")),"Creator credentials reached disk");
            }
        });
        Test("creator upload refuses changed packages and mismatched tickets before transfer",()=>{
            foreach(bool change in new[]{true,false}) {
                string folder=Path.Combine(root,"creator-stale-"+change);CreatorConfig(folder);string zip=LibraryPackage(folder,"creator-demo","1.0.0",1,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int reservations=0,transfers=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                    if(request.Method==System.Net.Http.HttpMethod.Put){transfers++;throw new Exception("Invalid upload transferred");}
                    reservations++;var row=CreatorRow(plan,"open");row["sha256"]=new string('0',64);return CreatorJson(row,201);
                }),()=>WorkshopTrustChecks.Now)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    if(change)File.AppendAllText(zip,"changed");
                    WorkshopReject(()=>client.Upload(plan,System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Stale or mismatched upload completed");Need(transfers==0 && reservations==(change?0:1),"Invalid package consumed the wrong upload stage");
                }
            }
        });
        Test("creator responses bound allocations reject invalid sessions and forget rejected authentication",()=>{
            foreach(string mode in new[]{"oversize","duplicate","expiry","unauthorized","logout"}) {
                string folder=Path.Combine(root,"creator-response-"+mode);CreatorConfig(folder);int requests=0;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                    requests++;if(mode=="oversize"){var response=CreatorJson(new {});response.Content.Headers.ContentLength=512*1024+1;return response;}
                    if(mode=="duplicate")return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.OK){Content=new System.Net.Http.StringContent("{\"token\":\"a\",\"token\":\"b\"}",System.Text.Encoding.UTF8,"application/json")};
                    if(mode=="expiry")return CreatorJson(new {token=new string('t',43),expires=WorkshopTrustChecks.Unix,account=new {username="fixture-creator",role="creator"}});
                    return requests==1?CreatorJson(CreatorLogin()):new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.Unauthorized);
                }),()=>WorkshopTrustChecks.Now)) {
                    if(mode=="unauthorized" || mode=="logout") {
                        client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                        WorkshopReject(()=>{if(mode=="logout")client.SignOut(System.Threading.CancellationToken.None).GetAwaiter().GetResult();else client.History(0,System.Threading.CancellationToken.None).GetAwaiter().GetResult();},"Rejected authentication was accepted");
                    }else WorkshopReject(()=>client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult(),"Malformed creator session accepted");
                    Need(!client.SignedIn,"Invalid creator response retained a session");
                }
            }
        });
        Test("creator history retains exhausted scanner errors without treating them as approval",()=>{
            string folder=Path.Combine(root,"creator-scan-error");CreatorConfig(folder);string zip=LibraryPackage(folder,"creator-error","1.0.0",1,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                var row=CreatorRow(plan,"scan-error",1);row["error"]="scan-unavailable";return CreatorJson(new {uploads=new[]{row}});
            }),()=>WorkshopTrustChecks.Now)) {
                client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                var row=client.History(0,System.Threading.CancellationToken.None).GetAwaiter().GetResult().Single();Need(row.State=="scan-error" && row.Error=="scan-unavailable","Exhausted scanner failure was hidden or misclassified");
                WorkshopReject(()=>new CreatorUpload(CreatorRow(plan,"unknown-state")),"An unknown server state was accepted");
            }
        });
        Test("creator expiry cancellation and service changes prevent stale authenticated requests",()=>{
            foreach(string mode in new[]{"expiry","cancel","origin","logout"}) {
                string folder=Path.Combine(root,"creator-auth-change-"+mode);CreatorConfig(folder);int requests=0;DateTime clock=WorkshopTrustChecks.Now;
                using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{requests++;return CreatorJson(CreatorLogin());}),()=>clock)) {
                    client.SignIn("fixture-creator","fixture-password-not-a-real-account",System.Threading.CancellationToken.None).GetAwaiter().GetResult();
                    var cancellation=new System.Threading.CancellationTokenSource();
                    if(mode=="expiry")clock=clock.AddHours(13);
                    if(mode=="origin")Core.Write(Core.PathIn(folder,"Support/Mods/service.json"),Json.Encode(new {service=WorkshopTrustChecks.Origin+":8443"}));
                    if(mode=="cancel" || mode=="logout")cancellation.Cancel();
                    WorkshopReject(()=>{if(mode=="logout")client.SignOut(cancellation.Token).GetAwaiter().GetResult();else client.History(0,cancellation.Token).GetAwaiter().GetResult();},"Stale or cancelled creator request completed");
                    Need(requests==1 && client.SignedIn==(mode=="cancel"),"Stale credentials were transmitted or session clearing differed");
                }
            }
        });
        Test("creator preview stays offline and the native account form clears secrets",()=>WithUiContext(()=>{
            using(var page=new ModCreatorPage(root,true)) {
                SetCreatorSecret(page.View,"CreatorPassword","preview-private-password");SetCreatorSecret(page.View,"CreatorInvitation",new string('s',43));
                Click(Find<Button>(page.View,"CreatorSignIn"));Need(page.StatusText.Contains("Preview only"),"Creator preview accessed authentication");
                page.ClearCredentials();
            }
            string folder=Path.Combine(root,"creator-ui");CreatorConfig(folder);
            string zip=LibraryPackage(folder,"creator-ui-demo","1.0.0",1,author:"fixture-creator");var plan=CreatorPackagePlan.Review(zip,System.Threading.CancellationToken.None);int transfers=0;
            using(var client=new ModCreatorClient(folder,WorkshopTrustChecks.Origin,new WorkshopHandler(request=>{
                if(request.RequestUri.AbsolutePath=="/v1/sessions")return CreatorJson(CreatorLogin());
                if(request.Method==System.Net.Http.HttpMethod.Delete)return new System.Net.Http.HttpResponseMessage(System.Net.HttpStatusCode.NoContent);
                if(request.Method==System.Net.Http.HttpMethod.Post)return CreatorJson(CreatorRow(plan,"open"),201);
                if(request.Method==System.Net.Http.HttpMethod.Put){transfers++;return CreatorJson(CreatorRow(plan,"queued"),202);}
                return CreatorJson(new {uploads=new[]{CreatorRow(plan,"pending-review",1)}});
            }),()=>WorkshopTrustChecks.Now))
            using(var page=new ModCreatorPage(folder,false,client)) {
                Find<TextBox>(page.View,"CreatorUsername").Text="fixture-creator";SetCreatorSecret(page.View,"CreatorPassword","fixture-password-not-a-real-account");
                Click(Find<Button>(page.View,"CreatorSignIn"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignOut"),"Creator sign-in did not return to its UI");
                Find<TextBox>(page.View,"CreatorPackagePath").Text=zip;Click(Find<Button>(page.View,"ReviewCreatorPackage"));
                PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="ConfirmCreatorUpload" && b.IsEnabled),"Creator ZIP review did not return to its UI");
                Need(transfers==0,"Creator review uploaded without confirmation");
                Click(Find<Button>(page.View,"ConfirmCreatorUpload"));PumpUntil(()=>page.StatusText.StartsWith("Package received privately"),"Creator upload did not return to its UI");
                Need(transfers==1 && !Directory.Exists(Path.Combine(folder,"Mods")),"Creator upload duplicated or installed a package");
                Click(Find<Button>(page.View,"RefreshCreatorUploads"));PumpUntil(()=>page.StatusText=="Upload status refreshed.","Creator history did not return to its UI");
                Need(All(page.View).OfType<TextBlock>().Any(t=>(t.Text??"").Contains("pending-review")),"Creator UI hid manual review status");
                Click(Find<Button>(page.View,"CreatorSignOut"));PumpUntil(()=>All(page.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Creator sign-out did not return to its UI");
#if MACOS || LINUX
                Need(Find<TextBox>(page.View,"CreatorPassword").Text=="" && Find<TextBox>(page.View,"CreatorInvitation").Text=="","Creator form retained secrets");
#else
                Need(Find<PasswordBox>(page.View,"CreatorPassword").Password=="" && Find<PasswordBox>(page.View,"CreatorInvitation").Password=="","Creator form retained secrets");
#endif
            }
            using(var workshop=new ModWorkshopPage(root,true)) {
                Click(workshop.CreatorTab);Need(All(workshop.View).OfType<Button>().Any(b=>b.Name=="CreatorSignIn"),"Workshop did not expose the creator page");
                Click(workshop.BrowseTab);Need(workshop.VisibleCount==2,"Leaving creator lost the workshop view");
            }
        }));
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
        Test("dependency enable review commits one complete profile and preserves independent mods",()=>{
            string folder=Path.Combine(root,"enable-closure");Directory.CreateDirectory(folder);bool running=false;var library=new ModLibrary(folder,()=>running);
            Action<string,string,string,Dictionary<string,string>,bool> install=(id,version,key,deps,script)=>{string zip=LibraryPackage(root,id,version,1,key,deps,script);library.Install(zip,Core.Hash(zip),System.Threading.CancellationToken.None);};
            install("enable-base","1.0.0","1111111111111111",null,false);
            install("enable-middle","1.0.0","2222222222222222",new Dictionary<string,string>{{"enable-base","1.0.0"}},false);
            install("enable-addon","1.0.0","3333333333333333",new Dictionary<string,string>{{"enable-middle","1.0.0"}},true);
            install("enable-independent","1.0.0","4444444444444444",null,false);
            string manifest=Core.PathIn(folder,LauncherPlatform.ManifestPath(folder));Directory.CreateDirectory(Path.GetDirectoryName(manifest));File.WriteAllText(manifest,"{\"mod_script_version\":1}");
            library.Enable("enable-independent@1.0.0",System.Threading.CancellationToken.None);
            string profilePath=Path.Combine(folder,"Mods/profile.json"),before=File.ReadAllText(profilePath);
            var plan=library.PreviewEnable("enable-addon@1.0.0",System.Threading.CancellationToken.None);
            Need(File.ReadAllText(profilePath)==before && plan.Gameplay && plan.Removed.Length==0,"Preview changed the active profile");
            Need(plan.Added.SequenceEqual(new[]{"enable-base@1.0.0","enable-middle@1.0.0","enable-addon@1.0.0"}),"Enable dependency order differed");
            var exposed=plan.EnabledKeys;exposed[0]="modified";Need(plan.EnabledKeys[0]=="enable-independent@1.0.0","Review could be mutated externally");
            using(var page=new ModWorkshopPage(folder,true,new ModWorkshopClient(new WorkshopHandler(_=>{throw new Exception("Enable review accessed network");})))) {
                page.ShowEnablePlan(plan);Need(Find<Button>(page.View,"ConfirmModEnable").IsEnabled && !page.Search.IsEnabled,"Enable review controls missing");
                Click(Find<Button>(page.View,"ConfirmModEnable"));Need(page.StatusText.Contains("Preview only") && File.ReadAllText(profilePath)==before,"UI preview enabled packages");
                Click(Find<Button>(page.View,"CancelModEnablePlan"));Need(File.ReadAllText(profilePath)==before,"Back changed active mods");
            }
            running=true;WorkshopReject(()=>library.ApplyEnable(plan,System.Threading.CancellationToken.None),"Reviewed profile changed while game ran");running=false;
            Need(File.ReadAllText(profilePath)==before,"Game-running refusal changed profile");
            var cancelled=new System.Threading.CancellationTokenSource();cancelled.Cancel();try{library.ApplyEnable(plan,cancelled.Token);throw new Exception("Cancelled enable committed");}catch(OperationCanceledException){}
            Need(File.ReadAllText(profilePath)==before,"Cancelled enable changed profile");
            library.ApplyEnable(plan,System.Threading.CancellationToken.None);
            Need(library.Enabled().SequenceEqual(plan.EnabledKeys),"Dependency closure was not enabled as one profile");
            var info=new System.Diagnostics.ProcessStartInfo();info.EnvironmentVariables["RECOMP_SAVE_DIR"]=Path.Combine(folder,"Original saves");library.Configure(info);
            Need(info.EnvironmentVariables.ContainsKey("TSFP_MOD_SCRIPT_CAPSULE"),"Reviewed gameplay closure did not produce a validated handoff");
        });
        Test("enable review refuses stale state and previews reversible version replacements",()=>{
            string folder=Path.Combine(root,"enable-review-stale");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            foreach(string version in new[]{"1.0.0","2.0.0"}){string zip=LibraryPackage(root,"enable-versioned",version,1);library.Install(zip,Core.Hash(zip),System.Threading.CancellationToken.None);}
            library.Enable("enable-versioned@1.0.0",System.Threading.CancellationToken.None);
            var plan=library.PreviewEnable("enable-versioned@2.0.0",System.Threading.CancellationToken.None);
            Need(plan.Removed.SequenceEqual(new[]{"enable-versioned@1.0.0"}) && plan.Added.SequenceEqual(new[]{"enable-versioned@2.0.0"}),"Replacement review omitted version changes");
            library.Enable("enable-versioned@1.0.0",System.Threading.CancellationToken.None);string profile=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));
            WorkshopReject(()=>library.ApplyEnable(plan,System.Threading.CancellationToken.None),"Stale generation was accepted");
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==profile,"Stale review changed profile");
            library.ApplyEnable(library.PreviewEnable("enable-versioned@2.0.0",System.Threading.CancellationToken.None),System.Threading.CancellationToken.None);
            library.ApplyEnable(library.PreviewEnable("enable-versioned@1.0.0",System.Threading.CancellationToken.None),System.Threading.CancellationToken.None);
            Need(library.Enabled().Single()=="enable-versioned@1.0.0" && library.List().Length==2,"Reviewed rollback lost a retained version");
            plan=library.PreviewEnable("enable-versioned@2.0.0",System.Threading.CancellationToken.None);
            var mod=library.ReadInstalled("enable-versioned@2.0.0");File.WriteAllText(Path.Combine(mod.Directory,"content/textures/0123456789abcdef.dds"),"tampered");
            profile=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));WorkshopReject(()=>library.ApplyEnable(plan,System.Threading.CancellationToken.None),"Tampered reviewed payload enabled");
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==profile,"Failed activation replaced profile");
        });
        Test("enable closure rejects missing cyclic conflicting and incompatible requirements",()=>{
            string folder=Path.Combine(root,"enable-invalid");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            Action<string,string,string,Dictionary<string,string>> install=(id,version,key,deps)=>{string zip=LibraryPackage(root,id,version,1,key,deps);library.Install(zip,Core.Hash(zip),System.Threading.CancellationToken.None);};
            install("enable-missing","1.0.0","1111111111111111",new Dictionary<string,string>{{"absent","1.0.0"}});
            WorkshopReject(()=>library.PreviewEnable("enable-missing@1.0.0",System.Threading.CancellationToken.None),"Missing dependency enabled");
            install("enable-cycle-a","1.0.0","2222222222222222",new Dictionary<string,string>{{"enable-cycle-b","1.0.0"}});
            install("enable-cycle-b","1.0.0","3333333333333333",new Dictionary<string,string>{{"enable-cycle-a","1.0.0"}});
            WorkshopReject(()=>library.PreviewEnable("enable-cycle-a@1.0.0",System.Threading.CancellationToken.None),"Dependency cycle enabled");
            install("enable-shared","1.0.0","4444444444444444",null);install("enable-shared","2.0.0","4444444444444444",null);
            install("enable-left","1.0.0","5555555555555555",new Dictionary<string,string>{{"enable-shared","1.0.0"}});
            install("enable-right","1.0.0","6666666666666666",new Dictionary<string,string>{{"enable-shared","2.0.0"}});
            install("enable-diamond","1.0.0","7777777777777777",new Dictionary<string,string>{{"enable-left","1.0.0"},{"enable-right","1.0.0"}});
            WorkshopReject(()=>library.PreviewEnable("enable-diamond@1.0.0",System.Threading.CancellationToken.None),"Incompatible required versions enabled");
            library.ApplyEnable(library.PreviewEnable("enable-left@1.0.0",System.Threading.CancellationToken.None),System.Threading.CancellationToken.None);
            string profile=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));
            WorkshopReject(()=>library.PreviewEnable("enable-shared@2.0.0",System.Threading.CancellationToken.None),"Retained dependent was silently broken");
            install("enable-conflict","1.0.0","4444444444444444",null);
            WorkshopReject(()=>library.PreviewEnable("enable-conflict@1.0.0",System.Threading.CancellationToken.None),"Conflicting enabled payload accepted");
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==profile,"Rejected preview changed profile");
        });
        Test("installed enable and confirm controls apply dependencies through the UI dispatcher",()=>WithUiContext(()=>{
            string folder=Path.Combine(root,"enable-ui-dispatch");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            foreach(string id in new[]{"ui-base","ui-root"}) {
                string zip=LibraryPackage(root,id,"1.0.0",1,id=="ui-base"?"1111111111111111":"2222222222222222",id=="ui-base"?null:new Dictionary<string,string>{{"ui-base","1.0.0"}});
                library.Install(zip,Core.Hash(zip),System.Threading.CancellationToken.None);
            }
            using(var page=new ModWorkshopPage(folder,false,new ModWorkshopClient(new WorkshopHandler(_=>{throw new Exception("Installed activation accessed network");})))) {
                bool busy=false;page.FileBusyChanged+=value=>busy=value;Click(page.InstalledTab);
                Click(All(page.View).OfType<Button>().Where(b=>b.Name=="ToggleMod").Last());
                PumpUntil(()=>!busy,"Enable preview did not return to the UI dispatcher");
                Need(library.Enabled().Length==0 && Find<Button>(page.View,"ConfirmModEnable").IsEnabled,"Enable control skipped its review");
                Click(Find<Button>(page.View,"ConfirmModEnable"));PumpUntil(()=>!busy,"Enable confirmation did not return to the UI dispatcher");
                Need(library.Enabled().SequenceEqual(new[]{"ui-base@1.0.0","ui-root@1.0.0"}) && page.StatusText.Contains("enabled"),"UI confirmation did not enable the closure");
            }
        }));
        Test("audio staging binds verified profile bytes and waits for playback capability",()=>{
            string folder=Path.Combine(root,"audio-staging-\u03a9");Directory.CreateDirectory(folder);
            Dictionary<string,object> corpus;
            using(var input=typeof(LauncherSharedUiChecks).Assembly.GetManifestResourceStream("mod-launch-vectors.json"))
            using(var memory=new MemoryStream()){input.CopyTo(memory);corpus=ModJson.Read(System.Text.Encoding.UTF8.GetString(memory.ToArray()));}
            var vector=(Dictionary<string,object>)((object[])corpus["audioVectors"])[0];byte[] wav=Convert.FromBase64String((string)vector["wav"]);
            var library=new ModLibrary(folder,()=>false);var token=System.Threading.CancellationToken.None;
            var payloads=new Dictionary<string,byte[]>{{"audio/00000000.wav",wav},{"audio/00011170.wav",wav}};
            string package=LibraryPackage(root,"audio-mode","1.0.0",1,script:true,additionalPayloads:payloads);
            var mode=library.Install(package,Core.Hash(package),token);
            string manifest=Core.PathIn(folder,LauncherPlatform.ManifestPath(folder));Directory.CreateDirectory(Path.GetDirectoryName(manifest));
            File.WriteAllText(manifest,"{\"mod_script_version\":1}");
            Need(mode.ScriptMode && mode.HasAudio && !library.CanEnable(mode),"Audio capability gate was bypassed");
            WorkshopReject(()=>library.Enable(mode.Key,token),"Audio enabled before playback support");
            // Fixture-only capability: production game builds do not advertise it yet.
            File.WriteAllText(manifest,"{\"mod_script_version\":1,\"mod_audio_version\":1}");library.Enable(mode.Key,token);
            var info=GameSettings.StartInfo(folder);string cache=info.EnvironmentVariables["TSFP_MOD_AUDIO_CACHE"],path=info.EnvironmentVariables["TSFP_MOD_AUDIO_CAPSULE"];
            var pins=new[]{new ModAudioPin{Key=0,Package=mode.Sha256,Sha256=ModTrust.Hash(wav),Bytes=(ulong)wav.Length},new ModAudioPin{Key=70000,Package=mode.Sha256,Sha256=ModTrust.Hash(wav),Bytes=(ulong)wav.Length}};
            var capsule=ModAudioCapsule.Create(new[]{mode.Sha256},pins);
            Need(File.ReadAllBytes(path).SequenceEqual(capsule.Bytes) && info.EnvironmentVariables["TSFP_MOD_AUDIO_CAPSULE_SHA256"]==capsule.Sha256,"Staged audio capsule differs from selected profile");
            string clip=Directory.GetFiles(cache).Single();Need(Path.GetFileName(clip)==ModTrust.Hash(wav)+".wav" && File.ReadAllBytes(clip).SequenceEqual(wav),"Flat audio cache did not deduplicate exact verified bytes");
            Need(!GameSettings.StartInfo(folder,true).EnvironmentVariables.ContainsKey("TSFP_MOD_AUDIO_CAPSULE"),"Original recovery inherited audio activation");
            File.WriteAllText(path,"changed");WorkshopReject(()=>GameSettings.StartInfo(folder),"Tampered audio capsule accepted");
            library.Enable(mode.Key,token);info=GameSettings.StartInfo(folder);cache=info.EnvironmentVariables["TSFP_MOD_AUDIO_CACHE"];
            File.WriteAllText(Directory.GetFiles(cache).Single(),"changed");WorkshopReject(()=>GameSettings.StartInfo(folder),"Tampered cached audio accepted");
            library.Enable(mode.Key,token);info=GameSettings.StartInfo(folder);File.WriteAllText(Path.Combine(info.EnvironmentVariables["TSFP_MOD_AUDIO_CACHE"],"foreign.wav"),"keep");
            WorkshopReject(()=>GameSettings.StartInfo(folder),"Foreign audio cache files accepted");
            // Preserve unknown files rather than trying to repair that generation.
            string foreign=Path.Combine(info.EnvironmentVariables["TSFP_MOD_AUDIO_CACHE"],"foreign.wav");
            library.DisableAll(token);Need(library.Enabled().Length==0 && !GameSettings.StartInfo(folder).EnvironmentVariables.ContainsKey("TSFP_MOD_AUDIO_CACHE"),"Disabled audio remained active");
            Need(File.ReadAllText(foreign)=="keep","Unknown audio cache files were deleted during disable");
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
        Test("gameplay activation binds verified packages and refuses stale capabilities and source",()=>{
            string folder=Path.Combine(root,"mode-activation");Directory.CreateDirectory(folder);var library=new ModLibrary(folder,()=>false);
            string package=LibraryPackage(root,"gameplay","1.0.0",1,script:true);var mode=library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);
            string manifest=Core.PathIn(folder,LauncherPlatform.ManifestPath(folder));Directory.CreateDirectory(Path.GetDirectoryName(manifest));
            foreach(string value in new[]{"0","2","true","\"1\""}) {
                File.WriteAllText(manifest,"{\"mod_script_version\":"+value+"}");
                Need(!library.CanEnable(mode),"Unsupported capability accepted");WorkshopReject(()=>library.Enable(mode.Key,System.Threading.CancellationToken.None),"Mode enabled for an unsupported game");
            }
            File.WriteAllText(manifest,"{\"mod_script_version\":1}");Need(library.CanEnable(mode),"Supported mode unavailable");
            library.Enable(mode.Key,System.Threading.CancellationToken.None);
            Func<System.Diagnostics.ProcessStartInfo> configured=()=>{var info=new System.Diagnostics.ProcessStartInfo();info.EnvironmentVariables["RECOMP_SAVE_DIR"]=Path.Combine(folder,"Saves");library.Configure(info);return info;};
            var initial=configured();string path=initial.EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE"];
            var expected=ModScriptCapsule.Create(mode.Sha256,new string[0],System.Text.Encoding.UTF8.GetBytes("return true\n"));
            Need(File.ReadAllBytes(path).SequenceEqual(expected.Bytes) && initial.EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE_SHA256"]==expected.Sha256,"Mode capsule differed from authenticated source");
            WorkshopReject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Mode launched without explicit original save location");
            string texture=LibraryPackage(root,"mode-texture","1.0.0",1);var asset=library.Install(texture,Core.Hash(texture),System.Threading.CancellationToken.None);library.Enable(asset.Key,System.Threading.CancellationToken.None);
            var combined=configured();expected=ModScriptCapsule.Create(mode.Sha256,new[]{asset.Sha256},System.Text.Encoding.UTF8.GetBytes("return true\n"));
            Need(combined.EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE_SHA256"]==expected.Sha256,"Independent asset package was omitted from save identity");
            string updatePackage=LibraryPackage(root,"gameplay","1.0.1",1,script:true);var update=library.Install(updatePackage,Core.Hash(updatePackage),System.Threading.CancellationToken.None);
            library.Enable(update.Key,System.Threading.CancellationToken.None);var updated=ModScriptCapsule.Create(update.Sha256,new[]{asset.Sha256},System.Text.Encoding.UTF8.GetBytes("return true\n"));
            Need(updated.ProfileSha256!=expected.ProfileSha256 && File.ReadAllBytes(configured().EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE"]).SequenceEqual(updated.Bytes),"Mode update reused its old source or save identity");
            library.Enable(mode.Key,System.Threading.CancellationToken.None);combined=configured();
            Need(combined.EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE_SHA256"]==expected.Sha256 && library.List().Any(m=>m.Key==update.Key),"Mode rollback lost its original identity or retained update");
            string profile=File.ReadAllText(Path.Combine(folder,"Mods/profile.json"));
            string other=LibraryPackage(root,"second-mode","1.0.0",1,script:true);var second=library.Install(other,Core.Hash(other),System.Threading.CancellationToken.None);
            WorkshopReject(()=>library.Enable(second.Key,System.Threading.CancellationToken.None),"Two gameplay modes enabled together");
            Need(File.ReadAllText(Path.Combine(folder,"Mods/profile.json"))==profile,"Rejected mode changed active profile");
            File.WriteAllText(combined.EnvironmentVariables["TSFP_MOD_SCRIPT_CAPSULE"],"tampered");WorkshopReject(()=>configured(),"Modified capsule accepted");
            library.Enable(mode.Key,System.Threading.CancellationToken.None);File.WriteAllText(Path.Combine(mode.Directory,"content/scripts/main.lua"),"return false\n");WorkshopReject(()=>configured(),"Modified source accepted");
            library.DisableAll(System.Threading.CancellationToken.None);Need(library.Enabled().Length==0,"Broken source prevented recovery");
            // Selected mode metadata never survives the explicit original-game path.
            var plain=GameSettings.StartInfo(folder,true);Need(!plain.EnvironmentVariables.ContainsKey("TSFP_MOD_SCRIPT_CAPSULE") && !plain.EnvironmentVariables.ContainsKey("TSFP_MOD_SCRIPT_CAPSULE_SHA256"),"Original-game launch inherited a capsule");
        });
        Test("gameplay cache preference is local, validated and bypassed by original-game recovery",()=>{
            string folder=Path.Combine(root,"mode-cache-location");Directory.CreateDirectory(folder);string cache=Path.Combine(folder,"Private cache \u03a9");Directory.CreateDirectory(cache);
            Need(ModCacheLocation.Load(folder)=="","Default cache preference changed");
            foreach(string bad in new[]{"relative",Path.Combine(folder,"missing"),cache+"\nother"})WorkshopReject(()=>ModCacheLocation.Save(folder,bad),"Invalid cache path accepted");
            using(var page=new ModWorkshopPage(folder,false,new ModWorkshopClient(new WorkshopHandler(request=>{throw new Exception("Storage controls attempted a network request");})))) {
                Click(page.InstalledTab);var field=Find<TextBox>(page.View,"ModCacheLocation");var save=Find<Button>(page.View,"SaveModCacheLocation");
                Need(field.IsEnabled && save.IsEnabled,"Gameplay storage controls are unavailable");field.Text=cache;Click(save);
                Need(ModCacheLocation.Load(folder)==cache && page.StatusText.Contains("saved"),"Gameplay storage controls did not save the Unicode path");
                field.Text="";Click(save);Need(ModCacheLocation.Load(folder)=="","Storage controls could not restore the default");
            }
            ModCacheLocation.Save(folder,cache);Need(ModCacheLocation.Load(folder)==cache,"Unicode cache path did not round trip");
            string manifest=Core.PathIn(folder,LauncherPlatform.ManifestPath(folder));Directory.CreateDirectory(Path.GetDirectoryName(manifest));File.WriteAllText(manifest,"{\"mod_script_version\":1}");
            var library=new ModLibrary(folder,()=>false);string package=LibraryPackage(root,"cache-mode","1.0.0",1,script:true);var mode=library.Install(package,Core.Hash(package),System.Threading.CancellationToken.None);library.Enable(mode.Key,System.Threading.CancellationToken.None);
            var info=GameSettings.StartInfo(folder);Need(info.EnvironmentVariables["TSFP_MOD_SCRIPT_CACHE_PARENT"]==cache,"Selected local cache was not handed to the game");
            Directory.Delete(cache);WorkshopReject(()=>GameSettings.StartInfo(folder),"Unavailable configured cache silently fell back");
            info=GameSettings.StartInfo(folder,true);Need(!info.EnvironmentVariables.ContainsKey("TSFP_MOD_SCRIPT_CACHE_PARENT"),"Original-game recovery read or inherited a cache override");
            ModCacheLocation.Save(folder,"");Need(ModCacheLocation.Load(folder)=="","Default cache could not be restored");
            Need(!GameSettings.StartInfo(folder).EnvironmentVariables.ContainsKey("TSFP_MOD_SCRIPT_CACHE_PARENT"),"Cleared cache override was emitted");
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
        Action run=()=>{string root=Path.Combine(TestBase(),"tsfp-workshop-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);try{WorkshopChecks(root);LibraryChecks(root);CreatorChecks(root);}finally{Directory.Delete(root,true);}};
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
            CreatorChecks(root);
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
