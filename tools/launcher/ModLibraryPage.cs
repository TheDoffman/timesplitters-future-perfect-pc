using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Diagnostics;

namespace TimeSplitters.Launcher
{
    // Unlike the legacy launcher JSON helper, package JSON rejects duplicate keys.
    // Limits are independent of any antivirus result or repository trust claim.
    internal sealed class ModJson
    {
        readonly string text;int at;
        ModJson(string text){this.text=text;}
        void Space(){while(at<text.Length && " \t\r\n".IndexOf(text[at])>=0)at++;}
        void Need(bool value){Core.Require(value,"Invalid or ambiguous mod JSON.");}
        string String()
        {
            int begin=at++;bool closed=false;
            while(at<text.Length){char c=text[at++];Need(c>=32);if(c=='\\'){Need(at<text.Length);at++;}else if(c=='"'){closed=true;break;}}
            Need(closed);return Json.Serializer().Deserialize<string>(text.Substring(begin,at-begin));
        }
        object Value(int depth)
        {
            Need(depth<=32);Space();Need(at<text.Length);char c=text[at];
            if(c=='"')return String();
            if(c=='{') {
                at++;var result=new Dictionary<string,object>(StringComparer.Ordinal);Space();
                if(at<text.Length && text[at]=='}'){at++;return result;}
                while(true){Space();Need(at<text.Length && text[at]=='"');string key=String();Need(!result.ContainsKey(key));Space();Need(at<text.Length && text[at++]==':');result.Add(key,Value(depth+1));Need(result.Count<=4096);Space();Need(at<text.Length);char end=text[at++];if(end=='}')return result;Need(end==',');}
            }
            if(c=='[') {
                at++;var result=new List<object>();Space();if(at<text.Length && text[at]==']'){at++;return result.ToArray();}
                while(true){result.Add(Value(depth+1));Need(result.Count<=4096);Space();Need(at<text.Length);char end=text[at++];if(end==']')return result.ToArray();Need(end==',');}
            }
            foreach(string token in new[]{"true","false","null"})if(text.Length-at>=token.Length && System.String.CompareOrdinal(text,at,token,0,token.Length)==0){at+=token.Length;return token=="null"?null:(object)(token=="true");}
            var number=Regex.Match(text.Substring(at),@"\A-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?");Need(number.Success);at+=number.Length;
            long integer;if(Int64.TryParse(number.Value,NumberStyles.AllowLeadingSign,CultureInfo.InvariantCulture,out integer))return integer;
            decimal fraction;Need(Decimal.TryParse(number.Value,NumberStyles.Float,CultureInfo.InvariantCulture,out fraction));return fraction;
        }
        public static Dictionary<string,object> Read(string text,int limit=256*1024)
        {
            Core.Require(Encoding.UTF8.GetByteCount(text)<=limit,"Mod metadata exceeds its size limit.");
            var reader=new ModJson(text);var value=reader.Value(0);reader.Space();reader.Need(reader.at==text.Length && value is Dictionary<string,object>);return (Dictionary<string,object>)value;
        }
    }
    public sealed class InstalledMod
    {
        public string Id,Version,Name,Author,Description,Sha256,Directory;
        public readonly Dictionary<string,string> Dependencies=new Dictionary<string,string>(StringComparer.Ordinal);
        public readonly Dictionary<string,ModPayload> Files=new Dictionary<string,ModPayload>(StringComparer.Ordinal);
        public string Key {get{return Id+"@"+Version;}}
        public bool TextureOnly {get{return Files.Count>0 && Files.Keys.All(name=>Regex.IsMatch(name,@"\Atextures/[0-9a-f]{16}\.dds\z"));}}
    }
    public sealed class ModPayload {public string Sha256;public long Size;}

    // One model for all native launchers. Original files, official textures,
    // settings and saves never enter this model's installation/cleanup roots.
    public sealed class ModLibrary
    {
        public const long MaxExpanded=256L*1024*1024;
        const int MaxFiles=2048,MaxManifest=256*1024;
        readonly string root;readonly Func<bool> running;
        static readonly Regex IdPattern=new Regex(@"\A[a-z][a-z0-9]*(?:-[a-z0-9]+)*\z");
        static readonly Regex VersionPattern=new Regex(@"\A(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\z");
        static readonly Regex HashPattern=new Regex(@"\A[0-9a-f]{64}\z");
        public ModLibrary(string root):this(root,Core.GameRunning){}
        internal ModLibrary(string root,Func<bool> running){this.root=Path.GetFullPath(root);this.running=running;}
        string PathIn(string name){return Core.PathIn(root,"Mods/"+name);}
        FileStream Lock(){Directory.CreateDirectory(Core.PathIn(root,"Mods"));return new FileStream(PathIn("library.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);}
        static Dictionary<string,object> Object(Dictionary<string,object> parent,string name)
        {object value;Core.Require(parent.TryGetValue(name,out value) && value is Dictionary<string,object>,"Missing mod metadata object: "+name);return (Dictionary<string,object>)value;}
        static string Text(Dictionary<string,object> parent,string name,int limit)
        {object value;Core.Require(parent.TryGetValue(name,out value) && value is string,"Missing mod metadata text: "+name);string text=(string)value;Core.Require(text.Length>0 && text.Length<=limit && !text.Any(Char.IsControl),"Invalid mod metadata: "+name);return text;}
        static long Number(Dictionary<string,object> parent,string name,long maximum)
        {object value;Core.Require(parent.TryGetValue(name,out value) && value is long && (long)value>0 && (long)value<=maximum,"Invalid mod size/schema: "+name);return (long)value;}
        static void Fields(Dictionary<string,object> value,params string[] keys)
        {Core.Require(value.Count==keys.Length && keys.All(value.ContainsKey),"Unknown or missing mod metadata fields.");}
        static void Key(string key)
        {
            Core.Require(key!=null && key.Length<=113,"Invalid installed mod identity.");string[] split=key.Split('@');
            Core.Require(split.Length==2 && split[0].Length<=80 && split[1].Length<=32 && IdPattern.IsMatch(split[0]) && VersionPattern.IsMatch(split[1]),"Invalid installed mod identity.");
        }
        static void PayloadName(string name)
        {
            Core.Require(name!=null && name.Length<=180,"Invalid mod path.");Core.Name(name);
            Core.Require(name.Split('/').All(part=>Regex.IsMatch(part,@"\A[A-Za-z0-9_][A-Za-z0-9_.-]*\z")),"Nonportable mod path.");
            string prefix=name.Split('/')[0],extension=Path.GetExtension(name).ToLowerInvariant();
            Core.Require(name.Contains('/') && ((prefix=="textures" && extension==".dds") || (prefix=="audio" && (extension==".wav" || extension==".ogg")) || (prefix=="maps" && extension==".json") || (prefix=="scripts" && extension==".lua")),"Unsupported mod payload.");
            if(prefix=="textures")Core.Require(Regex.IsMatch(name,@"\Atextures/[0-9a-f]{16}\.dds\z"),"Textures must use a flat, lowercase 16-digit game texture key.");
        }
        public static InstalledMod Manifest(string json)
        {
            var m=ModJson.Read(json);Fields(m,"schema","id","version","name","author","description","game","api","dependencies","files");
            Core.Require(Number(m,"schema",1)==1 && Text(m,"game",64)=="tsfp-us-45410066" && Text(m,"api",64)=="tsfp-mod-v1-draft","Unsupported mod game/API.");
            var mod=new InstalledMod {Id=Text(m,"id",80),Version=Text(m,"version",32),Name=Text(m,"name",120),Author=Text(m,"author",120),Description=Text(m,"description",4000)};Key(mod.Key);
            var dependencies=Object(m,"dependencies");Core.Require(dependencies.Count<=32,"Too many mod dependencies.");
            foreach(var pair in dependencies){Core.Require(pair.Value is string && pair.Key!=mod.Id,"Invalid or self-referencing dependency.");string version=(string)pair.Value;Key(pair.Key+"@"+version);mod.Dependencies.Add(pair.Key,version);}
            var files=Object(m,"files");Core.Require(files.Count>0 && files.Count<=MaxFiles,"Invalid mod file count.");long total=0;var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var pair in files) {
                PayloadName(pair.Key);Core.Require(seen.Add(pair.Key),"Case-colliding mod payloads.");var entry=pair.Value as Dictionary<string,object>;Core.Require(entry!=null,"Invalid mod file entry.");Fields(entry,"sha256","size");
                string sha=Text(entry,"sha256",64);Core.Require(HashPattern.IsMatch(sha),"Invalid mod file digest.");long size=Number(entry,"size",64L*1024*1024);total+=size;Core.Require(total<=MaxExpanded,"Expanded mod exceeds its limit.");mod.Files.Add(pair.Key,new ModPayload {Sha256=sha,Size=size});
            }
            foreach(string name in mod.Files.Keys){string parent=name;while(parent.Contains('/')){parent=parent.Substring(0,parent.LastIndexOf('/'));Core.Require(!seen.Contains(parent),"Mod file/directory collision.");}}
            return mod;
        }
        static byte[] Read(Stream input,long expected,long maximum,CancellationToken token)
        {
            Core.Require(expected>=0 && expected<=maximum,"Mod entry exceeds its size limit.");
            using(var output=new MemoryStream()) {byte[] buffer=new byte[65536];int count;while((count=input.Read(buffer,0,buffer.Length))>0){token.ThrowIfCancellationRequested();Core.Require(output.Length+count<=expected,"Mod decompression exceeds declared size.");output.Write(buffer,0,count);}Core.Require(output.Length==expected,"Truncated mod payload.");return output.ToArray();}
        }
        static uint U32(byte[] bytes,int at){return BitConverter.ToUInt32(bytes,at);}
        public static void ValidateDds(byte[] data)
        {
            Core.Require(data.Length>=128 && data.Length<=24*1024*1024 && U32(data,0)==0x20534444 && U32(data,4)==124 && U32(data,76)==32 && U32(data,112)==0 && U32(data,24)==0,"Invalid DDS header or size.");
            uint width=U32(data,16),height=U32(data,12),levels=U32(data,28),required=1;
            Core.Require(width>0 && height>0 && width<=4096 && height<=4096 && (width&(width-1))==0 && (height&(height-1))==0,"Unsupported DDS dimensions.");
            for(uint size=Math.Max(width,height);size>1;size>>=1)required++;
            Core.Require(levels==required && levels<=13,"DDS requires a complete mip chain.");
            uint flags=U32(data,80),fourcc=U32(data,84),block=0;
            if(flags==4 && fourcc==0x31545844)block=8;
            else if(flags==4 && (fourcc==0x33545844 || fourcc==0x35545844))block=16;
            else Core.Require(flags==0x41 && fourcc==0 && U32(data,88)==32 && U32(data,92)==0xff0000 && U32(data,96)==0xff00 && U32(data,100)==0xff && U32(data,104)==0xff000000,"Unsupported DDS pixel format.");
            long length=128;for(uint i=0;i<levels;i++){length+=block>0?((width+3)/4)*((height+3)/4)*block:width*height*4;width=Math.Max(1,width>>1);height=Math.Max(1,height>>1);}
            Core.Require(length==data.Length,"DDS mip data is truncated or has trailing bytes.");
        }
        static string Hash(byte[] bytes){using(var sha=System.Security.Cryptography.SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
        static void ValidatePayload(string name,byte[] bytes)
        {
            Core.Require(!(bytes.Length>=2 && bytes[0]=='M' && bytes[1]=='Z') && !(bytes.Length>=4 && bytes[0]==0x7f && bytes[1]=='E' && bytes[2]=='L' && bytes[3]=='F'),"Native executable payload forbidden.");
            if(name.StartsWith("textures/",StringComparison.Ordinal))ValidateDds(bytes);
            else if(name.StartsWith("scripts/",StringComparison.Ordinal)) {string source=new UTF8Encoding(false,true).GetString(bytes);Core.Require(!source.Contains('\0') && !source.StartsWith("\u001bLua",StringComparison.Ordinal),"Scripts must be UTF-8 Lua source.");}
            else if(name.StartsWith("maps/",StringComparison.Ordinal))ModJson.Read(new UTF8Encoding(false,true).GetString(bytes),64*1024*1024);
            else if(name.EndsWith(".wav",StringComparison.OrdinalIgnoreCase))Core.Require(bytes.Length>=12 && Encoding.ASCII.GetString(bytes,0,4)=="RIFF" && Encoding.ASCII.GetString(bytes,8,4)=="WAVE","Invalid WAV payload.");
            else Core.Require(bytes.Length>=4 && Encoding.ASCII.GetString(bytes,0,4)=="OggS","Invalid Ogg payload.");
        }
        InstalledMod Extract(string package,string destination,string expected,CancellationToken token)
        {
            Core.Require(HashPattern.IsMatch(expected??"") && new FileInfo(package).Length<=ModWorkshopClient.MaxPackageBytes && Core.Hash(package)==expected,"Mod archive integrity failure.");
            using(var archive=ZipFile.OpenRead(package)) {
                Core.Require(archive.Entries.Count>1 && archive.Entries.Count<=MaxFiles+1,"Invalid mod archive entry count.");var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach(var entry in archive.Entries) {
                    Core.Require(names.Add(entry.FullName),"Duplicate mod archive entry.");
                    if(entry.FullName!="mod.json")PayloadName(entry.FullName);
                    int type=(entry.ExternalAttributes>>16)&0xf000;
                    Core.Require((type==0 || type==0x8000) && entry.Name.Length>0 && (entry.ExternalAttributes & 16)==0,"Mod archive links/directories/special files forbidden.");
                }
                var metadata=archive.GetEntry("mod.json");Core.Require(metadata!=null,"Missing mod.json.");string json;
                using(var input=metadata.Open())json=new UTF8Encoding(false,true).GetString(Read(input,metadata.Length,MaxManifest,token));
                var mod=Manifest(json);Core.Require(archive.Entries.Count==mod.Files.Count+1,"Undeclared or missing mod payload.");
                foreach(var pair in mod.Files) {
                    token.ThrowIfCancellationRequested();var entry=archive.GetEntry(pair.Key);Core.Require(entry!=null && entry.Length==pair.Value.Size,"Mod payload/manifest mismatch.");byte[] bytes;
                    using(var input=entry.Open())bytes=Read(input,entry.Length,64L*1024*1024,token);
                    Core.Require(Hash(bytes)==pair.Value.Sha256,"Mod payload checksum mismatch.");ValidatePayload(pair.Key,bytes);
                    string path=Core.PathIn(destination,pair.Key);Directory.CreateDirectory(Path.GetDirectoryName(path));using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.None))output.Write(bytes,0,bytes.Length);
                }
                File.WriteAllText(Core.PathIn(destination,"mod.json"),json,new UTF8Encoding(false));mod.Sha256=expected;return mod;
            }
        }
        public InstalledMod Install(string package,string expected,CancellationToken token,string expectedId=null,string expectedVersion=null)
        {
            using(var operation=Lock()) {
            Core.Require(!running(),"Close the game before installing mods.");string temporary=PathIn(".install-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temporary);
            try {
                string snapshot=Core.PathIn(temporary,"package.zip");
                using(var input=File.OpenRead(package))using(var output=new FileStream(snapshot,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                    byte[] buffer=new byte[65536];int count;while((count=input.Read(buffer,0,buffer.Length))>0){token.ThrowIfCancellationRequested();Core.Require(output.Length+count<=ModWorkshopClient.MaxPackageBytes,"Mod archive exceeds its download limit.");output.Write(buffer,0,count);}
                }
                string content=Core.PathIn(temporary,"content");Directory.CreateDirectory(content);var mod=Extract(snapshot,content,expected,token);
                Core.Require((expectedId==null && expectedVersion==null) || (mod.Id==expectedId && mod.Version==expectedVersion),"The mod package identity does not match its workshop listing.");
                string target=PathIn("Packages/"+mod.Key);Core.Require(!Directory.Exists(target) && !File.Exists(target),"This mod version is already installed.");
                // Dependencies are resolved again before enabling; install order is unrestricted.
                Core.Write(Core.PathIn(temporary,"receipt.json"),Json.Encode(new {schema=1,id=mod.Id,version=mod.Version,sha256=expected}));
                token.ThrowIfCancellationRequested();Core.Require(!running(),"Close the game before installing mods.");Directory.CreateDirectory(Path.GetDirectoryName(target));Directory.Move(temporary,target);mod.Directory=target;return mod;
            } finally {if(Directory.Exists(temporary))DeleteOwned(temporary);}
            }
        }
        void DeleteOwned(string directory)
        {
            string store=PathIn("Packages");string active=PathIn("Active");string full=Path.GetFullPath(directory);
            Core.Require(full.StartsWith(store+Path.DirectorySeparatorChar,StringComparison.Ordinal) || full.StartsWith(active+Path.DirectorySeparatorChar,StringComparison.Ordinal) ||
                (Path.GetDirectoryName(full)==Core.PathIn(root,"Mods") && Path.GetFileName(full).StartsWith(".install-",StringComparison.Ordinal)),"Refusing cleanup outside owned mod directories.");
            CheckTree(full);
            if(full.StartsWith(active+Path.DirectorySeparatorChar,StringComparison.Ordinal))
                foreach(string file in Directory.GetFiles(full,"*",SearchOption.AllDirectories)) {
                    string relative=file.Substring(full.Length+1).Replace('\\','/');
                    Core.Require(relative=="manifest.json" || Regex.IsMatch(relative,@"\Atextures/[0-9a-f]{16}\.dds\z"),"Unexpected files in an old mod cache were preserved for review.");
                }
            Directory.Delete(full,true);
        }
        static void CheckTree(string directory)
        {Core.RequireUnlinked(directory,"Linked mod directory refused.");foreach(string file in Directory.GetFiles(directory))Core.RequireUnlinked(file,"Linked mod file refused.");foreach(string child in Directory.GetDirectories(directory))CheckTree(child);}
        public InstalledMod ReadInstalled(string key)
        {
            Key(key);string path=PathIn("Packages/"+key);CheckTree(path);
            string receiptPath=Core.PathIn(path,"receipt.json"),manifestPath=Core.PathIn(path,"content/mod.json");
            Core.Require(new FileInfo(receiptPath).Length<=8192 && new FileInfo(manifestPath).Length<=MaxManifest,"Installed mod metadata exceeds limits.");
            var receipt=ModJson.Read(File.ReadAllText(receiptPath));Core.Require(Number(receipt,"schema",1)==1,"Unsupported installed mod receipt.");string original;
            using(var archive=ZipFile.OpenRead(Core.PathIn(path,"package.zip"))) {
                var metadata=archive.GetEntry("mod.json");Core.Require(metadata!=null,"Installed package manifest is missing.");
                using(var input=metadata.Open())original=new UTF8Encoding(false,true).GetString(Read(input,metadata.Length,MaxManifest,CancellationToken.None));
            }
            Core.Require(File.ReadAllText(manifestPath)==original,"Installed manifest was modified. Reinstall the mod.");var mod=Manifest(original);
            Core.Require(mod.Key==key && Text(receipt,"id",80)==mod.Id && Text(receipt,"version",32)==mod.Version,"Installed mod identity mismatch.");
            mod.Sha256=Text(receipt,"sha256",64);Core.Require(HashPattern.IsMatch(mod.Sha256),"Invalid installed mod digest.");mod.Directory=path;return mod;
        }
        public InstalledMod[] List()
        {
            string path=PathIn("Packages");if(!Directory.Exists(path))return new InstalledMod[0];
            return Directory.GetDirectories(path).OrderBy(p=>p,StringComparer.Ordinal).Select(p=>ReadInstalled(Path.GetFileName(p))).ToArray();
        }
        Dictionary<string,object> Profile()
        {
            string path=PathIn("profile.json");if(!File.Exists(path))return null;
            Core.Require(new FileInfo(path).Length<=65536,"Mod profile exceeds its limit.");return ModJson.Read(File.ReadAllText(path),65536);
        }
        public string[] Enabled()
        {
            var profile=Profile();if(profile==null)return new string[0];Fields(profile,"schema","generation","enabled");
            Core.Require(Number(profile,"schema",1)==1,"Unsupported mod profile.");string generation=Text(profile,"generation",32);Core.Require(Regex.IsMatch(generation,@"\A[0-9a-f]{32}\z"),"Invalid mod generation.");
            object value;Core.Require(profile.TryGetValue("enabled",out value) && value is object[],"Invalid enabled mod list.");var array=(object[])value;Core.Require(array.Length<=128,"Too many enabled mods.");
            var keys=array.Select(item=>{var entry=item as Dictionary<string,object>;Core.Require(entry!=null,"Invalid enabled mod identity.");Fields(entry,"key","sha256");string key=Text(entry,"key",113);Key(key);Core.Require(HashPattern.IsMatch(Text(entry,"sha256",64)),"Invalid profile mod digest.");return key;}).ToArray();Core.Require(keys.Distinct(StringComparer.Ordinal).Count()==keys.Length,"Duplicate enabled mods.");return keys;
        }
        public void SetEnabled(IEnumerable<string> keys,CancellationToken token)
        {using(var operation=Lock())SetEnabledCore(keys,token);}
        void SetEnabledCore(IEnumerable<string> keys,CancellationToken token)
        {
            Core.Require(!running(),"Close the game before changing enabled mods.");var selected=keys.ToArray();Core.Require(selected.Length<=128 && selected.Distinct(StringComparer.Ordinal).Count()==selected.Length,"Invalid enabled mod list.");
            var mods=selected.Select(ReadInstalled).ToArray();Core.Require(mods.Select(m=>m.Id).Distinct(StringComparer.Ordinal).Count()==mods.Length,"Only one version of each mod can be enabled.");
            var assets=new Dictionary<string,string>(StringComparer.Ordinal);long total=0;
            foreach(var mod in mods) {
                Core.Require(mod.TextureOnly,"This build can enable texture-only mods. Other content remains installed until its runtime is available.");
                foreach(var dependency in mod.Dependencies)Core.Require(mods.Any(m=>m.Id==dependency.Key && m.Version==dependency.Value),"Enable the required dependency first: "+dependency.Key+"@"+dependency.Value);
                foreach(var pair in mod.Files){Core.Require(!assets.ContainsKey(pair.Key),"Texture conflict: "+pair.Key+" is provided by more than one enabled mod.");assets.Add(pair.Key,mod.Key);total+=pair.Value.Size;Core.Require(total<=MaxExpanded,"Enabled texture mods exceed the memory/storage budget.");}
            }
            // Reject cycles even if exact versions happen to all be present.
            var visited=new HashSet<string>();var pending=new HashSet<string>();
            foreach(var mod in mods)Visit(mod,mods,visited,pending);
            string generation=Guid.NewGuid().ToString("N"),directory=PathIn("Active/"+generation),textures=Core.PathIn(directory,"textures");Directory.CreateDirectory(textures);bool published=false;
            try {
                foreach(var mod in mods) {
                    token.ThrowIfCancellationRequested();Core.Require(Core.Hash(Core.PathIn(mod.Directory,"package.zip"))==mod.Sha256,"Installed package was modified; reinstall this mod.");
                    foreach(var pair in mod.Files) {
                        token.ThrowIfCancellationRequested();
                        string source=Core.PathIn(mod.Directory,"content/"+pair.Key);Core.Require(new FileInfo(source).Length==pair.Value.Size && Core.Hash(source)==pair.Value.Sha256,"Installed mod payload was modified; reinstall this mod.");
                        byte[] bytes=File.ReadAllBytes(source);ValidateDds(bytes);string target=Core.PathIn(textures,Path.GetFileName(pair.Key));File.WriteAllBytes(target,bytes);
                    }
                }
                Core.Write(Core.PathIn(directory,"manifest.json"),Json.Encode(mods.Select(m=>new {key=m.Key,sha256=m.Sha256}).ToArray()));
                token.ThrowIfCancellationRequested();Core.Require(!running(),"Close the game before changing enabled mods.");
                Core.Write(PathIn("profile.json"),Json.Encode(new {schema=1,generation=generation,enabled=mods.Select(m=>new {key=m.Key,sha256=m.Sha256}).ToArray()}));published=true;
                // Versioned packages provide rollback; abandoned overlay generations
                // are reproducible and must not accumulate another full texture copy.
                foreach(string old in Directory.GetDirectories(PathIn("Active")))if(Path.GetFileName(old)!=generation && Regex.IsMatch(Path.GetFileName(old),@"\A[0-9a-f]{32}\z")) {
                    try{DeleteOwned(old);}catch(Exception error){if(!(error is IOException) && !(error is UnauthorizedAccessException) && !(error is InvalidDataException))throw;}
                }
            } finally {if(!published && Directory.Exists(directory))DeleteOwned(directory);}
        }
        static void Visit(InstalledMod mod,InstalledMod[] mods,HashSet<string> visited,HashSet<string> pending)
        {if(visited.Contains(mod.Id))return;Core.Require(pending.Add(mod.Id),"Mod dependency cycle detected.");foreach(var dependency in mod.Dependencies)Visit(mods.Single(m=>m.Id==dependency.Key),mods,visited,pending);pending.Remove(mod.Id);visited.Add(mod.Id);}
        public void Enable(string key,CancellationToken token)
        {using(var operation=Lock()){var mod=ReadInstalled(key);SetEnabledCore(Enabled().Where(k=>k.Split('@')[0]!=mod.Id).Concat(new[]{key}),token);}}
        public void Disable(string key,CancellationToken token){using(var operation=Lock())SetEnabledCore(Enabled().Where(k=>k!=key),token);}
        public void DisableAll(CancellationToken token){SetEnabled(new string[0],token);}
        public void Uninstall(string key)
        {
            using(var operation=Lock()) {
            Core.Require(!running(),"Close the game before removing mods.");var mod=ReadInstalled(key);Core.Require(!Enabled().Contains(key),"Disable this mod before uninstalling it.");
            Core.Require(!List().Any(m=>m.Key!=key && m.Dependencies.Any(d=>d.Key==mod.Id && d.Value==mod.Version)),"Another installed mod requires this version. Remove its dependents first.");
            var expected=new HashSet<string>(mod.Files.Keys.Select(name=>"content/"+name).Concat(new[]{"package.zip","receipt.json","content/mod.json"}),StringComparer.Ordinal);
            foreach(string file in Directory.GetFiles(mod.Directory,"*",SearchOption.AllDirectories))
                Core.Require(expected.Contains(file.Substring(mod.Directory.Length+1).Replace('\\','/')),"Unexpected files were found in this mod version. Move your personal files out before uninstalling it.");
            DeleteOwned(mod.Directory);
            }
        }
        public void Configure(ProcessStartInfo info)
        {
            if(Enabled().Length==0)return;
            using(var operation=Lock()) {
            var keys=Enabled();if(keys.Length==0)return;var profile=Profile();string generation=Text(profile,"generation",32),directory=PathIn("Active/"+generation),textures=Core.PathIn(directory,"textures");CheckTree(directory);
            var bindings=((object[])profile["enabled"]).Cast<Dictionary<string,object>>().ToDictionary(e=>Text(e,"key",113),e=>Text(e,"sha256",64),StringComparer.Ordinal);
            var mods=keys.Select(ReadInstalled).ToArray();var expected=new HashSet<string>(StringComparer.Ordinal);
            Core.Require(mods.Select(m=>m.Id).Distinct(StringComparer.Ordinal).Count()==mods.Length && mods.Sum(m=>m.Files.Sum(f=>f.Value.Size))<=MaxExpanded,"Invalid active mod set or size.");
            var visited=new HashSet<string>();var pending=new HashSet<string>();
            foreach(var mod in mods) {
                Core.Require(mod.TextureOnly && mod.Sha256==bindings[mod.Key] && Core.Hash(Core.PathIn(mod.Directory,"package.zip"))==mod.Sha256,"Enabled mod is unsupported or has changed. Launch without mods or reinstall it.");
                foreach(var dependency in mod.Dependencies)Core.Require(mods.Any(m=>m.Id==dependency.Key && m.Version==dependency.Value),"The active mod profile is missing a required dependency.");
                foreach(var pair in mod.Files) {
                    string name=Path.GetFileName(pair.Key);Core.Require(expected.Add(name),"Enabled texture conflict.");string file=Core.PathIn(textures,name);
                    Core.Require(new FileInfo(file).Length==pair.Value.Size && Core.Hash(file)==pair.Value.Sha256,"Active mod data changed. Re-enable the profile or launch without mods.");
                }
            }
            foreach(var mod in mods)Visit(mod,mods,visited,pending);
            Core.Require(Directory.GetFiles(textures).Select(Path.GetFileName).OrderBy(s=>s,StringComparer.Ordinal).SequenceEqual(expected.OrderBy(s=>s,StringComparer.Ordinal)),"Unexpected active texture files.");
            info.EnvironmentVariables["TSFP_MOD_TEXTURE_DIR"]=textures;
            }
        }
    }
}
