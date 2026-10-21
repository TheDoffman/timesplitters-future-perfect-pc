using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

public static class LauncherTextureChecks
{
    static int passed;
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Test(string name,Action action){action();passed++;Console.WriteLine("PASS textures: "+name);}
    static void Reject(Action action){try{action();}catch(InvalidDataException){return;}throw new Exception("Invalid pack was accepted");}
    const string A="0123456789abcdef", B="fedcba9876543210";
    static byte[] Dds(byte pixel=7)
    {
        var words=new uint[32];words[0]=0x20534444;words[1]=124;words[3]=words[4]=1;words[7]=1;words[19]=32;words[20]=4;words[21]=0x31545844;
        using(var memory=new MemoryStream())using(var writer=new BinaryWriter(memory)){foreach(var word in words)writer.Write(word);writer.Write(Enumerable.Repeat(pixel,8).ToArray());return memory.ToArray();}
    }
    static void Put(string path,byte[] data){Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllBytes(path,data);}
    static string Zip(string root,string name,params string[] entries)
    {
        string path=Path.Combine(root,name+".zip");
        using(var archive=ZipFile.Open(path,ZipArchiveMode.Create))foreach(string entry in entries) {
            var item=archive.CreateEntry(entry);
            using(var stream=item.Open()){var data=Dds();stream.Write(data,0,data.Length);}
        }
        return path;
    }
    sealed class Immediate : IProgress<string>
    {
        readonly Action<string> report;
        public Immediate(Action<string> action){report=action;}
        public void Report(string message){report(message);}
    }
    sealed class Download : HttpMessageHandler
    {
        public byte[] Bytes;public int Requests;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token)
        {
            Requests++;
            return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK){RequestMessage=request,Content=new ByteArrayContent(Bytes)});
        }
    }
    public static int Run()
    {
        string area=Path.Combine(Path.GetFullPath("."),"texture-tests-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(area);
        int next=0;Func<string> root=()=>{string path=Path.Combine(area,"install-"+(++next));Directory.CreateDirectory(path);return path;};
        try {
            Test("folder import chooses runtime location and preserves unrelated files",()=>{
                string r=root(),source=root();Put(Path.Combine(source,A+".dds"),Dds());
                Put(Path.Combine(r,"Saves","user.txt"),new byte[]{42});
                Put(Path.Combine(TexturePack.Destination(r),B+".dds"),Dds(2));
                Need(TexturePack.Install(r,source,null,CancellationToken.None)==1,"Wrong count");
                Need(File.Exists(Path.Combine(TexturePack.Destination(r),A+".dds")),"Wrong destination");
                Need(File.Exists(Path.Combine(TexturePack.Destination(r),A+".json")),"No metadata generated");
                Need(File.ReadAllBytes(Path.Combine(r,"Saves","user.txt"))[0]==42,"Save changed");
                Need(File.ReadAllBytes(Path.Combine(TexturePack.Destination(r),B+".dds")).Last()==2,"Other texture changed");
                Need(!Directory.Exists(Path.Combine(r,".launcher","texture-install")),"Staging retained");
                Reject(()=>TexturePack.Install(r,TexturePack.Destination(r),null,CancellationToken.None));
            });
            Test("wrapped ZIP and DDS-only updates replace stale metadata",()=>{
                string r=root(),z=Zip(area,"wrapped","pack/"+A+".dds");
                TexturePack.Install(r,z,null,CancellationToken.None);
                var metadata=Path.Combine(TexturePack.Destination(r),A+".json");
                File.WriteAllText(metadata,"stale");
                TexturePack.Install(r,z,null,CancellationToken.None);
                Need(Json.String(Json.Object(File.ReadAllText(metadata)),"sha256")==Core.Hash(Path.Combine(TexturePack.Destination(r),A+".dds")),"Stale metadata survived");
            });
            Test("archive traversal, duplicate names, game files and multiple roots are rejected",()=>{
                int n=0;
                foreach(var entries in new[]{new[]{"../"+A+".dds"},new[]{A+".dds",A+".dds"},new[]{"default.xbe"},new[]{"a/"+A+".dds","b/"+B+".dds"},new[]{"Textures/"+A+".dds","Saves/data"}})
                    Reject(()=>TexturePack.Install(root(),Zip(area,"bad-"+(++n),entries),null,CancellationToken.None));
            });
            Test("symlink archive entries are rejected",()=>{
                string z=Zip(area,"linked",A+".dds");
                using(var zip=ZipFile.Open(z,ZipArchiveMode.Update))zip.Entries[0].ExternalAttributes=unchecked((int)(0xa1ffu<<16));
                Reject(()=>TexturePack.Install(root(),z,null,CancellationToken.None));
            });
            Test("late corrupt DDS rolls back replacements and leaves source intact",()=>{
                string r=root(),source=root(),dest=TexturePack.Destination(r);
                Put(Path.Combine(dest,A+".dds"),Dds(4));File.WriteAllText(Path.Combine(dest,A+".json"),"original metadata");
                Put(Path.Combine(source,A+".dds"),Dds(8));Put(Path.Combine(source,B+".dds"),new byte[]{1,2});
                Reject(()=>TexturePack.Install(r,source,null,CancellationToken.None));
                Need(File.ReadAllBytes(Path.Combine(dest,A+".dds")).Last()==4,"Replacement not rolled back");
                Need(File.ReadAllText(Path.Combine(dest,A+".json"))=="original metadata","Metadata not rolled back");
                Need(!File.Exists(Path.Combine(dest,B+".dds")) && Directory.GetFiles(source).Length==2,"Bad file or source modified");
            });
            Test("metadata checksum and key mismatches abort safely",()=>{
                string r=root(),source=root();Put(Path.Combine(source,A+".dds"),Dds());
                File.WriteAllText(Path.Combine(source,A+".json"),Json.Encode(new {key=A,sha256=new string('0',64)}));
                Reject(()=>TexturePack.Install(r,source,null,CancellationToken.None));
                Need(!File.Exists(Path.Combine(TexturePack.Destination(r),A+".dds")),"Corrupt metadata was installed");
            });
            Test("cancel after a completed key restores the whole current part",()=>{
                string r=root(),source=root();Put(Path.Combine(source,A+".dds"),Dds());Put(Path.Combine(source,B+".dds"),Dds());
                using(var cancel=new CancellationTokenSource()){
                    int seen=0;var progress=new Immediate(s=>{if(++seen==2)cancel.Cancel();});
                    try{TexturePack.Install(r,source,progress,cancel.Token);throw new Exception("Cancellation ignored");}catch(OperationCanceledException){}
                }
                Need(!Directory.GetFiles(TexturePack.Destination(r)).Any(),"Partial part survived cancellation");
            });
            Test("startup recovers interrupted renames and is idempotent",()=>{
                string r=root(),dest=TexturePack.Destination(r),work=Path.Combine(r,".launcher","texture-install");
                Put(Path.Combine(work,"old",A+".dds"),Dds(3));Put(Path.Combine(dest,A+".dds"),Dds(8));Put(Path.Combine(dest,B+".dds"),Dds(9));
                var journal=new TexturePack.Journal();journal.Files.Add(new Original{Name=A+".dds",Existed=true});journal.Files.Add(new Original{Name=B+".dds",Existed=false});
                File.WriteAllText(Path.Combine(work,"journal.json"),Json.Encode(journal));
                Need(TexturePack.Recover(r),"Recovery not reported");
                Need(File.ReadAllBytes(Path.Combine(dest,A+".dds")).Last()==3 && !File.Exists(Path.Combine(dest,B+".dds")),"Recovery failed");
                Need(!TexturePack.Recover(r),"Repeated recovery changed files");
            });
            Test("committed recovery only removes backups",()=>{
                string r=root(),work=Path.Combine(r,".launcher","texture-install");Put(Path.Combine(work,"old",A+".dds"),Dds(3));Put(Path.Combine(TexturePack.Destination(r),A+".dds"),Dds(8));
                var journal=new TexturePack.Journal{Committed=true};journal.Files.Add(new Original{Name=A+".dds",Existed=true});
                File.WriteAllText(Path.Combine(work,"journal.json"),Json.Encode(journal));
                TexturePack.Recover(r);Need(File.ReadAllBytes(Path.Combine(TexturePack.Destination(r),A+".dds")).Last()==8,"Committed pack reverted");
            });
            Test("DDS rejects incomplete mips, truncation, cubemaps and unsupported formats",()=>{
                foreach(int index in new[]{7,28,21}){
                    string path=Path.Combine(area,"invalid-"+index+".dds");var data=Dds();data[index*4]=0xff;File.WriteAllBytes(path,data);Reject(()=>TexturePack.ValidateDds(path));
                }
            });
            Test("download manifest binds bounded parts to this repository",()=>{
                Reject(()=>TextureDownload.Parse("{\"schema\":2,\"delivery\":\"bundled\"}"));
                string text=Json.Encode(new {schema=1,version="0.1.0-alpha.1",installed_bytes=256,texture_count=1,parts=new[]{new{name="textures-001.zip",size=256,sha256=new string('a',64)}}});
                var pack=TextureDownload.Parse(text);
                Need(pack.Parts[0].AssetUrl==ReleaseClient.ReleasesPage+"/download/textures-v0.1.0-alpha.1/textures-001.zip","Wrong download origin");
                Reject(()=>TextureDownload.Parse(text.Replace("\"installed_bytes\":256","\"installed_bytes\":6442450945")));
                Reject(()=>TextureDownload.Parse(text.Replace("textures-001.zip","../evil.zip")));
                Reject(()=>TextureDownload.Parse(text.Replace("\"size\":256","\"size\":67108865")));
            });
            Test("verified download installs automatically, clears cache and skips intact parts on retry",()=>{
                string r=root(),z=Zip(area,"network",A+".dds");
                var transport=new Download{Bytes=File.ReadAllBytes(z)};
                string text=Json.Encode(new {schema=1,version="0.1.0-alpha.1",installed_bytes=1024,texture_count=1,parts=new[]{new{name="textures-001.zip",size=transport.Bytes.Length,sha256=Core.Hash(z)}}});
                var pack=TextureDownload.Parse(text);
                using(var client=new ReleaseClient(transport)){
                    TextureDownload.Install(r,client,pack,null,CancellationToken.None).GetAwaiter().GetResult();
                    Need(File.Exists(Path.Combine(TexturePack.Destination(r),A+".dds")),"Downloaded pack not installed");
                    Need(!File.Exists(Path.Combine(r,".launcher","texture-download.zip")) && !File.Exists(Path.Combine(r,".launcher","downloads","textures.part")),"Download duplicated on disk");
                    TextureDownload.Install(r,client,pack,null,CancellationToken.None).GetAwaiter().GetResult();
                    Need(transport.Requests==1,"Intact installed part downloaded again");
                    File.WriteAllText(Path.Combine(TexturePack.Destination(r),A+".dds"),"damaged");
                    TextureDownload.Install(r,client,pack,null,CancellationToken.None).GetAwaiter().GetResult();
                    Need(transport.Requests==2,"Damaged part was not repaired");
                    TexturePack.ValidateDds(Path.Combine(TexturePack.Destination(r),A+".dds"));
                }
            });
#if MACOS
            Test("native checkout installs to private pack and rejects source symlinks",()=>{
                string r=root(),source=root();Put(Path.Combine(r,"tools","play-native-mac.py"),new byte[]{1});
                Put(Path.Combine(source,A+".dds"),Dds());
                TexturePack.Install(r,source,null,CancellationToken.None);
                Need(File.Exists(Path.Combine(r,"private","texture-remaster","pack",A+".dds")),"Checkout texture path not used");
                string linked=Path.Combine(area,"source-link");Directory.CreateSymbolicLink(linked,source);
                try{Reject(()=>TexturePack.Install(root(),linked,null,CancellationToken.None));}finally{Directory.Delete(linked);}
            });
#endif
            Console.WriteLine(passed+" texture installer checks passed");return 0;
        } finally { Directory.Delete(area,true); }
    }
}
