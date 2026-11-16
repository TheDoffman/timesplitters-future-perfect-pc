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
    public sealed class ModeratorTexturePreview
    {
        public readonly string Path,PayloadSha256,Format;
        public readonly int Width,Height,PreviewWidth,PreviewHeight;
        public readonly long Size;
        readonly byte[] pixels;
        internal ModeratorTexturePreview(Dictionary<string,object> data,ModeratorUpload review,string path)
        {
            ModTrust.Fields(data,"id sha256 path payload_sha256 size width height format preview_width preview_height rgba");
            ModTrust.Need(CreatorUpload.Text(data,"id",32)==review.Id && CreatorUpload.Text(data,"sha256",64)==review.Sha256 && CreatorUpload.Text(data,"path",180)==path,"texture preview identity mismatch.");
            ModPayload entry;ModTrust.Need(path.StartsWith("textures/",StringComparison.Ordinal) && path.EndsWith(".dds",StringComparison.Ordinal) && review.Package.Files.TryGetValue(path,out entry),"undeclared texture preview.");
            entry=review.Package.Files[path];Path=path;PayloadSha256=CreatorUpload.Text(data,"payload_sha256",64);Size=ModTrust.Number(data,"size",128,24*1024*1024);
            ModTrust.Need(Size==entry.Size && PayloadSha256==entry.Sha256,"texture preview differs from the manifest.");
            Width=(int)ModTrust.Number(data,"width",1,4096);Height=(int)ModTrust.Number(data,"height",1,4096);
            ModTrust.Need((Width&(Width-1))==0 && (Height&(Height-1))==0,"invalid texture dimensions.");
            Format=CreatorUpload.Text(data,"format",8);ModTrust.Need(new[]{"BC1","BC2","BC3","BGRA8"}.Contains(Format),"unsupported texture preview format.");
            int scale=Math.Max(1,Math.Max(Width,Height)/128);
            PreviewWidth=(int)ModTrust.Number(data,"preview_width",Math.Max(1,Width/scale),Math.Max(1,Width/scale));
            PreviewHeight=(int)ModTrust.Number(data,"preview_height",Math.Max(1,Height/scale),Math.Max(1,Height/scale));
            string encoded=ModTrust.Text(data,"rgba");ModTrust.Need(encoded.Length<=87384,"texture preview exceeds pixel budget.");pixels=Convert.FromBase64String(encoded);
            ModTrust.Need(pixels.Length==PreviewWidth*PreviewHeight*4 && Convert.ToBase64String(pixels)==encoded,"invalid texture preview pixels.");
        }
        internal byte[] DisplayPixels()
        {
            var bgra=new byte[pixels.Length];for(int i=0;i<pixels.Length;i+=4) {
                int p=i/4,background=((p%PreviewWidth/8+p/PreviewWidth/8)%2==0)?72:112,alpha=pixels[i+3];
                bgra[i]=(byte)((pixels[i+2]*alpha+background*(255-alpha)+127)/255);
                bgra[i+1]=(byte)((pixels[i+1]*alpha+background*(255-alpha)+127)/255);
                bgra[i+2]=(byte)((pixels[i]*alpha+background*(255-alpha)+127)/255);bgra[i+3]=255;
            }
            return bgra;
        }
        internal Image Image(out IDisposable lifetime)
        {
            return PreviewImage(DisplayPixels(),PreviewWidth,PreviewHeight,"ModeratorTextureImage",out lifetime);
        }
        internal static Image PreviewImage(byte[] bgra,int width,int height,string name,out IDisposable lifetime)
        {
#if MACOS || LINUX
            var bitmap=new Avalonia.Media.Imaging.WriteableBitmap(new PixelSize(width,height),new Vector(96,96),Avalonia.Platform.PixelFormat.Bgra8888,Avalonia.Platform.AlphaFormat.Opaque);
            using(var frame=bitmap.Lock())for(int y=0;y<height;y++)System.Runtime.InteropServices.Marshal.Copy(bgra,y*width*4,IntPtr.Add(frame.Address,y*frame.RowBytes),width*4);
            lifetime=bitmap;
#else
            var bitmap=System.Windows.Media.Imaging.BitmapSource.Create(width,height,96,96,System.Windows.Media.PixelFormats.Bgra32,null,bgra,width*4);bitmap.Freeze();lifetime=null;
#endif
            return new Image {Name=name,Source=bitmap,Width=width*2,Height=height*2,Margin=new Thickness(0,8,0,8)};
        }
    }
    public sealed class ModeratorAudioPreview
    {
        public readonly string Path,PayloadSha256;
        public readonly long Size,Frames,Offset,NextOffset;
        public readonly int Rate,Channels;
        readonly byte[] pcm;
        internal ModeratorAudioPreview(Dictionary<string,object> data,ModeratorUpload review,string path,long offset)
        {
            ModTrust.Fields(data,"id sha256 path payload_sha256 size rate channels frames offset next_offset pcm");
            ModTrust.Need(CreatorUpload.Text(data,"id",32)==review.Id && CreatorUpload.Text(data,"sha256",64)==review.Sha256 && CreatorUpload.Text(data,"path",180)==path,"audio preview identity mismatch.");
            ModPayload entry;ModTrust.Need(path.StartsWith("audio/",StringComparison.Ordinal) && path.EndsWith(".wav",StringComparison.Ordinal) && review.Package.Files.TryGetValue(path,out entry),"undeclared audio preview.");
            entry=review.Package.Files[path];Path=path;Size=ModTrust.Number(data,"size",44,64*1024*1024);PayloadSha256=CreatorUpload.Text(data,"payload_sha256",64);
            ModTrust.Need(Size==entry.Size && PayloadSha256==entry.Sha256,"audio preview differs from the manifest.");
            Rate=(int)ModTrust.Number(data,"rate",8000,48000);Channels=(int)ModTrust.Number(data,"channels",1,2);
            Frames=ModTrust.Number(data,"frames",1,Rate*600L);Offset=ModTrust.Number(data,"offset",offset,offset);
            ModTrust.Need(Offset>=0 && Offset<Frames && Frames*Channels*2<=Size-44,"invalid audio preview source bounds.");
            long end=Math.Min(Frames,Offset+Rate*2L);NextOffset=ModTrust.Number(data,"next_offset",end,end);
            string encoded=ModTrust.Text(data,"pcm");ModTrust.Need(encoded.Length<=512000,"audio preview exceeds PCM budget.");pcm=Convert.FromBase64String(encoded);
            ModTrust.Need(pcm.LongLength==(NextOffset-Offset)*Channels*2 && Convert.ToBase64String(pcm)==encoded,"invalid audio preview PCM.");
        }
        internal byte[] WaveformPixels()
        {
            const int width=256,lane=64;int height=lane*Channels;var bgra=new byte[width*height*4];
            for(int i=0;i<bgra.Length;i+=4){bgra[i]=38;bgra[i+1]=24;bgra[i+2]=11;bgra[i+3]=255;}
            int frames=pcm.Length/(2*Channels);
            for(int channel=0;channel<Channels;channel++)for(int x=0;x<width;x++) {
                int start=x*frames/width,end=Math.Max(start+1,(x+1)*frames/width),low=32767,high=-32768;
                for(int frame=start;frame<end;frame++){int at=(frame*Channels+channel)*2;int sample=(short)(pcm[at]|pcm[at+1]<<8);low=Math.Min(low,sample);high=Math.Max(high,sample);}
                int top=channel*lane+(32767-high)*63/65535,bottom=channel*lane+(32767-low)*63/65535;
                for(int y=top;y<=bottom;y++){int at=(y*width+x)*4;bgra[at]=250;bgra[at+1]=217;bgra[at+2]=167;}
            }
            return bgra;
        }
        internal byte[] PlaybackPcm(){return (byte[])pcm.Clone();}
        internal Image Image(out IDisposable lifetime)
        {return ModeratorTexturePreview.PreviewImage(WaveformPixels(),256,64*Channels,"ModeratorAudioWaveform",out lifetime);}
    }
    public sealed class WorkshopReport
    {
        public readonly string Id,Sha256,ModId,Version,Reporter,Reason,Description,State,Resolution;
        public readonly long Cursor,Created,Updated;
        internal WorkshopReport(Dictionary<string,object> data)
        {
            ModTrust.Fields(data,"cursor id sha256 mod_id version reporter reason description created updated state resolution");
            Id=CreatorUpload.Text(data,"id",32);Sha256=CreatorUpload.Text(data,"sha256",64);ModId=CreatorUpload.Text(data,"mod_id",80);Version=CreatorUpload.Text(data,"version",32);Reporter=CreatorUpload.Text(data,"reporter",32);
            ModTrust.Need(Regex.IsMatch(Id,@"\A[0-9a-f]{32}\z") && Regex.IsMatch(Sha256,@"\A[0-9a-f]{64}\z") && Regex.IsMatch(ModId,@"\A[a-z][a-z0-9]*(?:-[a-z0-9]+)*\z") && Regex.IsMatch(Version,@"\A(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\z") && Regex.IsMatch(Reporter,@"\A[a-z][a-z0-9-]{2,31}\z"),"invalid report identity.");
            Cursor=ModTrust.Number(data,"cursor",1,999999999);Created=ModTrust.Number(data,"created",0,253402300799);Updated=ModTrust.Number(data,"updated",Created,253402300799);
            Reason=CreatorUpload.Text(data,"reason",16);Description=CreatorUpload.Text(data,"description",1000);State=CreatorUpload.Text(data,"state",16);Resolution=CreatorUpload.Text(data,"resolution",1800);
            ModTrust.Need(new[]{"malware","broken","policy","other"}.Contains(Reason) && new[]{"open","closed"}.Contains(State) && Description.Trim().Length>0 && (State!="closed" || Resolution.Trim().Length>0),"invalid report content.");
        }
    }
    // Passwords stay in memory; explicit opt-in stores only a bounded OS-vault
    // session. No cookies, redirects, plaintext files or automatic upload retries.
    public sealed class CreatorInvitation
    {
        public readonly string Username,Secret,Service;public readonly long Expires;
        internal CreatorInvitation(Dictionary<string,object> data,string expected,string service,long now)
        {
            ModTrust.Fields(data,"username invitation expires");Username=CreatorUpload.Text(data,"username",32);
            Secret=CreatorUpload.Text(data,"invitation",43);Expires=ModTrust.Number(data,"expires",now+1,now+86400);Service=service;
            ModTrust.Need(Username==expected && Regex.IsMatch(Secret,@"\A[A-Za-z0-9_-]{43}\z"),"invalid creator invitation.");
        }
    }
    public sealed class ModCreatorClient : IDisposable
    {
        readonly string root,origin;readonly HttpClient http;readonly Func<DateTime> clock;
        readonly SemaphoreSlim gate=new SemaphoreSlim(1,1);readonly IModSessionVault vault;string token,pinHash,vaultBinding;long expires;
        public string VaultNotice {get;private set;}
        public string Username {get;private set;}
        public string Role {get;private set;}
        public string LastTicket {get;private set;}
        public bool SignedIn {get{return token!=null && Now<expires;}}
        public bool CanModerate {get{return SignedIn && (Role=="moderator" || Role=="admin");}}
        public bool CanInviteCreators {get{return SignedIn && Role=="admin";}}
        long Now {get{return new DateTimeOffset(clock()).ToUnixTimeSeconds();}}
        public ModCreatorClient(string root):this(root,ModWorkshopClient.Service(root),new HttpClientHandler {AllowAutoRedirect=false,UseCookies=false,UseDefaultCredentials=false},()=>DateTime.UtcNow,new ModSessionVault()){}
        internal ModCreatorClient(string root,string service,HttpMessageHandler handler,Func<DateTime> clock,IModSessionVault vault=null)
        {this.root=root;origin=ModWorkshopClient.ServiceUri(service).GetLeftPart(UriPartial.Authority);this.clock=clock;this.vault=vault;http=new HttpClient(handler,true);http.Timeout=TimeSpan.FromMinutes(15);}
        void Pin()
        {
            try {
                Core.Require(ModWorkshopClient.ServiceUri(ModWorkshopClient.Service(root)).GetLeftPart(UriPartial.Authority)==origin,"The workshop address changed. Restart the launcher before signing in again.");
                using(var store=new ModTrustStore(root,origin,Now)) {
                    ModTrust.Roots(store.Pinned,store.Updates,Now);string current=ModTrust.Hash(store.Pinned);
                    Core.Require(pinHash==null || pinHash==current,"The workshop trust root changed. Restart the launcher before signing in again.");
                    pinHash=current;vaultBinding=ModTrust.Hash(Encoding.UTF8.GetBytes(origin+"\0"+pinHash));
                }
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
                    if(code==401){Forget();await ClearSaved().ConfigureAwait(false);}
                    Core.Require(code==expected,code==401?"Sign in again to use the creator workshop.":code==403?"This workshop action is not allowed for this account.":code==409?"The request conflicts with an existing item or changed status. Refresh its workshop status before retrying.":code==429?"The workshop request limit was reached. Wait before trying again.":"The workshop could not complete this request (HTTP "+code+"). Refresh workshop status before retrying.");
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
        public async Task SignIn(string username,string password,CancellationToken cancel,bool remember=false)
        {await Login("v1/sessions",new {username=username,password=password},200,cancel,remember).ConfigureAwait(false);}
        public async Task Redeem(string invitation,string password,CancellationToken cancel,bool remember=false)
        {await Login("v1/accounts/redeem",new {invitation=invitation,password=password},201,cancel,remember).ConfigureAwait(false);}
        async Task ClearSaved()
        {
            if(vault==null || vaultBinding==null)return;
            try{await Task.Run(()=>vault.Delete(vaultBinding)).ConfigureAwait(false);}
            catch{VaultNotice="The saved sign-in could not be removed from the system credential store. Unlock it and choose Forget saved sign-in.";}
        }
        async Task Login(string path,object credentials,int expected,CancellationToken cancel,bool remember)
        {
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Forget();VaultNotice=null;var response=await Request(HttpMethod.Post,path,JsonBody(credentials),expected,false,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"token expires account");string secret=CreatorUpload.Text(response,"token",43);long expiry=ModTrust.Number(response,"expires",Now+1,Now+43200);
                ModTrust.Need(Regex.IsMatch(secret,@"\A[A-Za-z0-9_-]{43}\z"),"invalid creator session.");
                var account=ModTrust.Object(response,"account");ModTrust.Fields(account,"username role");string name=CreatorUpload.Text(account,"username",32),role=CreatorUpload.Text(account,"role",16);
                ModTrust.Need(Regex.IsMatch(name,@"\A[a-z][a-z0-9-]{2,31}\z") && new[]{"creator","moderator","admin"}.Contains(role),"invalid creator account.");
                token=secret;expires=expiry;Username=name;Role=role;LastTicket=null;
                if(remember) {
                    try {Core.Require(vault!=null,"Credential store unavailable.");byte[] saved=Encoding.UTF8.GetBytes(Json.Encode(new {schema=1,origin=origin,pin=pinHash,token=token,expires=expires,username=Username}));await Task.Run(()=>vault.Write(vaultBinding,saved)).ConfigureAwait(false);}
                    catch{VaultNotice="This sign-in was not saved. Unlock the system credential store to replace or forget any earlier saved sign-in.";}
                }else await ClearSaved().ConfigureAwait(false);
            }finally{gate.Release();}
        }
        public async Task<bool> Resume(CancellationToken cancel)
        {
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Forget();VaultNotice=null;Pin();Core.Require(vault!=null,"The system credential store is unavailable.");
                byte[] saved=await Task.Run(()=>vault.Read(vaultBinding),cancel).ConfigureAwait(false);if(saved==null)return false;
                bool invalid=false;
                try {
                    Core.Require(saved.Length>0 && saved.Length<=2048,"Invalid saved sign-in.");var data=ModJson.Read(new UTF8Encoding(false,true).GetString(saved),2048);
                    ModTrust.Fields(data,"schema origin pin token expires username");
                    ModTrust.Need(ModTrust.Number(data,"schema",1,1)==1 && ModTrust.Text(data,"origin")==origin && ModTrust.Text(data,"pin")==pinHash,"saved sign-in authority differs.");
                    string secret=CreatorUpload.Text(data,"token",43),name=CreatorUpload.Text(data,"username",32);
                    ModTrust.Need(Regex.IsMatch(secret,@"\A[A-Za-z0-9_-]{43}\z") && Regex.IsMatch(name,@"\A[a-z][a-z0-9-]{2,31}\z"),"invalid saved sign-in.");
                    expires=ModTrust.Number(data,"expires",Now+1,Now+43200);token=secret;Username=name;
                }catch{Forget();invalid=true;}
                if(invalid){await ClearSaved().ConfigureAwait(false);throw new InvalidOperationException("The saved sign-in expired or is invalid. Sign in again.");}
                Dictionary<string,object> account;
                try{account=await Request(HttpMethod.Get,"v1/account",null,200,true,cancel).ConfigureAwait(false);}
                catch{Forget();throw;}
                bool invalidAccount=false;
                try {
                    ModTrust.Fields(account,"username role");
                    string name=CreatorUpload.Text(account,"username",32),role=CreatorUpload.Text(account,"role",16);
                    ModTrust.Need(name==Username && new[]{"creator","moderator","admin"}.Contains(role),"saved sign-in account differs.");Role=role;LastTicket=null;
                }catch{Forget();invalidAccount=true;}
                if(invalidAccount){await ClearSaved().ConfigureAwait(false);throw new InvalidOperationException("The saved sign-in account is invalid. Sign in again.");}
                return true;
            }finally{gate.Release();}
        }
        public async Task ForgetSaved(CancellationToken cancel)
        {
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try{VaultNotice=null;if(vaultBinding==null){using(var store=new ModTrustStore(root,origin,Now))vaultBinding=ModTrust.Hash(Encoding.UTF8.GetBytes(origin+"\0"+ModTrust.Hash(store.Pinned)));}await ClearSaved().ConfigureAwait(false);Core.Require(VaultNotice==null,"The system credential store is locked or unavailable. Unlock it before forgetting the saved sign-in.");}
            finally{gate.Release();}
        }
        public async Task SignOut(CancellationToken cancel)
        {
            bool locked=false;Exception failure=null;VaultNotice=null;
            try {await gate.WaitAsync(cancel).ConfigureAwait(false);locked=true;if(SignedIn)await Request(HttpMethod.Delete,"v1/sessions/current",null,204,true,cancel).ConfigureAwait(false);}
            catch(Exception error){failure=error;}
            try{Forget();await ClearSaved().ConfigureAwait(false);}finally{if(locked)gate.Release();}
            if(failure!=null)System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure).Throw();
        }
        static void InviteName(string name)
        {Core.Require(name!=null && Regex.IsMatch(name,@"\A[a-z][a-z0-9-]{2,31}\z"),"Use a lowercase creator name of 3–32 letters, digits or hyphens.");}
        public async Task<CreatorInvitation> InviteCreator(string name,CancellationToken cancel)
        {
            InviteName(name);await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanInviteCreators,"Sign in with the workshop owner account to invite creators.");
                return new CreatorInvitation(await Request(HttpMethod.Post,"v1/creator-invitations",JsonBody(new {username=name}),201,true,cancel).ConfigureAwait(false),name,origin,Now);
            }finally{gate.Release();}
        }
        public async Task CancelCreatorInvitation(string name,CancellationToken cancel)
        {
            InviteName(name);await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanInviteCreators,"Sign in with the workshop owner account to cancel creator invitations.");
                await Request(HttpMethod.Delete,"v1/creator-invitations/"+name,null,204,true,cancel).ConfigureAwait(false);
            }finally{gate.Release();}
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
        public async Task<ModeratorTexturePreview> Texture(ModeratorUpload review,string path,CancellationToken cancel)
        {
            ModPayload entry;Core.Require(review!=null && path!=null && path.StartsWith("textures/",StringComparison.Ordinal) && path.EndsWith(".dds",StringComparison.Ordinal) && review.Package.Files.TryGetValue(path,out entry) && entry.Size<=24*1024*1024,"Select a declared DDS texture.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to inspect textures.");
                return new ModeratorTexturePreview(await Request(HttpMethod.Post,"v1/moderation/"+review.Id+"/texture",JsonBody(new {sha256=review.Sha256,path=path}),200,true,cancel).ConfigureAwait(false),review,path);
            }finally{gate.Release();}
        }
        public async Task<ModeratorAudioPreview> Audio(ModeratorUpload review,string path,long offset,CancellationToken cancel)
        {
            ModPayload entry;Core.Require(review!=null && path!=null && path.StartsWith("audio/",StringComparison.Ordinal) && path.EndsWith(".wav",StringComparison.Ordinal) && review.Package.Files.TryGetValue(path,out entry) && entry.Size<=64*1024*1024 && offset>=0 && offset<48000*600L,"Select a declared PCM WAV and valid frame offset.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to inspect audio.");
                return new ModeratorAudioPreview(await Request(HttpMethod.Post,"v1/moderation/"+review.Id+"/audio",JsonBody(new {sha256=review.Sha256,path=path,offset=offset}),200,true,cancel).ConfigureAwait(false),review,path,offset);
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
        public async Task<WorkshopReport> Report(string sha,string reason,string description,CancellationToken cancel)
        {
            Core.Require(sha!=null && Regex.IsMatch(sha,@"\A[0-9a-f]{64}\z") && new[]{"malware","broken","policy","other"}.Contains(reason),"Select a release and report category.");
            description=(description??"").Trim();Core.Require(description.Length>0 && description.Length<=1000 && !description.Any(Char.IsControl),"Enter a report of 1 to 1000 characters without control characters.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                var result=new WorkshopReport(await Request(HttpMethod.Post,"v1/reports",JsonBody(new {sha256=sha,reason=reason,description=description}),201,true,cancel).ConfigureAwait(false));
                ModTrust.Need(result.Sha256==sha && result.Reporter==Username && result.Reason==reason && result.Description==description && result.State=="open" && result.Resolution=="","report receipt differs from the submitted release. Refresh reports before retrying.");return result;
            }finally{gate.Release();}
        }
        public async Task<WorkshopReport[]> Reports(long after,bool moderation,CancellationToken cancel,bool history=false)
        {
            Core.Require(after>=0 && after<=999999999 && (!history || moderation),"Invalid report cursor or history scope.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(!moderation || CanModerate,"Sign in with a moderator account to review reports.");
                var response=await Request(HttpMethod.Get,(moderation?(history?"v1/moderation/report-history":"v1/moderation/reports"):"v1/reports")+"?after="+after.ToString(System.Globalization.CultureInfo.InvariantCulture),null,200,true,cancel).ConfigureAwait(false);
                ModTrust.Fields(response,"reports");var result=new List<WorkshopReport>();var ids=new HashSet<string>();long previous=after;
                foreach(var value in ModTrust.Array(response,"reports",20)) {
                    var data=value as Dictionary<string,object>;ModTrust.Need(data!=null,"invalid report history.");var row=new WorkshopReport(data);
                    ModTrust.Need(row.Cursor>previous && ids.Add(row.Id) && (moderation?(history || row.State=="open"):row.Reporter==Username),"unexpected report ownership/state/order.");previous=row.Cursor;result.Add(row);
                }return result.ToArray();
            }finally{gate.Release();}
        }
        public async Task<WorkshopReport> ResolveReport(WorkshopReport report,string action,string note,CancellationToken cancel)
        {
            Core.Require(report!=null && (action=="close" || action=="reopen"),"Invalid report decision.");note=(note??"").Trim();Core.Require(note.Length>0 && note.Length<=1800 && !note.Any(Char.IsControl),"Enter a resolution note of 1 to 1800 characters.");
            await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to resolve reports.");
                var result=new WorkshopReport(await Request(HttpMethod.Post,"v1/moderation/reports/"+report.Id,JsonBody(new {action=action,note=note}),200,true,cancel).ConfigureAwait(false));
                ModTrust.Need(result.Id==report.Id && result.Cursor==report.Cursor && result.Sha256==report.Sha256 && result.Reporter==report.Reporter && result.ModId==report.ModId && result.Version==report.Version && result.Description==report.Description && result.Reason==report.Reason && result.Created==report.Created && result.State==(action=="close"?"closed":"open") && result.Resolution==note,"report resolution differs from the reviewed record. Refresh reports before retrying.");return result;
            }finally{gate.Release();}
        }
        public async Task<ModeratorUpload> ReportedRelease(WorkshopReport report,CancellationToken cancel)
        {
            Core.Require(report!=null,"Select a report.");await gate.WaitAsync(cancel).ConfigureAwait(false);
            try {
                Core.Require(CanModerate,"Sign in with a moderator account to inspect reported releases.");
                var result=new ModeratorUpload(await Request(HttpMethod.Get,"v1/moderation/by-digest/"+report.Sha256,null,200,true,cancel).ConfigureAwait(false));
                ModTrust.Need(result.Sha256==report.Sha256 && result.ModId==report.ModId && result.Version==report.Version,"reported release differs from the selected report.");return result;
            }finally{gate.Release();}
        }
        public void Dispose(){Forget();http.Dispose();}
    }

    public sealed class ModCreatorPage : IDisposable
    {
        public readonly StackPanel View=new StackPanel();
        readonly TextBox username=new TextBox {Name="CreatorUsername",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly CheckBox remember=new CheckBox {Name="RememberCreatorSignIn",Content="Remember this sign-in on this computer",Margin=new Thickness(0,6,0,8)};
        readonly TextBox inviteName=new TextBox {Name="InviteCreatorUsername",MaxLength=32,MinWidth=260,Margin=new Thickness(0,6,0,8)};
        CreatorInvitation issuedInvitation;bool showInvitation;string cancelInvitationName;
        readonly TextBox package=new TextBox {Name="CreatorPackagePath",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly TextBox reviewNote=new TextBox {Name="ModeratorNote",MaxLength=1800,MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly ComboBox sourceFiles=new ComboBox {Name="ModeratorSourceFile",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly ComboBox textureFiles=new ComboBox {Name="ModeratorTextureFile",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        ModeratorTexturePreview texturePreview;
        IDisposable textureBitmap;
        readonly ComboBox audioFiles=new ComboBox {Name="ModeratorAudioFile",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        ModeratorAudioPreview audioPreview;IDisposable audioBitmap;
        readonly ComboBox reportReason=new ComboBox {Name="PlayerReportReason",MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly TextBox reportDescription=new TextBox {Name="PlayerReportDescription",MaxLength=1000,MinWidth=260,Margin=new Thickness(0,6,0,8)};
        readonly TextBox reportResolution=new TextBox {Name="ModeratorReportResolution",MaxLength=1800,MinWidth=260,Margin=new Thickness(0,6,0,8)};
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
        readonly string root;readonly bool preview;ModCreatorClient client;CancellationTokenSource stop;bool disposed,listening;
        readonly ModPreviewAudio audioPlayer;
        CreatorPackagePlan reviewed;CreatorUpload[] uploads=new CreatorUpload[0];long cursor;
        ModeratorUpload[] reviews=new ModeratorUpload[0];ModeratorUpload selectedReview;string decision,decisionNote;
        ModeratorEvidence scanEvidence;
        ModeratorSourcePage sourcePage;
        string reportSha,reportModId,reportVersion,reportDraftReason,reportDraftText;WorkshopReport[] reports=new WorkshopReport[0];
        WorkshopReport[] moderatorReports=new WorkshopReport[0];WorkshopReport selectedReport;bool reportHistory;string reportDecision,reportDecisionNote;
        public string StatusText {get{return status.Text;}}
        public ModCreatorPage(string root,bool preview=false):this(root,preview,null){}
        internal ModCreatorPage(string root,bool preview,ModCreatorClient client):this(root,preview,client,new ModPreviewAudio()){}
        internal ModCreatorPage(string root,bool preview,ModCreatorClient client,ModPreviewAudio audioPlayer){this.root=root;this.preview=preview;this.client=client;this.audioPlayer=audioPlayer;AutomationProperties.SetName(username,"Workshop username");AutomationProperties.SetName(password,"Workshop password");AutomationProperties.SetName(invitation,"Workshop invitation");AutomationProperties.SetName(package,"Mod ZIP file path");AutomationProperties.SetName(reportDescription,"Describe the problem with this release");foreach(string reason in new[]{"malware","broken","policy","other"})reportReason.Items.Add(new ComboBoxItem {Tag=reason,Content=reason});reportReason.SelectedIndex=3;status.Margin=new Thickness(0,0,0,12);Render();}
        public void BeginReport(string id,string version,string sha)
        {
            if(stop!=null){status.Text="Finish or cancel the current request before choosing another release.";return;}
            Core.Require(id!=null && id.Length<=80 && Regex.IsMatch(id,@"\A[a-z][a-z0-9]*(?:-[a-z0-9]+)*\z") && version!=null && version.Length<=32 && Regex.IsMatch(version,@"\A(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\z") && sha!=null && Regex.IsMatch(sha,@"\A[0-9a-f]{64}\z"),"Invalid report target.");
            reportSha=sha;reportModId=id;reportVersion=version;ClearReports();status.Text="Sign in to report this exact mod release. Reports are private to you and moderators.";Render();
        }
        void ClearReports(){reports=new WorkshopReport[0];reportDraftReason=null;reportDraftText=null;reportDescription.Text="";reportHistory=false;ClearModeratorReports();}
        void ClearModeratorReports(){moderatorReports=new WorkshopReport[0];selectedReport=null;reportDecision=null;reportDecisionNote=null;reportResolution.Text="";}
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
            Detach(username);Detach(remember);Detach(password);Detach(invitation);Detach(inviteName);Detach(package);Detach(reviewNote);Detach(sourceFiles);Detach(textureFiles);Detach(audioFiles);Detach(reportReason);Detach(reportDescription);Detach(reportResolution);View.Children.Clear();if(textureBitmap!=null){textureBitmap.Dispose();textureBitmap=null;}if(audioBitmap!=null){audioBitmap.Dispose();audioBitmap=null;}bool authenticated=SignedIn;if(!authenticated){ClearReviews();ClearReports();ClearInvitation();}
            View.Children.Add(status);var cancel=Button(View,"CancelCreatorOperation","Cancel request",()=>{if(stop!=null)stop.Cancel();});cancel.IsEnabled=stop!=null;Ui.Show(cancel,stop!=null);
            var account=PageTheme.Section(View,"Creator account",authenticated?"Signed in as "+client.Username+" ("+client.Role+")":"Use your workshop account or redeem an operator invitation.");
            if(!authenticated) {
                account.Children.Add(PageTheme.Text("Username",13));account.Children.Add(username);
                account.Children.Add(PageTheme.Text("Password (15 or more characters for a new account)",13));account.Children.Add(password);
                account.Children.Add(remember);remember.IsEnabled=stop==null;
                Button(account,"CreatorSignIn","Sign in",async()=>await Authenticate(false));
                account.Children.Add(PageTheme.Text("Invitation (for a new account or account recovery)",13));account.Children.Add(invitation);
                Button(account,"CreatorRedeem","Redeem invitation",async()=>await Authenticate(true));
                Button(account,"ResumeCreatorSignIn","Use saved sign-in",async()=>await RestoreSaved());
                account.Children.Add(PageTheme.Text("Remembering saves only your session in the system credential store, for up to 12 hours. Your password is never saved.",12,"#9EB4C8"));
            }else Button(account,"CreatorSignOut","Sign out",async()=>{reviewed=null;uploads=new CreatorUpload[0];ClearReviews();ClearReports();await Run(token=>Client.SignOut(token),"Signed out. Local game files were preserved.",35);});
            Button(account,"ForgetCreatorSignIn","Forget saved sign-in",async()=>await Run(token=>Client.ForgetSaved(token),"Saved sign-in forgotten.",40));
            username.IsEnabled=password.IsEnabled=invitation.IsEnabled=stop==null;
            if(client!=null && client.CanInviteCreators)RenderCreatorInvitations();else ClearInvitation();
            RenderReports(authenticated);
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
            if(client!=null && client.CanModerate){RenderModeratorReports();RenderModeration();}
        }
        void ClearReviews(){reviews=new ModeratorUpload[0];selectedReview=null;scanEvidence=null;sourcePage=null;texturePreview=null;textureFiles.ItemsSource=null;audioPreview=null;audioFiles.ItemsSource=null;decision=null;decisionNote=null;reviewNote.Text="";sourceFiles.ItemsSource=null;}
        void ClearInvitation(){issuedInvitation=null;showInvitation=false;cancelInvitationName=null;}
        void RenderCreatorInvitations()
        {
            var panel=PageTheme.Section(View,"Invite a creator","Give someone an account to submit their own mods. These invitations never grant moderation or administrator access.");
            panel.Children.Add(PageTheme.Text("Creator username · 3–32 lowercase letters, digits or hyphens",13));panel.Children.Add(inviteName);inviteName.IsEnabled=stop==null;
            if(cancelInvitationName!=null) {
                string target=cancelInvitationName;
                panel.Children.Add(PageTheme.Text("Cancel the pending creator invitation for "+target+"? Existing accounts, privileged invitations and account recovery are preserved.",13,"#FFDAA6"));
                Button(panel,"ConfirmCancelCreatorInvitation","Cancel this invitation",async()=>{ClearInvitation();await Run(token=>Client.CancelCreatorInvitation(target,token),"Pending creator invitation cancelled.",35);});
                Button(panel,"BackCreatorInvitation","Back",()=>{cancelInvitationName=null;Render();});return;
            }
            Button(panel,"CreateCreatorInvitation","Create invitation",async()=>{string name=inviteName.Text??"";ClearInvitation();await Run(async token=>{issuedInvitation=await Client.InviteCreator(name,token);},"Creator invitation ready. Share it privately before it expires.",35);});
            Button(panel,"ReviewCancelCreatorInvitation","Review cancellation",()=>{
                string name=inviteName.Text??"";
                if(!Regex.IsMatch(name,@"\A[a-z][a-z0-9-]{2,31}\z")){status.Text="Enter a valid lowercase creator username first.";return;}
                cancelInvitationName=name;showInvitation=false;Render();
            });
            panel.Children.Add(PageTheme.Text("Invitations expire after 24 hours and can be redeemed once. If delivery fails, cancel the pending invitation before creating a replacement.",12,"#9EB4C8"));
            if(issuedInvitation==null)return;
            panel.Children.Add(PageTheme.Text(issuedInvitation.Username+" · "+issuedInvitation.Service+" · expires "+DateTimeOffset.FromUnixTimeSeconds(issuedInvitation.Expires).LocalDateTime.ToString("g"),12,"#A7D9FA"));
            if(!showInvitation)Button(panel,"RevealCreatorInvitation","Reveal invitation",()=>{showInvitation=true;Render();});
            else {
                panel.Children.Add(new TextBox{Name="CreatedCreatorInvitationSecret",Text=issuedInvitation.Secret,IsReadOnly=true,Margin=new Thickness(0,6,0,8)});
                panel.Children.Add(PageTheme.Text("Copy this invitation only to its intended creator. It is held in memory and is not saved to a file or copied to your clipboard automatically.",12,"#FFDAA6"));
                Button(panel,"HideCreatorInvitation","Hide invitation",()=>{showInvitation=false;Render();});
            }
        }
        void RenderReports(bool authenticated)
        {
            var panel=PageTheme.Section(View,"Report a mod","Choose Report release in Browse to flag a specific version. Sign-in is required; reports are private to your account and moderators.");
            if(reportSha!=null) {
                panel.Children.Add(PageTheme.Text(reportModId+" @ "+reportVersion+" · SHA-256: "+reportSha,12,"#A7D9FA"));
                if(reportDraftReason==null) {
                    panel.Children.Add(PageTheme.Text("Category",13));panel.Children.Add(reportReason);reportReason.IsEnabled=authenticated && stop==null;
                    panel.Children.Add(PageTheme.Text("What happened? Up to 1000 characters. Omit passwords and personal information.",13));panel.Children.Add(reportDescription);reportDescription.IsEnabled=authenticated && stop==null;
                    Button(panel,"ReviewPlayerReport","Review report",()=>{string message=(reportDescription.Text??"").Trim();var category=reportReason.SelectedItem as ComboBoxItem;if(category==null || message.Length==0 || message.Length>1000 || message.Any(Char.IsControl)){status.Text="Choose a category and enter a report of 1 to 1000 characters without control characters.";return;}reportDraftReason=(string)category.Tag;reportDraftText=message;Render();},authenticated);
                }else {
                    panel.Children.Add(PageTheme.Text(reportDraftReason+": "+reportDraftText,13));
                    panel.Children.Add(PageTheme.Text("Send this report to moderators? This does not change the mod's approval or installed files.",12,"#FFDAA6"));
                    Button(panel,"ConfirmPlayerReport","Send report",async()=>{string sha=reportSha,id=reportModId,version=reportVersion,reason=reportDraftReason,message=reportDraftText;reportDraftReason=null;reportDraftText=null;await Run(async token=>{var result=await Client.Report(sha,reason,message,token);Core.Require(result.ModId==id && result.Version==version,"The report receipt describes a different release. Refresh reports before retrying.");reports=new[]{result};reportDescription.Text="";},"Report recorded. Refresh your reports for moderator updates.",35);},authenticated);
                    Button(panel,"CancelPlayerReport","Back",()=>{reportDraftReason=null;reportDraftText=null;Render();});
                }
            }
            Button(panel,"RefreshPlayerReports","Refresh your reports",async()=>await FetchReports(0),authenticated);
            foreach(var report in reports)panel.Children.Add(PageTheme.Text(report.ModId+" @ "+report.Version+" · Report "+report.Id+" · "+report.State+" · "+report.Reason+" · "+report.Description+(report.Resolution.Length>0?" · Moderator: "+report.Resolution:""),13));
            if(reports.Length==20)Button(panel,"NextPlayerReports","Next reports",async()=>await FetchReports(reports.Last().Cursor),authenticated);
        }
        async Task FetchReports(long after)
        {reports=new WorkshopReport[0];await Run(async token=>{reports=await Client.Reports(after,false,token);},"Report status refreshed.",35);}
        async Task FetchModeratorReports(long after,bool history)
        {ClearModeratorReports();reportHistory=history;await Run(async token=>{moderatorReports=await Client.Reports(after,true,token,history);},"Moderator reports refreshed.",35);}
        void RenderModeratorReports()
        {
            var panel=PageTheme.Section(View,"Player reports",reportHistory?"Report history includes open and closed reports.":"Open concerns from signed-in players. Report decisions do not change release approval.");
            Button(panel,"RefreshModeratorReports","Open reports",async()=>await FetchModeratorReports(0,false));
            Button(panel,"ModeratorReportHistory","Report history",async()=>await FetchModeratorReports(0,true));
            foreach(var item in moderatorReports){var row=item;Button(panel,"ModeratorReport"+row.Id,row.ModId+" @ "+row.Version+" · "+row.Reason+" · "+row.State,()=>{selectedReport=row;reportDecision=null;reportDecisionNote=null;reportResolution.Text="";Render();});}
            if(moderatorReports.Length==20)Button(panel,"NextModeratorReports","Next reports",async()=>await FetchModeratorReports(moderatorReports.Last().Cursor,reportHistory));
            if(selectedReport==null)return;var report=selectedReport;
            panel.Children.Add(PageTheme.Text("Report "+report.Id+" · By "+report.Reporter+" · "+report.State,13,"#A7D9FA"));
            panel.Children.Add(PageTheme.Text(report.ModId+" @ "+report.Version+" · SHA-256: "+report.Sha256,12));
            panel.Children.Add(PageTheme.Text(report.Reason+": "+report.Description,13));
            if(report.Resolution.Length>0)panel.Children.Add(PageTheme.Text("Previous note: "+report.Resolution,13));
            Button(panel,"InspectReportedRelease","Inspect reported release",async()=>{ClearReviews();await Run(async token=>{selectedReview=await Client.ReportedRelease(report,token);scanEvidence=await Client.Evidence(selectedReview,token);},"Reported release loaded for inspection. No package decision was made.",35);});
            if(reportDecision==null) {
                panel.Children.Add(PageTheme.Text("Resolution note",13));panel.Children.Add(reportResolution);reportResolution.IsEnabled=stop==null;
                Button(panel,"ReviewReportDecision",report.State=="open"?"Review close":"Review reopen",()=>{string note=(reportResolution.Text??"").Trim();if(note.Length==0 || note.Length>1800 || note.Any(Char.IsControl)){status.Text="Enter a resolution note of 1 to 1800 characters.";return;}reportDecision=report.State=="open"?"close":"reopen";reportDecisionNote=note;Render();});
            }else {
                panel.Children.Add(PageTheme.Text(reportDecision+" report "+report.Id+"? Note: "+reportDecisionNote,13,"#FFDAA6"));
                Button(panel,"ConfirmReportDecision","Confirm "+reportDecision,async()=>{string action=reportDecision,note=reportDecisionNote;ClearModeratorReports();await Run(async token=>{selectedReport=await Client.ResolveReport(report,action,note,token);},"Report decision recorded. Package approval was preserved.",35);});
                Button(panel,"CancelReportDecision","Back",()=>{reportDecision=null;reportDecisionNote=null;Render();});
            }
        }
        async Task Reviews(long after)
        {ClearReviews();await Run(async token=>{reviews=await Client.Reviews(after,token);},"Moderation queue refreshed.",35);}
        void RenderModeration()
        {
            var panel=PageTheme.Section(View,"Moderation","Review the creator's source and package contents before deciding. This view shows the submitted manifest; it does not run or install quarantined content.");
            Button(panel,"RefreshModeratorUploads","Refresh moderation",async()=>await Reviews(0));
            foreach(var item in reviews) {
                var row=item;
                Button(panel,"ModeratorUpload"+row.Id,row.Package.Name+" @ "+row.Version+" · "+row.State,async()=>{selectedReview=row;scanEvidence=null;sourcePage=null;texturePreview=null;textureFiles.ItemsSource=null;audioPreview=null;audioFiles.ItemsSource=null;sourceFiles.ItemsSource=null;decision=null;decisionNote=null;reviewNote.Text="";await Run(async token=>{scanEvidence=await Client.Evidence(row,token);},"Scan evidence refreshed.",35);});
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
            var textures=review.Package.Files.Keys.Where(path=>path.StartsWith("textures/",StringComparison.Ordinal) && path.EndsWith(".dds",StringComparison.Ordinal)).OrderBy(path=>path,StringComparer.Ordinal).ToArray();
            if(textures.Length>0) {
                string chosen=textureFiles.SelectedItem as string;textureFiles.ItemsSource=textures;textureFiles.SelectedItem=textures.Contains(chosen)?chosen:textures[0];textureFiles.IsEnabled=stop==null;panel.Children.Add(textureFiles);
                Button(panel,"InspectModeratorTexture","Preview DDS texture",async()=>{texturePreview=null;await Run(async token=>{texturePreview=await Client.Texture(review,(string)textureFiles.SelectedItem,token);},"Texture preview loaded.",45);});
                if(texturePreview!=null) {
                    var picture=texturePreview;panel.Children.Add(PageTheme.Text(picture.Path+" · "+picture.Width+" × "+picture.Height+" · "+picture.Format+" · SHA-256: "+picture.PayloadSha256,12));
                    panel.Children.Add(PageTheme.Text("Base-level sample. Checkerboard shows transparency. Fine detail may be omitted; this is not a safety verdict.",12));
                    panel.Children.Add(picture.Image(out textureBitmap));
                }
            }
            var audio=review.Package.Files.Keys.Where(path=>path.StartsWith("audio/",StringComparison.Ordinal) && path.EndsWith(".wav",StringComparison.Ordinal)).OrderBy(path=>path,StringComparer.Ordinal).ToArray();
            if(audio.Length>0) {
                string chosen=audioFiles.SelectedItem as string;audioFiles.ItemsSource=audio;audioFiles.SelectedItem=audio.Contains(chosen)?chosen:audio[0];audioFiles.IsEnabled=stop==null;panel.Children.Add(audioFiles);
                Button(panel,"InspectModeratorAudio","Inspect PCM audio",async()=>await ReadAudio(review,(string)audioFiles.SelectedItem,0));
                if(audioPreview!=null) {
                    var clip=audioPreview;
                    panel.Children.Add(PageTheme.Text(clip.Path+" · PCM16 · "+clip.Rate+" Hz · "+clip.Channels+" channel(s) · "+(clip.Frames/(double)clip.Rate).ToString("0.###")+" seconds · SHA-256: "+clip.PayloadSha256,12));
                    panel.Children.Add(PageTheme.Text("Waveform: "+(clip.Offset/(double)clip.Rate).ToString("0.###")+"–"+(clip.NextOffset/(double)clip.Rate).ToString("0.###")+" seconds. Each lane shows one channel at full scale. Listening uses reduced preview gain and preserves system volume. Neither is a safety verdict.",12));
                    panel.Children.Add(clip.Image(out audioBitmap));
                    if(listening){var halt=Button(panel,"StopModeratorAudio","Stop listening",()=>{if(stop!=null)stop.Cancel();});halt.IsEnabled=stop!=null;}
                    else Button(panel,"PlayModeratorAudio","Listen to this portion",async()=>await Listen(clip));
                    if(clip.NextOffset<clip.Frames)Button(panel,"NextModeratorAudio","Next audio portion",async()=>await ReadAudio(review,clip.Path,clip.NextOffset));
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
        async Task ReadAudio(ModeratorUpload review,string path,long offset)
        {audioPreview=null;await Run(async token=>{audioPreview=await Client.Audio(review,path,offset,token);},"PCM audio loaded for visual inspection. No audio was played or installed.",45);}
        async Task Listen(ModeratorAudioPreview clip)
        {
            if(disposed || stop!=null)return;listening=true;
            try{await Run(token=>audioPlayer.Play(clip,token),"Audio portion finished. No game files were changed.",8,"Listening to this two-second audio portion…");}
            finally{listening=false;if(!disposed)Render();}
        }
        async Task Authenticate(bool redeem)
        {
            string name=username.Text??"",secret=Password,invite=Invitation;bool save=remember.IsChecked==true;ClearCredentials();ClearReviews();ClearReports();ClearInvitation();reviewed=null;uploads=new CreatorUpload[0];cursor=0;
            await Run(token=>redeem?Client.Redeem(invite,secret,token,save):Client.SignIn(name,secret,token,save),"Signed in. Review a ZIP to submit your addition.",45);
        }
        async Task RestoreSaved()
        {
            ClearCredentials();ClearReviews();ClearReports();reviewed=null;uploads=new CreatorUpload[0];cursor=0;
            await Run(async token=>{Core.Require(await Client.Resume(token),"No saved sign-in is available. Sign in with your workshop password.");},"Signed in using the system credential store.",40);
        }
        async Task History()
        {uploads=new CreatorUpload[0];await Run(async token=>{uploads=await Client.History(cursor,token);},"Upload status refreshed.",35);}
        async Task Run(Func<CancellationToken,Task> operation,string success,int seconds,string working=null)
        {
            if(disposed || stop!=null)return;
            if(preview){ClearCredentials();status.Text="Preview only. No account request or upload was made.";return;}
            using(var cancellation=new CancellationTokenSource(TimeSpan.FromSeconds(seconds))) {
                stop=cancellation;status.Text=working??"Working with the creator workshop…";Render();
                try{await operation(cancellation.Token);if(!disposed)status.Text=success;}
                catch(OperationCanceledException){if(!disposed)status.Text=listening?"Listening stopped.":"Request cancelled. Refresh status before retrying.";}
                catch(Exception error){if(!disposed)status.Text=error.Message;}
                finally{stop=null;if(!disposed){if(client!=null && !String.IsNullOrEmpty(client.VaultNotice))status.Text+=" "+client.VaultNotice;Render();}}
            }
        }
        public void Dispose(){if(disposed)return;disposed=true;if(textureBitmap!=null){textureBitmap.Dispose();textureBitmap=null;}if(audioBitmap!=null){audioBitmap.Dispose();audioBitmap=null;}ClearCredentials();ClearReviews();ClearReports();ClearInvitation();if(stop!=null)stop.Cancel();if(client!=null)client.Dispose();}
    }
}
