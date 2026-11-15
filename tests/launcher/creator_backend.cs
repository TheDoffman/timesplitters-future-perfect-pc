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
    public static int RunSafely(bool scanner=false)
    {try{return scanner?RunScanner():Run();}catch(Exception error){Console.Error.WriteLine(error);return 1;}}
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Reject(Action action,string marker,string message){try{action();}catch(Exception error){Need(error.Message.Contains(marker),"The real backend refusal occurred at the wrong stage.");return;}throw new Exception(message);}
    sealed class Backend : HttpMessageHandler
    {
        readonly Process child;readonly object exchange=new object();readonly Task<string> errors;
        readonly int timeout;
        public readonly string Invitation,OtherInvitation,ReviewerInvitation,Package,OtherPackage;
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
            using(var other=new ModCreatorClient(root,WorkshopTrustChecks.Origin,new Borrowed(backend),()=>WorkshopTrustChecks.Now)) {
                const string password="Disposable native account password! e\u0301",loginPassword="Disposable native account password! \u00e9";
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
    public static int RunScanner()
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
                client.SignOut(CancellationToken.None).GetAwaiter().GetResult();Need(!Directory.Exists(Core.PathIn(root,"Mods")),"Scanner acceptance installed or enabled content.");
            }
            return 0;
        }finally{Directory.Delete(root,true);}
#endif
    }
    sealed class Borrowed : HttpMessageHandler
    {
        readonly HttpMessageInvoker invoker;
        public Borrowed(Backend backend){invoker=new HttpMessageInvoker(backend,false);}
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken cancel){return invoker.SendAsync(request,cancel);}
        protected override void Dispose(bool disposing){if(disposing)invoker.Dispose();base.Dispose(disposing);}
    }
}
