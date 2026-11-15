using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

// Golden public envelopes were signed by Python/PyCA, never by this verifier.
// Their ephemeral private keys were discarded and are not trusted by any release.
public static class WorkshopTrustChecks
{
    static readonly Dictionary<string,object> fixtures=Load();
    static Dictionary<string,object> Load()
    {
        using(var input=typeof(WorkshopTrustChecks).Assembly.GetManifestResourceStream("workshop-trust-fixtures.json"))
        using(var output=new MemoryStream()){input.CopyTo(output);return ModJson.Read(Encoding.UTF8.GetString(output.ToArray()));}
    }
    public static byte[] Bytes(string name){return Convert.FromBase64String((string)fixtures[name]);}
    public static string Origin {get{return (string)fixtures["origin"];}}
    public static DateTime Now {get{return DateTimeOffset.FromUnixTimeSeconds((long)fixtures["unix"]).UtcDateTime;}}
    public static long Unix {get{return (long)fixtures["unix"];}}
    public static string Sha {get{return (string)fixtures["package_sha256"];}}
    public static void Pin(string root)
    {Directory.CreateDirectory(Core.PathIn(root,"Support/Mods"));File.WriteAllBytes(Core.PathIn(root,"Support/Mods/trusted-root.json"),Bytes("root1"));}
    public static HttpResponseMessage Response(string name)
    {return new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(Bytes(name))};}
    sealed class Handler : HttpMessageHandler
    {
        readonly Func<HttpRequestMessage,HttpResponseMessage> reply;
        internal Handler(Func<HttpRequestMessage,HttpResponseMessage> reply){this.reply=reply;}
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token)
        {token.ThrowIfCancellationRequested();return Task.FromResult(reply(request));}
    }
    static ModWorkshopClient Client(string folder,Func<int> version,Func<DateTime> clock=null,Action<string> request=null)
    {
        return new ModWorkshopClient(new Handler(r=>{
            string path=r.RequestUri.AbsolutePath;if(request!=null)request(path);
            if(path=="/v1/catalog")return Response("catalog"+version());
            if(path=="/v1/trust/catalog")return Response("metadata"+version());
            if(path=="/v1/trust/roots/2")return Response("root2");
            if(path=="/v1/packages/"+Sha)return Response("package");
            throw new Exception("Unexpected trust request: "+path);
        }),folder,clock??(()=>Now));
    }
    static WorkshopRelease[] Read(ModWorkshopClient client){return client.Read(Origin,CancellationToken.None).GetAwaiter().GetResult();}
    public static WorkshopRelease[] DependencyReleases(){return ModWorkshopClient.Parse(Encoding.UTF8.GetString(Bytes("dependency_catalog1")),Now);}
    static ModWorkshopClient DependencyClient(string folder,Func<int> version,Action<string> packageRequest=null)
    {
        var releases=DependencyReleases();
        return new ModWorkshopClient(new Handler(request=>{
            string path=request.RequestUri.AbsolutePath;
            if(path=="/v1/catalog")return Response("dependency_catalog"+version());
            if(path=="/v1/trust/catalog")return Response("dependency_metadata"+version());
            var release=releases.Single(r=>path=="/v1/packages/"+r.Sha256);
            if(packageRequest!=null)packageRequest(release.Id);
            return Response("dependency_package_"+release.Id);
        }),folder,()=>Now);
    }
    static ModLibrary DependencyLibrary(string folder)
    {
        Directory.CreateDirectory(Core.PathIn(folder,"Support/Mods"));File.WriteAllBytes(Core.PathIn(folder,"Support/Mods/trusted-root.json"),Bytes("dependency_root"));
        return new ModLibrary(folder,()=>false,()=>Now);
    }
    static WorkshopInstallPlan DependencyPlan(ModWorkshopClient client,ModLibrary library)
    {return client.Plan(Origin,DependencyReleases().Single(r=>r.Id=="addon"),library,CancellationToken.None).GetAwaiter().GetResult();}
    static string State(string folder){return Core.PathIn(folder,".launcher/mod-trust/"+ModTrust.Hash(Encoding.UTF8.GetBytes(Origin))+"/state.json");}
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Reject(Action action,string message){bool rejected=false;try{action();}catch{rejected=true;}Need(rejected,message);}
    sealed class NonPumpingContext : SynchronizationContext
    {
        public int Posts;
        public override void Post(SendOrPostCallback callback,object state){Interlocked.Increment(ref Posts);}
    }
    public static void Run(string root,Action<string,Action> test)
    {
        test("native trust binds supported HTTPS ports across catalog verification transport and rotation",()=>{
            foreach(string port in new[]{"8443","10000"}) {
                var authority=ModTrust.Roots(Bytes("port_root"+port),new byte[0][],Unix);
                Need(authority.Origin==Origin+":"+port,"Signed port was discarded");
                ModTrust.Catalog(Bytes("port_metadata"+port),Bytes("port_catalog"),authority,Unix);
            }
            var first=ModTrust.Roots(Bytes("port_root8443"),new byte[0][],Unix);
            Reject(()=>ModTrust.Catalog(Bytes("port_metadatawrongdefault"),Bytes("port_catalog"),first,Unix),"Same-key metadata crossed HTTPS authorities");
            Reject(()=>ModTrust.Roots(Bytes("port_root8443"),new[]{Bytes("port_root10000")},Unix),"Root rotation changed the HTTPS port");
            Reject(()=>ModTrust.Roots(Bytes("port_root_invalid444"),new byte[0][],Unix),"Signed but unapproved port accepted");
            string folder=Path.Combine(root,"port-transport");Directory.CreateDirectory(Core.PathIn(folder,"Support/Mods"));File.WriteAllBytes(Core.PathIn(folder,"Support/Mods/trusted-root.json"),Bytes("port_root8443"));int requests=0;
            using(var client=new ModWorkshopClient(new Handler(request=>{
                requests++;Need(request.RequestUri.Port==8443,"Transport silently used the default port");
                return Response(request.RequestUri.AbsolutePath=="/v1/catalog"?"port_catalog":"port_metadata8443");
            }),folder,()=>Now)) {
                Need(client.Read(Origin+":8443",CancellationToken.None).GetAwaiter().GetResult().Length==0,"Supported-port catalog failed");
                int before=requests;Reject(()=>client.Read(Origin,CancellationToken.None).GetAwaiter().GetResult(),"Wrong-port service reused a pinned authority");Need(requests==before,"Wrong authority reached the network");
            }
        });
        test("workshop planning completes without posting continuations to a blocked caller context",()=>{
            string folder=Path.Combine(root,"dependency-context");var library=DependencyLibrary(folder);
            using(var client=DependencyClient(folder,()=>1)) {
                var prior=SynchronizationContext.Current;var blocked=new NonPumpingContext();
                try {
                    SynchronizationContext.SetSynchronizationContext(blocked);
                    var task=client.Plan(Origin,DependencyReleases().Single(r=>r.Id=="addon"),library,CancellationToken.None);
                    Need(task.Wait(TimeSpan.FromSeconds(10)),"Workshop planner depends on the caller's UI dispatcher");
                    Need(task.Result.Downloads.Length==3 && blocked.Posts==0,"Planner captured the caller context");
                }finally{SynchronizationContext.SetSynchronizationContext(prior);}
            }
        });
        test("signed dependency installation verifies archives reuses exact packages and preserves the active profile",()=>{
            string folder=Path.Combine(root,"dependency-complete");var library=DependencyLibrary(folder);int requests=0;
            using(var client=DependencyClient(folder,()=>1,_=>requests++)) {
                var plan=DependencyPlan(client,library);Need(plan.Downloads.Length==3,"Required packages missing");
                client.Install(Origin,plan,library,folder,CancellationToken.None).GetAwaiter().GetResult();
                Need(requests==3 && library.List().Length==3 && library.Enabled().Length==0,"Install downloaded incorrectly or enabled packages");
                library.SetEnabled(plan.Ordered.Select(r=>r.Id+"@"+r.Version),CancellationToken.None);
                string profile=File.ReadAllText(Core.PathIn(folder,"Mods/profile.json"));
                var reuse=DependencyPlan(client,library);Need(reuse.Downloads.Length==0,"Matching installed dependencies were not reused");
                client.Install(Origin,reuse,library,folder,CancellationToken.None).GetAwaiter().GetResult();
                Need(requests==3 && File.ReadAllText(Core.PathIn(folder,"Mods/profile.json"))==profile,"Reuse changed the profile or downloaded packages");
                var baseline=library.ReadInstalled("base@1.0.0");File.WriteAllText(Path.Combine(baseline.Directory,"content/textures/1111111111111111.dds"),"changed");
                Reject(()=>client.Install(Origin,reuse,library,folder,CancellationToken.None).GetAwaiter().GetResult(),"Tampered installed payload reused");
                Need(File.ReadAllText(Core.PathIn(folder,"Mods/profile.json"))==profile,"Rejected reuse changed the active profile");
            }
        });
        test("signed dependency cancellation and combined download bounds retain only completed inactive packages",()=>{
            foreach(bool cancel in new[]{true,false}) {
                string folder=Path.Combine(root,"dependency-partial-"+cancel);var library=DependencyLibrary(folder);
                using(var client=DependencyClient(folder,()=>1))using(var stop=new CancellationTokenSource()) {
                    var plan=DependencyPlan(client,library);
                    long budget=Bytes("dependency_package_base").Length+Bytes("dependency_package_middle").Length-1;
                    Reject(()=>client.Install(Origin,plan,library,folder,stop.Token,text=>{if(cancel && text.StartsWith("Downloading 2 of"))stop.Cancel();},cancel?512L*1024*1024:budget).GetAwaiter().GetResult(),"Cancelled or oversized plan completed");
                    Need(library.List().Single().Id=="base" && library.Enabled().Length==0,"Partial install published the dependent or changed the active profile");
                    Need(Directory.GetFiles(Core.PathIn(folder,".launcher/mod-downloads"),"*.partial").Length==0,"Partial transfer was not cleaned up");
                }
            }
        });
        test("signed dependency revocation during transfer prevents the dependent and later activation",()=>{
            string folder=Path.Combine(root,"dependency-revoked");var library=DependencyLibrary(folder);int version=1;
            using(var client=DependencyClient(folder,()=>version,id=>{if(id=="addon")version=2;})) {
                var plan=DependencyPlan(client,library);
                Reject(()=>client.Install(Origin,plan,library,folder,CancellationToken.None).GetAwaiter().GetResult(),"Revoked dependency plan completed");
                Need(library.List().Select(m=>m.Id).OrderBy(id=>id).SequenceEqual(new[]{"base","middle"}) && library.Enabled().Length==0,"A revoked plan published the dependent or enabled packages");
                Reject(()=>library.Enable("base@1.0.0",CancellationToken.None),"Revoked partial dependency enabled");
            }
        });
        test("signed dependency metadata changes require a new preview and mismatched archives never publish",()=>{
            foreach(bool changeBefore in new[]{true,false}) {
                string folder=Path.Combine(root,"dependency-metadata-"+changeBefore);var library=DependencyLibrary(folder);int version=changeBefore?1:3,requests=0;
                using(var client=DependencyClient(folder,()=>version,_=>requests++)) {
                    var plan=DependencyPlan(client,library);version=3;
                    Reject(()=>client.Install(Origin,plan,library,folder,CancellationToken.None).GetAwaiter().GetResult(),"Changed or misleading metadata installed");
                    Need(!library.List().Any(m=>m.Id=="addon") && library.Enabled().Length==0,"Mismatched archive published or enabled");
                    Need(requests==(changeBefore?0:3),"Stale plan downloaded before re-review or archive comparison did not run");
                }
            }
        });
        test("native trust verifies Python signatures and rejects tampering and incomplete root thresholds",()=>{
            var first=ModTrust.Roots(Bytes("root1"),new byte[0][],Unix);
            var metadata=ModTrust.Catalog(Bytes("metadata1"),Bytes("catalog1"),first,Unix);
            Need(metadata.Version==1,"Python catalog signature did not verify");
            var sameVersion=ModTrust.Catalog(Bytes("metadata1").Concat(new byte[]{32}).ToArray(),Bytes("catalog1"),first,Unix);
            Reject(()=>ModTrust.Forward(metadata,Unix,sameVersion,Unix),"Same version with changed signed metadata was accepted");
            byte[] changed=Bytes("catalog1").Concat(new byte[]{32}).ToArray();
            Reject(()=>ModTrust.Catalog(Bytes("metadata1"),changed,first,Unix),"Changed catalog bytes accepted");
            Reject(()=>ModTrust.Catalog(Bytes("metadata1"),Bytes("catalog1"),first,Unix+86400),"Expired metadata accepted");
            var wire=ModJson.Read(Encoding.UTF8.GetString(Bytes("metadata1")));wire["payload"]=Convert.ToBase64String(Encoding.UTF8.GetBytes("{}"));
            Reject(()=>ModTrust.Catalog(Encoding.UTF8.GetBytes(Json.Encode(wire)),Bytes("catalog1"),first,Unix),"Forged metadata accepted");
            var next=ModTrust.Roots(Bytes("root1"),new[]{Bytes("root2")},Unix);
            Need(ModTrust.Catalog(Bytes("metadata5"),Bytes("catalog5"),next,Unix).RootVersion==2,"Cross-signed key rotation failed");
            Reject(()=>ModTrust.Catalog(Bytes("metadata1"),Bytes("catalog1"),next,Unix),"Retired catalog key accepted");
            var rotated=ModJson.Read(Encoding.UTF8.GetString(Bytes("root2")));rotated["signatures"]=((object[])rotated["signatures"]).Skip(2).ToArray();
            Reject(()=>ModTrust.Roots(Bytes("root1"),new[]{Encoding.UTF8.GetBytes(Json.Encode(rotated))},Unix),"New root authorized itself without old threshold");
            Reject(()=>ModTrust.Roots(Bytes("root1"),new[]{Bytes("root2"),Bytes("root2")},Unix),"Repeated root version accepted");
        });
        test("native trust persists signed state across restarts and rejects catalog and root rollback",()=>{
            string folder=Path.Combine(root,"trust-restart");Pin(folder);int version=1,roots=0;
            using(var client=Client(folder,()=>version,null,path=>{if(path.StartsWith("/v1/trust/roots/"))roots++;}))Need(Read(client).Length==1,"Signed catalog unavailable");
            byte[] original=File.ReadAllBytes(State(folder));version=3;
            using(var client=Client(folder,()=>version))Need(Read(client).Length==1,"Restart lost trust state");
            version=1;using(var client=Client(folder,()=>version))Reject(()=>Read(client),"Older catalog accepted after restart");
            Need(!File.ReadAllBytes(State(folder)).SequenceEqual(original),"Newer catalog was not saved");
            version=5;using(var client=Client(folder,()=>version,null,path=>{if(path.StartsWith("/v1/trust/roots/"))roots++;}))Read(client);
            Need(roots==1,"Root rotation was not fetched once");
            using(var client=Client(folder,()=>version,null,path=>{if(path.StartsWith("/v1/trust/roots/"))roots++;}))Read(client);
            Need(roots==1,"Restart unnecessarily downloaded the saved root chain");
            version=3;using(var client=Client(folder,()=>version))Reject(()=>Read(client),"Older root accepted after restart");
        });
        test("native trust refuses damaged missing changed pins and concurrent writers",()=>{
            string folder=Path.Combine(root,"trust-corruption");Pin(folder);using(var client=Client(folder,()=>1))Read(client);
            string path=State(folder);byte[] saved=File.ReadAllBytes(path);File.WriteAllText(path,"{}");int requests=0;
            using(var client=Client(folder,()=>1,null,p=>requests++))Reject(()=>Read(client),"Damaged cache silently reset");Need(requests==0,"Damaged state reached network");
            File.Delete(path);using(var client=Client(folder,()=>1))Reject(()=>Read(client),"Missing initialized state silently reset");File.WriteAllBytes(path,saved);
            using(var held=new ModTrustStore(folder,Origin,Unix))using(var client=Client(folder,()=>1))Reject(()=>Read(client),"Concurrent trust writer was allowed");
            using(var client=Client(folder,()=>1,()=>Now.AddSeconds(-1)))Reject(()=>Read(client),"Clock rollback was accepted");
            File.WriteAllBytes(Core.PathIn(folder,"Support/Mods/trusted-root.json"),Bytes("root2"));
            using(var client=Client(folder,()=>5))Reject(()=>Read(client),"Changed local pin silently reset state");
            Need(File.ReadAllBytes(path).SequenceEqual(saved),"Rejected trust update changed the last accepted state");
        });
        test("native trust rejects forgotten revocations and unsigned or oversized metadata",()=>{
            string folder=Path.Combine(root,"trust-revocations");Pin(folder);int version=4;
            using(var client=Client(folder,()=>version))Need(Read(client).Length==0,"Revoked package remained listed");
            version=6;using(var client=Client(folder,()=>version))Reject(()=>Read(client),"Catalog forgot a prior revocation");
            Reject(()=>ModTrustStore.RequireAvailable(folder,Origin,Sha,Now),"Revoked package remained usable");
            foreach(bool oversized in new[]{false,true})using(var client=new ModWorkshopClient(new Handler(r=>{
                var result=Response("catalog1");if(oversized)result.Content.Headers.ContentLength=ModTrust.MaxMetadata+1;return result;
            }),folder,()=>Now))Reject(()=>Read(client),"Unsigned or oversized metadata accepted");
        });
        test("native workshop install activation and game launch enforce cached expiry and revocation",()=>{
            string folder=Path.Combine(root,"trust-activation");Pin(folder);int version=1;DateTime clock=Now;
            using(var client=Client(folder,()=>version)) {
                var selected=Read(client).Single();string package=client.Download(Origin,selected,folder,CancellationToken.None).GetAwaiter().GetResult();
                var library=new ModLibrary(folder,()=>false,()=>clock);var installed=library.Install(package,Sha,CancellationToken.None,selected.Id,selected.Version,Origin);
                Need(library.ReadInstalled(installed.Key).WorkshopOrigin==Origin,"Workshop provenance lost");library.Enable(installed.Key,CancellationToken.None);
                var info=new System.Diagnostics.ProcessStartInfo();library.Configure(info);Need(info.EnvironmentVariables.ContainsKey("TSFP_MOD_TEXTURE_DIR"),"Fresh trusted package did not mount");
                clock=Now.AddDays(1);Reject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Expired workshop package mounted");
                Reject(()=>library.Enable(installed.Key,CancellationToken.None),"Expired workshop package enabled");clock=Now;
                version=4;Read(client);Reject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Revoked package mounted");
                Reject(()=>library.Enable(installed.Key,CancellationToken.None),"Revoked package enabled");
                string profile=File.ReadAllText(Core.PathIn(folder,"Mods/profile.json"));File.WriteAllText(State(folder),"{}");
                Reject(()=>library.Configure(new System.Diagnostics.ProcessStartInfo()),"Damaged trust state permitted mounting");
                Need(!GameSettings.StartInfo(folder,true).EnvironmentVariables.ContainsKey("TSFP_MOD_TEXTURE_DIR"),"Original-game recovery tried to mount workshop mods");
                Need(File.ReadAllText(Core.PathIn(folder,"Mods/profile.json"))==profile,"Recovery changed the enabled profile");
                library.SetEnabled(new string[0],CancellationToken.None);library.Uninstall(installed.Key);Need(library.List().Length==0,"Revoked package could not be disabled/removed");
                Need(File.Exists(package),"Removal damaged the verified download cache");
            }
        });
        test("native download rejects revocation during transfer and keeps cancelled trust updates atomic",()=>{
            string folder=Path.Combine(root,"trust-transfer");Pin(folder);int version=1;
            using(var client=new ModWorkshopClient(new Handler(r=>{
                string path=r.RequestUri.AbsolutePath;
                if(path=="/v1/trust/catalog")return Response("metadata"+version);
                if(path=="/v1/catalog")return Response("catalog"+version);
                version=4;return Response("package");
            }),folder,()=>Now)) {
                var release=Read(client).Single();Reject(()=>client.Download(Origin,release,folder,CancellationToken.None).GetAwaiter().GetResult(),"Package revoked during transfer was retained");
                Need(Directory.GetFiles(Core.PathIn(folder,".launcher/mod-downloads")).Length==0,"Rejected transfer left package or partial bytes");
            }
            byte[] previous=File.ReadAllBytes(State(folder));
            using(var cancel=new CancellationTokenSource())using(var client=new ModWorkshopClient(new Handler(r=>{cancel.Cancel();return Response("metadata4");}),folder,()=>Now))
                Reject(()=>client.Read(Origin,cancel.Token).GetAwaiter().GetResult(),"Cancelled trust transfer accepted");
            Need(File.ReadAllBytes(State(folder)).SequenceEqual(previous),"Cancellation changed accepted trust state");
            using(var client=new ModWorkshopClient(new Handler(r=>{
                var content=new ByteArrayContent(new byte[ModTrust.MaxMetadata+1]);content.Headers.ContentLength=1;
                return new HttpResponseMessage(HttpStatusCode.OK){Content=content};
            }),folder,()=>Now))Reject(()=>Read(client),"Actual metadata body exceeded its declared size and limit");
            Need(File.ReadAllBytes(State(folder)).SequenceEqual(previous),"Oversized transfer changed accepted trust state");
        });
    }
}
