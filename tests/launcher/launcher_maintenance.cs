using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

public static class LauncherMaintenanceChecks
{
    static IEnumerable<T> Descendants<T>(System.Windows.DependencyObject root) where T:System.Windows.DependencyObject
    {foreach(var child in System.Windows.LogicalTreeHelper.GetChildren(root).OfType<System.Windows.DependencyObject>()){if(child is T)yield return (T)child;foreach(var item in Descendants<T>(child))yield return item;}}
    static string temp;
    static int passed;
    static bool readOnly;
    static void TransactionTest(string name,Action body) { if(readOnly)Console.WriteLine("SKIP transaction check while preserving active game: "+name); else Test(name,body); }
    static void Require(bool value,string message="Maintenance assertion failed.") { if(!value)throw new Exception(message); }
    static void Reject(Action body) { bool failed=false;try{body();}catch{failed=true;}Require(failed,"Expected operation to be rejected."); }
    static void Test(string name,Action body) { try{body();}catch(Exception ex){throw new Exception("Maintenance check failed: "+name,ex);}passed++;Console.WriteLine("PASS "+name); }
    static void Put(string root,string name,string text) { string p=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(p));File.WriteAllText(p,text,new UTF8Encoding(false)); }
    static string Read(string root,string name) { return File.ReadAllText(Core.PathIn(root,name)); }
    static string Hash(byte[] bytes) { using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant(); }
    static string Hash(string text) { return Hash(Encoding.UTF8.GetBytes(text)); }
    static string Root() { string root=Path.Combine(temp,Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);return root; }
    static Dictionary<string,string> GameFiles() { return new Dictionary<string,string> { {"Bin/futureperfect_recomp.exe","game"},{"Launch.ps1","launch"},{"Settings/pc-controls.json","defaults"},{"Bin/runtime.dll","library"} }; }
    static void Self(string root) {
        Put(root,Core.LauncherName,"themed bootstrap");Put(root,Core.AppPath,"themed app");
        Put(root,Core.SelfManifest,SelfManifest(LauncherVersion.Value,"themed bootstrap","themed app"));
    }
    static string SelfManifest(string version,string boot,string app) { return Json.Encode(new { schema=1,version=version,files=new Dictionary<string,string> { {Core.LauncherName,Hash(boot)},{Core.AppPath,Hash(app)} } }); }
    static string Game(string root) {
        var files=GameFiles();foreach(var pair in files)Put(root,pair.Key,pair.Value);
        // Deliberately stale bundled launcher hashes must not flag an independent launcher.
        files[Core.LauncherName]="bundled bootstrap";files[Core.AppPath]="bundled app";
        Put(root,"BUILD.json",Json.Encode(new {version="v1.0.0",files=files.ToDictionary(p=>p.Key,p=>Hash(p.Value))}));
        Self(root);Put(root,"Settings/pc-controls.json","personal controls");Put(root,"Settings/pc-display.ini","personal display");
        Put(root,"Saves/profile","personal save");Put(root,"Data/owned.iso","personal ISO");Put(root,"Textures/pack.dds","personal textures");Put(root,"custom.txt","untracked");return root;
    }
    static void Personal(string root) {
        foreach(var item in new[]{new[]{"Settings/pc-controls.json","personal controls"},new[]{"Settings/pc-display.ini","personal display"},new[]{"Saves/profile","personal save"},new[]{"Data/owned.iso","personal ISO"},new[]{"Textures/pack.dds","personal textures"},new[]{"custom.txt","untracked"}})Require(Read(root,item[0])==item[1]);
    }
    static byte[] Archive(bool launcher) {
        var files=launcher?new Dictionary<string,string> { {Core.LauncherName,"release bootstrap"},{Core.AppPath,"release app"},{Core.SelfManifest,SelfManifest(LauncherVersion.Value,"release bootstrap","release app")} }:GameFiles();
        if(!launcher) {
            files[Core.LauncherName]="newer bundled bootstrap";
            files["BUILD.json"]=Json.Encode(new {version="v1.0.0",launcher_version="99.0.0",files=files.ToDictionary(p=>p.Key,p=>Hash(p.Value))});
        }
        using(var buffer=new MemoryStream()) {
            using(var zip=new ZipArchive(buffer,ZipArchiveMode.Create,true))foreach(var pair in files)
                using(var writer=new StreamWriter(zip.CreateEntry(pair.Key).Open(),new UTF8Encoding(false)))writer.Write(pair.Value);
            return buffer.ToArray();
        }
    }
    sealed class Progress : IProgress<double> { public Action<double> Action;public void Report(double value){if(Action!=null)Action(value);} }
    sealed class Handler : HttpMessageHandler {
        public Func<HttpRequestMessage,HttpResponseMessage> Reply;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token){var response=Reply(request);response.RequestMessage=request;return Task.FromResult(response);}
    }
    static string Url(string tag,string asset) { return "https://github.com/"+ReleaseClient.Repository+"/releases/download/"+tag+"/"+asset; }
    static object GameRelease(byte[] archive,string version="v1.0.0",bool draft=false) {
        string name="TimeSplitters-Future-Perfect-PC-"+version+"-windows-x64.zip";
        return new { tag_name=version,draft=draft,prerelease=true,assets=new[]{new { name=name,state="uploaded",size=archive.Length,digest="sha256:"+Hash(archive),browser_download_url=Url(version,name)} } };
    }
    static HttpResponseMessage Response(string text) { return new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(text)}; }
    static ReleaseClient GameClient(byte[] archive,Action<HttpRequestMessage> request=null,byte[] actual=null) {
        return new ReleaseClient(new Handler {Reply=r=> {
            if(request!=null)request(r);
            return r.RequestUri.Host=="api.github.com"?Response(Json.Encode(GameRelease(archive))):new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(actual??archive)};
        }});
    }
    static Dictionary<string,string> Snapshot(string root) { return Directory.GetFiles(root,"*",SearchOption.AllDirectories).ToDictionary(p=>p.Substring(root.Length),p=>Core.Hash(p)); }
    public static int Run(bool onlyReadOnly=false) {
        readOnly=onlyReadOnly;passed=0;temp=Path.Combine(Path.GetDirectoryName(typeof(LauncherMaintenanceChecks).Assembly.Location),"maintenance-tests-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temp);
        try {
            Test("verification is read-only and excludes personal files and independent launcher hashes",()=> {
                string root=Game(Root());var before=Snapshot(root);
                var result=InstallationMaintenance.Verify(root,false,null,CancellationToken.None);
                Require(result.Healthy && result.Checked==3 && result.Skipped==3);
                File.Delete(Core.PathIn(root,"Bin/runtime.dll"));Put(root,"Launch.ps1","damaged");before=Snapshot(root);
                result=InstallationMaintenance.Verify(root,false,null,CancellationToken.None);
                Require(result.Issues.Count==2 && result.Issues.Any(s=>s.Contains("missing")) && result.Issues.Any(s=>s.Contains("mismatch")));
                Require(before.OrderBy(p=>p.Key).SequenceEqual(Snapshot(root).OrderBy(p=>p.Key)));Personal(root);
            });
            Test("missing and protected manifests report safely without creating files",()=> {
                string root=Root();Require(!InstallationMaintenance.Verify(root,false,null,CancellationToken.None).Healthy && Directory.GetFileSystemEntries(root).Length==0);
                Put(root,"BUILD.json",Json.Encode(new{version="v1.0.0",files=new Dictionary<string,string>{{"Data/owned.iso",Hash("secret")}}}));
                var before=Snapshot(root);var result=InstallationMaintenance.Verify(root,false,null,CancellationToken.None);
                Require(!result.Healthy && result.Checked==0 && before.SequenceEqual(Snapshot(root)));
                Put(root,Core.SelfManifest,"invalid manifest");before=Snapshot(root);
                Require(!InstallationMaintenance.Verify(root,true,null,CancellationToken.None).Healthy && before.SequenceEqual(Snapshot(root)));
            });
            Test("launcher verification identifies missing bootstrap and malformed manifests",()=> {
                string root=Game(Root());Require(InstallationMaintenance.Verify(root,true,null,CancellationToken.None).Healthy);
                File.Delete(Core.PathIn(root,Core.LauncherName));var result=InstallationMaintenance.Verify(root,true,null,CancellationToken.None);
                Require(!result.Healthy && result.Version==LauncherVersion.Value && result.Issues.Count==1);
            });
            Test("verification cancellation is observed between files without writes",()=> {
                string root=Game(Root());var before=Snapshot(root);
                using(var cancel=new CancellationTokenSource()) {
                    var progress=new Progress {Action=p=>cancel.Cancel()};
                    Reject(()=>InstallationMaintenance.Verify(root,false,progress,cancel.Token));
                }Require(before.SequenceEqual(Snapshot(root)));
            });
            Test("cancelled graphics query returns without starting system discovery",()=> {
                using(var cancel=new CancellationTokenSource()) {cancel.Cancel();Reject(()=>InstallationMaintenance.GraphicsWithTimeout(cancel.Token).GetAwaiter().GetResult());}
            });
            Test("diagnostic reports omit paths, raw logs, secrets, saves and ISO contents",()=> {
                string root=Game(Root());string secret="SECRET_MARKER_847593";
                Put(root,".launcher/launcher.log","2026-10-17 01:00:00Z System.IO.IOException: "+root+" "+secret+" user@example.com 10.2.3.4\n arbitrary secret: "+secret);
                var before=Snapshot(root);string report=InstallationMaintenance.Report(root,null,null,"Synthetic GPU / driver 1");
                Require(report.Contains("System.IO.IOException") && report.Contains("Synthetic GPU"));
                Require(!report.Contains(secret) && !report.Contains(root) && !report.Contains("user@example.com") && !report.Contains("10.2.3.4") && !report.Contains("personal save") && !report.Contains("owned.iso"));
                Require(before.SequenceEqual(Snapshot(root)));
            });
            Test("GitHub drafts route to bug issues or Ideas without leaking diagnostics",()=> {
                string report="TimeSplitters launcher diagnostics\nRunning launcher: "+LauncherVersion.Value+"\nGPU: Test & Renderer + details = 日本語 / é\nVerification: Support/example.txt: checksum mismatch";
                foreach(bool enhancement in new[]{false,true}) {
                    var draft=DiagnosticIssueDraft.Create(enhancement,report);var uri=new Uri(draft.Url);
                    Require(uri.Scheme=="https" && uri.Host=="github.com" && uri.AbsolutePath=="/"+ReleaseClient.Repository+(enhancement?"/discussions/new":"/issues/new"));
                    var query=uri.Query.TrimStart('?').Split('&').Select(p=>p.Split(new[]{'='},2)).ToDictionary(p=>p[0],p=>Uri.UnescapeDataString(p[1]));
                    if(enhancement)Require(query.Count==1 && query["category"]=="ideas" && draft.ClipboardText=="" && !draft.RequiresPaste,
                        "Improvement drafts must not include diagnostics or require clipboard paste.");
                    else Require(query["template"]=="01-bug-report.yml" && query[DiagnosticIssueDraft.DiagnosticsField]==report && draft.ClipboardText==report && !draft.RequiresPaste);
                    Require(!query.ContainsKey("labels") && !query.ContainsKey("assignees"),"Draft must not require label/assignment privileges.");
                }
            });
            Test("improvement drafts need no diagnostic report even when none is available",()=> {
                foreach(string report in new[]{null,"",new String('x',70000)}) {
                    var draft=DiagnosticIssueDraft.Create(true,report);
                    Require(draft.Url=="https://github.com/"+ReleaseClient.Repository+"/discussions/new?category=ideas");
                    Require(draft.ClipboardText=="" && !draft.RequiresPaste);
                }
            });
            Test("long diagnostic drafts retain the complete report with a bounded paste fallback",()=> {
                foreach(string tail in new[]{new String('x',70000),String.Concat(Enumerable.Repeat("日本語 & \n",200))}) {
                    string report="TimeSplitters launcher diagnostics\nRunning launcher: "+LauncherVersion.Value+"\nInstalled game: v1.0.0\n"+tail;
                    var draft=DiagnosticIssueDraft.Create(false,report);
                    Require(draft.RequiresPaste && draft.Url.Length<=DiagnosticIssueDraft.MaximumUrlLength && draft.ClipboardText==report);
                    Require(Uri.UnescapeDataString(draft.Url).Contains("complete diagnostic report") && !draft.Url.Contains(tail));
                }
                Reject(()=>DiagnosticIssueDraft.Create(false,"   "));
            });
            Test("only the privacy-filtered report is transferred to GitHub drafts",()=> {
                string root=Game(Root());string secret="DO_NOT_EXPORT_946834";
                Put(root,".launcher/launcher.log","2026-10-17 01:00:00Z System.IO.IOException: "+root+" "+secret);
                string report=InstallationMaintenance.Report(root,null,null,"Synthetic GPU");
                foreach(bool enhancement in new[]{false,true}) {
                    var draft=DiagnosticIssueDraft.Create(enhancement,report);
                    Require(!draft.ClipboardText.Contains(root) && !draft.ClipboardText.Contains(secret));
                    Require(!Uri.UnescapeDataString(draft.Url).Contains(secret) && !Uri.UnescapeDataString(draft.Url).Contains(root));
                }
            });
            Test("GitHub report controls are available only on Diagnostics and disabled while busy",()=> {
                Exception failure=null;var thread=new Thread(()=> {try {
                    var page=new MaintenancePage(true);Require(page.ReportBug!=null && page.RequestEnhancement!=null && page.OpenFiles==null);
                    page.SetBusy(true,false);Require(!page.ReportBug.IsEnabled && !page.RequestEnhancement.IsEnabled);
                    page.SetBusy(false,false);Require(page.ReportBug.IsEnabled && page.RequestEnhancement.IsEnabled);
                    var repair=new MaintenancePage(false);Require(repair.ReportBug==null && repair.RequestEnhancement==null && repair.OpenFiles!=null);
                    repair.SetBusy(true,false);Require(!repair.OpenFiles.IsEnabled);
                    repair.SetBusy(false,false);Require(repair.OpenFiles.IsEnabled);
                }catch(Exception ex){failure=ex;}});thread.IsBackground=true;thread.SetApartmentState(ApartmentState.STA);thread.Start();Require(thread.Join(60000));if(failure!=null)throw failure;
            });
            Test("repair pins exact game version and refuses drafts and missing releases",()=> {
                byte[] archive=Archive(false);
                using(var client=GameClient(archive,r=> {if(r.RequestUri.Host=="api.github.com")Require(r.RequestUri.AbsolutePath.EndsWith("/releases/tags/v1.0.0"));}))
                    Require(client.FindGame("v1.0.0",CancellationToken.None).GetAwaiter().GetResult().Tag=="v1.0.0");
                foreach(var response in new[]{new HttpResponseMessage(HttpStatusCode.NotFound),Response(Json.Encode(GameRelease(archive,"v1.0.0",true))),Response(Json.Encode(GameRelease(archive,"v2.0.0")))})
                    using(var client=new ReleaseClient(new Handler {Reply=r=>response}))Reject(()=>client.FindGame("v1.0.0",CancellationToken.None).GetAwaiter().GetResult());
            });
            TransactionTest("game repair changes only damaged owned files and preserves game rollback",()=> {
                string root=Game(Root());Put(root,"Bin/runtime.dll","broken");File.Delete(Core.PathIn(root,"Launch.ps1"));
                Put(root,".launcher/rollback/files/BUILD.json","previous game marker");Put(root,".launcher/rollback/journal.json","[]");
                string self=Read(root,Core.SelfManifest);byte[] archive=Archive(false);
                using(var client=GameClient(archive))InstallationMaintenance.PrepareGameRepair(root,"v1.0.0",client,new Progress(),CancellationToken.None).GetAwaiter().GetResult();
                Require(Read(root,"Bin/runtime.dll")=="broken");Core.InstallPending(root);
                Require(Read(root,"Bin/runtime.dll")=="library" && Read(root,"Launch.ps1")=="launch" && Read(root,Core.SelfManifest)==self && Read(root,Core.LauncherName)=="themed bootstrap");
                Require(Read(root,".launcher/rollback/files/BUILD.json")=="previous game marker" && Directory.Exists(Core.PathIn(root,".launcher/repair-rollback")));
                Require(!File.Exists(Core.PathIn(root,".launcher/repair-rollback/files/Bin/futureperfect_recomp.exe")),"Healthy runtime was unnecessarily replaced.");Personal(root);
                string build=Core.GameBuildHash(root);
                Core.FinishGameSession(root,build,false,0);Require(Directory.Exists(Core.PathIn(root,".launcher/repair-rollback")));
                Core.FinishGameSession(root,build,true,1);Require(Directory.Exists(Core.PathIn(root,".launcher/repair-rollback")));
                Core.FinishGameSession(root,build,true,0);Require(!Directory.Exists(Core.PathIn(root,".launcher/repair-rollback")));Personal(root);
            });
            TransactionTest("repair cancels downloads and rejects corrupt archives without installed changes",()=> {
                foreach(bool cancelDownload in new[]{false,true}) {
                    string root=Game(Root());var before=Snapshot(root);byte[] archive=Archive(false);
                    using(var cancel=new CancellationTokenSource())using(var client=GameClient(archive,null,cancelDownload?null:Encoding.UTF8.GetBytes("bad"))) {
                        var progress=new Progress {Action=p=> {if(cancelDownload)cancel.Cancel();}};
                        Reject(()=>InstallationMaintenance.PrepareGameRepair(root,"v1.0.0",client,progress,cancel.Token).GetAwaiter().GetResult());
                    }
                    var after=Snapshot(root);
                    var installed=after.Where(pair=>!pair.Key.Replace('\\','/').StartsWith("/.launcher/downloads/",StringComparison.OrdinalIgnoreCase));
                    Require(before.SequenceEqual(installed) && !Directory.Exists(Core.PathIn(root,".launcher/pending")));
                    string cache=Core.PathIn(root,".launcher/downloads");
                    Require(!Directory.Exists(cache) || Directory.GetFiles(cache).All(file=>new[]{"game.part","game.json","game.lock"}.Contains(Path.GetFileName(file))));
                }
            });
            TransactionTest("repair preserves pre-existing staging and rejects changed installed version",()=> {
                string root=Game(Root());Put(root,".launcher/pending/unknown","keep");int requests=0;
                using(var client=GameClient(Archive(false),r=>requests++))Reject(()=>InstallationMaintenance.PrepareGameRepair(root,"v1.0.0",client,null,CancellationToken.None).GetAwaiter().GetResult());
                Require(requests==0 && Read(root,".launcher/pending/unknown")=="keep");
                root=Game(Root());using(var client=GameClient(Archive(false)))InstallationMaintenance.PrepareGameRepair(root,"v1.0.0",client,new Progress(),CancellationToken.None).GetAwaiter().GetResult();
                Put(root,"BUILD.json","{\"version\":\"v2.0.0\"}");var before=Snapshot(root);Reject(()=>Core.InstallPending(root));Require(before.SequenceEqual(Snapshot(root)));
            });
            TransactionTest("repair cleanup preserves an ISO placed inside managed staging",()=> {
                foreach(bool launcher in new[]{false,true}) {
                    string root=Game(Root());byte[] archive=Archive(launcher);
                    string staging=launcher?".launcher/self-pending":".launcher/pending";
                    using(var client=new ReleaseClient(new Handler {Reply=r=> {
                        if(r.RequestUri.Host=="api.github.com")return Response(Json.Encode(GameRelease(archive)));
                        Put(root,staging+"/owned.iso","preserve this ISO");
                        return new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(new byte[0])};
                    }})) {
                        if(launcher) {
                            var release=new Release {Tag=LauncherVersion.Value,Size=archive.Length,Digest="sha256:"+Hash(archive),AssetUrl=Url("launcher-v"+LauncherVersion.Value,"launcher.zip")};
                            Reject(()=>SelfUpdate.Prepare(root,release,client,new Progress(),CancellationToken.None,true).GetAwaiter().GetResult());
                        } else Reject(()=>InstallationMaintenance.PrepareGameRepair(root,"v1.0.0",client,new Progress(),CancellationToken.None).GetAwaiter().GetResult());
                    }
                    Require(Read(root,staging+"/owned.iso")=="preserve this ISO");Personal(root);
                }
            });
            TransactionTest("interrupted repair recovers without replacing the previous game rollback",()=> {
                string root=Game(Root());Put(root,".launcher/rollback/preserve","previous game");
                Put(root,".launcher/repair-transaction/files/Launch.ps1","launch");
                Put(root,".launcher/repair-transaction/journal.json",Json.Encode(new[]{new Original {Name="Launch.ps1",Existed=true}}));
                Put(root,"Launch.ps1","partial repair");Require(Core.Recover(root));
                Require(Read(root,"Launch.ps1")=="launch" && Read(root,".launcher/rollback/preserve")=="previous game" && !Directory.Exists(Core.PathIn(root,".launcher/repair-transaction")));Personal(root);
            });
            TransactionTest("forced same-version launcher repair uses verified package and protects staging",()=> {
                byte[] archive=Archive(true);string version=LauncherVersion.Value;string root=Game(Root());
                var release=new Release {Tag=version,Size=archive.Length,Digest="sha256:"+Hash(archive),AssetUrl=Url("launcher-v"+version,"launcher.zip")};
                using(var client=new ReleaseClient(new Handler {Reply=r=>new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(archive)}})) {
                    Require(!SelfUpdate.Prepare(root,release,client,new Progress(),CancellationToken.None).GetAwaiter().GetResult());
                    Require(SelfUpdate.Prepare(root,release,client,new Progress(),CancellationToken.None,true).GetAwaiter().GetResult());
                    Require(Read(root,Core.AppPath)=="themed app");SelfUpdate.InstallPending(root);
                    Require(SelfUpdate.Complete(root) && Read(root,Core.AppPath)=="release app");Personal(root);
                    Put(root,".launcher/self-pending/unknown","keep");Reject(()=>SelfUpdate.Prepare(root,release,client,new Progress(),CancellationToken.None,true).GetAwaiter().GetResult());
                    Require(Read(root,".launcher/self-pending/unknown")=="keep");
                }
            });
            Test("launcher exact-release lookup verifies metadata and never substitutes a different version",()=> {
                byte[] archive=Archive(true);string version=LauncherVersion.Value;string name="TimeSplitters-Launcher-v"+version+"-windows-x64.zip";
                object metadata=new {schema=1,version=version,asset_name=name,size=archive.Length,sha256=Hash(archive)};
                var record=new {tag_name="launcher-v"+version,draft=false,prerelease=false,assets=new object[]{
                    new {name=name,state="uploaded",size=archive.Length,digest="sha256:"+Hash(archive),browser_download_url=Url("launcher-v"+version,name)},
                    new {name="launcher-update.json",state="uploaded",browser_download_url=Url("launcher-v"+version,"launcher-update.json")} }};
                using(var client=new ReleaseClient(new Handler {Reply=r=>Response(Json.Encode(r.RequestUri.Host=="api.github.com"?(object)record:metadata))})) {
                    Require(client.FindLauncher(version,CancellationToken.None).GetAwaiter().GetResult().Tag==version);
                    Reject(()=>client.FindLauncher("99.0.0",CancellationToken.None).GetAwaiter().GetResult());
                }
            });
            Test("maintenance and diagnostics share the main window and preserve settings drafts",()=> {
                Exception failure=null;var thread=new Thread(()=> {try {
                    var ui=new LauncherWindow(Root(),true);
                    try {
                        Require(ui.Window.FindName("FilesNav")==null && ui.Window.FindName("DiagnosticsNav")==null);
                        ui.ShowSettings();var settings=(System.Windows.Controls.ContentControl)ui.Window.FindName("SettingsHost");var draft=settings.Content;
                        ui.ShowMaintenance(false);var host=(System.Windows.Controls.ContentControl)ui.Window.FindName("MaintenanceHost");var area=host.Content;
                        Require(host.Visibility==System.Windows.Visibility.Visible && settings.Visibility==System.Windows.Visibility.Collapsed && System.Windows.Window.GetWindow((System.Windows.DependencyObject)host.Content)==ui.Window);
                        Require(((System.Windows.Controls.Border)ui.Window.FindName("SidebarPanel")).Visibility==System.Windows.Visibility.Visible);
                        var tabs=Descendants<System.Windows.Controls.TabControl>((System.Windows.DependencyObject)host.Content).Single();
                        Require(tabs.Items.Count==2 && tabs.SelectedIndex==0);
                        Require(((System.Windows.Controls.TextBlock)((System.Windows.Controls.TabItem)tabs.Items[0]).Header).Text=="Repair / Verify" && ((System.Windows.Controls.TextBlock)((System.Windows.Controls.TabItem)tabs.Items[1]).Header).Text=="Diagnostics");
                        var repair=(System.Windows.DependencyObject)((System.Windows.Controls.TabItem)tabs.Items[0]).Content;
                        var diagnostics=(System.Windows.DependencyObject)((System.Windows.Controls.TabItem)tabs.Items[1]).Content;
                        var repairLogs=Descendants<System.Windows.Controls.Button>(repair).Single(c=>c.Name=="MaintenanceLogs");
                        var diagnosticLogs=Descendants<System.Windows.Controls.Button>(diagnostics).Single(c=>c.Name=="MaintenanceLogs");
                        Require(repairLogs.Visibility==System.Windows.Visibility.Visible && diagnosticLogs.Visibility==System.Windows.Visibility.Visible && repairLogs.Parent is System.Windows.Controls.DockPanel && diagnosticLogs.Parent is System.Windows.Controls.DockPanel);
                        var verification=Descendants<System.Windows.Controls.TextBox>(repair).Single();verification.Text="Retained verification result";
                        tabs.SelectedIndex=1;var report=Descendants<System.Windows.Controls.TextBox>(diagnostics).Single();Require(report.Text.Contains("interface preview"));report.Text="Retained diagnostics report";
                        foreach(string button in new[]{"RefreshDiagnostics","CopyDiagnostics","ReportDiagnosticBug","RequestDiagnosticEnhancement"})Require(Descendants<System.Windows.Controls.Button>(diagnostics).Any(c=>c.Name==button));
                        diagnosticLogs.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));
                        Require(Descendants<System.Windows.Controls.TextBlock>(diagnostics).Any(c=>c.Text.Contains("local launcher log folder")));
                        tabs.SelectedIndex=0;repairLogs.RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));tabs.SelectedIndex=1;
                        Require(report.Text=="Retained diagnostics report" && verification.Text=="Retained verification result" && ReferenceEquals(host.Content,area));
                        var busy=typeof(LauncherWindow).GetField("busy",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
                        var refresh=typeof(LauncherWindow).GetMethod("Refresh",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
                        try{busy.SetValue(ui,true);refresh.Invoke(ui,null);Require(tabs.IsEnabled && repairLogs.IsEnabled && diagnosticLogs.IsEnabled);tabs.SelectedIndex=0;Require(tabs.SelectedIndex==1 && Descendants<System.Windows.Controls.Button>(diagnostics).Single(c=>c.Name=="CancelMaintenance").IsEnabled);}finally{busy.SetValue(ui,false);refresh.Invoke(ui,null);}
                        ((System.Windows.Controls.Button)ui.Window.FindName("RepairNav")).RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(tabs.SelectedIndex==0 && tabs.IsEnabled);
                        ui.ShowMaintenance(true);Require(host.Visibility==System.Windows.Visibility.Visible && tabs.SelectedIndex==1 && report.Text=="Retained diagnostics report");
                        var checking=typeof(LauncherWindow).GetField("checking",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
                        try{checking.SetValue(ui,true);refresh.Invoke(ui,null);((System.Windows.Controls.Button)ui.Window.FindName("RepairNav")).RaiseEvent(new System.Windows.RoutedEventArgs(System.Windows.Controls.Button.ClickEvent));Require(tabs.SelectedIndex==0 && repairLogs.IsEnabled);}finally{checking.SetValue(ui,false);refresh.Invoke(ui,null);}

                        ui.ShowSettings();Require(ReferenceEquals(settings.Content,draft) && host.Visibility==System.Windows.Visibility.Collapsed);
                    }finally{ui.Window.Close();}
                }catch(Exception ex){failure=ex;}});
                thread.IsBackground=true;thread.SetApartmentState(ApartmentState.STA);thread.Start();Require(thread.Join(60000),"Maintenance UI timed out.");if(failure!=null)throw failure;
            });
            Console.WriteLine("PASS: "+passed+" isolated launcher maintenance checks.");return 0;
        } finally {
            string parent=Path.GetDirectoryName(typeof(LauncherMaintenanceChecks).Assembly.Location);
            Require(Path.GetDirectoryName(temp)==parent && (File.GetAttributes(temp)&FileAttributes.ReparsePoint)==0);
            Directory.Delete(temp,true);
        }
    }
}
