using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Automation;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class WorkshopRelease
    {
        public string Id, Name, Version, Author, Description, Sha256, ScanEngine;
        public DateTime ScannedUtc;
        public string[] Categories=new string[0];
        public string[] Dependencies=new string[0];
        public long PayloadBytes;
        internal InstalledMod Package;
        public string SearchText { get { return Id+" "+Name+" "+Author+" "+Description; } }
        public static readonly IComparer<string> Versions=Comparer<string>.Create((left,right)=>{
            string[] a=left.Split('.'),b=right.Split('.');
            for(int i=0;i<3;i++){int order=a[i].Length.CompareTo(b[i].Length);if(order==0)order=String.CompareOrdinal(a[i],b[i]);if(order!=0)return order;}
            return 0;
        });
        public static WorkshopRelease[] Select(IEnumerable<WorkshopRelease> source,string query,string creator,string category,bool older,string sort)
        {
            var selected=source;
            if(!older)selected=selected.GroupBy(r=>r.Id,StringComparer.Ordinal).Select(group=>group.OrderByDescending(r=>r.Version,Versions).First());
            selected=selected.Where(r=>r.SearchText.IndexOf((query??"").Trim(),StringComparison.OrdinalIgnoreCase)>=0 &&
                (r.Author??"").IndexOf((creator??"").Trim(),StringComparison.OrdinalIgnoreCase)>=0 &&
                (String.IsNullOrEmpty(category) || r.Categories.Contains(category)));
            if(sort=="creator")return selected.OrderBy(r=>r.Author,StringComparer.OrdinalIgnoreCase).ThenBy(r=>r.Id,StringComparer.Ordinal).ThenByDescending(r=>r.Version,Versions).ToArray();
            if(sort=="version")return selected.OrderByDescending(r=>r.Version,Versions).ThenBy(r=>r.Id,StringComparer.Ordinal).ToArray();
            return selected.OrderBy(r=>r.Name,StringComparer.OrdinalIgnoreCase).ThenBy(r=>r.Id,StringComparer.Ordinal).ThenByDescending(r=>r.Version,Versions).ToArray();
        }
        public static string ContentName(string kind){return kind=="textures"?"Textures":kind=="scripts"?"Gameplay scripts":kind=="maps"?"Maps":kind=="audio"?"Audio":"Other content";}
    }

    // Pure planning against one freshly verified catalog. Execution must recheck
    // availability and installed bytes; this model never downloads or activates.
    public sealed class WorkshopInstallPlan
    {
        public readonly WorkshopRelease[] Ordered, Downloads;
        public readonly long PayloadBytes, NewPayloadBytes;
        WorkshopInstallPlan(WorkshopRelease[] ordered,WorkshopRelease[] downloads)
        {Ordered=ordered;Downloads=downloads;PayloadBytes=ordered.Sum(r=>r.PayloadBytes);NewPayloadBytes=downloads.Sum(r=>r.PayloadBytes);}
        public static WorkshopInstallPlan Create(WorkshopRelease selected,IEnumerable<WorkshopRelease> catalog,IEnumerable<InstalledMod> installed,string origin)
        {
            Core.Require(ModWorkshopClient.ServiceUri(origin).GetLeftPart(UriPartial.Authority)==origin,"Use the canonical workshop origin for installation.");
            var available=new Dictionary<string,WorkshopRelease>(StringComparer.Ordinal);
            foreach(var release in catalog) {
                Core.Require(release.Package!=null && release.Id==release.Package.Id && release.Version==release.Package.Version,"Missing validated workshop package metadata.");
                string key=release.Id+"@"+release.Version;
                Core.Require(!available.ContainsKey(key),"Duplicate workshop release identity.");available.Add(key,release);
            }
            WorkshopRelease current;
            Core.Require(selected!=null && available.TryGetValue(selected.Id+"@"+selected.Version,out current),"The selected mod is no longer available.");
            current=available[selected.Id+"@"+selected.Version];
            Core.Require(current.Sha256==selected.Sha256,"The selected workshop package changed. Refresh before installing.");
            var chosen=new Dictionary<string,string>(StringComparer.Ordinal);
            var pending=new HashSet<string>(StringComparer.Ordinal);var visited=new HashSet<string>(StringComparer.Ordinal);
            var ordered=new List<WorkshopRelease>();
            Action<WorkshopRelease> visit=null;
            visit=release=>{
                string version;
                Core.Require(!chosen.TryGetValue(release.Id,out version) || version==release.Version,"Required mods need incompatible versions of "+release.Id+".");
                if(visited.Contains(release.Id))return;
                Core.Require(pending.Add(release.Id),"The required mods contain a dependency cycle.");
                chosen[release.Id]=release.Version;Core.Require(chosen.Count<=32,"This install needs more than 32 packages.");
                foreach(var dependency in release.Package.Dependencies.OrderBy(pair=>pair.Key,StringComparer.Ordinal)) {
                    WorkshopRelease required;
                    Core.Require(available.TryGetValue(dependency.Key+"@"+dependency.Value,out required),"Required release is unavailable: "+dependency.Key+" @ "+dependency.Value+".");
                    visit(required);
                }
                pending.Remove(release.Id);visited.Add(release.Id);ordered.Add(release);
            };
            visit(current);
            var files=new HashSet<string>(StringComparer.Ordinal);long total=0;
            foreach(var release in ordered)foreach(var file in release.Package.Files) {
                Core.Require(files.Add(file.Key),"Required packages conflict on "+file.Key+".");
                total+=file.Value.Size;Core.Require(total<=ModLibrary.MaxExpanded,"Required mods exceed the expanded payload budget.");
            }
            var existing=installed.ToArray();var downloads=new List<WorkshopRelease>();
            foreach(var release in ordered) {
                var matches=existing.Where(mod=>mod.Key==release.Id+"@"+release.Version).ToArray();
                Core.Require(matches.Length<=1,"Duplicate installed mod identity.");
                if(matches.Length==0)downloads.Add(release);
                else Core.Require(matches[0].Sha256==release.Sha256 && matches[0].WorkshopOrigin==origin,
                    "An installed version differs from this workshop: "+release.Id+" @ "+release.Version+". Review it before installing.");
            }
            return new WorkshopInstallPlan(ordered.ToArray(),downloads.ToArray());
        }
    }

    // Shared, bounded catalog/download client. Downloading never activates code.
    public sealed class ModWorkshopClient : IDisposable
    {
        public const string DefaultService = ""; // Fill after the Mac's HTTPS endpoint is verified.
        public const int MaxCatalogBytes = 4 * 1024 * 1024;
        public const long MaxPackageBytes = 256L * 1024 * 1024;
        readonly HttpClient http;readonly string installation;readonly Func<DateTime> clock;
        public ModWorkshopClient() : this(new HttpClientHandler { AllowAutoRedirect=false, UseCookies=false }) { }
        public ModWorkshopClient(string root) : this(new HttpClientHandler { AllowAutoRedirect=false, UseCookies=false },root) { }
        internal ModWorkshopClient(HttpMessageHandler handler,string root=null,Func<DateTime> clock=null) { http=new HttpClient(handler);http.Timeout=TimeSpan.FromSeconds(30);installation=root;this.clock=clock??(()=>DateTime.UtcNow); }
        public static Uri ServiceUri(string value)
        {
            Uri uri;
            Core.Require(Uri.TryCreate(value,UriKind.Absolute,out uri) && uri.UserInfo.Length==0 &&
                uri.AbsolutePath=="/" && uri.Query.Length==0 && uri.Fragment.Length==0 &&
                uri.Scheme=="https" && uri.IsDefaultPort,
                "The workshop needs an HTTPS service address.");
            return uri;
        }
        public static string Service(string root)
        {
            string path=Core.PathIn(root,"Support/Mods/service.json");
            if(!File.Exists(path))return DefaultService;
            Core.Require(new FileInfo(path).Length<=8192,"Workshop service settings are too large.");
            string value=Json.String(Json.Object(File.ReadAllText(path)),"service");
            if(value.Length>0)ServiceUri(value);
            return value;
        }
        static Dictionary<string,object> Object(Dictionary<string,object> source,string key)
        {
            object value;Core.Require(source!=null && source.TryGetValue(key,out value) && value is Dictionary<string,object>,"Invalid workshop catalog object.");
            return (Dictionary<string,object>)source[key];
        }
        static string Text(Dictionary<string,object> source,string key,int max)
        {
            object value;Core.Require(source.TryGetValue(key,out value) && value is string,"Invalid workshop text.");
            string text=(string)value;
            Core.Require(text.Length>0 && text.Length<=max && !text.Any(Char.IsControl),"Invalid workshop text length or characters.");return text;
        }
        public static WorkshopRelease[] Parse(string json,DateTime now)
        {
            Core.Require(Encoding.UTF8.GetByteCount(json)<=MaxCatalogBytes,"Workshop catalog exceeds the download limit.");
            var data=ModJson.Read(json,MaxCatalogBytes);object records=null;
            Core.Require(data!=null && Json.String(data,"schema")=="1" && data.TryGetValue("releases",out records) &&
                (records is object[] || records is System.Collections.ArrayList),"Unsupported workshop catalog.");
            var list=records is object[]?(object[])records:((System.Collections.ArrayList)records).ToArray();
            Core.Require(list.Length<=1000,"Workshop catalog has too many releases.");
            var result=new List<WorkshopRelease>();var seen=new HashSet<string>(StringComparer.Ordinal);
            foreach(var item in list) {
                var entry=item as Dictionary<string,object>;Core.Require(entry!=null,"Invalid workshop release.");
                var manifest=Object(entry,"manifest");var scan=Object(entry,"scan");
                var package=ModLibrary.Manifest(Json.Encode(manifest));
                string sha=Text(entry,"sha256",64),id=Text(manifest,"id",80),version=Text(manifest,"version",32);
                Core.Require(Regex.IsMatch(sha,"\\A[0-9a-f]{64}\\z") && Regex.IsMatch(id,"\\A[a-z][a-z0-9]*(?:-[a-z0-9]+)*\\z") &&
                    Regex.IsMatch(version,"\\A(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\z"),"Invalid workshop identity or digest.");
                Core.Require(Json.String(manifest,"game")=="tsfp-us-45410066" && Json.String(manifest,"api")=="tsfp-mod-v1-draft","Unsupported workshop game or API.");
                Core.Require(Json.String(entry,"download")=="/v1/packages/"+sha && seen.Add(id+"@"+version),"Invalid or duplicate workshop download.");
                long seconds;Core.Require(Int64.TryParse(Json.String(scan,"time"),out seconds) && seconds>=0 && seconds<=253402300799,"Invalid workshop scan date.");
                var release=new WorkshopRelease {Id=id,Version=version,Sha256=sha,Name=Text(manifest,"name",120),Author=Text(manifest,"author",120),
                    Description=Text(manifest,"description",4000),ScanEngine=Text(scan,"engine",120),ScannedUtc=DateTimeOffset.FromUnixTimeSeconds(seconds).UtcDateTime};
                release.Categories=package.Files.Keys.Select(name=>name.Split('/')[0]).Distinct().OrderBy(name=>name,StringComparer.Ordinal).ToArray();
                release.Dependencies=package.Dependencies.OrderBy(pair=>pair.Key,StringComparer.Ordinal).Select(pair=>pair.Key+" @ "+pair.Value).ToArray();
                release.PayloadBytes=package.Files.Values.Sum(file=>file.Size);
                release.Package=package;
                if(release.ScannedUtc<=now && now-release.ScannedUtc<TimeSpan.FromHours(24))result.Add(release);
            }
            return result.ToArray();
        }
        async Task<byte[]> Fetch(Uri origin,string path,int maximum,CancellationToken token)
        {
            using(var response=await http.GetAsync(new Uri(origin,path),HttpCompletionOption.ResponseHeadersRead,token)) {
                Core.Require(response.StatusCode==System.Net.HttpStatusCode.OK,"The workshop is unavailable. Your installed game is still available.");
                Core.Require(!response.Content.Headers.ContentLength.HasValue || response.Content.Headers.ContentLength<=maximum,"Workshop metadata exceeds the download limit.");
                using(var input=await response.Content.ReadAsStreamAsync())using(var output=new MemoryStream()) {
                    byte[] buffer=new byte[16384];int count;
                    while((count=await input.ReadAsync(buffer,0,buffer.Length,token))>0){Core.Require(output.Length+count<=maximum,"Workshop metadata exceeds the download limit.");output.Write(buffer,0,count);}
                    return output.ToArray();
                }
            }
        }
        public async Task<WorkshopRelease[]> Read(string service,CancellationToken token)
        {
            var origin=ServiceUri(service);long now=new DateTimeOffset(clock()).ToUnixTimeSeconds();
            using(var trust=new ModTrustStore(installation,origin.GetLeftPart(UriPartial.Authority),now)) {
                byte[] metadata=await Fetch(origin,"v1/trust/catalog",ModTrust.MaxMetadata,token);
                long target=ModTrust.TargetVersion(metadata);
                ModTrust.Need(target>=trust.Authority.Version && target-trust.Authority.Version<=64-trust.Updates.Count,"root rollback or rotation budget exceeded.");
                while(trust.Authority.Version<target) {
                    byte[] update=await Fetch(origin,"v1/trust/roots/"+(trust.Authority.Version+1).ToString(CultureInfo.InvariantCulture),ModTrust.MaxRoot,token);
                    trust.Updates.Add(update);trust.Authority=ModTrust.Roots(trust.Pinned,trust.Updates,now,false);
                }
                byte[] catalog=await Fetch(origin,"v1/catalog",MaxCatalogBytes,token);
                // Re-read wall time after transfer: metadata can expire while downloading.
                now=new DateTimeOffset(clock()).ToUnixTimeSeconds();
                var accepted=ModTrust.Catalog(metadata,catalog,trust.Authority,now);
                if(trust.Previous!=null)ModTrust.Forward(trust.Previous,trust.Observed,accepted,now);
                var releases=Parse(ModTrust.Utf8.GetString(catalog),clock());token.ThrowIfCancellationRequested();
                trust.Commit(metadata,catalog,accepted,now);return releases;
            }
        }
        public async Task<string> Download(string service,WorkshopRelease selected,string root,CancellationToken token)
        {
            // Recheck the current catalog so old page state cannot bypass a removal.
            var current=await Read(service,token);
            Core.Require(current.Any(r=>r.Sha256==selected.Sha256 && r.Id==selected.Id && r.Version==selected.Version),"This mod is no longer available. Refresh the workshop.");
            Core.Require(Path.GetFullPath(root)==Path.GetFullPath(installation),"The mod download must use its verified installation folder.");
            string folder=Core.PathIn(root,".launcher/mod-downloads");Directory.CreateDirectory(folder);
            string partial=Path.Combine(folder,Guid.NewGuid().ToString("N")+".partial");
            string target=Path.Combine(folder,selected.Sha256+".zip");
            try {
                using(var response=await http.GetAsync(new Uri(ServiceUri(service),"v1/packages/"+selected.Sha256),HttpCompletionOption.ResponseHeadersRead,token)) {
                    Core.Require(response.StatusCode==System.Net.HttpStatusCode.OK,"This mod download is unavailable.");
                    Core.Require(!response.Content.Headers.ContentLength.HasValue || response.Content.Headers.ContentLength<=MaxPackageBytes,"Mod package exceeds the download limit.");
                    using(var input=await response.Content.ReadAsStreamAsync())using(var output=new FileStream(partial,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                        byte[] buffer=new byte[65536];int count;
                        while((count=await input.ReadAsync(buffer,0,buffer.Length,token))>0){Core.Require(output.Length+count<=MaxPackageBytes,"Mod package exceeds the download limit.");await output.WriteAsync(buffer,0,count,token);}
                    }
                }
                string hash;using(var algorithm=SHA256.Create())using(var stream=File.OpenRead(partial))hash=BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
                Core.Require(hash==selected.Sha256,"Mod download failed its integrity check.");token.ThrowIfCancellationRequested();
                // Availability and trust can change during a large transfer.
                var refreshed=await Read(service,token);
                Core.Require(refreshed.Any(r=>r.Sha256==selected.Sha256 && r.Id==selected.Id && r.Version==selected.Version),"This mod became unavailable during download.");
                // A cached package may only be reused if its bytes still match.
                if(File.Exists(target)) {
                    string old;using(var algorithm=SHA256.Create())using(var stream=File.OpenRead(target))old=BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
                    Core.Require(old==hash,"An existing mod download was changed. Move it aside and retry.");
                }else File.Move(partial,target);
                return target;
            } finally {if(File.Exists(partial))File.Delete(partial);}
        }
        public void Dispose(){http.Dispose();}
    }

    public sealed class ModWorkshopPage : IDisposable
    {
        public readonly Grid View=new Grid();
        public readonly TextBox Search=new TextBox {Name="ModSearch",MinWidth=230,Margin=new Thickness(0,0,12,8)};
        public readonly Button Refresh=new Button {Name="RefreshMods",Content="Refresh mods",Margin=new Thickness(0,0,10,8)};
        public readonly Button Cancel=new Button {Name="CancelModTransfer",Content="Cancel",IsEnabled=false,Margin=new Thickness(0,0,10,8)};
        public readonly Button BrowseTab=new Button {Content="Browse",Margin=new Thickness(0,0,10,8)};
        public readonly Button InstalledTab=new Button {Content="Installed",Margin=new Thickness(0,0,10,8)};
        public readonly ComboBox Category=new ComboBox {Name="ModCategory",MinWidth=155,Margin=new Thickness(0,0,12,8)};
        public readonly TextBox Creator=new TextBox {Name="ModCreator",MinWidth=150,Margin=new Thickness(0,0,12,8)};
        public readonly ComboBox Sort=new ComboBox {Name="ModSort",MinWidth=150,Margin=new Thickness(0,0,12,8)};
        public readonly CheckBox OlderVersions=new CheckBox {Name="OlderModVersions",Content="Include older releases",Margin=new Thickness(0,7,12,8)};
        public readonly Button Previous=new Button {Name="PreviousModPage",Content="Previous",Margin=new Thickness(0,0,10,8)};
        public readonly Button Next=new Button {Name="NextModPage",Content="Next",Margin=new Thickness(0,0,10,8)};
        public event Action PlayWithoutModsRequested;
        public event Action<bool> FileBusyChanged;
        readonly StackPanel results=new StackPanel();readonly TextBlock status;
        readonly string root;readonly bool preview;readonly ModWorkshopClient client;
        readonly ModLibrary library;bool installedTab;
        readonly WrapPanel filters=new WrapPanel(),pager=new WrapPanel();
        readonly TextBlock pageLabel=PageTheme.Text("",12,"#9EB4C8");
        readonly ScrollViewer scroll;
        int page;
        public const int PageSize=20;
        public int MatchingCount {get;private set;}
        public int PageNumber {get{return page+1;}}
        public int PageCount {get{return Math.Max(1,(MatchingCount+PageSize-1)/PageSize);}}
        CancellationTokenSource stop;WorkshopRelease[] releases=new WorkshopRelease[0];bool disposed;string service="";
        public string StatusText {get{return status.Text;}}
        public int VisibleCount {get;private set;}
        public ModWorkshopPage(string root,bool preview=false) : this(root,preview,new ModWorkshopClient(root)) { }
        internal ModWorkshopPage(string root,bool preview,ModWorkshopClient client)
        {
            this.root=root;this.preview=preview;this.client=client;library=new ModLibrary(root);
            View.Margin=new Thickness(30,24,30,20);Ui.Theme(View);
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            View.RowDefinitions.Add(new RowDefinition {Height=new GridLength(1,GridUnitType.Star)});
            View.RowDefinitions.Add(new RowDefinition {Height=GridLength.Auto});
            var header=PageTheme.Header("COMMUNITY WORKSHOP","Mods","Browse community creations, check their details and download packages here.");View.Children.Add(header);
            var toolbar=new WrapPanel();header.Children.Add(toolbar);
            toolbar.Children.Add(BrowseTab);toolbar.Children.Add(InstalledTab);
            BrowseTab.Click+=(s,e)=>{installedTab=false;ResetPage();};InstalledTab.Click+=(s,e)=>{installedTab=true;ResetPage();};
            var searchLabel=PageTheme.Text("Search mods",13);searchLabel.Margin=new Thickness(0,9,12,8);toolbar.Children.Add(searchLabel);
            toolbar.Children.Add(Search);toolbar.Children.Add(Refresh);toolbar.Children.Add(Cancel);
            AutomationProperties.SetName(Search,"Search mods by name, creator or description");
            Ui.Tip(Search,"Search by name, creator or description");Ui.TextChanged(Search,ResetPage);
            header.Children.Add(filters);
            filters.Children.Add(FilterLabel("Content"));filters.Children.Add(Category);
            foreach(var item in new[]{new[]{"","All content"},new[]{"textures","Textures"},new[]{"scripts","Gameplay scripts"},new[]{"maps","Maps"},new[]{"audio","Audio"}})
                Category.Items.Add(new ComboBoxItem {Tag=item[0],Content=item[1]});
            Category.SelectedIndex=0;
            filters.Children.Add(FilterLabel("Creator"));filters.Children.Add(Creator);
            filters.Children.Add(FilterLabel("Sort"));filters.Children.Add(Sort);
            foreach(var item in new[]{new[]{"name","Name"},new[]{"creator","Creator"},new[]{"version","Version, newest first"}})
                Sort.Items.Add(new ComboBoxItem {Tag=item[0],Content=item[1]});
            Sort.SelectedIndex=0;filters.Children.Add(OlderVersions);
            AutomationProperties.SetName(Category,"Filter mods by content");AutomationProperties.SetName(Creator,"Filter mods by creator");AutomationProperties.SetName(Sort,"Sort mod releases");
            Category.SelectionChanged+=(s,e)=>ResetPage();Sort.SelectionChanged+=(s,e)=>ResetPage();Ui.TextChanged(Creator,ResetPage);Ui.CheckedChanged(OlderVersions,ResetPage);
            Refresh.Click+=async(s,e)=>await Load();Cancel.Click+=(s,e)=>{if(stop!=null)stop.Cancel();};
            scroll=new ScrollViewer {Content=results,VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled};Grid.SetRow(scroll,1);View.Children.Add(scroll);
            var footer=new StackPanel {Margin=new Thickness(0,12,0,0)};Grid.SetRow(footer,2);View.Children.Add(footer);
            pager.Children.Add(Previous);pager.Children.Add(Next);pager.Children.Add(pageLabel);footer.Children.Add(pager);
            Previous.Click+=(s,e)=>{page--;Render();Ui.ScrollHome(scroll);};Next.Click+=(s,e)=>{page++;Render();Ui.ScrollHome(scroll);};
            footer.Children.Add(PageTheme.Text("Textures and offline gameplay modes require a compatible game build. Map and audio packages can be installed but cannot be activated yet.",12,"#FFDAA6"));
            var recovery=new WrapPanel();footer.Children.Add(recovery);
            var disableAll=new Button {Name="DisableAllMods",Content="Disable all mods",Margin=new Thickness(0,8,10,8)};
            disableAll.Click+=async(s,e)=>await ChangeLibrary(token=>library.DisableAll(token),"All mods disabled. Installed packages were preserved.");recovery.Children.Add(disableAll);
            var withoutMods=new Button {Name="PlayWithoutMods",Content="Play without mods",Margin=new Thickness(0,8,10,8)};
            withoutMods.Click+=(s,e)=>{if(!preview && stop==null && PlayWithoutModsRequested!=null)PlayWithoutModsRequested();};recovery.Children.Add(withoutMods);
            status=PageTheme.Text("The community workshop server has not been connected yet.",12,"#9EB4C8");footer.Children.Add(status);
            if(preview)releases=new[]{new WorkshopRelease {Id="preview-map",Name="Example community map",Version="1.0.0",Author="Example creator",Categories=new[]{"maps"},Description="Interface example for a community map listing. No map or gameplay is included.",ScanEngine="Preview example",ScannedUtc=DateTime.UtcNow},
                new WorkshopRelease {Id="preview-mode",Name="Example game mode",Version="1.0.0",Author="Example creator",Categories=new[]{"scripts"},Description="Interface example for a scripted game mode listing. No script is downloaded or executed.",ScanEngine="Preview example",ScannedUtc=DateTime.UtcNow}};
            Render();
        }
        public void SetReleases(WorkshopRelease[] value){releases=value??new WorkshopRelease[0];ResetPage();}
        static TextBlock FilterLabel(string text){var label=PageTheme.Text(text,13);label.Margin=new Thickness(0,9,8,8);return label;}
        void ResetPage(){page=0;Render();if(scroll!=null)Ui.ScrollHome(scroll);}
        static string Choice(ComboBox box){var item=box.SelectedItem as ComboBoxItem;return item==null?"":(string)item.Tag;}
        void Render()
        {
            results.Children.Clear();VisibleCount=0;string query=(Search.Text??"").Trim();
            Ui.Show(filters,!installedTab);Ui.Show(pager,!installedTab);
            if(installedTab){RenderInstalled(query);return;}
            var selected=WorkshopRelease.Select(releases,query,Creator.Text,Choice(Category),OlderVersions.IsChecked==true,Choice(Sort));
            MatchingCount=selected.Length;page=Math.Max(0,Math.Min(page,PageCount-1));
            Previous.IsEnabled=page>0;Next.IsEnabled=page+1<PageCount;
            pageLabel.Text=MatchingCount+" matching release(s) · Page "+PageNumber+" of "+PageCount;
            foreach(var release in selected.Skip(page*PageSize).Take(PageSize)) {
                VisibleCount++;
                var card=PageTheme.Section(results,release.Name+"  "+release.Version,release.Description);
                card.Children.Add(PageTheme.Text("By "+release.Author,13,"#A7D9FA"));
                string size=release.PayloadBytes<1048576?release.PayloadBytes.ToString(CultureInfo.InvariantCulture)+" bytes":(release.PayloadBytes/1048576.0).ToString("0.##",CultureInfo.InvariantCulture)+" MiB";
                string content=String.Join(" · ",release.Categories.Select(WorkshopRelease.ContentName));
                card.Children.Add(PageTheme.Text(content+(release.PayloadBytes>0?" · Payload "+size:""),12,"#B4C7D8"));
                if(release.Dependencies.Length>0)card.Children.Add(PageTheme.Text("Requires: "+String.Join(", ",release.Dependencies),12,"#B4C7D8"));
                card.Children.Add(PageTheme.Text("Reviewed by the repository operator · scanned with "+release.ScanEngine+" · "+release.ScannedUtc.ToString("u",CultureInfo.InvariantCulture),11,"#9EB4C8"));
                var button=new Button {Name="DownloadMod",Content="Download package",IsEnabled=stop==null,Margin=new Thickness(0,12,0,0)};
                AutomationProperties.SetName(button,"Download "+release.Name+" "+release.Version);
                button.Click+=async(s,e)=>await Download(release);card.Children.Add(button);
                var install=new Button {Name="InstallMod",Content="Install package",IsEnabled=stop==null,Margin=new Thickness(0,8,0,0)};
                AutomationProperties.SetName(install,"Install "+release.Name+" "+release.Version);install.Click+=async(s,e)=>await Download(release,true);card.Children.Add(install);
            }
            if(VisibleCount==0)results.Children.Add(PageTheme.Text(releases.Length==0?"Approved mods will appear here when the workshop is available.":"No mods match your search.",14,"#B4C7D8"));
        }
        void RenderInstalled(string query)
        {
            if(preview){results.Children.Add(PageTheme.Text("Preview only. No installed packages are read or changed.",14));return;}
            var storage=PageTheme.Section(results,"Gameplay mod storage","Optional: choose an existing private folder if your default cache cannot run gameplay mods. Leave blank for the default. This keeps helper files here; game progress stays in separate mod saves. Use a local drive with write access restricted to your account. Shared folders and exFAT are unsupported.");
            var location=new TextBox {Name="ModCacheLocation",Margin=new Thickness(0,8,0,8),IsEnabled=stop==null};
            try{location.Text=ModCacheLocation.Load(root);}catch(Exception error){status.Text=error.Message;}
            storage.Children.Add(location);
            var saveLocation=new Button {Name="SaveModCacheLocation",Content="Save storage folder",IsEnabled=stop==null,Margin=new Thickness(0,0,0,8)};
            saveLocation.Click+=(s,e)=>{try{ModCacheLocation.Save(root,location.Text);status.Text="Gameplay mod storage preference saved. Folder permissions are checked when the mode starts.";}catch(Exception error){status.Text=error.Message;}};storage.Children.Add(saveLocation);
            try {
                string[] enabled=library.Enabled();
                foreach(var mod in library.List().Where(m=>(m.Id+" "+m.Name+" "+m.Author+" "+m.Description).IndexOf(query,StringComparison.OrdinalIgnoreCase)>=0)) {
                    VisibleCount++;var card=PageTheme.Section(results,mod.Name+"  "+mod.Version,mod.Description);bool active=enabled.Contains(mod.Key);
                    card.Children.Add(PageTheme.Text("By "+mod.Author+" · "+(active?(mod.ScriptMode?"Enabled offline gameplay mode · separate saves":"Enabled for Enhanced textures"):"Installed, disabled"),13,"#A7D9FA"));
                    var actions=new WrapPanel();card.Children.Add(actions);
                    var toggle=new Button {Name="ToggleMod",Content=active?"Disable":mod.TextureOnly?"Enable textures":library.CanEnable(mod)?"Enable gameplay mode":"Runtime unavailable",IsEnabled=stop==null && (active || library.CanEnable(mod)),Margin=new Thickness(0,12,10,0)};
                    toggle.Click+=async(s,e)=>await ChangeLibrary(token=>{if(active)library.Disable(mod.Key,token);else library.Enable(mod.Key,token);},active?"Mod disabled. Other files were preserved.":mod.ScriptMode?"Gameplay mode enabled for offline Arcade Deathmatch. Progress uses separate saves. Start a new match to play.":"Texture mod enabled. Use Enhanced textures in a compatible game build.");actions.Children.Add(toggle);
                    var remove=new Button {Name="UninstallMod",Content="Uninstall",IsEnabled=stop==null && !active,Margin=new Thickness(0,12,10,0)};
                    remove.Click+=async(s,e)=>await ChangeLibrary(token=>library.Uninstall(mod.Key),"Mod version uninstalled. Game files, settings and saves were preserved.");actions.Children.Add(remove);
                }
                if(VisibleCount==0)results.Children.Add(PageTheme.Text("No installed mods match this view. Browse the workshop to install a package.",14,"#B4C7D8"));
            }catch(Exception){status.Text="The installed mod library needs attention. Disable all mods or play without mods to recover.";}
        }
        async Task ChangeLibrary(Action<CancellationToken> change,string success)
        {
            if(disposed || stop!=null)return;if(preview){status.Text="Preview only. No installed files were changed.";return;}
            using(var operation=new CancellationTokenSource(TimeSpan.FromMinutes(5))) {
                stop=operation;Busy(true);if(FileBusyChanged!=null)FileBusyChanged(true);
                try {await Task.Run(()=>change(operation.Token));if(!disposed)status.Text=success;}
                catch(OperationCanceledException){if(!disposed)status.Text="Mod change cancelled. The previous active profile is preserved.";}
                catch(Exception error){if(!disposed)status.Text=error.Message;}
                finally{stop=null;if(FileBusyChanged!=null)FileBusyChanged(false);if(!disposed)Busy(false);}
            }
        }
        void Busy(bool value){Refresh.IsEnabled=!value;Cancel.IsEnabled=value;Render();}
        public async Task Load()
        {
            if(disposed||stop!=null)return;
            if(preview){status.Text="Preview only. No connection or download was made.";return;}
            using(var operation=new CancellationTokenSource(TimeSpan.FromSeconds(35))) {
                stop=operation;Busy(true);status.Text="Connecting to the community workshop…";
                try {
                    // Remove old cards before any failed refresh or service change.
                    SetReleases(new WorkshopRelease[0]);service=ModWorkshopClient.Service(root);
                    if(service.Length==0){status.Text="The community workshop server has not been connected yet.";return;}
                    SetReleases(await client.Read(service,operation.Token));status.Text=releases.Length+" reviewed release(s). Scanning cannot guarantee safety.";
                }catch(OperationCanceledException){if(!disposed)status.Text="Workshop request cancelled. Your game is still available.";}
                catch(Exception){if(!disposed)status.Text="Could not reach the workshop. Try Refresh mods later. Your game is still available.";}
                finally{stop=null;if(!disposed)Busy(false);}
            }
        }
        async Task Download(WorkshopRelease release,bool install=false)
        {
            if(disposed||stop!=null)return;
            if(preview){status.Text="Preview only. No package was downloaded.";return;}
            using(var operation=new CancellationTokenSource(TimeSpan.FromMinutes(10))) {
                stop=operation;Busy(true);status.Text="Downloading "+release.Name+"…";if(install && FileBusyChanged!=null)FileBusyChanged(true);
                try {
                    string path=await client.Download(service,release,root,operation.Token);
                    if(install){await Task.Run(()=>library.Install(path,release.Sha256,operation.Token,release.Id,release.Version,ServiceUriOrigin(service)));installedTab=true;}
                    if(!disposed)status.Text=install?"Installed "+release.Name+". Enable supported content from Installed when ready.":"Downloaded and verified "+release.Name+". The package remains inactive.";
                }
                catch(OperationCanceledException){if(!disposed)status.Text="Download cancelled. No mod was installed.";}
                catch(Exception error){if(!disposed)status.Text=install?error.Message:"Could not verify this mod download. Refresh mods and try again.";}
                finally{stop=null;if(install && FileBusyChanged!=null)FileBusyChanged(false);if(!disposed)Busy(false);}
            }
        }
        public void Dispose(){disposed=true;if(stop!=null)stop.Cancel();client.Dispose();}
        static string ServiceUriOrigin(string value){return ModWorkshopClient.ServiceUri(value).GetLeftPart(UriPartial.Authority);}
    }
}
