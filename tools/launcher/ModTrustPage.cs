using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace TimeSplitters.Launcher
{
    // Shared wire verifier, not a UI page. The Page suffix includes this model in
    // all three existing shared-source builds. No cryptographic math is implemented here.
    internal static class ModTrust
    {
        internal const int MaxMetadata=2*1024*1024,MaxRoot=65536,MaxState=16*1024*1024;
        internal const long MaxVersion=9007199254740991;
        internal static readonly UTF8Encoding Utf8=new UTF8Encoding(false,true);
        static readonly byte[] Domain=Encoding.ASCII.GetBytes("TSFP-WORKSHOP-METADATA-V1\0");
        internal sealed class Root
        {
            internal Dictionary<string,object> Data;internal long Version,Issued,Expires;internal string Origin;
        }
        internal sealed class Metadata
        {
            internal long Version,RootVersion,Generated,Expires,Length;internal string Origin,CatalogHash,WireHash;
            internal string[] Revoked;
        }
        internal static void Need(bool value,string message){Core.Require(value,"Workshop trust: "+message);}
        internal static void Fields(Dictionary<string,object> data,string names)
        {var fields=names.Split(' ');Need(data.Count==fields.Length && fields.All(data.ContainsKey),"unknown or missing metadata fields.");}
        internal static long Number(Dictionary<string,object> data,string name,long minimum=1,long maximum=MaxVersion)
        {object v;Need(data.TryGetValue(name,out v) && v is long && (long)v>=minimum && (long)v<=maximum,"invalid metadata integer.");return (long)v;}
        internal static string Text(Dictionary<string,object> data,string name)
        {object v;Need(data.TryGetValue(name,out v) && v is string,"invalid metadata text.");return (string)v;}
        internal static Dictionary<string,object> Object(Dictionary<string,object> data,string name)
        {object v;Need(data.TryGetValue(name,out v) && v is Dictionary<string,object>,"invalid metadata object.");return (Dictionary<string,object>)v;}
        internal static object[] Array(Dictionary<string,object> data,string name,int maximum)
        {object v;Need(data.TryGetValue(name,out v) && v is object[] && ((object[])v).Length<=maximum,"invalid metadata array.");return (object[])v;}
        internal static bool IsHash(string value){return value!=null && Regex.IsMatch(value,@"\A[0-9a-f]{64}\z");}
        internal static byte[] Base64(string text,int maximum)
        {
            Need(text.Length<=4*((maximum+2)/3),"base64 exceeds its budget.");byte[] data;
            try{data=Convert.FromBase64String(text);}catch(FormatException){throw new InvalidDataException("Workshop trust: invalid base64.");}
            Need(data.Length<=maximum && Convert.ToBase64String(data)==text,"noncanonical base64.");return data;
        }
        internal static string Hash(byte[] bytes)
        {using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
        internal static Dictionary<string,object> JsonBytes(byte[] bytes,int maximum)
        {Need(bytes.Length<=maximum,"metadata exceeds its budget.");return ModJson.Read(Utf8.GetString(bytes),maximum);}
        static Dictionary<string,object>[] Envelope(byte[] wire,int maximum,out byte[] payload)
        {
            var data=JsonBytes(wire,maximum);Fields(data,"payload signatures");payload=Base64(Text(data,"payload"),maximum/2);
            var signatures=Array(data,"signatures",8);Need(signatures.Length>0,"missing signature.");var seen=new HashSet<string>(StringComparer.Ordinal);
            return signatures.Select(value=>{var item=value as Dictionary<string,object>;Need(item!=null,"invalid signature object.");Fields(item,"keyid signature");
                string id=Text(item,"keyid");Need(IsHash(id) && seen.Add(id),"duplicate or invalid signer.");Need(Base64(Text(item,"signature"),384).Length==384,"invalid signature size.");return item;}).ToArray();
        }
        static byte[] Modulus(Dictionary<string,object> key)
        {
            Fields(key,"scheme modulus exponent");Need(Text(key,"scheme")=="rsa-pkcs1-sha256" && Text(key,"exponent")=="AQAB","unsupported signing key.");
            byte[] modulus=Base64(Text(key,"modulus"),384);Need(modulus.Length==384 && (modulus[0]&128)!=0 && (modulus[383]&1)!=0,"invalid RSA3072 modulus.");return modulus;
        }
        static string KeyId(Dictionary<string,object> key)
        {
            Modulus(key);return Hash(Encoding.ASCII.GetBytes("{\"exponent\":\"AQAB\",\"modulus\":\""+Text(key,"modulus")+"\",\"scheme\":\"rsa-pkcs1-sha256\"}"));
        }
        static Dictionary<string,object> VerifyRole(byte[] wire,Root root,string name,int maximum)
        {
            byte[] payload;var signatures=Envelope(wire,maximum,out payload);var roles=Object(root.Data,"roles");var role=Object(roles,name);
            var ids=new HashSet<string>(Array(role,"keyids",4).Cast<string>(),StringComparer.Ordinal);var keys=Object(root.Data,"keys");int valid=0;
            byte[] message=new byte[Domain.Length+payload.Length];Buffer.BlockCopy(Domain,0,message,0,Domain.Length);Buffer.BlockCopy(payload,0,message,Domain.Length,payload.Length);
            foreach(var signature in signatures) {
                string id=Text(signature,"keyid");if(!ids.Contains(id))continue;
                using(var rsa=RSA.Create()) {
                    rsa.ImportParameters(new RSAParameters {Modulus=Modulus(Object(keys,id)),Exponent=new byte[]{1,0,1}});
                    if(rsa.VerifyData(message,Base64(Text(signature,"signature"),384),HashAlgorithmName.SHA256,RSASignaturePadding.Pkcs1))valid++;
                }
            }
            Need(valid>=Number(role,"threshold"),"signature threshold not met.");return JsonBytes(payload,maximum/2);
        }
        internal static Root ReadRoot(byte[] wire)
        {
            byte[] payload;Envelope(wire,MaxRoot,out payload);var data=JsonBytes(payload,MaxRoot/2);Fields(data,"type schema version origin issued expires keys roles");
            Need(Text(data,"type")=="tsfp-workshop-root" && Number(data,"schema")==1,"unsupported root.");
            var root=new Root {Data=data,Version=Number(data,"version"),Issued=Number(data,"issued",0),Expires=Number(data,"expires",0),Origin=Text(data,"origin")};
            Need(Regex.IsMatch(root.Origin,@"\Ahttps://(?=.{1,253}\z)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?\z"),"invalid canonical HTTPS origin.");
            Need(root.Expires>root.Issued && root.Expires-root.Issued<=366L*86400,"invalid root lifetime.");
            var keys=Object(data,"keys");Need(keys.Count>=3 && keys.Count<=8,"invalid root key count.");
            foreach(var pair in keys){var key=pair.Value as Dictionary<string,object>;Need(key!=null && KeyId(key)==pair.Key,"key identifier mismatch.");}
            var roles=Object(data,"roles");Fields(roles,"root catalog");var all=new HashSet<string>(StringComparer.Ordinal);
            foreach(var pair in roles) {
                var role=pair.Value as Dictionary<string,object>;Need(role!=null,"invalid signing role.");Fields(role,"keyids threshold");var ids=Array(role,"keyids",4);Need(ids.Length>0,"empty signing role.");
                foreach(var value in ids){var id=value as string;Need(id!=null && keys.ContainsKey(id) && all.Add(id),"signing roles must use distinct known keys.");}
                Number(role,"threshold",pair.Key=="root"?2:1,ids.Length);
            }
            Need(all.Count==keys.Count,"unused signing key.");return root;
        }
        internal static Root Roots(byte[] pinned,IEnumerable<byte[]> updates,long now,bool requireLive=true)
        {
            var root=ReadRoot(pinned);VerifyRole(pinned,root,"root",MaxRoot);int count=0;
            foreach(byte[] wire in updates) {
                Need(++count<=64,"too many root rotations.");var next=ReadRoot(wire);
                Need(next.Version==root.Version+1 && next.Origin==root.Origin && next.Issued>=root.Issued && next.Issued<=now,"invalid root rotation sequence or origin.");
                VerifyRole(wire,root,"root",MaxRoot);VerifyRole(wire,next,"root",MaxRoot);root=next;
            }
            Need(root.Issued<=now && (!requireLive || now<root.Expires),"root expired or not yet valid.");return root;
        }
        internal static long TargetVersion(byte[] wire)
        {byte[] payload;Envelope(wire,MaxMetadata,out payload);return Number(JsonBytes(payload,MaxMetadata/2),"root_version");}
        internal static Metadata Catalog(byte[] wire,byte[] catalog,Root root,long now,bool requireLive=true)
        {
            var data=VerifyRole(wire,root,"catalog",MaxMetadata);Fields(data,"type schema origin root_version version generated expires catalog_sha256 catalog_length revoked");
            var result=new Metadata {Version=Number(data,"version"),RootVersion=Number(data,"root_version"),Generated=Number(data,"generated",0),Expires=Number(data,"expires",0),
                Length=Number(data,"catalog_length",1,ModWorkshopClient.MaxCatalogBytes),Origin=Text(data,"origin"),CatalogHash=Text(data,"catalog_sha256"),WireHash=Hash(wire)};
            Need(Text(data,"type")=="tsfp-workshop-catalog" && Number(data,"schema")==1 && result.Origin==root.Origin && result.RootVersion==root.Version,"catalog identity mismatch.");
            Need(root.Issued<=result.Generated && result.Generated<=now && result.Expires>result.Generated && result.Expires-result.Generated<=86400 && result.Expires<=root.Expires &&
                (!requireLive || (now<result.Expires && now<root.Expires)),"catalog expired or invalid dates.");
            Need(catalog.LongLength==result.Length && Hash(catalog)==result.CatalogHash,"signed catalog bytes changed.");
            result.Revoked=Array(data,"revoked",4000).Select(v=>{var text=v as string;Need(IsHash(text),"invalid revoked hash.");return text;}).ToArray();
            Need(result.Revoked.SequenceEqual(result.Revoked.Distinct(StringComparer.Ordinal).OrderBy(s=>s,StringComparer.Ordinal)),"revocations must be sorted and unique.");
            var body=JsonBytes(catalog,ModWorkshopClient.MaxCatalogBytes);Need(Number(body,"schema")==1 && Number(body,"generated",0)==result.Generated && Number(body,"expires",0)==result.Expires,"catalog dates differ from signed metadata.");
            foreach(var item in Array(body,"releases",1000)){var entry=item as Dictionary<string,object>;Need(entry!=null && !result.Revoked.Contains(Text(entry,"sha256")),"catalog advertises a revoked package.");}
            return result;
        }
        internal static void Forward(Metadata previous,long observed,Metadata next,long now)
        {
            Need(now>=observed,"clock moved backwards.");Need(next.RootVersion>=previous.RootVersion && next.Version>=previous.Version,"catalog rollback rejected.");
            Need(next.Version!=previous.Version || next.WireHash==previous.WireHash,"catalog version reused for different metadata.");
            Need(previous.Revoked.All(v=>next.Revoked.Contains(v)),"catalog forgot an earlier revocation.");
        }
    }

    internal sealed class ModTrustStore : IDisposable
    {
        // Set only after the production offline ceremony and independent verification.
        internal const string DefaultRootBase64="";
        readonly string directory,statePath,markerPath;readonly FileStream operation;
        internal readonly byte[] Pinned;internal readonly List<byte[]> Updates=new List<byte[]>();
        internal ModTrust.Root Authority;internal ModTrust.Metadata Previous;internal long Observed;
        internal byte[] CatalogBytes,MetadataBytes;
        internal static byte[] ReadFile(string path,int maximum)
        {
            Core.RequireUnlinked(path,"Linked workshop trust file refused.");ModTrust.Need(new FileInfo(path).Length<=maximum,"saved trust file exceeds its budget.");
            using(var input=File.OpenRead(path))using(var output=new MemoryStream()) {
                byte[] buffer=new byte[16384];int count;while((count=input.Read(buffer,0,buffer.Length))>0){ModTrust.Need(output.Length+count<=maximum,"saved trust file exceeds its budget.");output.Write(buffer,0,count);}return output.ToArray();
            }
        }
        internal ModTrustStore(string root,string origin,long now)
        {
            ModTrust.Need(!String.IsNullOrEmpty(root),"an installation folder is required.");
            Pinned=DefaultRootBase64.Length>0?ModTrust.Base64(DefaultRootBase64,ModTrust.MaxRoot):ReadFile(Core.PathIn(root,"Support/Mods/trusted-root.json"),ModTrust.MaxRoot);
            Authority=ModTrust.Roots(Pinned,Updates,now,false);ModTrust.Need(Authority.Origin==origin,"service does not match the installed trust root.");
            directory=Core.PathIn(root,".launcher/mod-trust/"+ModTrust.Hash(Encoding.UTF8.GetBytes(origin)));Directory.CreateDirectory(directory);
            statePath=Core.PathIn(directory,"state.json");markerPath=Core.PathIn(directory,"initialized");
            operation=new FileStream(Core.PathIn(directory,"trust.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None);
            try {
                if(!File.Exists(statePath)){ModTrust.Need(!File.Exists(markerPath),"saved trust state is missing; repair it before using workshop mods.");return;}
                var state=ModTrust.JsonBytes(ReadFile(statePath,ModTrust.MaxState),ModTrust.MaxState);ModTrust.Fields(state,"schema pin updates metadata catalog observed");
                ModTrust.Need(ModTrust.Number(state,"schema")==1 && ModTrust.Text(state,"pin")==ModTrust.Hash(Pinned),"saved trust anchor changed.");
                foreach(var update in ModTrust.Array(state,"updates",64)){ModTrust.Need(update is string,"invalid saved root chain.");Updates.Add(ModTrust.Base64((string)update,ModTrust.MaxRoot));}
                Observed=ModTrust.Number(state,"observed",0);ModTrust.Need(now>=Observed,"clock moved backwards.");Authority=ModTrust.Roots(Pinned,Updates,now,false);
                MetadataBytes=ModTrust.Base64(ModTrust.Text(state,"metadata"),ModTrust.MaxMetadata);CatalogBytes=ModTrust.Base64(ModTrust.Text(state,"catalog"),ModWorkshopClient.MaxCatalogBytes);
                Previous=ModTrust.Catalog(MetadataBytes,CatalogBytes,Authority,now,false);ModTrust.Need(Observed>=Previous.Generated,"invalid saved observation time.");
            }catch{operation.Dispose();throw;}
        }
        internal void Commit(byte[] metadata,byte[] catalog,ModTrust.Metadata accepted,long now)
        {
            var state=new {schema=1,pin=ModTrust.Hash(Pinned),updates=Updates.Select(Convert.ToBase64String).ToArray(),metadata=Convert.ToBase64String(metadata),catalog=Convert.ToBase64String(catalog),observed=now};
            var serializer=Json.Serializer();serializer.MaxJsonLength=ModTrust.MaxState;
            byte[] bytes=ModTrust.Utf8.GetBytes(serializer.Serialize(state));ModTrust.Need(bytes.Length<=ModTrust.MaxState,"saved trust state exceeds its budget.");
            string partial=Core.PathIn(directory,Guid.NewGuid().ToString("N")+".partial");
            try {
                using(var output=new FileStream(partial,FileMode.CreateNew,FileAccess.Write,FileShare.None)){output.Write(bytes,0,bytes.Length);output.Flush(true);}
                // A first-commit crash can require repair, never silent bootstrap.
                if(!File.Exists(markerPath))using(var marker=new FileStream(markerPath,FileMode.CreateNew,FileAccess.Write,FileShare.None)){marker.WriteByte(1);marker.Flush(true);}
                if(File.Exists(statePath))Core.AtomicReplace(partial,statePath);else File.Move(partial,statePath);
                Previous=accepted;Observed=now;MetadataBytes=metadata;CatalogBytes=catalog;
            }finally{if(File.Exists(partial))File.Delete(partial);}
        }
        internal static void RequireAvailable(string root,string origin,string sha,DateTime? verificationTime=null)
        {RequireAvailable(root,origin,new[]{sha},verificationTime);}
        internal static void RequireAvailable(string root,string origin,IEnumerable<string> hashes,DateTime? verificationTime=null)
        {
            DateTime current=verificationTime??DateTime.UtcNow;long now=new DateTimeOffset(current).ToUnixTimeSeconds();
            using(var store=new ModTrustStore(root,origin,now)) {
                ModTrust.Need(store.Previous!=null,"refresh Mods before enabling a workshop package, or play without mods.");
                ModTrust.Catalog(store.MetadataBytes,store.CatalogBytes,store.Authority,now);
                var available=new HashSet<string>(ModWorkshopClient.Parse(ModTrust.Utf8.GetString(store.CatalogBytes),current).Select(r=>r.Sha256),StringComparer.Ordinal);
                ModTrust.Need(hashes.All(sha=>!store.Previous.Revoked.Contains(sha) && available.Contains(sha)),
                    "this package is unavailable or revoked. Disable it or play without mods.");
            }
        }
        public void Dispose(){operation.Dispose();}
    }
}
