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
        internal static bool CanonicalOrigin(string value)
        {return value!=null && value.Length<=255 && Regex.IsMatch(value,@"\Ahttps://(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?(?::(?:8443|10000))?\z");}
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
            Need(CanonicalOrigin(root.Origin),"invalid canonical HTTPS origin.");
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
        // Origin-bound release pin: custom authorities keep their explicit local
        // bootstrap root; a file can never replace the compiled production pin.
        internal const string DefaultOrigin="https://dans-mac-mini.tail7e8ab8.ts.net:8443";
        internal const string DefaultRootBase64="eyJwYXlsb2FkIjoiZXlKbGVIQnBjbVZ6SWpveE9ESTJOREV3TmpjNExDSnBjM04xWldRaU9qRTNPVFEzT0RneU56Z3NJbXRsZVhNaU9uc2lOMkkxWTJaa056TXdNVFV5TVdJM01qWTJOMkU1WmpFNFltSmpaVFk0TW1abU5XRmxPRFJtT1RFNVpEUTNNRGczTVdWa1lqUmxNRGt6TldGa05EWmtNeUk2ZXlKbGVIQnZibVZ1ZENJNklrRlJRVUlpTENKdGIyUjFiSFZ6SWpvaWN6RjJiMmc0T0ZJNFIzUklTbFUyZGl0RmVYVTFiRlJRYTNCb1VYUnRZVFp6U1VGeFF5OTBUVmhtVlZNd1VsTm1ZamhuTml0clVHSkNVMlUxY1Roa09HMUxVV1ZPVlV0cGIxb3pSMUoyVVV4bFp6WnFaM2w2VGsxaVdHUnBTQzgwTm05TFZGRkNPVXRuVG5vd1dUaDVZMGRvYW5CYVMxTTFTbUZNZVRoYWMxQndRemhVZVRGUVRqSXlRbE0zV0hvM1YxTkxkbGxwWmpCUlRVTXZabk1yTW1SRmMxaGFTV1ZpUjBzNWRVTjRhR1ZaTjJZdlRXUklVVkZOTVZCRlJIVlBNazFpYjBSeE5IcFZZVzFhSzBOSFlXaDJWa3R1VEhWa01tWldiVzFUY1VOTmJYRllWR0pIZFZZNVZqRXlaMFJqSzB4b1JYaHhUalJ6V25kUU5WZFplbFJtT1ZsUk5rdDNjVVJOUVVFMFVHZzJiVXMxYjNoaU1EQllhWFpOUVVSclprVTFTblEyTHpoNFZuTlNSRkZZYnpBMVRqYzJjVFo1UTNOVlVtNVNTa1ZqTVZjclYwMDFiVmM1TVRsR1VVeE1TVVZSUlhoYU16aHVNa2RNZEc1NGQzRXJUa3hGTWs1eFdtY3JkVEY1WWpsUWRFeHhaelp2U1VoTWNWTXhURkUzVjAweVIxRXdPRXAxWjFkamFuUkpkSFJuZUVOWFVVbHFNVkl4UjBJM01DOHdPVzVOV25wMmNFOXdOMEphVEU5WFJIbFhVVEZwT0ZodmJVRmlZa0Z0Y0ROb1VETkdTMUJCVVRWUk5HWm5TSFI1ZW1rM1lrdFBMMlZQYWxSNk9WUkZSMVZZV2tnM1NEUmljbGxOVWpGNk1IQnhUa3hYU1d0UFdXMW9kbTVVVTBZaUxDSnpZMmhsYldVaU9pSnljMkV0Y0d0amN6RXRjMmhoTWpVMkluMHNJamt3WVRVMk1UWTJabVUwWXprMk16YzRNV1UzTjJOaU9HUTFPRE5oTnpObVpUVm1ObVU1TUdSbFlXRmxNVFF5TURObU1EazNaR0l4TldKbE1UWmxOR1VpT25zaVpYaHdiMjVsYm5RaU9pSkJVVUZDSWl3aWJXOWtkV3gxY3lJNkltOUpSMDlFZWxobVR6VlRObGxpWVRWcVFXb3ZWa1UxZWpsRFlWRlZNeXN4WkZwWFUwTkVPVVJPYmt4VVptTlVaVlJQTlcxTFdFODFSMll2ZW04cmFXZFdSV3g0TVd0NlJrVnVUVWxrUnpsVkwwczRhRE40TTJSclF6TlBibTV0UW5KWWExSkJLMHRCV0ZBMFRUTkZTRVJvVTNaaldVNWhlRkZwVEVOM2RGWjRhV0oyTHl0NE5qWnhkSEZ2VEUxdlkyUndZalV2ZGtWdmJWSXpRa2RuTTA5Mk5URktOa2xNVlhoRWJHcGhOVlJyVjJsWk5EQnhkVFY1ZW1adVdVVkdRVVJTVUROelVHZEZSMDhyVlU1TFMwSnJVMUZvVVd3eFIxbFFTMFF3VDNwT1FXSnFRblpSZW5kcmFsSTJhVUZzWkd4Q2VFcG1jV2QxUVRKQlJreDRaRmQ0YUVrM2RtSlhibHBUYVRsQ2JHTlpkRFpzYzI5UVRVaExZMFY1V2psb1UxVkpkRzl4ZUVwS1FuVmhjMUpZWlM5MFZqbGFNM1Y0TlRJdlNsVlBkVTExUjNwdFIyUmpiV1ZXYjBwWU55OXFSRGwwYzJkdlJXaG1aVWw1UTNSWWRuQnNOemt6ZDBGc04wNHhNakJGSzJKWE1ub3lTVEIyWldFd1NGbFdRMHAzSzFsbWRGUkVjbmxYYTJNdlJUazNVRmx1UmtwR2FVOWlhMmx0Y201a05sZDFWbk13U1dwek1WTlJUSFJ6TjFwdE1pOXpSMjFaVjJSc09IRlVPVEJWVDBSc1JVdHZPVUV6ZFc1TFpsWXpPWGMxYVdGUVpWQlVRMU0yV0RCaFVuZFdSekJLUnk5WmVtRm5kR3BaUmk5VFVFeDBVVlpsWkVSU1duaDZNa3BvUjJrdklpd2ljMk5vWlcxbElqb2ljbk5oTFhCclkzTXhMWE5vWVRJMU5pSjlMQ0prWkRZNU5qaG1PV1l6TTJZd016WTBaalJoWVRJMFkyUXlOVE0zWW1VME9ERmtNVEkxTXpjME5ERmxORFZpTW1Ka05qQTJNalZtWmpjMk5EVm1NV1ppSWpwN0ltVjRjRzl1Wlc1MElqb2lRVkZCUWlJc0ltMXZaSFZzZFhNaU9pSnlVMVZDVUdGaVlYbHJUMEZKZFV0SE5URXdhbVpyV2pRdlF6WXpNbEpOVUhoUlEyWTFTblk1TkUxNE9WUTBjMEZwYWtnNUsydEtLekJUVWpaS1dtTlhiV1IzYzNOdGMwMUplbVV4ZDJKWU5ESnJTazFvWm1oSVVrMVRjemxQVUZwWk5YWm9NMDEzYWpaaFVqVTFWMGx2YlhkWlNIaHdXa1pUZDBWblJVNHlaV2x0ZERoTlpTOHpPRTVGWVdOR1NHcGxhVFpTTDNKVkwyOVRjMnd6UTNkaFEzcHhiWEUxWm1OM1ZrWkJUazVGTWtWaFR5dGxWMmRCTWtac01EZHZPRGRCTVVSVWIzSlRRbUpUYUhCVlFVaDBWbGRLTDFGUlVXWTRSWFowVUVKMlZGWmtlVWhtYkZWc04yeFFNM2t5VWxsTU1VMDBhRXR2V0ZKSk1YWmllbVY2WTNCNGJGVkNkVmhhYzFSQ0wwZENRVTVoZGpOelNEZEZiUzloVDJ3eFIybHBSVXBVZG1OV1NUTkplRWRoUkhsMFdFaDZjRFZrU21sNFVHcGlVU3RqUWtSaGRtSTFlamN3TjFwd1VYaDZNVTF2V0VsQ1FUQXJkVlpXTXpsbVZtWmxUMFZNYVdaclpEZHBZV0ZrYjI1YWMwMHJNVUprTUZnMlNVSkNlbXQxWVdNdmVrMWpNRE00Y0ROMGNYZDVjMHRXYVNzNWFXVjZkblZZZUhZNE9XMHpTVGxaWlROblJUVXJWRzVNUVZJM2IySk9NMlpxV0c1eVZUUlNZMkpZUjB4SGIwRkVZV05XVERaRldHMURVbEpzTDFaUE5GVnViVzB6V0VKWmFEbDZTbUowTlVkRlFuZFFUak5NVnpOemRtbDZVblUxVVZGT1MycExkV0pzWjBaNlpDSXNJbk5qYUdWdFpTSTZJbkp6WVMxd2EyTnpNUzF6YUdFeU5UWWlmWDBzSW05eWFXZHBiaUk2SW1oMGRIQnpPaTh2WkdGdWN5MXRZV010YldsdWFTNTBZV2xzTjJVNFlXSTRMblJ6TG01bGREbzRORFF6SWl3aWNtOXNaWE1pT25zaVkyRjBZV3h2WnlJNmV5SnJaWGxwWkhNaU9sc2laR1EyT1RZNFpqbG1Nek5tTURNMk5HWTBZV0V5TkdOa01qVXpOMkpsTkRneFpERXlOVE0zTkRReFpUUTFZakppWkRZd05qSTFabVkzTmpRMVpqRm1ZaUpkTENKMGFISmxjMmh2YkdRaU9qRjlMQ0p5YjI5MElqcDdJbXRsZVdsa2N5STZXeUk1TUdFMU5qRTJObVpsTkdNNU5qTTNPREZsTnpkallqaGtOVGd6WVRjelptVTFaalpsT1RCa1pXRmhaVEUwTWpBelpqQTVOMlJpTVRWaVpURTJaVFJsSWl3aU4ySTFZMlprTnpNd01UVXlNV0kzTWpZMk4yRTVaakU0WW1KalpUWTRNbVptTldGbE9EUm1PVEU1WkRRM01EZzNNV1ZrWWpSbE1Ea3pOV0ZrTkRaa015SmRMQ0owYUhKbGMyaHZiR1FpT2pKOWZTd2ljMk5vWlcxaElqb3hMQ0owZVhCbElqb2lkSE5tY0MxM2IzSnJjMmh2Y0MxeWIyOTBJaXdpZG1WeWMybHZiaUk2TVgwPSIsInNpZ25hdHVyZXMiOlt7ImtleWlkIjoiOTBhNTYxNjZmZTRjOTYzNzgxZTc3Y2I4ZDU4M2E3M2ZlNWY2ZTkwZGVhYWUxNDIwM2YwOTdkYjE1YmUxNmU0ZSIsInNpZ25hdHVyZSI6ImdRMXgrR3lob0JQalUxc2xrT3dCRHZTK1E0OXl0ZWFDZ3QzTmxadFlYaHZHK0Y3OUhuUGlLRE1aRjY3YzdaOU1EYnBVWkdaazZHM2tQcGNMaFdReXNBbGlZcUhXTzdKdXN1dHdzVzdOTGYweThpb3prZmlQZTRZNlVETisyNG9YeE1mQ0tXRm9wZW1Uamd2TzE4NzVyRmwzZjNQTjFMclZqU2x3MzJCZk90T2NYcy82OURMakJuWVpzRERvN2ttMjlVQitCd3NucFVEend2S3p3T2JJZ0tsTG1DNGdzYU82QUp6RzRueHpUcUM4WVJVek5YTkw0My9RbHFpc0VKRnlDNmw5aG1NT0owSDM2Tzlzd2trakZWdkxyMXVYK0l2ZG1ob3BUTUwySS9DYVV1dzB4blVRbkp5RWkzZks2QlFnUEd4Mm82dmo3a0dDMEpvM21jV2xuYm9ocHFYK2NhZWMzdkFhSGJUNCtqRFByeUFhSGFNWUpqWFN0RDRYU01BZVl2VzlQc1B3MGN0cVdYZXMrVzhHTWZjbUlTdkkzc04zR0dXazBwWUVxQ016bStFTml5YjhSUThvVW9CRGIrZUk5Y0xEYlRVMEZzSUhZSmhBRkw0c1EvUkFGTENBTUJmWmpmMytzcldWWm93TTYrbmx1cTVKTzhqUkh5YVhpSEhDIn0seyJrZXlpZCI6IjdiNWNmZDczMDE1MjFiNzI2NjdhOWYxOGJiY2U2ODJmZjVhZTg0ZjkxOWQ0NzA4NzFlZGI0ZTA5MzVhZDQ2ZDMiLCJzaWduYXR1cmUiOiJJS04zYU1SNU1seW1DdDlOOHgxYXNJNnZxbG4veGkwdzVoVldjWTRreHd6SHhwTUQrWW5POGh4OFZPMEZBRTVjVjFFZmFKSkhidEpHeEJTM2JGdS9SZUxva1RWazhQcW9sZ3pyUytTZnd0dWF3WXIyeUx0TExmN0ZURjZtNWkyZEh4cDIyNHRXSnBhbWdJQ3hYdDc5OG5JNklNSWdIQlY2TW85dElyNmpHekZEZHhjZ293QXgrVW9zTXY2ZHB6YlJ3K2c4c0pDdXZKQWFCTzZlenpma0VORjFob25pcWMvK2NhZnQ0bWUxcUFWMS8zWlNrN2s2QUY3Y1dvUXJqOHpjb0tCdVNpOTZ5Nm92WmdjekozK3lZbC96UnIxUDBNcEkzSU9hYVlpNG1HYVFHYW9tVlg1MHYyVVBmZUZhZkNIVHhmR1Q1M01IaVk4RVNiQmZmTlJscUtHVUJvS3pRanNOTzA4enFxVGszNkFnSzNIN2pMT2g3TFI1VU5XSWNtUW1mR2J5UjM5eENDbTFxUjlxT3F0UVRwV0ZkLzBOaXRXbGVjRW5vNmlvQXJ5Z0kzaW9XOE5CNXdKb2E4UmsvUVhMMjkrV3F0cEZnVkZSUEpYb3FDVVZHMmtBWElRbzZDekp4MHBBOFhYWWhIbjdXWEJZWkk0UUF5cHJJMzBUTC9XZiJ9XX0=";
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
            Pinned=origin==DefaultOrigin?ModTrust.Base64(DefaultRootBase64,ModTrust.MaxRoot):ReadFile(Core.PathIn(root,"Support/Mods/trusted-root.json"),ModTrust.MaxRoot);
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
