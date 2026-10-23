// Synthetic release metadata and native UI only; no game or user files.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using TimeSplitters.Launcher;

public static class LauncherVersionChecks
{
    static int passed;
    static void Need(bool value) {if(!value)throw new Exception("Game versions assertion failed.");}
    static void Test(string name,Action body) {body();passed++;Console.WriteLine("PASS "+name);}
    static void Reject(Action body) {try{body();}catch(InvalidDataException){return;}throw new Exception("Expected release rejection.");}
    sealed class Handler : HttpMessageHandler {
        public Func<HttpRequestMessage,HttpResponseMessage> Reply;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token) {token.ThrowIfCancellationRequested();return Task.FromResult(Reply(request));}
    }
    sealed class WaitingHandler : HttpMessageHandler {
        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token) {await Task.Delay(Timeout.Infinite,token);return null;}
    }
    static Dictionary<string,object> Record(string tag,bool preview=false,bool draft=false)
    {
        string name="TimeSplitters-Future-Perfect-PC-"+tag+"-windows-x64.zip";
        return Json.Object(Json.Encode(new {tag_name=tag,prerelease=preview,draft=draft,published_at="2026-10-17T12:00:00Z",body="## Changes\nCompare this build.",assets=new[]{new {
            name=name,state="uploaded",size=100,digest="sha256:"+new string('a',64),browser_download_url=ReleaseClient.ReleasesPage+"/download/"+tag+"/"+name}}}));
    }
    static Dictionary<string,object> Asset(Dictionary<string,object> record) {return (Dictionary<string,object>)((System.Collections.IList)record["assets"])[0];}
    static GameReleaseList Page(params object[] records) {return ReleaseClient.GameVersions(Json.Encode(records));}
    static IEnumerable<T> All<T>(DependencyObject root) where T:DependencyObject
    {foreach(var child in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>()){if(child is T)yield return (T)child;foreach(var next in All<T>(child))yield return next;}}
    static void Complete(Task task)
    {
        var dispatcher=Dispatcher.CurrentDispatcher;
        if(!task.IsCompleted){var frame=new DispatcherFrame();task.ContinueWith(t=>dispatcher.BeginInvoke(new Action(()=>frame.Continue=false)));Dispatcher.PushFrame(frame);}
        task.GetAwaiter().GetResult();
    }
    public static int Run()
    {
        Exception failure=null;var thread=new Thread(()=>{try{RunCore();}catch(Exception ex){failure=ex;}});
        thread.SetApartmentState(ApartmentState.STA);thread.IsBackground=true;thread.Start();Need(thread.Join(60000));if(failure!=null)throw failure;
        Console.WriteLine("PASS: "+passed+" isolated game version checks.");return 0;
    }
    static void RunCore()
    {
        var records=new[]{Record("v1.0.0"),Record("v1.0.0-alpha.10"),Record("v1.0.0-alpha.2"),Record("v1.1.0",true),Record("v9.0.0",false,true),Record("launcher-v1.2.12")};
        Test("channel discovery reaches older release pages without substituting unrelated experiments",()=>{
            int requests=0;
            using(var client=new ReleaseClient(new Handler {Reply=r=>{
                requests++;Need(r.Method==HttpMethod.Get);
                bool first=r.RequestUri.Query.EndsWith("&page=1",StringComparison.Ordinal);
                var page=first?Enumerable.Repeat(Record("launcher-v1.3.0"),100).ToArray():new[]{Record("v0.4.1"),Record("v0.5.0-story-coop.2",true),Record("v0.6.0-other.7",true)};
                return new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(Json.Encode(page))};
            }})) {
                var catalog=client.ReadGameCatalog(CancellationToken.None).GetAwaiter().GetResult();
                Need(requests==2 && GameChannels.Select(catalog,"stable").Tag=="v0.4.1" && GameChannels.Select(catalog,"story-coop").Tag=="v0.5.0-story-coop.2");
            }
        });
        Test("named release news does not select a different experiment",()=>{
            using(var client=new ReleaseClient(new Handler {Reply=r=>new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(Json.Encode(new[]{Record("v0.4.1"),Record("v0.5.0-story-coop.1",true),Record("v0.6.0-other.8",true)}))}})) {
                var news=client.ReadNews("story-coop","","",CancellationToken.None).GetAwaiter().GetResult();
                Need(news.Game.Tag=="v0.5.0-story-coop.1" && news.Preview && news.Launcher==null);
            }
        });
        Test("history lists every compatible stable and preview release in semantic order",()=>{
            var list=ReleaseClient.GameVersions(Json.Encode(records));Need(list.Releases.Select(r=>r.Tag).SequenceEqual(new[]{"v1.1.0","v1.0.0","v1.0.0-alpha.10","v1.0.0-alpha.2"}));
            Need(list.Releases[0].IsPreview && !list.Releases[1].IsPreview && list.Releases[2].IsPreview && !list.HasMore);
            Need(ReleaseClient.Select(Json.Encode(records),false).Tag=="v1.0.0"); // Normal updates keep their channel rules.
        });
        Test("history skips source-only, incomplete, unverified and foreign assets without hiding valid releases",()=>{
            var source=Record("v1.1.0");source["assets"]=new object[0];
            var pending=Record("v1.2.0");Asset(pending)["state"]="new";
            var badHash=Record("v1.3.0");Asset(badHash)["digest"]="";
            var foreign=Record("v1.4.0");Asset(foreign)["browser_download_url"]="https://example.com/game.zip";
            var tooBig=Record("v1.5.0");Asset(tooBig)["size"]=5L*1024*1024*1024;
            var list=Page(source,pending,badHash,foreign,tooBig,null,"malformed",Record("v1.0.0"));Need(list.Releases.Length==1 && list.Releases[0].Tag=="v1.0.0");
        });
        Test("older releases can use a repository checksum sidecar",()=>{
            var record=Record("v0.1.0");var asset=Asset(record);asset["digest"]="";
            string name=(string)asset["name"]+".sha256";record["assets"]=new object[]{asset,new {name=name,state="uploaded",browser_download_url=ReleaseClient.ReleasesPage+"/download/v0.1.0/"+name}};
            var release=Page(record).Releases.Single();Need(release.ChecksumUrl.EndsWith(".sha256") && release.Digest=="");
        });
        Test("pagination counts raw records including launcher releases and removes duplicate game tags",()=>{
            var list=Page(Enumerable.Repeat((object)Record("launcher-v1.2.12"),100).ToArray());Need(list.HasMore && list.Releases.Length==0);
            Need(Page(Record("v1.0.0"),Record("v1.0.0")).Releases.Length==1);
            Need(!Page().HasMore);Reject(()=>ReleaseClient.GameVersions("{}"));Reject(()=>Page(Enumerable.Repeat((object)Record("v1.0.0"),101).ToArray()));
        });
        Test("release history requests are paginated GETs pinned to the repository",()=>{
            using(var client=new ReleaseClient(new Handler {Reply=r=>{Need(r.Method==HttpMethod.Get && r.RequestUri.ToString()=="https://api.github.com/repos/"+ReleaseClient.Repository+"/releases?per_page=100&page=2");return new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(Json.Encode(records))};}})) {
                Need(client.ReadGameVersions(2,CancellationToken.None).GetAwaiter().GetResult().Releases.Length==4);
                Reject(()=>client.ReadGameVersions(0,CancellationToken.None).GetAwaiter().GetResult());
            }
        });
        Test("rate limit, missing responses and cancellation do not become empty successful lists",()=>{
            foreach(var code in new[]{HttpStatusCode.Forbidden,(HttpStatusCode)429,HttpStatusCode.NotFound}) {
                bool failed=false;using(var client=new ReleaseClient(new Handler {Reply=r=>new HttpResponseMessage(code)}))try{client.ReadGameVersions(1,CancellationToken.None).GetAwaiter().GetResult();}catch(IOException){failed=true;}catch(InvalidDataException){failed=true;}Need(failed);
            }
            using(var client=new ReleaseClient(new WaitingHandler()))using(var cancel=new CancellationTokenSource(40)) {
                bool canceled=false;try{client.ReadGameVersions(1,cancel.Token).GetAwaiter().GetResult();}catch(OperationCanceledException){canceled=true;}Need(canceled);
            }
        });
        Test("version picker preserves selection through older pages and refresh failure",()=>{
            var page=new GameVersionsPage();page.SetInstalled("v1.0.0");page.SetReleases(Page(records),true);Need(page.Selected.Tag=="v1.0.0" && page.Install.Content.ToString().Contains("Reinstall"));
            page.SetReleases(Page(Record("v0.1.0"),Record("v1.0.0")),false);Need(page.Count==5 && page.Selected.Tag=="v1.0.0");
            page.SetLoading(true);Need(!page.Install.IsEnabled && !page.Refresh.IsEnabled);page.Failed();Need(page.Install.IsEnabled && page.Refresh.IsEnabled && page.Count==5 && page.Selected.Tag=="v1.0.0" && page.StatusText.Contains("kept"));
            page.SetReleases(Page(Record("v0.1.0")),true);Need(page.Count==1 && page.Selected.Tag=="v0.1.0");
        });
        Test("version install is blocked while busy or playing and empty pages can load older releases",()=>{
            var page=new GameVersionsPage();page.SetReleases(new GameReleaseList {Releases=new Release[0],HasMore=true},true);Need(!page.Install.IsEnabled && page.Older.IsEnabled);
            page.SetReleases(Page(Record("v1.0.0")),false);Need(page.Install.IsEnabled && !page.Older.IsEnabled);
            page.SetBusy(false,true);Need(!page.Install.IsEnabled && page.OnGitHub.IsEnabled);page.SetBusy(true,false);Need(!page.Install.IsEnabled && !page.Refresh.IsEnabled && !page.Versions.IsEnabled);page.SetBusy(false,false);Need(page.Install.IsEnabled);
        });
        Test("remote notes are bounded inert native text",()=>{
            var record=Record("v1.0.0");record["body"]="<Window><Button>Injected</Button></Window> ![Image](https://invalid.example/remote.png)"+new string('x',25000);
            var page=new GameVersionsPage();page.SetReleases(Page(record),true);var document=All<ReleaseNotesMarkdown>(page.View).Single().Document;var text=new System.Windows.Documents.TextRange(document.ContentStart,document.ContentEnd).Text;
            Need(text.Contains("<Window>") && text.Contains("Image: view on GitHub") && text.Contains("Notes shortened") && text.Length<24500 && !All<WebBrowser>(page.View).Any() && !All<Image>(page.View).Any());
        });
        Test("normal updates refuse an older game while explicit download and retry retain the chosen older tag",()=>{
            string root=Path.Combine(Path.GetTempPath(),"tsfp-version-retry-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(Path.Combine(root,"Bin"));
            File.WriteAllText(Path.Combine(root,"BUILD.json"),Json.Encode(new {version="v2.0.0"}));File.WriteAllText(Path.Combine(root,"Bin/futureperfect_recomp.exe"),"synthetic installed runtime");File.WriteAllText(Path.Combine(root,"Launch.ps1"),"synthetic launch");
            var requests=new List<string>();
            var client=new ReleaseClient(new Handler {Reply=r=>{requests.Add(r.RequestUri.AbsolutePath);return new HttpResponseMessage(HttpStatusCode.Forbidden){Content=new StringContent("Synthetic request limit")};}});
            var previous=SynchronizationContext.Current;SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext(Dispatcher.CurrentDispatcher));
            var ui=new LauncherWindow(root,false,client);const BindingFlags flags=BindingFlags.NonPublic|BindingFlags.Instance;
            try {
                var chosen=Page(Record("v1.0.0")).Releases.Single();typeof(LauncherWindow).GetField("available",flags).SetValue(ui,chosen);
                Complete((Task)typeof(LauncherWindow).GetMethod("Update",flags).Invoke(ui,null));Need(requests.Count==0);
                Complete((Task)typeof(LauncherWindow).GetMethod("DownloadGame",flags).Invoke(ui,new object[]{chosen,true}));Need(requests.Count==1);
                typeof(LauncherWindow).GetField("available",flags).SetValue(ui,Page(Record("v3.0.0")).Releases.Single());
                var retry=(Func<Task>)typeof(LauncherWindow).GetField("retryOperation",flags).GetValue(ui);Need(retry!=null);Complete(retry());
                Need(requests.Count==2 && requests.All(path=>path.Contains("/download/v1.0.0/")) && Core.InstalledVersion(root)=="v2.0.0" && !Directory.Exists(Path.Combine(root,".launcher/pending")));
            }finally{ui.Window.Close();SynchronizationContext.SetSynchronizationContext(previous);Directory.Delete(root,true);}
        });
        Test("version page uses the main window, keeps the sidebar and leaves settings and maintenance drafts intact",()=>{
            string root=Path.Combine(Path.GetTempPath(),"tsfp-version-ui-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
            var ui=new LauncherWindow(root,true);
            try {
                ui.ShowSettings();var settings=(ContentControl)ui.Window.FindName("SettingsHost");object draft=settings.Content;
                ui.ShowMaintenance(false);var maintenance=(ContentControl)ui.Window.FindName("MaintenanceHost");object repair=maintenance.Content;
                ui.ShowGameVersions();var host=(ContentControl)ui.Window.FindName("VersionsHost");object versionView=host.Content;
                Need(host.Visibility==Visibility.Visible && settings.Visibility==Visibility.Collapsed && maintenance.Visibility==Visibility.Collapsed && Window.GetWindow((DependencyObject)versionView)==ui.Window);
                Need(((Border)ui.Window.FindName("SidebarPanel")).Visibility==Visibility.Visible);
                ui.ShowNews();Need(host.Visibility==Visibility.Collapsed);ui.ShowGameVersions();Need(ReferenceEquals(host.Content,versionView));
                var install=All<Button>((DependencyObject)versionView).Single(b=>b.Content.ToString().Contains("selected version"));install.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                Need(All<TextBlock>((DependencyObject)versionView).Any(t=>t.Text.Contains("No installed files were changed")));
                ui.ShowSettings();Need(host.Visibility==Visibility.Collapsed && ReferenceEquals(settings.Content,draft));ui.ShowMaintenance(true);Need(host.Visibility==Visibility.Collapsed && ReferenceEquals(maintenance.Content,repair));
                ((Button)ui.Window.FindName("HomeNav")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));Need(host.Visibility==Visibility.Collapsed && ((Grid)ui.Window.FindName("CommandCenter")).Visibility==Visibility.Visible);
                Need(!Directory.EnumerateFileSystemEntries(root).Any());
            }finally{ui.Window.Close();Directory.Delete(root);}
        });
    }
}
