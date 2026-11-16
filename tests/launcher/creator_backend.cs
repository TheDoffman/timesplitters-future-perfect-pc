using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

// Uses real WorkshopAPI/Accounts/Uploads and SQLite, without a network server.
// The transport seam below belongs only to this explicit developer test mode.
public static class CreatorBackendChecks
{
    public static int RunSafely(bool scanner=false,bool moderation=false)
    {try{return scanner?RunScanner(moderation):Run();}catch(Exception error){Console.Error.WriteLine(error);return 1;}}
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Reject(Action action,string marker,string message){try{action();}catch(Exception error){Need(error.Message.Contains(marker),"The real backend refusal occurred at the wrong stage.");return;}throw new Exception(message);}
    sealed class Backend : HttpMessageHandler
    {
        readonly Process child;readonly object exchange=new object();readonly Task<string> errors;
        readonly int timeout;
        public readonly string Invitation,OtherInvitation,ReviewerInvitation,AdminInvitation,Package,OtherPackage;
        public readonly Dictionary<string,object> ScanPackages;
        public long Unix=WorkshopTrustChecks.Unix;
        public string LastBearer;
        public Backend(string folder,bool scanning=false)
        {
            string python=Environment.GetEnvironmentVariable("TSFP_TEST_CREATOR_PYTHON");
            string source=Environment.GetEnvironmentVariable("TSFP_TEST_CREATOR_SOURCE_ROOT");
            Need(!String.IsNullOrEmpty(python) && !String.IsNullOrEmpty(source),"Run the creator backend check through its build command with explicit developer Python.");
            var info=new ProcessStartInfo(python,"-I -B "+SelfUpdate.Quote(Path.Combine(source,"tests/launcher/native_creator_backend.py"))+" "+SelfUpdate.Quote(folder)+" "+SelfUpdate.Quote(WorkshopTrustChecks.Origin)+" "+WorkshopTrustChecks.Unix) {
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardInput=true,RedirectStandardOutput=true,RedirectStandardError=true,
                StandardOutputEncoding=new UTF8Encoding(false),StandardErrorEncoding=new UTF8Encoding(false),WorkingDirectory=source
            };
            info.EnvironmentVariables["TSFP_TEST_CREATOR_REAL_SCAN"]=scanning?"1":"0";timeout=scanning?210000:15000;
            child=Process.Start(info);errors=child.StandardError.ReadToEndAsync();
            try {
                var hello=Read();Need(hello.ContainsKey("ready") && hello["ready"] is bool && (bool)hello["ready"],"Creator backend did not initialize.");
                Invitation=ModTrust.Text(hello,"invitation");OtherInvitation=ModTrust.Text(hello,"other_invitation");Package=ModTrust.Text(hello,"package");OtherPackage=ModTrust.Text(hello,"other_package");
                ScanPackages=ModTrust.Object(hello,"scan_packages");
                ReviewerInvitation=ModTrust.Text(hello,"reviewer_invitation");
                AdminInvitation=ModTrust.Text(hello,"admin_invitation");
            }catch{Dispose();throw;}
        }
        Dictionary<string,object> Read()
        {
            var read=child.StandardOutput.ReadLineAsync();Need(read.Wait(timeout),"Creator backend response timed out.");
            string line=read.GetAwaiter().GetResult();Need(line!=null && line.Length<=2*1024*1024,"Creator backend ended or exceeded the response limit.");return ModJson.Read(line,2*1024*1024);
        }
        Dictionary<string,object> Exchange(object packet)
        {lock(exchange){byte[] wire=Encoding.UTF8.GetBytes(Json.Encode(packet)+"\n");child.StandardInput.BaseStream.Write(wire,0,wire.Length);child.StandardInput.BaseStream.Flush();return Read();}}
        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken cancel)
        {
            cancel.ThrowIfCancellationRequested();byte[] body=request.Content==null?new byte[0]:await request.Content.ReadAsByteArrayAsync().ConfigureAwait(false);
            string bearer=request.Headers.Authorization==null?"":request.Headers.Authorization.ToString();if(bearer.Length>0)LastBearer=bearer;
            var packet=new {method=request.Method.Method,path=request.RequestUri.AbsolutePath,query=request.RequestUri.Query.TrimStart('?'),authority=request.RequestUri.Authority,
                origin=request.Headers.GetValues("Origin").Single(),authorization=bearer,content_type=request.Content==null?"":Convert.ToString(request.Content.Headers.ContentType),
                length=request.Content==null?0:request.Content.Headers.ContentLength??body.LongLength,body=Convert.ToBase64String(body)};
            var response=await Task.Run(()=>Exchange(packet),cancel).ConfigureAwait(false);
            var result=new HttpResponseMessage((HttpStatusCode)ModTrust.Number(response,"status",100,599)){Content=new ByteArrayContent(Convert.FromBase64String(ModTrust.Text(response,"body")))};
            string kind=ModTrust.Text(response,"content_type");if(kind.Length>0)result.Content.Headers.ContentType=MediaTypeHeaderValue.Parse(kind);return result;
        }
        public int Probe(string path,string bearer)
        {
            var response=Exchange(new {method="GET",path=path,query="",authority=new Uri(WorkshopTrustChecks.Origin).Authority,origin=WorkshopTrustChecks.Origin,authorization=bearer,content_type="",length=0,body=""});
            return (int)ModTrust.Number(response,"status",100,599);
        }
        public Dictionary<string,object> Scan(string kind){return Exchange(new {scan_next=kind});}
        public Dictionary<string,object> Observe(){return Exchange(new {observe_repository=true});}
        public void Advance(){Unix=ModTrust.Number(Exchange(new {advance=301}),"now",Unix+301,Unix+301);}
        protected override void Dispose(bool disposing)
        {
            if(disposing) {
                try{if(!child.HasExited){Exchange(new {stop=true});Need(child.WaitForExit(5000),"Creator backend did not stop.");}}
                finally {
                    try{if(!child.HasExited){child.Kill();child.WaitForExit(5000);}Need(errors.Wait(5000) && errors.Result.Length==0,"Creator backend emitted unexpected diagnostics.");}
                    finally{child.Dispose();}
                }
            }
            base.Dispose(disposing);
        }
    }
    public static int Run()
    {
        string parent=Path.GetFullPath(Path.GetTempPath());
#if MACOS || LINUX
        parent=Path.GetFullPath(".");
#endif
        string root=Path.Combine(parent,"tsfp-creator-backend-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            WorkshopTrustChecks.Pin(root);Core.Write(Core.PathIn(root,"Support/Mods/service.json"),Json.Encode(new {service=WorkshopTrustChecks.Origin}));
            using(var backend=new Backend(Path.Combine(root,"private-backend")))
            using(var first=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now))
            using(var reviewer=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now))
            using(var owner=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now))
            using(var invited=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now))
            using(var other=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now)) {
                const string password="Disposable native account password! e\u0301",loginPassword="Disposable native account password! \u00e9";
                owner.Redeem(backend.AdminInvitation,password,CancellationToken.None).GetAwaiter().GetResult();
                Need(owner.CanInviteCreators && owner.CanModerate,"The real owner lost invitation privileges.");
                var invitation=owner.InviteCreator("invited-author",CancellationToken.None).GetAwaiter().GetResult();
                Need(invitation.Username=="invited-author" && invitation.Service==WorkshopTrustChecks.Origin,"Creator invitation identity changed.");
                invited.Redeem(invitation.Secret,password,CancellationToken.None).GetAwaiter().GetResult();
                Need(invited.Username=="invited-author" && !invited.CanInviteCreators && !invited.CanModerate,"Creator invitation granted privileged access.");
                Reject(()=>invited.InviteCreator("forbidden-author",CancellationToken.None).GetAwaiter().GetResult(),"owner account","A creator issued an invitation.");
                var cancelled=owner.InviteCreator("cancelled-author",CancellationToken.None).GetAwaiter().GetResult();
                owner.CancelCreatorInvitation(cancelled.Username,CancellationToken.None).GetAwaiter().GetResult();
                Reject(()=>invited.Redeem(cancelled.Secret,password,CancellationToken.None).GetAwaiter().GetResult(),"Sign in again","A cancelled invitation was redeemed.");
                var replacement=owner.InviteCreator(cancelled.Username,CancellationToken.None).GetAwaiter().GetResult();
                Need(replacement.Secret!=cancelled.Secret,"Invitation replacement reused the cancelled secret.");
                owner.CancelCreatorInvitation(replacement.Username,CancellationToken.None).GetAwaiter().GetResult();
                owner.SignOut(CancellationToken.None).GetAwaiter().GetResult();
                invited.SignOut(CancellationToken.None).GetAwaiter().GetResult();
                first.Redeem(backend.Invitation,password,CancellationToken.None).GetAwaiter().GetResult();Need(first.Username=="native-creator","Real account redemption differed.");
                var plan=CreatorPackagePlan.Review(backend.Package,CancellationToken.None);
                var uploaded=first.Upload(plan,CancellationToken.None).GetAwaiter().GetResult();Need(uploaded.State=="queued" && uploaded.Sha256==plan.Sha256,"Real upload did not enter private quarantine.");
                Need(backend.Probe("/v1/moderation",backend.LastBearer)==403,"The real API allowed a creator to list moderation records.");
                Reject(()=>first.Reviews(0,CancellationToken.None).GetAwaiter().GetResult(),"moderator account","The native creator issued a moderator request.");
                reviewer.Redeem(backend.ReviewerInvitation,password,CancellationToken.None).GetAwaiter().GetResult();
                Need(reviewer.CanModerate && reviewer.Reviews(0,CancellationToken.None).GetAwaiter().GetResult().Length==0,"The moderator queue exposed an unscanned upload.");
                reviewer.SignOut(CancellationToken.None).GetAwaiter().GetResult();
                var history=first.History(0,CancellationToken.None).GetAwaiter().GetResult();Need(history.Length==1 && history[0].Id==uploaded.Id && history[0].State=="queued","Real upload history differed.");
                Need(first.History(history[0].Cursor,CancellationToken.None).GetAwaiter().GetResult().Length==0,"Real history cursor repeated an upload.");
                Need(backend.Probe("/v1/packages/"+plan.Sha256,backend.LastBearer)==404,"The creator API exposed quarantine download bytes.");
                Reject(()=>first.Upload(plan,CancellationToken.None).GetAwaiter().GetResult(),"conflicts","The backend accepted a duplicate immutable version.");
                Need(first.History(0,CancellationToken.None).GetAwaiter().GetResult().Length==1,"A rejected retry duplicated an upload.");
                other.Redeem(backend.OtherInvitation,password,CancellationToken.None).GetAwaiter().GetResult();
                var conflicting=CreatorPackagePlan.Review(backend.OtherPackage,CancellationToken.None);
                Need(conflicting.Author==other.Username,"The ownership control did not reach an author-matched account.");
                Reject(()=>other.Upload(conflicting,CancellationToken.None).GetAwaiter().GetResult(),"not allowed","A different real account stole the mod ID.");
                Need(other.History(0,CancellationToken.None).GetAwaiter().GetResult().Length==0,"Creator history exposed another account's uploads.");
                first.SignOut(CancellationToken.None).GetAwaiter().GetResult();Need(!first.SignedIn && backend.Probe("/v1/account",backend.LastBearer)==401,"Sign-out failed to revoke the real backend session.");
                first.SignIn("native-creator",loginPassword,CancellationToken.None).GetAwaiter().GetResult();Need(first.SignedIn,"Real Unicode password login failed after sign-out.");
                first.SignOut(CancellationToken.None).GetAwaiter().GetResult();other.SignOut(CancellationToken.None).GetAwaiter().GetResult();
                Need(!Directory.Exists(Core.PathIn(root,"Mods")),"Creator backend flow installed or enabled a package.");
                Need(!Directory.GetFiles(root,"*",SearchOption.AllDirectories).Any(path=>new FileInfo(path).Length<8*1024*1024 && (Encoding.UTF8.GetString(File.ReadAllBytes(path)).Contains(password) || Encoding.UTF8.GetString(File.ReadAllBytes(path)).Contains(loginPassword))),"A fixture password reached persistent storage.");
            }
            Console.WriteLine("PASS creator backend: real invitation, exact upload, quarantine history, ownership, immutable versions and session revocation.");return 0;
        }finally{Directory.Delete(root,true);}
    }
    public static int RunScanner(bool moderation=false)
    {
#if !MACOS
        throw new InvalidOperationException("Real native creator scanner acceptance requires the Mac workshop host.");
#else
        Need(!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("TSFP_TEST_CREATOR_SCANNER_CONFIG")),"Select the verified private scanner acceptance configuration.");
        string root=Path.Combine(Path.GetFullPath("."),"tsfp-creator-scanner-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            WorkshopTrustChecks.Pin(root);Core.Write(Core.PathIn(root,"Support/Mods/service.json"),Json.Encode(new {service=WorkshopTrustChecks.Origin}));
            using(var backend=new Backend(Path.Combine(root,"private-backend"),true))
            using(var client=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>DateTimeOffset.FromUnixTimeSeconds(backend.Unix).UtcDateTime)) {
                client.Redeem(backend.Invitation,"Disposable native scanner password!",CancellationToken.None).GetAwaiter().GetResult();
                foreach(string kind in new[]{"clean","eicar","empty","missing"}) {
                    var plan=CreatorPackagePlan.Review(ModTrust.Text(backend.ScanPackages,kind),CancellationToken.None);
                    var uploaded=client.Upload(plan,CancellationToken.None).GetAwaiter().GetResult();Need(uploaded.State=="queued","Native scanner fixture did not enter quarantine.");
                    bool error=kind=="empty" || kind=="missing";int attempts=error?3:1;
                    for(int attempt=1;attempt<=attempts;attempt++) {
                        var result=backend.Scan(error?kind:"normal");var job=ModTrust.Object(result,"job");var scan=ModTrust.Object(result,"scan");
                        Need(ModTrust.Text(job,"id")==uploaded.Id,"Scanner processed the wrong owned fixture.");
                        string status=ModTrust.Text(scan,"status");
                        Need(status==(error?"error":kind=="clean"?"clean":"blocked"),"Unexpected real scanner result: "+Json.Encode(scan));
                        if(!error) {
                            Need(ModTrust.Text(scan,"isolation")=="macos-seatbelt-v3" && ModTrust.Text(scan,"version").StartsWith("ClamAV "),"Scanner did not prove its isolated engine identity.");
                            if(kind=="eicar")Need(ModTrust.Text(scan,"detail").Contains("FOUND"),"The real engine did not report its EICAR detection.");
                        }
                        Need(ModTrust.Number(result,"catalog_count",0,0)==0,"Scanner acceptance published an unreviewed package.");
                        var current=client.History(0,CancellationToken.None).GetAwaiter().GetResult().Single(row=>row.Id==uploaded.Id);
                        string expected=error?(attempt<3?"retry":"scan-error"):kind=="clean"?"pending-review":"rejected";
                        Need(current.State==expected && current.Sha256==plan.Sha256,"Native history misrepresented real scan/retry state.");
                        if(error)Need(current.Error=="scan-unavailable","Scanner errors lost their fail-closed status.");
                        Need(backend.Probe("/v1/packages/"+plan.Sha256,backend.LastBearer)==404,"Creator API exposed scanner fixture bytes.");
                        Console.WriteLine("PASS creator scanner: "+kind+" attempt "+attempt+" -> "+expected+"; no public catalog entry.");
                        if(error && attempt<3)backend.Advance();
                    }
                }
                if(moderation)RunModeration(root,backend,client);
                client.SignOut(CancellationToken.None).GetAwaiter().GetResult();Need(!Directory.Exists(Core.PathIn(root,"Mods")),"Scanner acceptance installed or enabled content.");
            }
            return 0;
        }finally{Directory.Delete(root,true);}
#endif
    }
    static void RunModeration(string root,Backend backend,ModCreatorClient creator)
    {
        Reject(()=>creator.Reviews(0,CancellationToken.None).GetAwaiter().GetResult(),"moderator account","A creator listed the real review queue.");
        using(var reviewer=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>DateTimeOffset.FromUnixTimeSeconds(backend.Unix).UtcDateTime)) {
            reviewer.Redeem(backend.ReviewerInvitation,"Disposable native reviewer password!",CancellationToken.None).GetAwaiter().GetResult();
            var queue=reviewer.Reviews(0,CancellationToken.None).GetAwaiter().GetResult();
            Need(queue.Length==4 && queue.Count(row=>row.State=="pending-review")==1 && queue.Count(row=>row.State=="rejected")==1 && queue.Count(row=>row.State=="scan-error")==2,"Real moderation queue misrepresented completed scans.");
            foreach(var review in queue) {
                var evidence=reviewer.Evidence(review,CancellationToken.None).GetAwaiter().GetResult();
                Need(evidence.Engine=="ClamAV" && evidence.Fresh==(review.State=="pending-review") && evidence.Status==(review.State=="pending-review"?"clean":review.State=="rejected"?"blocked":"error"),"Native evidence misrepresented the actual scanner result.");
                if(evidence.Fresh)Need(evidence.Version.StartsWith("ClamAV ") && evidence.Isolation=="macos-seatbelt-v3" && evidence.DatabaseMaxAgeDays==2,"Clean evidence lost the guarded engine/freshness policy.");
            }
            Console.WriteLine("PASS native moderation: role-gated evidence matches all4 actual clean/detection/error scans without raw scanner output.");
            Need(reviewer.Reviews(queue.Last().Cursor,CancellationToken.None).GetAwaiter().GetResult().Length==0,"Real moderation cursor repeated records.");
            foreach(var blocked in queue.Where(row=>row.State!="pending-review"))
                Reject(()=>reviewer.Moderate(blocked,"approve","Refuse unsafe fixture",CancellationToken.None).GetAwaiter().GetResult(),"awaiting review","A detected/error package reached native approval.");
            var clean=queue.Single(row=>row.ModId=="native-clean");Need(clean.State=="pending-review" && clean.Package.Author==creator.Username,"The native reviewer selected a different package.");
            var source=reviewer.Source(clean,"scripts/main.lua",0,CancellationToken.None).GetAwaiter().GetResult();
            Need(source.Text=="return {api=1}\n" && source.Offset==0 && source.NextOffset==source.Size,"Actual source inspection differed from the immutable original script.");
            var texture=reviewer.Texture(clean,"textures/0123456789abcdef.dds",CancellationToken.None).GetAwaiter().GetResult();
            Need(texture.Width==4 && texture.Height==4 && texture.PreviewWidth==4 && texture.PreviewHeight==4 && texture.Format=="BC1","Actual texture preview differed from the immutable original DDS.");
            Need(texture.DisplayPixels().SequenceEqual(Enumerable.Range(0,64).Select(i=>(byte)(i%4>=2?255:0))),"Actual texture preview did not show the original red pixels.");
            Console.WriteLine("PASS native moderation: role-gated Lua preview matches exact archived original bytes without extraction/execution.");
            var before=backend.Observe();Need(ModTrust.Array(before,"catalog",1).Length==0 && ModTrust.Array(before,"audit",1).Length==0,"Native moderation started from nonempty approved/audit state.");
            const string approval="Reviewed original native acceptance source",revocation="Withdraw disposable native acceptance";
            reviewer.Moderate(clean,"approve",approval,CancellationToken.None).GetAwaiter().GetResult();
            var approved=reviewer.Reviews(0,CancellationToken.None).GetAwaiter().GetResult().Single(row=>row.Id==clean.Id);
            Need(approved.State=="approved" && approved.Sha256==clean.Sha256,"Real approval did not reach native moderation history.");
            Need(creator.History(0,CancellationToken.None).GetAwaiter().GetResult().Single(row=>row.Id==clean.Id).State=="approved","Creator history hid the actual moderator decision.");
            var catalog=ModTrust.Array(backend.Observe(),"catalog",1);Need(catalog.Length==1 && catalog[0] is string && (string)catalog[0]==clean.Sha256,"Approval exposed a different/unsafe package.");
            Need(backend.Probe("/v1/packages/"+clean.Sha256,backend.LastBearer)==404,"The private API became a package download service.");
            Console.WriteLine("PASS native moderation: real clean scan -> native approval -> exact repository catalog/blob; no public download route.");
            using(var player=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>DateTimeOffset.FromUnixTimeSeconds(backend.Unix).UtcDateTime)) {
                player.Redeem(backend.OtherInvitation,"Disposable native player report password!",CancellationToken.None).GetAwaiter().GetResult();
                var report=player.Report(clean.Sha256,"broken","Original synthetic report for moderator acceptance",CancellationToken.None).GetAwaiter().GetResult();
                Need(report.Sha256==clean.Sha256 && report.Reporter==player.Username,"Actual report targeted another package/account.");
                Need(creator.Reports(0,false,CancellationToken.None).GetAwaiter().GetResult().Length==0,"Report privacy leaked another account's submission.");
                var incoming=reviewer.Reports(0,true,CancellationToken.None).GetAwaiter().GetResult();Need(incoming.Length==1 && incoming[0].Id==report.Id,"Moderator did not receive the real player report.");
                var closed=reviewer.ResolveReport(incoming[0],"close","Reviewed original fixture; no package decision",CancellationToken.None).GetAwaiter().GetResult();
                Need(closed.State=="closed" && player.Reports(0,false,CancellationToken.None).GetAwaiter().GetResult().Single().State=="closed","Reporter history did not receive moderator resolution.");
                Need(reviewer.Reports(0,true,CancellationToken.None).GetAwaiter().GetResult().Length==0,"Closed report remained in the open moderation queue.");
                Need(reviewer.Reports(0,true,CancellationToken.None,true).GetAwaiter().GetResult().Single().State=="closed","Moderator history lost the actual closed report.");
                Need(reviewer.ReportedRelease(closed,CancellationToken.None).GetAwaiter().GetResult().Id==clean.Id,"Native report inspection selected another archived upload.");
                Need(ModTrust.Array(backend.Observe(),"catalog",1).Length==1,"Reporting/resolution changed package availability.");
                player.SignOut(CancellationToken.None).GetAwaiter().GetResult();
                Console.WriteLine("PASS native reports: another player -> private real report -> moderator closure -> reporter status; catalog unchanged.");
            }
            reviewer.Moderate(approved,"revoke",revocation,CancellationToken.None).GetAwaiter().GetResult();
            Need(reviewer.Reviews(0,CancellationToken.None).GetAwaiter().GetResult().Single(row=>row.Id==clean.Id).State=="revoked","Revocation did not reach native review history.");
            Need(creator.History(0,CancellationToken.None).GetAwaiter().GetResult().Single(row=>row.Id==clean.Id).State=="revoked","Creator history hid revocation.");
            var after=backend.Observe();Need(ModTrust.Array(after,"catalog",1).Length==0,"Revocation retained repository availability.");
            var audit=ModTrust.Array(after,"audit",2);var events=ModTrust.Array(after,"events",2);Need(audit.Length==2 && events.Length==2,"Real moderation did not produce both audit trails.");
            for(int i=0;i<2;i++) {
                var account=audit[i] as Dictionary<string,object>;var release=events[i] as Dictionary<string,object>;Need(account!=null && release!=null,"Invalid audit observation.");
                string action=i==0?"approve":"revoke",note=i==0?approval:revocation;
                Need(ModTrust.Text(account,"actor")=="user:native-reviewer" && ModTrust.Text(account,"action")==action && ModTrust.Text(account,"target")==clean.Sha256 && ModTrust.Text(account,"note")==note,"The account audit lost moderator/decision binding.");
                Need(ModTrust.Text(release,"sha256")==clean.Sha256 && ModTrust.Text(release,"action")==(i==0?"approved":"revoked") && ModTrust.Text(release,"note")=="native-reviewer: "+note,"The release audit lost the decision binding.");
            }
            reviewer.SignOut(CancellationToken.None).GetAwaiter().GetResult();
            Console.WriteLine("PASS native moderation: native revoke -> withdrawn catalog; creator/reviewer histories and both actual audit trails agree.");
        }
    }
    sealed class Borrowed : HttpMessageHandler
    {
        readonly HttpMessageInvoker invoker;
        public Borrowed(Backend backend){invoker=new HttpMessageInvoker(backend,false);}
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken cancel){return invoker.SendAsync(request,cancel);}
        protected override void Dispose(bool disposing){if(disposing)invoker.Dispose();base.Dispose(disposing);}
    }
}
