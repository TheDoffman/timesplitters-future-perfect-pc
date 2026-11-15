using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
#if MACOS || LINUX
using Avalonia;
using Avalonia.Controls;
using Avalonia.Automation;
using Element= Avalonia.Controls.Control;
#else
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using Element= System.Windows.FrameworkElement;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class CreatorPackagePlan
    {
        internal readonly string Path;
        public readonly string Id,Version,Name,Author,Sha256;
        public readonly long Size;
        CreatorPackagePlan(string path,InstalledMod mod,long size)
        {Path=path;Id=mod.Id;Version=mod.Version;Name=mod.Name;Author=mod.Author;Sha256=mod.Sha256;Size=size;}
        public static CreatorPackagePlan Review(string path,CancellationToken token)
        {
            path=System.IO.Path.GetFullPath(path);Core.RequireUnlinked(path,"Linked upload package refused.");
            var info=new FileInfo(path);Core.Require(info.Length>0 && info.Length<=ModWorkshopClient.MaxPackageBytes,"Upload package exceeds the archive limit.");
            var mod=ModLibrary.InspectPackage(path,token);token.ThrowIfCancellationRequested();
            return new CreatorPackagePlan(path,mod,info.Length);
        }
    }
    public sealed class CreatorUpload
    {
        public readonly string Id,ModId,Version,Sha256,State,Error;
        public readonly long Size,Created,Expires,RetryAt,Cursor;
        internal CreatorUpload(Dictionary<string,object> data,bool cursor=false)
        {
            ModTrust.Fields(data,"id mod_id version sha256 size created expires state retry_at error"+(cursor?" cursor":""));
            Id=Text(data,"id",32);ModTrust.Need(Regex.IsMatch(Id,@"\A[0-9a-f]{32}\z"),"invalid upload identity.");
            ModId=Text(data,"mod_id",80);Version=Text(data,"version",32);Sha256=Text(data,"sha256",64);
            ModTrust.Need(Regex.IsMatch(ModId,@"\A[a-z][a-z0-9]*(?:-[a-z0-9]+)*\z") && Regex.IsMatch(Version,@"\A(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\z") && Regex.IsMatch(Sha256,@"\A[0-9a-f]{64}\z"),"invalid upload package identity.");
            Size=ModTrust.Number(data,"size",1,ModWorkshopClient.MaxPackageBytes);Created=ModTrust.Number(data,"created",0);Expires=ModTrust.Number(data,"expires",Created);RetryAt=ModTrust.Number(data,"retry_at",0);
            State=Text(data,"state",32);Error=Text(data,"error",2000);
            ModTrust.Need(new[]{"open","receiving","queued","processing","retry","complete","pending-review","approved","rejected","scan-error","held","revoked","expired","failed"}.Contains(State),"unknown upload state.");
            Cursor=cursor?ModTrust.Number(data,"cursor",1,999999999):0;
        }
        internal static string Text(Dictionary<string,object> data,string key,int max)
        {string value=ModTrust.Text(data,key);ModTrust.Need(value.Length<=max && !value.Any(Char.IsControl),"invalid creator response text.");return value;}
        internal void Bind(CreatorPackagePlan plan)
        {Core.Require(ModId==plan.Id && Version==plan.Version && Sha256==plan.Sha256 && Size==plan.Size,"The workshop upload reservation differs from the reviewed package.");}
    }
    public sealed class ModeratorUpload
    {
        public readonly string Id,ModId,Version,Sha256,State;
        public readonly long Cursor;
        public readonly InstalledMod Package;
        internal ModeratorUpload(Dictionary<string,object> data)
        {
            ModTrust.Fields(data,"cursor id mod_id version sha256 state manifest");
            Id=CreatorUpload.Text(data,"id",32);Sha256=CreatorUpload.Text(data,"sha256",64);
            ModTrust.Need(Regex.IsMatch(Id,@"\A[0-9a-f]{32}\z") && Regex.IsMatch(Sha256,@"\A[0-9a-f]{64}\z"),"invalid review identity.");
            ModId=CreatorUpload.Text(data,"mod_id",80);Version=CreatorUpload.Text(data,"version",32);
            State=CreatorUpload.Text(data,"state",32);Cursor=ModTrust.Number(data,"cursor",1,999999999);
            ModTrust.Need(new[]{"pending-review","approved","rejected","scan-error","revoked"}.Contains(State),"unknown review state.");
            Package=ModLibrary.Manifest(ModTrust.Text(data,"manifest"));
            ModTrust.Need(Package.Id==ModId && Package.Version==Version,"review manifest identity mismatch.");
        }
    }
    public sealed class ModeratorEvidence
    {
        public readonly string Engine,Version,Status,Isolation;
        public readonly long Time,DatabaseMaxAgeDays;
        public readonly bool Fresh;
        internal ModeratorEvidence(Dictionary<string,object> data,ModeratorUpload review)
        {
            ModTrust.Fields(data,"id sha256 state scan");
            ModTrust.Need(CreatorUpload.Text(data,"id",32)==review.Id && CreatorUpload.Text(data,"sha256",64)==review.Sha256 && CreatorUpload.Text(data,"state",32)==review.State,"scan evidence changed. Refresh the moderation queue.");
            var scan=ModTrust.Object(data,"scan");ModTrust.Fields(scan,"engine version status time isolation database_max_age_days fresh");
            Engine=CreatorUpload.Text(scan,"engine",128);Version=CreatorUpload.Text(scan,"version",512);Status=CreatorUpload.Text(scan,"status",16);Isolation=CreatorUpload.Text(scan,"isolation",64);
            Time=ModTrust.Number(scan,"time",0,253402300799);DatabaseMaxAgeDays=ModTrust.Number(scan,"database_max_age_days",0,36500);
            ModTrust.Need(new[]{"clean","blocked","error"}.Contains(Status) && scan["fresh"] is bool,"invalid scan evidence.");Fresh=(bool)scan["fresh"];
            ModTrust.Need(!Fresh || Status=="clean","inconsistent scan freshness.");
        }
    }
    public sealed class ModeratorSourcePage
    {
        public readonly string Path,Text,PayloadSha256;
        public readonly long Offset,NextOffset,Size;
        internal ModeratorSourcePage(Dictionary<string,object> data,ModeratorUpload review,string path,long offset)
        {
            ModTrust.Fields(data,"id sha256 path payload_sha256 size offset next_offset text");
            ModTrust.Need(CreatorUpload.Text(data,"id",32)==review.Id && CreatorUpload.Text(data,"sha256",64)==review.Sha256 && CreatorUpload.Text(data,"path",180)==path,"source preview identity mismatch.");
            ModPayload entry;ModTrust.Need(review.Package.Files.TryGetValue(path,out entry),"undeclared source preview.");Path=path;
            PayloadSha256=CreatorUpload.Text(data,"payload_sha256",64);Size=ModTrust.Number(data,"size",1,1024*1024);Offset=ModTrust.Number(data,"offset",offset,offset);NextOffset=ModTrust.Number(data,"next_offset",offset+1,Size);
            ModTrust.Need(Size==entry.Size && PayloadSha256==entry.Sha256,"source preview differs from the manifest.");
            Text=ModTrust.Text(data,"text");byte[] bytes=new UTF8Encoding(false,true).GetBytes(Text);
            ModTrust.Need(bytes.Length<=65536 && bytes.LongLength==NextOffset-Offset,"invalid source preview length.");
            if(Offset==0 && NextOffset==Size)using(var hash=System.Security.Cryptography.SHA256.Create())ModTrust.Need(BitConverter.ToString(hash.ComputeHash(bytes)).Replace("-","").ToLowerInvariant()==PayloadSha256,"source preview digest mismatch.");
        }
        public string DisplayText()
        {
            var output=new StringBuilder();foreach(char c in Text) {
                bool hidden=(Char.IsControl(c) && c!='\n' && c!='\t') || c=='\u061c' || c=='\u200e' || c=='\u200f' || (c>='\u202a' && c<='\u202e') || (c>='\u2066' && c<='\u2069');
                if(hidden)output.Append("\\u").Append(((int)c).ToString("x4"));else output.Append(c);
            }return output.ToString();
        }
    }
    // Passwords and bearer sessions stay in memory. No cookies, redirect,
    // credential files, diagnostic payloads or automatic upload retries.
    public sealed class ModCreatorClient : IDisposable
    {
        readonly string root,origin;readonly HttpClient http;readonly Func<DateTime> clock;
        readonly SemaphoreSlim gate=new SemaphoreSlim(1,1);string token;long expires;
        public string Username {get;private set;}
        public string Role {get;private set;}
        public string LastTicket {get;private set;}
        public bool SignedIn {get{return token!=null && Now<expires;}}
        public bool CanModerate {get{return SignedIn && (Role=="moderator" || Role=="admin");}}
        long Now {get{return new DateTimeOffset(clock()).ToUnixTimeSeconds();}}
        public ModCreatorClient(string root):this(root,ModWorkshopClient.Service(root),new HttpClientHandler {AllowAutoRedirect=false,UseCookies=false,UseDefaultCredentials=false},()=>DateTime.UtcNow){}
        internal ModCreatorClient(string root,string service,HttpMessageHandler handler,Func<DateTime> clock)
        {this.root=root;origin=ModWorkshopClient.ServiceUri(service).GetLeftPart(UriPartial.Authority);this.clock=clock;http=new HttpClient(handler,true);http.Timeout=TimeSpan.FromMinutes(15);}
        void Pin()
        {
            try {
                Core.Require(ModWorkshopClient.ServiceUri(ModWorkshopClient.Service(root)).GetLeftPart(UriPartial.Authority)==origin,"The workshop address changed. Restart the launcher before signing in again.");
                using(var store=new ModTrustStore(root,origin,Now))ModTrust.Roots(store.Pinned,store.Updates,Now);
            }catch{Forget();throw;}
        }
        void Forget(){token=null;expires=0;Username=null;Role=null;}
        async Task<Dictionary<string,object>> Request(HttpMethod method,string path,HttpContent content,int expected,bool authenticated,CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();Pin();
            if(authenticated && !SignedIn){Forget();throw new InvalidOperationException("Your workshop session expired. Sign in again.");}
            using(var request=new HttpRequestMessage(method,new Uri(new Uri(origin+"/"),path))) {
                request.Content=content;request.Headers.TryAddWithoutValidation("Origin",origin);
                if(authenticated)request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",token);
                using(var response=await http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead,cancel).ConfigureAwait(false)) {
                    int code=(int)response.StatusCode;
                    if(code==401)Forget();
                    Core.Require(code==expected,code==401?"Sign in again to use the creator workshop.":code==403?"This workshop action is not allowed for this account.":code==409?"The package identity, version or ownership conflicts with an existing upload.":code==429?"The workshop request limit was reached. Wait before trying again.":"The workshop could not complete this request (HTTP "+code+"). Refresh upload status before retrying.");
                    if(code==204)return null;
                    Core.Require(response.Content!=null && response.Content.Headers.ContentType!=null && response.Content.Headers.ContentType.MediaType=="application/json","Invalid creator response format.");
                    const int maximum=512*1024;
                    Core.Require(!response.Content.Headers.ContentLength.HasValue || response.Content.Headers.ContentLength.Value<=maximum,"Creator response exceeds its limit.");
                    using(var input=await response.Content.ReadAsStreamAsync().ConfigureAwait(false))using(var output=new MemoryStream()) {
                        byte[] buffer=new byte[16384];int count;while((count=await input.ReadAsync(buffer,0,buffer.Length,cancel).ConfigureAwait(false))>0){Core.Require(output.Length+count<=maximum,"Creator response exceeds its limit.");output.Write(buffer,0,count);}
                        return ModJson.Read(new UTF8Encoding(false,true).GetString(output.ToArray()),maximum);
                    }
                }
            }
        }
        static HttpContent JsonBody(object value)
        {string text=Json.Encode(value);Core.Require(Encoding.UTF8.GetByteCount(text)<=16384,"Creator request exceeds its limit.");return new StringContent(text,Encoding.UTF8,"application/json");}
        public async Task SignIn(string username,string password,CancellationToken cancel)
        {await Login("v1/sessions",new {username=username,password=password},200,cancel).ConfigureAwait(false);}
        public async Task Redeem(string invitation,string password,CancellationToken cancel)
        {await Login("v1/accounts/redeem",new {invitation=invitation,password=password},201,cancel).ConfigureAwait(false);}
        async Task Login(string path,object credentials,int expected,CancellationToken cancel)
        {
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Forget();var response=await Request(HttpMethod.Post,path,JsonBody(credentials),expected,false,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"token expires account");string secret=CreatorUpload.Text(response,"token",43);long expiry=ModTrust.Number(response,"expires",Now+1,Now+43200);
                ModTrust.Need(Regex.IsMatch(secret,@"\A[A-Za-z0-9_-]{43}\z"),"invalid creator session.");
                var account=ModTrust.Object(response,"account");ModTrust.Fields(account,"username role");string name=CreatorUpload.Text(account,"username",32),role=CreatorUpload.Text(account,"role",16);
                ModTrust.Need(Regex.IsMatch(name,@"\A[a-z][a-z0-9-]{2,31}\z") && new[]{"creator","moderator","admin"}.Contains(role),"invalid creator account.");
                token=secret;expires=expiry;Username=name;Role=role;LastTicket=null;
            }finally{gate.Release();}
        }
        public async Task SignOut(CancellationToken cancel)
        {
            bool locked=false;
            try {await gate.WaitAsync(cancel).ConfigureAwait(false);locked=true;if(SignedIn)await Request(HttpMethod.Delete,"v1/sessions/current",null,204,true,cancel).ConfigureAwait(false);}
            finally{Forget();if(locked)gate.Release();}
        }
        public async Task<CreatorUpload[]> History(long after,CancellationToken cancel)
        {
            Core.Require(after>=0 && after<=999999999,"Invalid creator history cursor.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                var response=await Request(HttpMethod.Get,"v1/uploads?after="+after.ToString(System.Globalization.CultureInfo.InvariantCulture),null,200,true,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"uploads");var records=ModTrust.Array(response,"uploads",100);var result=new List<CreatorUpload>();var ids=new HashSet<string>();long previous=after;
                foreach(var value in records){var data=value as Dictionary<string,object>;ModTrust.Need(data!=null,"invalid upload history.");var row=new CreatorUpload(data,true);ModTrust.Need(row.Cursor>previous && ids.Add(row.Id),"unordered or duplicate upload history.");previous=row.Cursor;result.Add(row);}return result.ToArray();
            }finally{gate.Release();}
        }
        public async Task<CreatorUpload> Upload(CreatorPackagePlan plan,CancellationToken cancel)
        {
            Core.Require(plan!=null,"Review a package before uploading.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(SignedIn && plan.Author==Username,"The package author must match your signed-in creator account.");
                Core.RequireUnlinked(plan.Path,"Linked upload package refused.");
                using(var input=new FileStream(plan.Path,FileMode.Open,FileAccess.Read,FileShare.Read)) {
                    Core.Require(input.Length==plan.Size,"The reviewed package changed. Review it again.");
                    var current=await Task.Run(()=>CreatorPackagePlan.Review(plan.Path,cancel),cancel).ConfigureAwait(false);
                    Core.Require(current.Sha256==plan.Sha256 && current.Size==plan.Size,"The reviewed package changed. Review it again.");
                    var ticket=new CreatorUpload(await Request(HttpMethod.Post,"v1/uploads",JsonBody(new {id=plan.Id,version=plan.Version,sha256=plan.Sha256,size=plan.Size}),201,true,cancel).ConfigureAwait(false));
                    ticket.Bind(plan);Core.Require(ticket.State=="open" && ticket.Expires>Now,"The upload reservation is unavailable.");LastTicket=ticket.Id;
                    var body=new StreamContent(input);body.Headers.ContentType=new MediaTypeHeaderValue("application/zip");body.Headers.ContentLength=plan.Size;
                    var result=new CreatorUpload(await Request(HttpMethod.Put,"v1/uploads/"+ticket.Id,body,202,true,cancel).ConfigureAwait(false));
                    Core.Require(result.Id==ticket.Id,"The workshop returned a different upload identity.");result.Bind(plan);return result;
                }
            }finally{gate.Release();}
        }
        public async Task<ModeratorUpload[]> Reviews(long after,CancellationToken cancel)
        {
            Core.Require(after>=0 && after<=999999999,"Invalid moderation cursor.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to review packages.");
                var response=await Request(HttpMethod.Get,"v1/moderation?after="+after.ToString(System.Globalization.CultureInfo.InvariantCulture),null,200,true,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"uploads");var result=new List<ModeratorUpload>();var ids=new HashSet<string>();var hashes=new HashSet<string>();long previous=after;
                foreach(var value in ModTrust.Array(response,"uploads",100)) {
                    var data=value as Dictionary<string,object>;ModTrust.Need(data!=null,"invalid moderation record.");var row=new ModeratorUpload(data);
                    ModTrust.Need(row.Cursor>previous && ids.Add(row.Id) && hashes.Add(row.Sha256),"unordered or duplicate moderation history.");previous=row.Cursor;result.Add(row);
                }return result.ToArray();
            }finally{gate.Release();}
        }
        public async Task<ModeratorEvidence> Evidence(ModeratorUpload review,CancellationToken cancel)
        {
            Core.Require(review!=null,"Select a moderation record.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to inspect scans.");
                return new ModeratorEvidence(await Request(HttpMethod.Get,"v1/moderation/"+review.Id+"/evidence",null,200,true,cancel).ConfigureAwait(false),review);
            }finally{gate.Release();}
        }
        public async Task<ModeratorSourcePage> Source(ModeratorUpload review,string path,long offset,CancellationToken cancel)
        {
            ModPayload entry;Core.Require(review!=null && path!=null && path.StartsWith("scripts/",StringComparison.Ordinal) && path.EndsWith(".lua",StringComparison.OrdinalIgnoreCase) && review.Package.Files.TryGetValue(path,out entry) && entry.Size<=1024*1024 && offset>=0 && offset<entry.Size,"Select a declared Lua source file and valid offset.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to inspect source.");
                return new ModeratorSourcePage(await Request(HttpMethod.Post,"v1/moderation/"+review.Id+"/source",JsonBody(new {sha256=review.Sha256,path=path,offset=offset}),200,true,cancel).ConfigureAwait(false),review,path,offset);
            }finally{gate.Release();}
        }
        public async Task Moderate(ModeratorUpload review,string action,string note,CancellationToken cancel)
        {
            Core.Require(review!=null && (action=="approve" || action=="revoke"),"Invalid moderation decision.");
            note=(note??"").Trim();Core.Require(note.Length>0 && note.Length<=1800 && !note.Any(Char.IsControl),"Enter a review note of 1 to 1800 characters without control characters.");
            Core.Require(action!="approve" || review.State=="pending-review","Only a package awaiting review can be approved.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to review packages.");
                var response=await Request(HttpMethod.Post,"v1/moderation/"+review.Id,JsonBody(new {action=action,note=note}),200,true,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"id sha256 state");
                ModTrust.Need(CreatorUpload.Text(response,"id",32)==review.Id && CreatorUpload.Text(response,"sha256",64)==review.Sha256 && CreatorUpload.Text(response,"state",32)==(action=="approve"?"approved":"revoked"),"moderation result differs from the reviewed decision. Refresh the review queue before retrying.");
            }finally{gate.Release();}
        }
        public void Dispose(){Forget();http.Dispose();}
    }

    public sealed class ModCreatorPage : IDisposable
    {
        public readonly StackPanel View=new StackPanel();
        readonly TextBox username=new TextBox {Name="CreatorUsername",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly TextBox package=new TextBox {Name="CreatorPackagePath",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly TextBox reviewNote=new TextBox {Name="ModeratorNote",MaxLength=1800,MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly ComboBox sourceFiles=new ComboBox {Name="ModeratorSourceFile",MinWidth=260,Margin=new Thickness(0,6,0,8)};
#if MACOS || LINUX
        readonly TextBox password=new TextBox {Name="CreatorPassword",PasswordChar='\u25cf',Margin=new Thickness(0,6,0,8)};
        readonly TextBox invitation=new TextBox {Name="CreatorInvitation",PasswordChar='\u25cf',Margin=new Thickness(0,6,0,8)};
        string Password {get{return password.Text??"";}}
        string Invitation {get{return invitation.Text??"";}}
#else
        static PasswordBox SecretBox(string name)
        {return new PasswordBox {Name=name,Margin=new Thickness(0,6,0,8),MinHeight=36,FontSize=14,Padding=new Thickness(10,8,10,8),Background=Ui.Brush("#0B1826"),Foreground=Ui.Brush("#E4EDF5"),BorderBrush=Ui.Brush("#3D5971"),BorderThickness=new Thickness(1),VerticalContentAlignment=VerticalAlignment.Center};}
        readonly PasswordBox password=SecretBox("CreatorPassword");
        readonly PasswordBox invitation=SecretBox("CreatorInvitation");
        string Password {get{return password.Password;}}
        string Invitation {get{return invitation.Password;}}
#endif
        readonly TextBlock status=PageTheme.Text("Sign in to submit your own packaged additions for scanning and review.",13,"#B4C7D8");
        readonly string root;readonly bool preview;ModCreatorClient client;CancellationTokenSource stop;bool disposed;
        CreatorPackagePlan reviewed;CreatorUpload[] uploads=new CreatorUpload[0];long cursor;
        ModeratorUpload[] reviews=new ModeratorUpload[0];ModeratorUpload selectedReview;string decision,decisionNote;
        ModeratorEvidence scanEvidence;
        ModeratorSourcePage sourcePage;
        public string StatusText {get{return status.Text;}}
        public ModCreatorPage(string root,bool preview=false):this(root,preview,null){}
        internal ModCreatorPage(string root,bool preview,ModCreatorClient client){this.root=root;this.preview=preview;this.client=client;AutomationProperties.SetName(username,"Workshop username");AutomationProperties.SetName(password,"Workshop password");AutomationProperties.SetName(invitation,"Workshop invitation");AutomationProperties.SetName(package,"Mod ZIP file path");status.Margin=new Thickness(0,0,0,12);Render();}
        ModCreatorClient Client {get{if(client==null)client=new ModCreatorClient(root);return client;}}
        bool SignedIn {get{return client!=null && client.SignedIn;}}
        public void ClearCredentials()
        {
#if MACOS || LINUX
            password.Text="";
            invitation.Text="";
#else
            password.Clear();
            invitation.Clear();
#endif
        }
        static void Detach(Element control){var parent=control.Parent as Panel;if(parent!=null)parent.Children.Remove(control);}
        Button Button(StackPanel panel,string name,string title,Action action,bool enabled=true)
        {var button=new Button {Name=name,Content=title,IsEnabled=stop==null && enabled,Margin=new Thickness(0,8,10,0)};button.Click+=(s,e)=>action();panel.Children.Add(button);return button;}
        void Render()
        {
            Detach(username);Detach(password);Detach(invitation);Detach(package);Detach(reviewNote);Detach(sourceFiles);View.Children.Clear();bool authenticated=SignedIn;if(!authenticated)ClearReviews();
            View.Children.Add(status);var cancel=Button(View,"CancelCreatorOperation","Cancel request",()=>{if(stop!=null)stop.Cancel();});cancel.IsEnabled=stop!=null;Ui.Show(cancel,stop!=null);
            var account=PageTheme.Section(View,"Creator account",authenticated?"Signed in as "+client.Username+" ("+client.Role+")":"Use your workshop account or redeem an operator invitation.");
            if(!authenticated) {
                account.Children.Add(PageTheme.Text("Username",13));account.Children.Add(username);
                account.Children.Add(PageTheme.Text("Password (15 or more characters for a new account)",13));account.Children.Add(password);
                Button(account,"CreatorSignIn","Sign in",async()=>await Authenticate(false));
                account.Children.Add(PageTheme.Text("Invitation (for a new account or account recovery)",13));account.Children.Add(invitation);
                Button(account,"CreatorRedeem","Redeem invitation",async()=>await Authenticate(true));
                account.Children.Add(PageTheme.Text("Sign-in details are kept for this launcher session only.",12,"#9EB4C8"));
            }else Button(account,"CreatorSignOut","Sign out",async()=>{reviewed=null;uploads=new CreatorUpload[0];ClearReviews();await Run(token=>Client.SignOut(token),"Signed out. Local game files were preserved.",35);});
            username.IsEnabled=password.IsEnabled=invitation.IsEnabled=stop==null;
            var submission=PageTheme.Section(View,"Submit a package","Build a ZIP with the mod SDK, enter its full file path, then review it before upload. Its author must match your creator username.");
            submission.Children.Add(package);package.IsEnabled=stop==null;
            Button(submission,"ReviewCreatorPackage","Review ZIP",async()=>await Run(async token=>{string path=package.Text;var plan=await Task.Run(()=>CreatorPackagePlan.Review(path,token),token);Core.Require(plan.Author==Client.Username,"The package author must match your signed-in creator account.");reviewed=plan;},"Review the package details before uploading.",120),authenticated);
            if(reviewed!=null && authenticated) {
                submission.Children.Add(PageTheme.Text(reviewed.Name+" @ "+reviewed.Version+" · By "+reviewed.Author+" · "+reviewed.Size+" bytes",14,"#A7D9FA"));
                submission.Children.Add(PageTheme.Text("Package ID: "+reviewed.Id+" · SHA-256: "+reviewed.Sha256,12,"#9EB4C8"));
                Button(submission,"ConfirmCreatorUpload","Upload for scanning and review",async()=>{var plan=reviewed;await Run(async token=>{var result=await Client.Upload(plan,token);reviewed=null;uploads=new[]{result};cursor=0;},"Package received privately. A clean scan and moderator approval are required before publication.",900);});
            }
            submission.Children.Add(PageTheme.Text("Uploading does not publish or enable the package. Cancelled transfers may retain a reservation until it expires; check upload status before retrying.",12,"#FFDAA6"));
            var history=PageTheme.Section(View,"Your uploads","Scan and review status comes from the workshop. Approval does not guarantee safety.");
            Button(history,"RefreshCreatorUploads","Refresh uploads",async()=>{cursor=0;await History();},authenticated);
            foreach(var upload in uploads)history.Children.Add(PageTheme.Text(upload.ModId+" @ "+upload.Version+" · "+upload.State+(upload.Error.Length>0?" · "+upload.Error:""),13,"#B4C7D8"));
            if(uploads.Length==100)Button(history,"NextCreatorUploads","Next uploads",async()=>{cursor=uploads.Last().Cursor;await History();},authenticated);
            if(client!=null && client.CanModerate)RenderModeration();
        }
        void ClearReviews(){reviews=new ModeratorUpload[0];selectedReview=null;scanEvidence=null;sourcePage=null;decision=null;decisionNote=null;reviewNote.Text="";sourceFiles.ItemsSource=null;}
        async Task Reviews(long after)
        {ClearReviews();await Run(async token=>{reviews=await Client.Reviews(after,token);},"Moderation queue refreshed.",35);}
        void RenderModeration()
        {
            var panel=PageTheme.Section(View,"Moderation","Review the creator's source and package contents before deciding. This view shows the submitted manifest; it does not run or install quarantined content.");
            Button(panel,"RefreshModeratorUploads","Refresh moderation",async()=>await Reviews(0));
            foreach(var item in reviews) {
                var row=item;
                Button(panel,"ModeratorUpload"+row.Id,row.Package.Name+" @ "+row.Version+" · "+row.State,async()=>{selectedReview=row;scanEvidence=null;sourcePage=null;sourceFiles.ItemsSource=null;decision=null;decisionNote=null;reviewNote.Text="";await Run(async token=>{scanEvidence=await Client.Evidence(row,token);},"Scan evidence refreshed.",35);});
            }
            if(reviews.Length==100)Button(panel,"NextModeratorUploads","Next reviews",async()=>await Reviews(reviews.Last().Cursor));
            if(selectedReview==null)return;
            var review=selectedReview;
            panel.Children.Add(PageTheme.Text(review.ModId+" @ "+review.Version+" · By "+review.Package.Author+" · "+review.State,14,"#A7D9FA"));
            panel.Children.Add(PageTheme.Text("SHA-256: "+review.Sha256,12,"#9EB4C8"));
            if(scanEvidence!=null) {
                panel.Children.Add(PageTheme.Text("Scan: "+scanEvidence.Status+" · "+scanEvidence.Engine+" · "+scanEvidence.Version,13,"#A7D9FA"));
                panel.Children.Add(PageTheme.Text("Scanned "+DateTimeOffset.FromUnixTimeSeconds(scanEvidence.Time).UtcDateTime.ToString("u")+" · Isolation: "+scanEvidence.Isolation+" · Definitions age limit: "+scanEvidence.DatabaseMaxAgeDays+" days",12));
                panel.Children.Add(PageTheme.Text(scanEvidence.Fresh?"Server reports a fresh clean scan. Approval rechecks freshness.":"No fresh clean scan. Approval is unavailable.",12,"#FFDAA6"));
            }else panel.Children.Add(PageTheme.Text("Scan evidence unavailable. Approval requires a fresh clean scan.",12,"#FFDAA6"));
            panel.Children.Add(PageTheme.Text(review.Package.Description,13));
            panel.Children.Add(PageTheme.Text("Dependencies: "+(review.Package.Dependencies.Count==0?"None":String.Join(", ",review.Package.Dependencies.Select(p=>p.Key+" @ "+p.Value))),12));
            foreach(var file in review.Package.Files.Take(32))panel.Children.Add(PageTheme.Text(file.Key+" · "+file.Value.Size+" bytes · "+file.Value.Sha256,12,"#9EB4C8"));
            if(review.Package.Files.Count>32)panel.Children.Add(PageTheme.Text("Showing 32 of "+review.Package.Files.Count+" declared files. Inspect the full package independently before approval.",12));
            var scripts=review.Package.Files.Keys.Where(path=>path.StartsWith("scripts/",StringComparison.Ordinal) && path.EndsWith(".lua",StringComparison.OrdinalIgnoreCase)).OrderBy(path=>path,StringComparer.Ordinal).ToArray();
            if(scripts.Length>0) {
                string chosen=sourceFiles.SelectedItem as string;sourceFiles.ItemsSource=scripts;sourceFiles.SelectedItem=scripts.Contains(chosen)?chosen:scripts[0];sourceFiles.IsEnabled=stop==null;panel.Children.Add(sourceFiles);
                Button(panel,"InspectModeratorSource","Read Lua source",async()=>await ReadSource(review,(string)sourceFiles.SelectedItem,0));
                if(sourcePage!=null) {
                    var page=sourcePage;panel.Children.Add(PageTheme.Text(page.Path+" · bytes "+page.Offset+"–"+page.NextOffset+" of "+page.Size+" · SHA-256: "+page.PayloadSha256,12));
                    panel.Children.Add(PageTheme.Text("Read-only text. Hidden control and direction characters are shown as Unicode escapes.",12));
                    var sourceView=new TextBox {Name="ModeratorSourceText",Text=page.DisplayText(),IsReadOnly=true,AcceptsReturn=true,MinHeight=120,MaxHeight=360};Ui.ScrollBars(sourceView);
#if MACOS || LINUX
                    sourceView.TextWrapping=Avalonia.Media.TextWrapping.Wrap;
#else
                    sourceView.TextWrapping=System.Windows.TextWrapping.Wrap;
#endif
                    panel.Children.Add(sourceView);
                    if(page.NextOffset<page.Size)Button(panel,"NextModeratorSource","Next source portion",async()=>await ReadSource(review,page.Path,page.NextOffset));
                }
            }
            if(decision==null) {
                panel.Children.Add(PageTheme.Text("Review note",13));panel.Children.Add(reviewNote);reviewNote.IsEnabled=stop==null;
                Button(panel,"ReviewModeratorApproval","Review approval",()=>ReviewDecision("approve"),review.State=="pending-review" && scanEvidence!=null && scanEvidence.Fresh);
                Button(panel,"ReviewModeratorRevocation","Review revocation",()=>ReviewDecision("revoke"),review.State!="revoked");
            }else {
                panel.Children.Add(PageTheme.Text((decision=="approve"?"Approve":"Revoke")+" this exact package? Note: "+decisionNote,14,"#FFDAA6"));
                panel.Children.Add(PageTheme.Text("Approval permits signed publication. Revocation withdraws repository availability; public snapshots and installed clients must also receive the change.",12));
                Button(panel,"ConfirmModeratorDecision","Confirm "+decision,async()=>{string action=decision,note=decisionNote;ClearReviews();await Run(token=>Client.Moderate(review,action,note,token),"Moderation decision recorded. Refresh the queue to check current status.",35);});
                Button(panel,"CancelModeratorDecision","Back",()=>{decision=null;decisionNote=null;Render();});
            }
        }
        void ReviewDecision(string action)
        {
            if(action=="approve" && (selectedReview==null || selectedReview.State!="pending-review" || scanEvidence==null || !scanEvidence.Fresh)){status.Text="Refresh scan evidence before approval.";return;}
            string note=(reviewNote.Text??"").Trim();
            if(note.Length==0 || note.Length>1800 || note.Any(Char.IsControl)){status.Text="Enter a review note of 1 to 1800 characters without control characters.";return;}
            decision=action;decisionNote=note;Render();
        }
        async Task ReadSource(ModeratorUpload review,string path,long offset)
        {sourcePage=null;await Run(async token=>{sourcePage=await Client.Source(review,path,offset,token);},"Lua source loaded for inspection. Nothing was executed or installed.",45);}
        async Task Authenticate(bool redeem)
        {
            string name=username.Text??"",secret=Password,invite=Invitation;ClearCredentials();ClearReviews();reviewed=null;uploads=new CreatorUpload[0];cursor=0;
            await Run(token=>redeem?Client.Redeem(invite,secret,token):Client.SignIn(name,secret,token),"Signed in. Review a ZIP to submit your addition.",45);
        }
        async Task History()
        {uploads=new CreatorUpload[0];await Run(async token=>{uploads=await Client.History(cursor,token);},"Upload status refreshed.",35);}
        async Task Run(Func<CancellationToken,Task> operation,string success,int seconds)
        {
            if(disposed || stop!=null)return;
            if(preview){ClearCredentials();status.Text="Preview only. No account request or upload was made.";return;}
            using(var cancellation=new CancellationTokenSource(TimeSpan.FromSeconds(seconds))) {
                stop=cancellation;status.Text="Working with the creator workshop…";Render();
                try{await operation(cancellation.Token);if(!disposed)status.Text=success;}
                catch(OperationCanceledException){if(!disposed)status.Text="Request cancelled. Refresh upload status before retrying a transfer.";}
                catch(Exception error){if(!disposed)status.Text=error.Message;}
                finally{stop=null;if(!disposed)Render();}
            }
        }
        public void Dispose(){disposed=true;ClearCredentials();ClearReviews();if(stop!=null)stop.Cancel();if(client!=null)client.Dispose();}
    }
}
